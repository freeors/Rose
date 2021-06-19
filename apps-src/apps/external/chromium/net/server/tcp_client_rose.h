// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_TCP_CLIENT_ROSE_H_
#define NET_SERVER_TCP_CLIENT_ROSE_H_

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
#include <net/base/address_list.h>
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/dns/host_resolver.h"
#include "net/server/http_connection.h" // will use met::HttpConnection::QueuedWriteIOBuffer

#include <net/cert/mock_cert_verifier.h>
#include <net/http/transport_security_state.h>
#include <net/cert/do_nothing_ct_verifier.h>
#include <net/cert/ct_policy_enforcer.h>
#include <net/socket/ssl_client_socket.h>

// webrtc
#include "serialization/string_utils.hpp"
#include "rtc_base/event.h"
#include "base/threading/thread.h"

#include "gui/dialogs/dialog.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"

namespace net {

class TcpConnection;
class IPEndPoint;
class ServerSocket;
class StreamSocket;
class ClientSocketFactory;

// A mock CTPolicyEnforcer that returns a custom verification result.
class MockCTPolicyEnforcer : public CTPolicyEnforcer
{
 public:
/*
  MOCK_METHOD3(CheckCompliance,
               ct::CTPolicyCompliance(X509Certificate* cert,
                                      const ct::SCTList&,
                                      const NetLogWithSource&));
*/
};

class TcpClient
{
public:
	// Delegate to handle http/websocket events. Beware that it is not safe to
	// destroy the TcpServer in any of these callbacks.
	class Delegate {
	public:
		virtual ~Delegate() {}

		virtual void OnResult(int rv) = 0;
		virtual int OnTcpRequest(const char* data, int len) = 0;
		virtual void OnConnect() = 0;
		virtual void OnClose() = 0;
	};

	// Instantiates a http server with |server_socket| which already started
	// listening, but not accepting.  This constructor schedules accepting
	// connections asynchronously in case when |delegate| is not ready to get
	// callbacks yet.
	TcpClient(const HostPortPair& host_port_pair, TcpClient::Delegate* delegate);
	~TcpClient();

	void start_internal();
	int SendData(const std::string& data);
	StreamSocket* socket() const { return ctrl_socket_.get(); }
	void Close(int rv);

	struct tdid_read_lock {
		tdid_read_lock(TcpClient& _owner, const uint8_t* data, int size)
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
		TcpClient& owner;
	};
	int did_read_layer(uint8_t* data, int bytes);
	int did_write_layer(const uint8_t* data, int bytes);

private:
	friend class TcpClientRose;

	enum State {
		STATE_RESOLVE_HOST,
		STATE_RESOLVE_HOST_COMPLETE,
		STATE_TRANSPORT_CONNECT,
		STATE_TRANSPORT_CONNECT_COMPLETE,
		STATE_HANDSHAKE,
		STATE_HANDSHAKE_COMPLETE,
		STATE_READ_PDU,
		STATE_READ_PDU_COMPLETE,
		STATE_NONE,
	};

	void OnHandshakeComplete(int result);
	void OnIOComplete(int result);
	int DoLoop(int result);

	int DoResolveHost();
	int DoResolveHostComplete(int result);
	int DoTransportConnect();
	int DoTransportConnectComplete(int result);
	int DoHandshake();
	int DoHandshakeComplete(int result);
	int HandleHandshakeResult(int result);
	int DoReadPDU();
	int DoReadPDUComplete(int result);

	net::StreamSocket* getNetSock() { return ctrl_socket_.get(); }
	int sendData_chromium(net::StreamSocket* netSock, const uint8_t* data, int dataSize);

	int DoReadLoop();
	void OnReadCompleted(int rv);
	int HandleReadResult(int rv);

	int DoWriteLoop(NetworkTrafficAnnotationTag traffic_annotation);
	void OnWriteCompleted(NetworkTrafficAnnotationTag traffic_annotation,
						int rv);
	int HandleWriteResult(int rv);

public:
	// rdpContext& rdp_context;
	threading::mutex main_thread_write_buf_mutex;
	std::vector<std::string> main_thread_write_buf;

private:
	rtc::ThreadChecker thread_checker_;
	SDL_threadID tid_;
	// const IPEndPoint& server_address_;
	const HostPortPair& host_port_pair_;
	TcpClient::Delegate* const delegate_;

	base::WeakPtrFactory<TcpClient> weak_ptr_factory_;

	uint32_t create_ticks_;
	uint32_t handshaked_ticks_;

	AddressList addresses_;
	NetLogWithSource net_log2_;

