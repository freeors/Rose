// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/tcp_connection.h"

#include <utility>

#include "base/logging.h"
#include "net/socket/stream_socket.h"
#include "net/base/net_errors.h"
#include "net/server/tcp_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include <wml_exception.hpp>
#include <SDL_log.h>

namespace net {

TcpConnection::TcpConnection(TcpServer& server, SSLServerContext& server_context, int id, std::unique_ptr<StreamSocket> socket, bool ssl)
    : server_(server)
    , server_context_(server_context)
    , id_(id)
    , socket_(std::move(socket))
    , ssl_(ssl)
    , read_buf_(new HttpConnection::ReadIOBuffer())
    , write_buf_(new HttpConnection::QueuedWriteIOBuffer())
    , closing_(false)
    , create_ticks_(SDL_GetTicks())
    , handshaked_ticks_(0)
    , next_state_(STATE_NONE)
    , weak_ptr_factory_(this)
    , read_layer_ptr_(nullptr)
	, read_layer_size_(0)
	, read_layer_offset_(0)
    , max_write_buffer_size_(posix_align_ceil(3 * 1024 * 1024, 1024)) // 
    , shoule_disconnect_ticks_()
    , keepalive_ts_()
    , proto_ver_(nposm)
	, i32data_(nposm)
    , peer_ipv4_(0)

{
    SDL_Log("TcpConnection::TcpConnection [write buffer] ==> max: %3.1fM", 
            1.0 * max_write_buffer_size_ / (1024 * 1024));

    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    task_runner_ = base::SequencedTaskRunnerHandle::Get();

    socket_->GetPeerAddress(&peer_ip_);
	if (peer_ip_.address().IsIPv4()) {
		memcpy(&peer_ipv4_, peer_ip_.address().bytes().data(), 4);
	}
}

// TcpConnection::~TcpConnection() = default;
TcpConnection::~TcpConnection()
{
    SDL_Log("---TcpConnection::~TcpConnection---"); 
}

void TcpConnection::start_internal()
{
    // Need to write video data, need to be larger
	// even if 48M, when 4Mbps maybe too small. require
	write_buf_->set_max_buffer_size(max_write_buffer_size_);
	server_.delegate_->OnConnect(*this);

    DCHECK_EQ(next_state_, STATE_NONE);
    next_state_ = STATE_READ_PDU; // STATE_READ_CR
    DoLoop(OK);
}

int TcpConnection::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_READ_CR:
        DCHECK_EQ(OK, rv);
        rv = DoReadCR();
        break;
      case STATE_READ_CR_COMPLETE:
        rv = DoReadCRComplete(rv);
        break;
      case STATE_WRITE_CC:
        DCHECK_EQ(OK, rv);
        rv = DoWriteCC();
        break;
      case STATE_WRITE_CC_COMPLETE:
        rv = DoWriteCCComplete(rv);
        break;
      case STATE_HANDSHAKE:
        DCHECK_EQ(OK, rv);
        rv = DoHandshake();
        break;
      case STATE_HANDSHAKE_COMPLETE:
        rv = DoHandshakeComplete(rv);
        break;
      case STATE_READ_PDU:
        DCHECK_EQ(OK, rv);
        rv = DoReadPDU();
        break;
      case STATE_READ_PDU_COMPLETE:
        rv = DoReadPDUComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int TcpConnection::DoReadCR()
{
    next_state_ = STATE_READ_CR_COMPLETE;
    return DoReadLoop();
}

int TcpConnection::DoReadCRComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_WRITE_CC;
    return OK;
}

int TcpConnection::SendData(const std::string& data)
{
    VALIDATE_CALLED_ON_THIS_THREAD(server_.tid_);

    VALIDATE(!data.empty(), null_str);
    if (next_state_ == STATE_READ_CR_COMPLETE) {
        cc_data = data;
        return data.size();
    }

    {
		// SDL_Log("%u TcpConnection::SendData, data: %s", SDL_GetTicks(), data.c_str());

        const char* c_str= data.c_str();
        if (data.size() == 8 && c_str[0] == 'S' && c_str[1] == '3' && c_str[2] == '1' && c_str[3] == 'Q') {
		    int ii = 0;
		    // SDL_Log("%i, [strange-card]TcpConnection::SendData: %s", SDL_GetTicks(), c_str);
	    }
	}

    bool writing_in_progress = !write_buf_->IsEmpty();
    if (write_buf_->Append(data) && !writing_in_progress) {
        DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    }

    return data.size();
}

bool TcpConnection::should_disconnect(uint32_t now) const
{
    return shoule_disconnect_ticks_ != 0 && now >= shoule_disconnect_ticks_;
}

int TcpConnection::DoWriteCC()
{
    next_state_ = STATE_WRITE_CC_COMPLETE;
/*    
    uint8_t buf[19] = {0x03, 0x00, 0x00, 0x13, 0xe, 0xd0, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x02, 0x01, 0x08, 0x00, 0x01,
        0x00, 0x00, 0x00};
    std::string data((const char*)buf, sizeof(buf));
*/
    VALIDATE(!cc_data.empty(), null_str);
    bool writing_in_progress = !write_buf_->IsEmpty();
    VALIDATE(!writing_in_progress, null_str);
/*
    if (write_buf_->Append(data) && !writing_in_progress) {
        return DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    }
*/
    write_buf_->Append(cc_data);
    return DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
}

int TcpConnection::DoWriteCCComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_HANDSHAKE;
    return OK;
}

