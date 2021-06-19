// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_RDP_CONNECTION_H_
#define NET_SERVER_RDP_CONNECTION_H_

#include <memory>
#include <string>

#include "net/server/http_connection.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "net/base/ip_endpoint.h"

#include "thread.hpp"
#include "serialization/string_utils.hpp"
#include <wml_exception.hpp>


namespace net {

class StreamSocket;
class RdpServer;
class SSLServerContext;

// A container which has all information of an http connection. It includes
// id, underlying socket, and pending read/write data.
class RdpConnection
{
public:
	RdpConnection(RdpServer& server, SSLServerContext& server_context, int id, std::unique_ptr<StreamSocket> socket);
	~RdpConnection();

	int id() const { return id_; }
	StreamSocket* socket() const { return socket_.get(); }
	const IPEndPoint& peer_ip() const { return peer_ip_; }
	HttpConnection::ReadIOBuffer* read_buf() const { return read_buf_.get(); }
	HttpConnection::QueuedWriteIOBuffer* write_buf() const { return write_buf_.get(); }

	int alert_buffer_threshold() const { return alert_buffer_threshold_; }

	void set_closing() { closing_ = true; }
	bool closing() const { return closing_; }

	void start_internal();
	int SendData(const std::string& data);
	bool write_buf_is_safe() const;
	bool write_buf_is_alert() const;
	bool should_disconnect(uint32_t now) const;

	int record_screen_pause_balance() const { return record_screen_pause_balance_; }
	void tune_record_screen_pause_balance(bool increase)
	{
		if (increase) {
			record_screen_pause_balance_ ++;
		} else {
			record_screen_pause_balance_ --;
		}
	}

	uint32_t shoule_disconnect_ticks() const { return shoule_disconnect_ticks_; }
	void set_shoule_disconnect_ticks(uint32_t ticks) { shoule_disconnect_ticks_ = ticks; }

	uint32_t create_ticks() const { return create_ticks_; }

	uint32_t handshaked_ticks() const { return handshaked_ticks_; }
	bool handshaked() const { return handshaked_ticks_ > 0; }

	void set_connectionfinished_ticks(uint32_t ticks)
	{ 
		VALIDATE(connectionfinished_ticks_ == 0 && ticks > handshaked_ticks_, null_str);
		connectionfinished_ticks_ = ticks;
	}
	uint32_t connectionfinished_ticks() const { return connectionfinished_ticks_; }
	bool connectionfinished() const { return connectionfinished_ticks_ > 0; }

	struct tdid_read_lock {
		tdid_read_lock(RdpConnection& _owner, const uint8_t* data, int size)
			: owner(_owner)
		{
			VALIDATE(owner.read_layer_ptr_ == nullptr && owner.read_layer_size_ == 0 && owner.read_layer_offset_ == 0, null_str);
			owner.read_layer_ptr_ = data;
			owner.read_layer_size_ = size;
		}
		int consumed() const
		{
			VALIDATE(owner.read_layer_ptr_ != nullptr && owner.read_layer_size_ >= owner.read_layer_offset_, null_str);
			return owner.read_layer_offset_;
		}
		~tdid_read_lock()
		{
			VALIDATE(owner.read_layer_ptr_ != nullptr && owner.read_layer_size_ >= owner.read_layer_offset_, null_str);
			owner.read_layer_ptr_ = nullptr;
			owner.read_layer_size_ = 0;
			owner.read_layer_offset_ = 0;
		}
		RdpConnection& owner;
	};

	int did_read_layer(uint8_t* data, int bytes);
	int did_write_layer(const uint8_t* data, int bytes);

private:
	friend class RdpServer;

	enum State {
		STATE_READ_CR,
		STATE_READ_CR_COMPLETE,
		STATE_WRITE_CC,
		STATE_WRITE_CC_COMPLETE,
		STATE_HANDSHAKE,
		STATE_HANDSHAKE_COMPLETE,
		STATE_READ_PDU,
		STATE_READ_PDU_COMPLETE,
		STATE_NONE,
	};

	void OnHandshakeComplete(int result);
	int DoLoop(int result);

	int DoReadCR();
	int DoReadCRComplete(int result);
	int DoWriteCC();
	int DoWriteCCComplete(int result);
	int DoHandshake();
	int DoHandshakeComplete(int result);
	int HandleHandshakeResult(int result);
	int DoReadPDU();
	int DoReadPDUComplete(int result);

	int DoReadLoop();
	void OnReadCompleted(int connection_id, int rv);
	int HandleReadResult(int rv);

	int DoWriteLoop(NetworkTrafficAnnotationTag traffic_annotation);
	void OnWriteCompleted(int connection_id,
						NetworkTrafficAnnotationTag traffic_annotation,
						int rv);
	int HandleWriteResult(int rv);

public:
	void* client_ptr;
	std::string cc_data;

	// The task runner of the sequence on which the watch was started.
	scoped_refptr<base::SequencedTaskRunner> task_runner_;

	uint32_t next_rtt_ticks;
	uint16_t next_rtt_sequence_number;

private:
	RdpServer& server_;
	SSLServerContext& server_context_;
	const int id_;
	std::unique_ptr<StreamSocket> socket_;
	IPEndPoint peer_ip_;
	const scoped_refptr<HttpConnection::ReadIOBuffer> read_buf_;
	const scoped_refptr<HttpConnection::QueuedWriteIOBuffer> write_buf_;

	bool closing_;
	uint32_t create_ticks_;
	uint32_t handshaked_ticks_;
	uint32_t connectionfinished_ticks_;
	State next_state_;

	base::WeakPtrFactory<RdpConnection> weak_ptr_factory_;

	const int max_write_buffer_size_;
	int alert_buffer_threshold_;

	uint32_t shoule_disconnect_ticks_;
	int record_screen_pause_balance_;

	const uint8_t* read_layer_ptr_;
	int read_layer_size_;
	int read_layer_offset_;

	DISALLOW_COPY_AND_ASSIGN(RdpConnection);
};

}  // namespace net

#endif  // NET_SERVER_RDP_CONNECTION_H_