	NetLogWithSource net_log_dns_;
	std::unique_ptr<HostResolver> host_resolver_;
	std::unique_ptr<HostResolver::ResolveHostRequest> resolve_request_;

/*
	scoped_refptr<net::IOBufferWithSize> write_buf_;
*/
	State next_state_;
/*
	int resolve_result_;
*/
	ClientSocketFactory* const socket_factory_;
/*
	bool use_chromium_;
	responseHandler* describeResponseHandler_;
	Authenticator* describeAuthenticator_;
	int socket_op_result_;
*/
	std::unique_ptr<StreamSocket> ctrl_socket_;
/*
	net::StreamSocket* fOutputSocket_;
	bool read_by_rtpinterface_;
*/
	const int max_read_buffer_size_;
	const int max_write_buffer_size_;
	const scoped_refptr<HttpConnection::ReadIOBuffer> read_buf_;
	const scoped_refptr<HttpConnection::QueuedWriteIOBuffer> write_buf_;

	std::unique_ptr<SSLConfigService> ssl_config_service_;
	std::unique_ptr<MockCertVerifier> cert_verifier_;
	std::unique_ptr<TransportSecurityState> transport_security_state_;
	std::unique_ptr<DoNothingCTVerifier> ct_verifier_;
	std::unique_ptr<DefaultCTPolicyEnforcer> ct_policy_enforcer_;
	std::unique_ptr<SSLClientSessionCache> ssl_client_session_cache_;
	std::unique_ptr<SSLClientContext> context_;
	std::unique_ptr<SSLClientSocket> sock_;

	const uint8_t* read_layer_ptr_;
	int read_layer_size_;
	int read_layer_offset_;

	DISALLOW_COPY_AND_ASSIGN(TcpClient);
};

class TcpClientRose: public TcpClient::Delegate
{
	friend class ttcpc_manager;
public:
	TcpClientRose(base::Thread& thread, base::WaitableEvent& e);
	~TcpClientRose();

	void SetUp(const std::string& host, uint16_t port);

	void OnConnect() override;
	void OnClose() override;
	void OnResult(int rv) override;
	int OnTcpRequest(const char* data, int len) override;

	TcpClient* client() { return client_.get(); }

private:
	// void did_connect_bh();
	void tcpc_slice(int timeout);
	void did_slice_quited(int timeout);

	virtual int app_OnTcpRequest(const char* data, int len) = 0;

protected:
	base::Thread& thread_;
	base::WaitableEvent& delete_pend_tasks_e_;
	std::unique_ptr<TcpClient> client_;
	// IPEndPoint server_address_;
	HostPortPair host_port_pair_;
	std::string server_url_;
	int timeout_; // unit: millisecond
	bool connected_;
	int rv_;
	uint32_t should_disconnect_ticks_;
	uint64_t total_read_bytes_;

private:
	base::WeakPtrFactory<TcpClientRose> weak_ptr_factory_;
	const int check_slice_timeout_;
	bool slice_running_;
};

class ttcpc_manager
{
public:
	ttcpc_manager(const std::function<TcpClientRose* (base::Thread&, base::WaitableEvent&)>& create);
	~ttcpc_manager();

	const std::string& url() const
	{ 
		// return delegate_.get() != nullptr? delegate_->server_url(): null_str; 
		return null_str;
	}

	TcpClient& tcp_client() 
	{ 
		return *delegate_.get()->client_.get();
	}

	TcpClientRose& tcp_delegate() 
	{ 
		return *delegate_.get();
	}

	void start(const std::string& host, uint16_t port, int timeout);
	void stop();

	bool xmit_transaction(const std::string& host, uint16_t port, const tuint8cdata& req, int timeout);

private:
	void did_set_event();
	void start_internal(const std::string& host, uint16_t port, int timeout);
	void stop_internal();

	bool xmit_transaction_internal(gui2::tprogress_& progress, const std::string& host, uint16_t port, const tuint8cdata& req, int timeout);
	// impletement example: 
	//    return new TcpClientApp(thread);
	// TcpClientApp is derived from TcpClientRose.
	// virtual TcpClientRose* app_create_delegate(base::Thread& thread, base::WaitableEvent& e) = 0;

private:
	const std::function<TcpClientRose* (base::Thread& thread, base::WaitableEvent& e)> create_delegate_;
	std::unique_ptr<base::Thread> thread_;
	std::unique_ptr<TcpClientRose> delegate_;
	base::WaitableEvent e_;
};

//
// default impletement
//
class TcpClientDefault: public TcpClientRose
{
public:
	TcpClientDefault(base::Thread& thread, base::WaitableEvent& e);

private:
	int app_OnTcpRequest(const char* data, int len) override;
};

TcpClientRose* app_create_delegate_default(base::Thread& thread, base::WaitableEvent& e);

}  // namespace net

#endif // NET_SERVER_TCP_CLIENT_ROSE_H_
