// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "tcp_server_rose.h"

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
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

#include <SDL_stdinc.h>
#include <SDL_log.h>

#include "rose_config.hpp"
#include "base_instance.hpp"
#include "gui/widgets/grid.hpp"

#include "base/files/file_util.h"
#include "net/cert/x509_certificate.h"
#include "crypto/rsa_private_key.h"
#include "net/ssl/ssl_server_config.h"

#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "json/json.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "base/strings/utf_string_conversions.h"

using namespace std::placeholders;

namespace net {

TcpServerRose::TcpServerRose(base::Thread& thread, base::WaitableEvent& e)
	: thread_(thread)
	, delete_pend_tasks_e_(e)
	, fake_peer_socket_(10)
	, weak_ptr_factory_(this)
	, check_slice_timeout_(300) // 300 ms
	, slice_running_(false)
	, startup_verbose_ticks_(0)
	, last_verbose_ticks_(0)
{
}

TcpServerRose::~TcpServerRose()
{
	VALIDATE_CALLED_ON_THIS_THREAD(server_->tid_);

	SDL_Log("TcpServerRose::~TcpServerRose()---");
	// don't call serve_.reset(), after server_->Release(), some require it's some variable keep valid.
	server_->CloseAllConnection();
	thread_.SetRequireDeletePendingTasks(delete_pend_tasks_e_);
	SDL_Log("---TcpServerRose::~TcpServerRose() X");
}

void TcpServerRose::SetUp(uint32_t ipaddr, uint16_t port)
{
	std::unique_ptr<ServerSocket> server_socket(new TCPServerSocket(NULL, NetLogSource()));
	// server_socket->ListenWithAddressAndPort("127.0.0.1", 8080, 1);

	// Must not use 127.0.0.1
	// make sure we have a value in a known byte order: big endian
	uint32_t addrNBO = htonl(ipaddr);
	std::stringstream addr_ss;
	addr_ss << (int)((addrNBO >> 24) & 0xFF) << "." << (int)((addrNBO>>16) & 0xFF);
	addr_ss << "." << (int)((addrNBO>>8)&0xFF) << "." << (int)(addrNBO & 0xFF);
	server_socket->ListenWithAddressAndPort(addr_ss.str(), port, 1);

	server_.reset(new TcpServer(std::move(server_socket), this));
	server_->GetLocalAddress(&server_address_);
	server_url_ = server_address_.ToString();
}

void TcpServerRose::TearDown()
{
	// Run the event loop some to make sure that the memory handed over to
	// DeleteSoon gets fully freed.
	base::RunLoop().RunUntilIdle();
}

void TcpServerRose::did_slice_quited(int timeout1)
{
	VALIDATE(server_.get() != nullptr, null_str);
	if (server_->connection_count() == 0) {
		SDL_Log("will stop tcpd_slice");
		slice_running_ = false;
		return;
	}

	const base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(20); // 20 ms.
	base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::Bind(&TcpServerRose::tcpd_slice, weak_ptr_factory_.GetWeakPtr(), timeout1), timeout);
}

void TcpServerRose::tcpd_slice(int timeout)
{
	VALIDATE_CALLED_ON_THIS_THREAD(server_->tid_);
	
	tauto_destruct_executor destruct_executor(std::bind(&TcpServerRose::did_slice_quited, this, timeout));

	uint32_t now = SDL_GetTicks();
	std::map<int, std::unique_ptr<TcpConnection> >& connections = server_->connections();
	for (std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = connections.begin(); it != connections.end(); ++ it) {
        TcpConnection* connection = it->second.get();
        if (connection->closing()) {
           continue;
        }
		if (connection->should_disconnect(now)) {
			SDL_Log("%u tcpd_slice(%i) hasn't receive keepalive over threadhold seconds, think as disconnect", 
				now, connection->id());
			server_->Close(connection->id());
			// for keepalive, one slice max disconnect one connection.
			break;
		}
    }

	if (server_.get() != nullptr) {
		if (!main_thread_write_buf.empty()) {
			threading::lock lock2(main_thread_write_buf_mutex);
			std::vector<tsend_data>& write_buf = main_thread_write_buf;
			const std::map<int, std::unique_ptr<TcpConnection> >& connections = server_->connections();
			for (std::vector<tsend_data>::const_iterator it  = write_buf.begin(); it != write_buf.end(); ++ it) {
				const tsend_data& data = *it;
				for (std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = connections.begin(); it != connections.end(); ++ it) {
					TcpConnection* connection = it->second.get();
					if (connection->closing()) {
					   continue;
					}
					if (connection->peer_ipv4_ == data.peer) {
						connection->SendData(data.data);
						break;
					}
				}
			}
			write_buf.clear();
		}
	}
}

