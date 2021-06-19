// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_RDP_SERVER_H_
#define NET_SERVER_RDP_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include <rtc_base/thread_checker.h>
#include "net/server/rdp_connection.h"

// webrtc
#include "serialization/string_utils.hpp"
#include "rtc_base/event.h"
#include "base/threading/thread.h"

#include "net/socket/ssl_server_socket.h"
#include "thread.hpp"

namespace net {

// class RdpConnection;
class IPEndPoint;
class ServerSocket;
class StreamSocket;

class RdpServerRequestInfo
{
public:
	RdpServerRequestInfo() {}
	~RdpServerRequestInfo() {}

	// Request peer address.
	IPEndPoint peer;
};

class RdpServer
{
public:
	// Delegate to handle http/websocket events. Beware that it is not safe to
	// destroy the RdpServer in any of these callbacks.
	class Delegate {
	public:
		virtual ~Delegate() {}

		virtual void OnConnect(RdpConnection& connection) = 0;
		virtual void OnRdpRequest(RdpConnection::tdid_read_lock& lock, RdpConnection& connection, const uint8_t* buf, int buf_len) = 0;
		virtual void OnClose(RdpConnection& connection) = 0;
	};

	// Instantiates a http server with |server_socket| which already started
	// listening, but not accepting.  This constructor schedules accepting
	// connections asynchronously in case when |delegate| is not ready to get
	// callbacks yet.
	RdpServer(std::unique_ptr<ServerSocket> server_socket, RdpServer::Delegate* delegate);
	~RdpServer();

	void Close(int connection_id);

	void SetReceiveBufferSize(int connection_id, int32_t size);
	void SetSendBufferSize(int connection_id, int32_t size);

	// Copies the local address to |address|. Returns a network error code.
	int GetLocalAddress(IPEndPoint* address);

	StreamSocket* FindSocket(int connection_id);
	bool sessionid_is_valid(int connection_id) const;
	int connection_count() const { return id_to_connection_.size(); }
	int normal_connection_count() const;
	RdpConnection* FindFirstNormalConnection();
	RdpConnection* FindNormalConnection(int connection_id);
	RdpConnection* FindFirstHandshakedConnection();
	RdpConnection* FindFirstConnectionfinishedConnection();
	// void PostClose(int connection_id);

private:
	friend class RdpServerRose;
	friend class RdpConnection;

	void DoAcceptLoop();
	void OnAcceptCompleted(int rv);
	int HandleAcceptResult(int rv);

	void OnConnectionHandshakeComplete(int connection_id, int result);
	void OnConnectionReadCompleted(int connection_id, int rv);
	void OnConnectionWriteCompleted(int connection_id, NetworkTrafficAnnotationTag traffic_annotation, int rv);

	// Whether or not Close() has been called during delegate callback processing.
	// bool HasClosedConnection(RdpConnection* connection);
	void PostClose(RdpConnection& connection);

	void CloseAllConnection();

private:
	rtc::ThreadChecker thread_checker_;
	const std::unique_ptr<ServerSocket> server_socket_;
	std::unique_ptr<StreamSocket> accepted_socket_;
	RdpServer::Delegate* const delegate_;

	int last_id_;
	std::map<int, std::unique_ptr<RdpConnection> > id_to_connection_;
	int fRequestBufferBytesLeftMinus1_;

	base::WeakPtrFactory<RdpServer> weak_ptr_factory_;

	std::unique_ptr<SSLServerContext> server_context_;
	threading::mutex connections_mutex_;

	DISALLOW_COPY_AND_ASSIGN(RdpServer);
};

}  // namespace net

#endif // NET_SERVER_RDP_SERVER_H_
