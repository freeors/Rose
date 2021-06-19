// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/rtsp_server.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/server/http_connection.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

#include <liveMedia/include/Media.hh>
#include <SDL_stdinc.h>
#include <SDL_log.h>

#include "rose_config.hpp"

namespace net {

RtspServer::RtspServer(std::unique_ptr<ServerSocket> server_socket,
                       RtspServer::Delegate* delegate)
    : server_socket_(std::move(server_socket))
	, delegate_(delegate)
	, last_id_(0)
	, weak_ptr_factory_(this)
	, fRequestBufferBytesLeftMinus1_(1023)
{
	DCHECK(server_socket_);
	// Start accepting connections in next run loop in case when delegate is not
	// ready to get callbacks.
	base::ThreadTaskRunnerHandle::Get()->PostTask(
		FROM_HERE,
		base::Bind(&RtspServer::DoAcceptLoop, weak_ptr_factory_.GetWeakPtr()));
}

RtspServer::~RtspServer() = default;

void RtspServer::SendRaw(int connection_id,
                         const std::string& data,
                         NetworkTrafficAnnotationTag traffic_annotation) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection == NULL)
    return;

  bool writing_in_progress = !connection->write_buf()->IsEmpty();
  if (connection->write_buf()->Append(data) && !writing_in_progress)
    DoWriteLoop(connection, traffic_annotation);
}

bool RtspServer::SendRawBySocket(StreamSocket* netSock, const std::string& data, NetworkTrafficAnnotationTag traffic_annotation)
{
	HttpConnection* selected_connection = nullptr;
	for (std::map<int, std::unique_ptr<HttpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
		HttpConnection& connection = *it->second.get();
		if (connection.socket() == netSock) {
			selected_connection = &connection;
			break;
		}
	}

    if (selected_connection == nullptr) {
        return false;
    }

	if (selected_connection->closing()) {
		return false;
	}

	bool writing_in_progress = !selected_connection->write_buf()->IsEmpty();
	bool append_result = selected_connection->write_buf()->Append(data);
	if (append_result) {
		if (!writing_in_progress) {
			return DoWriteLoop(selected_connection, traffic_annotation);
		}
	} else {
		SDL_Log("connection->write_buf()->Append(data) fail, call PostCloseConnection");
		PostCloseConnection(*selected_connection);
	}
	return true;
}

void RtspServer::SendResponse(int connection_id,
                              const HttpServerResponseInfo& response,
                              NetworkTrafficAnnotationTag traffic_annotation) {
  SendRaw(connection_id, response.Serialize(), traffic_annotation);
}

void RtspServer::Send(int connection_id,
                      HttpStatusCode status_code,
                      const std::string& data,
                      const std::string& content_type,
                      NetworkTrafficAnnotationTag traffic_annotation) {
  HttpServerResponseInfo response(status_code);
  response.SetContentHeaders(data.size(), content_type);
  SendResponse(connection_id, response, traffic_annotation);
  SendRaw(connection_id, data, traffic_annotation);
}

void RtspServer::Close(int connection_id)
{
	SDL_Log("RtspServer::Close(%i), id_to_connection_.size: %i", connection_id, id_to_connection_.size());

	auto it = id_to_connection_.find(connection_id);
	if (it == id_to_connection_.end()) {
		return;
	}

	std::unique_ptr<HttpConnection> connection = std::move(it->second);
	id_to_connection_.erase(it);
	delegate_->OnClose(connection_id);

	// The call stack might have callbacks which still have the pointer of
	// connection. Instead of referencing connection with ID all the time,
	// destroys the connection in next run loop to make sure any pending
	// callbacks in the call stack return.
	base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, connection.release());
}

int RtspServer::GetLocalAddress(IPEndPoint* address) {
  return server_socket_->GetLocalAddress(address);
}

void RtspServer::SetReceiveBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->read_buf()->set_max_buffer_size(size);
}

void RtspServer::SetSendBufferSize(int connection_id, int32_t size) {
  HttpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->write_buf()->set_max_buffer_size(size);
}