void TcpServerRose::OnConnect(TcpConnection& connection)
{
	VALIDATE_CALLED_ON_THIS_THREAD(server_->tid_);

	app_OnConnect(connection);

	if (!slice_running_) {
		SDL_Log("will run rdpd_slice");
		slice_running_ = true;
		base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&TcpServerRose::tcpd_slice, weak_ptr_factory_.GetWeakPtr(), check_slice_timeout_));
	}
}

int TcpServerRose::OnTcpRequest(TcpConnection& connection, const char* buf, int buf_len)
{
	VALIDATE_CALLED_ON_THIS_THREAD(server_->tid_);
	// Do not use synchronous operations with the main thread, such as webrtc's Send.
	// Main thread may block because gui, it will result TcpdThread thread all the time.
	return app_OnTcpRequest(connection, buf, buf_len);
}

void TcpServerRose::OnClose(TcpConnection& connection)
{
	VALIDATE_CALLED_ON_THIS_THREAD(server_->tid_);
/*
	freerdp_peer* client = static_cast<freerdp_peer*>(connection.client_ptr);
	SDL_Log("TcpServerRose::Close(%i)--- client: %p", connection.id(), client);
	if (client != nullptr) {
		rose_did_shadow_peer_disconnect(freerdp_server_, client, &gfxstatus_, UpdateSubscriber_);
		SDL_Log("TcpServerRose::Close(%i) pre rose_shadow_subsystem_stop", connection.id());
		rose_shadow_subsystem_stop(freerdp_server_->subsystem);
		connection.client_ptr = nullptr;
	}

	freerdp_server_->rose_delegate = nullptr;
*/
	SDL_Log("------TcpServerRose::Close(%i) X", connection.id());
}

void TcpServerRose::SendDataEx(uint32_t peer, const char* data, int len)
{
	VALIDATE(len > 0, null_str);
	if (server_->connection_count() == 0) {
		return;
	}

	std::string data2((const char*)data, len);
	SDL_threadID id = SDL_ThreadID();
	// if send without peer_ipv4, it should use TcpConnection::SendData directly.
	VALIDATE(server_->tid_ != id, null_str);
	if (server_->tid_ != id) {
		// on android, as if in main-thread, thread_checker_.CalledOnValidThread() always return true!
		// I had to use specialized variable(in_main_thread) that current is in main thread.
		VALIDATE_IN_MAIN_THREAD();

		threading::lock lock2(main_thread_write_buf_mutex);
		main_thread_write_buf.push_back(tsend_data(peer, data2));
	}
}

ttcpd_manager::ttcpd_manager(uint16_t port)
	: tserver_(server_tcpd)
	, port_(port)
	, e_(base::WaitableEvent::ResetPolicy::AUTOMATIC, base::WaitableEvent::InitialState::NOT_SIGNALED)
{}

bool ttcpd_manager::ip_is_connected(uint32_t ipv4) const
{
	VALIDATE_IN_MAIN_THREAD();

	net::TcpServerRose* delegate_ptr = delegate_.get();
	if (delegate_ptr == nullptr) {
		return false;
	}
	net::TcpServer& server = delegate_ptr->server();
	return server.ip_is_connected(ipv4);
}

void ttcpd_manager::did_set_event()
{
	e_.Signal();
}

void ttcpd_manager::start_internal(uint32_t ipaddr)
{
	tauto_destruct_executor destruct_executor(std::bind(&ttcpd_manager::did_set_event, this));
	delegate_.reset(app_create_delegate());
	delegate_->SetUp(ipaddr, port_);
	delegate_->server_address().ToString();

	// net::HttpServer server(std::move(server_socket), &delegate);
	// delegate.set_server(&server);
}

void ttcpd_manager::stop_internal()
{
	// thread_.SetRequireDeletePendingTasks in TcpServerRose::~TcpServerRose() signal e_, don't use tauto_destruct_executor.
	// tauto_destruct_executor destruct_executor(std::bind(&ttcpd_manager::did_set_event, this));
	VALIDATE(delegate_.get() != nullptr, null_str);
	delegate_.reset();
}

void ttcpd_manager::start(uint32_t ipaddr)
{
	CHECK(thread_.get() == nullptr);
	thread_.reset(new base::Thread("TcpdThread"));

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&ttcpd_manager::start_internal, base::Unretained(this), ipaddr));
	e_.Wait();
}

void ttcpd_manager::stop()
{
	if (thread_.get() == nullptr) {
		VALIDATE(delegate_.get() == nullptr, null_str);
		return;
	}

	CHECK(delegate_.get() != nullptr);
	// stop_internal will reset delegate_.

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&ttcpd_manager::stop_internal, base::Unretained(this)));
	e_.Wait();

	thread_.reset();
}

}  // namespace net
