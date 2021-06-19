// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/rdp_connection.h"

#include <utility>

#include "base/logging.h"
#include "net/socket/stream_socket.h"
#include "net/base/net_errors.h"
#include "net/server/rdp_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include <wml_exception.hpp>
#include <SDL_log.h>

namespace net {

RdpConnection::RdpConnection(RdpServer& server, SSLServerContext& server_context, int id, std::unique_ptr<StreamSocket> socket)
    : server_(server)
    , server_context_(server_context)
    , id_(id)
    , socket_(std::move(socket))
    , read_buf_(new HttpConnection::ReadIOBuffer())
    , write_buf_(new HttpConnection::QueuedWriteIOBuffer())
    , closing_(false)
    , create_ticks_(SDL_GetTicks())
    , handshaked_ticks_(0)
    , connectionfinished_ticks_(0)
    , next_state_(STATE_NONE)
    , weak_ptr_factory_(this)
    , client_ptr(nullptr)
    , read_layer_ptr_(nullptr)
	, read_layer_size_(0)
	, read_layer_offset_(0)
    , max_write_buffer_size_(posix_align_ceil(3 * 1024 * 1024, 1024)) // 
    , shoule_disconnect_ticks_(0)
    , record_screen_pause_balance_(0)
    , next_rtt_ticks(0)
	, next_rtt_sequence_number(0)

{
    alert_buffer_threshold_ = max_write_buffer_size_ * 1 / 4;

    SDL_Log("RdpConnection::RdpConnection [write buffer] ==> max: %3.1fM alert: %3.2f", 
            1.0 * max_write_buffer_size_ / (1024 * 1024),
            1.0 * alert_buffer_threshold_ /  (1024 * 1024));

    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    task_runner_ = base::SequencedTaskRunnerHandle::Get();

    socket_->GetPeerAddress(&peer_ip_);
}

// RdpConnection::~RdpConnection() = default;
RdpConnection::~RdpConnection()
{
    SDL_Log("---RdpConnection::~RdpConnection---"); 
}

void RdpConnection::start_internal()
{
    // Need to write video data, need to be larger
	// even if 48M, when 4Mbps maybe too small. require
	write_buf_->set_max_buffer_size(max_write_buffer_size_);
	server_.delegate_->OnConnect(*this);

    DCHECK_EQ(next_state_, STATE_NONE);
    next_state_ = STATE_READ_CR;
    DoLoop(OK);
}

int RdpConnection::DoLoop(int result) {
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

int RdpConnection::DoReadCR()
{
    next_state_ = STATE_READ_CR_COMPLETE;
    return DoReadLoop();
}

int RdpConnection::DoReadCRComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_WRITE_CC;
    return OK;
}

int RdpConnection::SendData(const std::string& data)
{
    // DCHECK_CALLED_ON_VALID_THREAD(server_.thread_checker_);

    VALIDATE(!data.empty(), null_str);
    if (next_state_ == STATE_READ_CR_COMPLETE) {
        cc_data = data;
        return data.size();
    }

    bool writing_in_progress = !write_buf_->IsEmpty();
    if (write_buf_->Append(data) && !writing_in_progress) {
        if (data.size() == 27) {
            const uint8_t* data2 = (const uint8_t*)data.c_str();
            if (data2[23] == 0x50 && data2[24] == 0x00 && data2[25] == 0x01 && data2[26] == 0x00) {
                int ii = 0;
                SDL_Log("SendData, DVC Capabilities Request PDU, write_buf's length: %i", write_buf_->GetSizeToWrite());
            }
        }
        DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    }

    return data.size();
}

bool RdpConnection::write_buf_is_alert() const
{
    return write_buf_->total_size() >= alert_buffer_threshold_;
}

bool RdpConnection::should_disconnect(uint32_t now) const
{
    return shoule_disconnect_ticks_ != 0 && now >= shoule_disconnect_ticks_;
}

int RdpConnection::DoWriteCC()
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

int RdpConnection::DoWriteCCComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_HANDSHAKE;
    return OK;
}

int RdpConnection::DoHandshake()
{
    next_state_ = STATE_HANDSHAKE_COMPLETE;
    std::unique_ptr<SSLServerSocket> server(
        server_context_.CreateSSLServerSocket(std::move(socket_)));

    int rv = server->Handshake(base::Bind(&RdpServer::OnConnectionHandshakeComplete,
                                           server_.weak_ptr_factory_.GetWeakPtr(), id_));
    socket_ = std::move(server);
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleHandshakeResult(rv);
    return rv;
}

void RdpConnection::OnHandshakeComplete(int result)
{
    // SDL_Log("RdpConnection::OnIOComplete, result: %i", result);
    VALIDATE(next_state_ == STATE_HANDSHAKE_COMPLETE, null_str);

    result = HandleHandshakeResult(result);
    DoLoop(result);
}