void RtspServer::DoAcceptLoop() {
  int rv;
  do {
    rv = server_socket_->Accept(&accepted_socket_,
                                base::Bind(&RtspServer::OnAcceptCompleted,
                                           weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleAcceptResult(rv);
  } while (rv == OK);
}

void RtspServer::OnAcceptCompleted(int rv) {
  if (HandleAcceptResult(rv) == OK)
    DoAcceptLoop();
}

int RtspServer::HandleAcceptResult(int rv)
{
	if (rv < 0) {
		LOG(ERROR) << "Accept error: rv=" << rv;
		return rv;
	}

	if (last_id_ == INT_MAX) {
		last_id_ = 0;
	}
	++ last_id_;
	SDL_Log("RtspServer::HandleAcceptResult, connection's id: %i", last_id_);

	std::unique_ptr<HttpConnection> connection_ptr = std::make_unique<HttpConnection>(last_id_, std::move(accepted_socket_));
	HttpConnection* connection = connection_ptr.get();
	id_to_connection_[connection->id()] = std::move(connection_ptr);
	// 8 seconds * 8Mbps
	connection->write_buf()->set_max_buffer_size(posix_align_ceil(8 * 1000000, 1024));
	delegate_->OnConnect(connection->id());
	if (!HasClosedConnection(connection)) {
		DoReadLoop(connection);
	}
	return OK;
}

void RtspServer::DoReadLoop(HttpConnection* connection) {
  int rv;
  do {
    HttpConnection::ReadIOBuffer* read_buf = connection->read_buf();
    // Increases read buffer size if necessary.
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
      Close(connection->id());
      return;
    }

	int buf_len = SDL_min(fRequestBufferBytesLeftMinus1_, read_buf->RemainingCapacity());
    rv = connection->socket()->Read(
        read_buf,
        buf_len,
        base::Bind(&RtspServer::OnReadCompleted,
                   weak_ptr_factory_.GetWeakPtr(), connection->id()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleReadResult(connection, rv);
  } while (rv == OK);
}

void RtspServer::OnReadCompleted(int connection_id, int rv) {
  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by write error.
    return;

  if (HandleReadResult(connection, rv) == OK)
    DoReadLoop(connection);
}

int RtspServer::HandleReadResult(HttpConnection* connection, int rv)
{
	if (rv <= 0) {
		Close(connection->id());
		return rv == 0 ? ERR_CONNECTION_CLOSED : rv;
	}

	HttpConnection::ReadIOBuffer* read_buf = connection->read_buf();
	read_buf->DidRead(rv);

	// Handles http requests or websocket messages.
	RtspServerRequestInfo request;
	
	// Sets peer address if exists.
	connection->socket()->GetPeerAddress(&request.peer);
	fRequestBufferBytesLeftMinus1_ = delegate_->OnRtspRequest(connection->id(), read_buf->StartOfBuffer(), read_buf->GetSize());
	read_buf->DidConsume(read_buf->GetSize());
	if (HasClosedConnection(connection)) {
		return ERR_CONNECTION_CLOSED;
	}

	return OK;
}

bool RtspServer::DoWriteLoop(HttpConnection* connection,
                             NetworkTrafficAnnotationTag traffic_annotation) {
  int rv = OK;
  HttpConnection::QueuedWriteIOBuffer* write_buf = connection->write_buf();
  while (rv == OK && write_buf->GetSizeToWrite() > 0) {
    rv = connection->socket()->Write(
        write_buf, write_buf->GetSizeToWrite(),
        base::Bind(&RtspServer::OnWriteCompleted,
                   weak_ptr_factory_.GetWeakPtr(), connection->id(),
                   traffic_annotation),
        traffic_annotation);
    if (rv == ERR_IO_PENDING || rv == OK) {
	  if (rv == ERR_IO_PENDING) {
		  dbg_variable.fWritePendingTimes ++;
	  }
      return true;
	}
    rv = HandleWriteResult(connection, rv);
  }
  return rv == OK;
}

void RtspServer::OnWriteCompleted(
    int connection_id,
    NetworkTrafficAnnotationTag traffic_annotation,
    int rv) {
  HttpConnection* connection = FindConnection(connection_id);
  if (!connection)  // It might be closed right before by read error.
    return;

  if (HandleWriteResult(connection, rv) == OK)
    DoWriteLoop(connection, traffic_annotation);
}

void RtspServer::PostCloseConnection(HttpConnection& connection)
{
	connection.set_closing();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&RtspServer::Close, base::Unretained(this), connection.id()));	
}

void RtspServer::PostCloseConnection(int connection_id)
{
	HttpConnection* connection = FindConnection(connection_id);
	DCHECK(connection != nullptr);
	connection->set_closing();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&RtspServer::Close, base::Unretained(this), connection->id()));	
}

