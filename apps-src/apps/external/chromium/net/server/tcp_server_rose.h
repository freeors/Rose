// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_TCP_SERVER_ROSE_H_
#define NET_SERVER_TCP_SERVER_ROSE_H_

#include "net/server/tcp_server.h"

#include "base_instance.hpp"
#include "serialization/string_utils.hpp"

// webrtc
#include "rtc_base/event.h"
#include "base/threading/thread.h"
#include <wml_exception.hpp>

namespace net {

// Bas on HttpServerTest of chromium.
class TcpServerRose: public TcpServer::Delegate
{
	friend class ttcpd_manager;
public:
	TcpServerRose(base::Thread& thread, base::WaitableEvent& e);
	~TcpServerRose();

	void SetUp(uint32_t ipaddr, uint16_t port);

	void TearDown();

	void OnConnect(TcpConnection& connection) override;

	int OnTcpRequest(TcpConnection& connection, const char* buf, int buf_len) override;

	void OnClose(TcpConnection& connection) override;

	TcpServer& server() { return *server_.get(); }
	const IPEndPoint& server_address() const { return server_address_; }
	const std::string& server_url() const { return server_url_; }

	void SendDataEx(uint32_t peer, const char* data, int len);

private:
	void did_connect_bh();
	void tcpd_slice(int timeout);
	void did_slice_quited(int timeout);

	virtual void app_OnConnect(TcpConnection& connection) {}
	virtual int app_OnTcpRequest(TcpConnection& connection, const char* data, int len) { return len; }

protected:
	base::Thread& thread_;
	base::WaitableEvent& delete_pend_tasks_e_;
	std::unique_ptr<TcpServer> server_;
	IPEndPoint server_address_;
	std::string server_url_;

private:
	base::WeakPtrFactory<TcpServerRose> weak_ptr_factory_;
	const int check_slice_timeout_;

	const int fake_peer_socket_;
	void* UpdateSubscriber_;

	bool slice_running_;
	uint32_t startup_verbose_ticks_;
	uint32_t last_verbose_ticks_;

	struct tsend_data {
		tsend_data(uint32_t peer, const std::string& data)
			: peer(peer)
			, data(data)
		{}

		const uint32_t peer;
		const std::string data;
	};
	threading::mutex main_thread_write_buf_mutex;
	std::vector<tsend_data> main_thread_write_buf;
};

class ttcpd_manager: public tserver_
{
public:
	ttcpd_manager(uint16_t port);

	const std::string& url() const override
	{ 
		return delegate_.get() != nullptr? delegate_->server_url(): null_str; 
	}

	TcpServerRose* tcp_delegate()
	{ 
		return delegate_.get();
	}

	TcpServer& tcp_server()
	{ 
		return *delegate_.get()->server_.get();
	}

	bool ip_is_connected(uint32_t ipv4) const;

private:
	virtual TcpServerRose* app_create_delegate() { return new TcpServerRose(*thread_.get(), e_); }

	void start(uint32_t ipaddr) override;
	void stop() override;

	void did_set_event();
	void start_internal(uint32_t ipaddr);
	void stop_internal();

protected:
	const uint16_t port_;
	std::unique_ptr<base::Thread> thread_;
	std::unique_ptr<TcpServerRose> delegate_;
	base::WaitableEvent e_;
};

}  // namespace net

#endif // NET_SERVER_TCP_SERVER_ROSE_H_
