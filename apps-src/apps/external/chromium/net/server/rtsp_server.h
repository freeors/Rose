// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_RTSP_SERVER_H_
#define NET_SERVER_RTSP_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"

namespace net {

class HttpConnection;
class HttpServerRequestInfo;
class HttpServerResponseInfo;
class IPEndPoint;
class ServerSocket;
class StreamSocket;

class RtspServerRequestInfo
{
public:
	RtspServerRequestInfo() {}
	~RtspServerRequestInfo() {}

	// Request peer address.
	IPEndPoint peer;
};

class RtspServer
{
public:
	// Delegate to handle http/websocket events. Beware that it is not safe to
	// destroy the RtspServer in any of these callbacks.
	class Delegate {
	public:
		virtual ~Delegate() {}

		virtual void OnConnect(int connection_id) = 0;
		virtual int OnRtspRequest(int connection_id, const char* buf, int buf_len) = 0;
		virtual void OnClose(int connection_id) = 0;
	};

	// Instantiates a http server with |server_socket| which already started
	// listening, but not accepting.  This constructor schedules accepting
	// connections asynchronously in case when |delegate| is not ready to get
	// callbacks yet.
	RtspServer(std::unique_ptr<ServerSocket> server_socket, RtspServer::Delegate* delegate);
	~RtspServer();

	// Sends the provided data directly to the given connection. No validation is
	// performed that data constitutes a valid HTTP response. A valid HTTP
	// response may be split across multiple calls to SendRaw.
	void SendRaw(int connection_id,
				const std::string& data,
				NetworkTrafficAnnotationTag traffic_annotation);
	// TODO(byungchul): Consider replacing function name with SendResponseInfo
	void SendResponse(int connection_id,
					const HttpServerResponseInfo& response,
					NetworkTrafficAnnotationTag traffic_annotation);
	void Send(int connection_id,
			HttpStatusCode status_code,
			const std::string& data,
			const std::string& mime_type,
			NetworkTrafficAnnotationTag traffic_annotation);
	bool SendRawBySocket(StreamSocket* netSock, const std::string& data, NetworkTrafficAnnotationTag traffic_annotation);

	void Close(int connection_id);

	void SetReceiveBufferSize(int connection_id, int32_t size);
	void SetSendBufferSize(int connection_id, int32_t size);

	// Copies the local address to |address|. Returns a network error code.
	int GetLocalAddress(IPEndPoint* address);

	StreamSocket* FindSocket(int connection_id);
	bool sessionid_is_valid(int connection_id) const;
	int connection_count() const { return id_to_connection_.size(); }
	void PostCloseConnection(int connection_id);

private:
	friend class RtspServerRose;

	void DoAcceptLoop();
	void OnAcceptCompleted(int rv);
	int HandleAcceptResult(int rv);

	void DoReadLoop(HttpConnection* connection);
	void OnReadCompleted(int connection_id, int rv);
	int HandleReadResult(HttpConnection* connection, int rv);

	bool DoWriteLoop(HttpConnection* connection,
					NetworkTrafficAnnotationTag traffic_annotation);
	void OnWriteCompleted(int connection_id,
						NetworkTrafficAnnotationTag traffic_annotation,
						int rv);
	int HandleWriteResult(HttpConnection* connection, int rv);

	// Expects the raw data to be stored in recv_data_. If parsing is successful,
	// will remove the data parsed from recv_data_, leaving only the unused
	// recv data. If all data has been consumed successfully, but the headers are
	// not fully parsed, *pos will be set to zero. Returns false if an error is
	// encountered while parsing, true otherwise.
	bool ParseHeaders(const char* data,
					size_t data_len,
					HttpServerRequestInfo* info,
					size_t* pos);

	HttpConnection* FindConnection(int connection_id);

	// Whether or not Close() has been called during delegate callback processing.
	bool HasClosedConnection(HttpConnection* connection);
	void PostCloseConnection(HttpConnection& connection);

private:
	const std::unique_ptr<ServerSocket> server_socket_;
	std::unique_ptr<StreamSocket> accepted_socket_;
	RtspServer::Delegate* const delegate_;

	int last_id_;
	std::map<int, std::unique_ptr<HttpConnection> > id_to_connection_;
	int fRequestBufferBytesLeftMinus1_;

	base::WeakPtrFactory<RtspServer> weak_ptr_factory_;

	DISALLOW_COPY_AND_ASSIGN(RtspServer);
};

}  // namespace net

#endif // NET_SERVER_HTTP_SERVER_H_