int RtspServer::HandleWriteResult(HttpConnection* connection, int rv)
{
	if (rv < 0) {
		SDL_Log("RtspServer::HandleWriteResult(..., %i), call PostCloseConnection", rv);
		PostCloseConnection(*connection);
		// Close(connection->id());
		return rv;
	}

	connection->write_buf()->DidConsume(rv);
	return OK;
}

namespace {

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_LWS,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method
  ST_URL,        // Receiving the URL
  ST_PROTO,      // Receiving the protocol
  ST_HEADER,     // Starting a Request Header
  ST_NAME,       // Receiving a request header name
  ST_SEPARATOR,  // Receiving the separator between header name and value
  ST_VALUE,      // Receiving a request header value
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
const int parser_state[MAX_STATES][MAX_INPUTS] = {
    /* METHOD    */ {ST_URL, ST_ERR, ST_ERR, ST_ERR, ST_METHOD},
    /* URL       */ {ST_PROTO, ST_ERR, ST_ERR, ST_URL, ST_URL},
    /* PROTOCOL  */ {ST_ERR, ST_HEADER, ST_NAME, ST_ERR, ST_PROTO},
    /* HEADER    */ {ST_ERR, ST_ERR, ST_NAME, ST_ERR, ST_ERR},
    /* NAME      */ {ST_SEPARATOR, ST_DONE, ST_ERR, ST_VALUE, ST_NAME},
    /* SEPARATOR */ {ST_SEPARATOR, ST_ERR, ST_ERR, ST_VALUE, ST_ERR},
    /* VALUE     */ {ST_VALUE, ST_HEADER, ST_NAME, ST_VALUE, ST_VALUE},
    /* DONE      */ {ST_DONE, ST_DONE, ST_DONE, ST_DONE, ST_DONE},
    /* ERR       */ {ST_ERR, ST_ERR, ST_ERR, ST_ERR, ST_ERR}};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch (ch) {
    case ' ':
    case '\t':
      return INPUT_LWS;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
  }
  return INPUT_DEFAULT;
}

}  // namespace

bool RtspServer::ParseHeaders(const char* data,
                              size_t data_len,
                              HttpServerRequestInfo* info,
                              size_t* ppos) {
  size_t& pos = *ppos;
  int state = ST_METHOD;
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = data[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    HttpServerRequestInfo::HeadersMap::iterator it;
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->path = buffer;
          buffer.clear();
          break;
        case ST_PROTO:
          if (buffer != "HTTP/1.1") {
            LOG(ERROR) << "Cannot handle request with protocol: " << buffer;
            next_state = ST_ERR;
          }
          buffer.clear();
          break;
        case ST_NAME:
          header_name = base::ToLowerASCII(buffer);
          buffer.clear();
          break;
        case ST_VALUE:
          base::TrimWhitespaceASCII(buffer, base::TRIM_LEADING, &header_value);
          it = info->headers.find(header_name);
          // See the second paragraph ("A sender MUST NOT generate multiple
          // header fields...") of tools.ietf.org/html/rfc7230#section-3.2.2.
          if (it == info->headers.end()) {
            info->headers[header_name] = header_value;
          } else {
            it->second.append(",");
            it->second.append(header_value);
          }
          buffer.clear();
          break;
        case ST_SEPARATOR:
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          // We got CR to get this far, also need the LF
          return (input == INPUT_LF);
        case ST_ERR:
          return false;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet. Signal this to
  // the caller by setting |pos| to zero.
  pos = 0;
  return true;
}

HttpConnection* RtspServer::FindConnection(int connection_id) {
  auto it = id_to_connection_.find(connection_id);
  if (it == id_to_connection_.end())
    return nullptr;
  return it->second.get();
}

StreamSocket* RtspServer::FindSocket(int connection_id) {
	HttpConnection* connection = FindConnection(connection_id);
	DCHECK(connection != nullptr);
	return connection->socket();
}

bool RtspServer::sessionid_is_valid(int connection_id) const
{
	return id_to_connection_.count(connection_id) != 0;
}

// This is called after any delegate callbacks are called to check if Close()
// has been called during callback processing. Using the pointer of connection,
// |connection| is safe here because Close() deletes the connection in next run
// loop.
bool RtspServer::HasClosedConnection(HttpConnection* connection) {
  return FindConnection(connection->id()) != connection;
}

}  // namespace net