int TcpConnection::DoHandshake()
{
    next_state_ = STATE_HANDSHAKE_COMPLETE;
    std::unique_ptr<SSLServerSocket> server(
        server_context_.CreateSSLServerSocket(std::move(socket_)));

    int rv = server->Handshake(base::Bind(&TcpServer::OnConnectionHandshakeComplete,
                                           server_.weak_ptr_factory_.GetWeakPtr(), id_));
    socket_ = std::move(server);
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleHandshakeResult(rv);
    return rv;
}

void TcpConnection::OnHandshakeComplete(int result)
{
    // SDL_Log("TcpConnection::OnIOComplete, result: %i", result);
    VALIDATE(next_state_ == STATE_HANDSHAKE_COMPLETE, null_str);

    result = HandleHandshakeResult(result);
    DoLoop(result);
}

int TcpConnection::HandleHandshakeResult(int result)
{
    // 0(OK) is ok for SocketImpl::Handshake
    if (result < 0) {
        SDL_Log("TcpConnection::HandleHandshakeResult(rv: %i), will server_.Close()", result);
		server_.Close(id_);
		return result == 0 ? ERR_CONNECTION_CLOSED : result;
	}
    return OK;
}

int TcpConnection::DoHandshakeComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_READ_PDU;

    handshaked_ticks_ = SDL_GetTicks();
    return OK;
}

int TcpConnection::DoReadPDU()
{
    next_state_ = STATE_READ_PDU_COMPLETE;
    return DoReadLoop();
}

int TcpConnection::DoReadPDUComplete(int result)
{
    if (result != OK) {
        return result;
    }
    return DoReadPDU();
}

int TcpConnection::did_read_layer(uint8_t* data, int bytes)
{
	int read = SDL_min(bytes, read_layer_size_ - read_layer_offset_);
	if (read > 0) {
		memcpy(data, read_layer_ptr_ + read_layer_offset_, read);
		read_layer_offset_ += read;
	}
	return read;
}

int TcpConnection::did_write_layer(const uint8_t* data, int bytes)
{
	VALIDATE(bytes > 0, null_str);
	std::string data2((const char*)data, bytes);
	return SendData(data2);
}

int TcpConnection::DoReadLoop()
{
    // Increases read buffer size if necessary.
    HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
        SDL_Log("TcpConnection::DoReadLoop, will server_.Close()");
        server_.Close(id());
        return ERR_CONNECTION_CLOSED;
    }

    int buf_len = read_buf->RemainingCapacity();
    int rv = socket()->Read(
        read_buf,
        buf_len,
        base::Bind(&TcpServer::OnConnectionReadCompleted,
                    server_.weak_ptr_factory_.GetWeakPtr(), id()));
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleReadResult(rv);
    return rv;
}

void TcpConnection::OnReadCompleted(int connection_id, int rv)
{
    // SDL_Log("TcpConnection::OnReadCompleted, connection_id: %i, result: %i", connection_id, rv);
    VALIDATE(connection_id == id_, null_str);
    rv = HandleReadResult(rv);
    DoLoop(rv);
}

int TcpConnection::HandleReadResult(int rv)
{
	if (rv <= 0) {
        SDL_Log("TcpConnection::HandleReadResult(rv: %i), will server_.Close()", rv);
		server_.Close(id_);
		return rv == 0 ? ERR_CONNECTION_CLOSED : rv;
	}

	HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
	read_buf->DidRead(rv);

    const char* data_ptr = read_buf->StartOfBuffer();
    const int data_size = read_buf->GetSize();
    {
        // SDL_Log("TcpConnection::HandleReadResult, data_size: %i, peer: %s", data_size, peer.ToString().c_str());

        int consumed_size = server_.delegate_->OnTcpRequest(*this, data_ptr, data_size);
        VALIDATE(consumed_size <= data_size, null_str);
        if (consumed_size != 0) {
            read_buf->DidConsume(consumed_size);
        }
    }

	return OK;
}

int TcpConnection::DoWriteLoop(NetworkTrafficAnnotationTag traffic_annotation) {
    HttpConnection::QueuedWriteIOBuffer* write_buf = write_buf_.get();
    if (write_buf->GetSizeToWrite() == 0) {
        return OK;
    }

    int rv = socket()->Write(
        write_buf, write_buf->GetSizeToWrite(),
        base::Bind(&TcpServer::OnConnectionWriteCompleted,
                    server_.weak_ptr_factory_.GetWeakPtr(), id(),
                    traffic_annotation),
        traffic_annotation);
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleWriteResult(rv);
    return rv;
}

void TcpConnection::OnWriteCompleted(
    int connection_id,
    NetworkTrafficAnnotationTag traffic_annotation,
    int rv) {
  VALIDATE(connection_id == id_, null_str);

  rv = HandleWriteResult(rv);
  if (next_state_ == STATE_WRITE_CC_COMPLETE) {
    DoLoop(rv);
  }
}

int TcpConnection::HandleWriteResult(int rv)
{
	if (rv < 0) {
		SDL_Log("TcpConnection::HandleWriteResult(rv: %i), call server_.Close()", rv);
		server_.PostClose(*this);
		// server_.Close(id());
		return rv;
	}

    HttpConnection::QueuedWriteIOBuffer* write_buf = write_buf_.get();
	write_buf->DidConsume(rv);
    if (write_buf->GetSizeToWrite() != 0) {
        // If there's still data written, write it out.
        rv = DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    } else {
        rv = OK;
    }
	return rv;
}

}  // namespace net