int RdpConnection::HandleHandshakeResult(int result)
{
    // 0(OK) is ok for SocketImpl::Handshake
    if (result < 0) {
        SDL_Log("RdpConnection::HandleHandshakeResult(rv: %i), will server_.Close()", result);
		server_.Close(id_);
		return result == 0 ? ERR_CONNECTION_CLOSED : result;
	}
    return OK;
}

int RdpConnection::DoHandshakeComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_READ_PDU;

    handshaked_ticks_ = SDL_GetTicks();
    return OK;
}

int RdpConnection::DoReadPDU()
{
    next_state_ = STATE_READ_PDU_COMPLETE;
    return DoReadLoop();
}

int RdpConnection::DoReadPDUComplete(int result)
{
    if (result != OK) {
        return result;
    }
    return DoReadPDU();
}

int RdpConnection::did_read_layer(uint8_t* data, int bytes)
{
	int read = SDL_min(bytes, read_layer_size_ - read_layer_offset_);
	if (read > 0) {
		memcpy(data, read_layer_ptr_ + read_layer_offset_, read);
		read_layer_offset_ += read;
	}
	return read;
}

int RdpConnection::did_write_layer(const uint8_t* data, int bytes)
{
	VALIDATE(bytes > 0, null_str);
	std::string data2((const char*)data, bytes);
	return SendData(data2);
}

int RdpConnection::DoReadLoop()
{
    // Increases read buffer size if necessary.
    HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
        SDL_Log("RdpConnection::DoReadLoop, will server_.Close()");
        server_.Close(id());
        return ERR_CONNECTION_CLOSED;
    }

    int buf_len = read_buf->RemainingCapacity();
    int rv = socket()->Read(
        read_buf,
        buf_len,
        base::Bind(&RdpServer::OnConnectionReadCompleted,
                    server_.weak_ptr_factory_.GetWeakPtr(), id()));
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleReadResult(rv);
    return rv;
}

void RdpConnection::OnReadCompleted(int connection_id, int rv)
{
    // SDL_Log("RdpConnection::OnReadCompleted, connection_id: %i, result: %i", connection_id, rv);
    VALIDATE(connection_id == id_, null_str);
    rv = HandleReadResult(rv);
    DoLoop(rv);
}

int RdpConnection::HandleReadResult(int rv)
{
	if (rv <= 0) {
        SDL_Log("RdpConnection::HandleReadResult(rv: %i), will server_.Close()", rv);
		server_.Close(id_);
		return rv == 0 ? ERR_CONNECTION_CLOSED : rv;
	}

	HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
	read_buf->DidRead(rv);
/*
	IPEndPoint peer;	
	// Sets peer address if exists.
	socket()->GetPeerAddress(&peer);
*/ 
    const char* data_ptr = read_buf->StartOfBuffer();
    const int data_size = read_buf->GetSize();
    {
        RdpConnection::tdid_read_lock lock(*this, (const uint8_t*)data_ptr, data_size);
        // SDL_Log("RdpConnection::HandleReadResult, data_size: %i", data_size);

        server_.delegate_->OnRdpRequest(lock, *this, (const uint8_t*)data_ptr, data_size);
        int consumed_size = lock.consumed();
        VALIDATE(consumed_size <= data_size, null_str);
        if (consumed_size != 0) {
            if (consumed_size != data_size) {
                int ii = 0;
            }
            read_buf->DidConsume(consumed_size);
        }
    }

	return OK;
}

int RdpConnection::DoWriteLoop(NetworkTrafficAnnotationTag traffic_annotation) {
    HttpConnection::QueuedWriteIOBuffer* write_buf = write_buf_.get();
    if (write_buf->GetSizeToWrite() == 0) {
        return OK;
    }

    int rv = socket()->Write(
        write_buf, write_buf->GetSizeToWrite(),
        base::Bind(&RdpServer::OnConnectionWriteCompleted,
                    server_.weak_ptr_factory_.GetWeakPtr(), id(),
                    traffic_annotation),
        traffic_annotation);
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleWriteResult(rv);
    return rv;
}

void RdpConnection::OnWriteCompleted(
    int connection_id,
    NetworkTrafficAnnotationTag traffic_annotation,
    int rv) {
  VALIDATE(connection_id == id_, null_str);

  rv = HandleWriteResult(rv);
  if (next_state_ == STATE_WRITE_CC_COMPLETE) {
    DoLoop(rv);
  }
}

int RdpConnection::HandleWriteResult(int rv)
{
	if (rv < 0) {
		SDL_Log("RdpConnection::HandleWriteResult(rv: %i), call server_.Close()", rv);
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
