// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/http_server_rose.hpp"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/bind.h"
// #include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"

#include "base_instance.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"
#include "SDL_log.h"
using namespace std::placeholders;

namespace net {

HttpServerRose::HttpServerRose()
	: send_helper_(this)
	, quit_on_close_connection_(-1)
{}

void HttpServerRose::SetUp(uint32_t ipaddr)
{
	std::unique_ptr<ServerSocket> server_socket(new TCPServerSocket(NULL, NetLogSource()));
	// server_socket->ListenWithAddressAndPort("127.0.0.1", 8080, 1);

	// Must not use 127.0.0.1
	// make sure we have a value in a known byte order: big endian
	uint32_t addrNBO = htonl(ipaddr);
	std::stringstream addr_ss;
	addr_ss << (int)((addrNBO >> 24) & 0xFF) << "." << (int)((addrNBO>>16) & 0xFF);
	addr_ss << "." << (int)((addrNBO>>8)&0xFF) << "." << (int)(addrNBO & 0xFF);
	server_socket->ListenWithAddressAndPort(addr_ss.str(), 8080, 1);

	server_.reset(new HttpServer(std::move(server_socket), this));
	// ASSERT_THAT(server_->GetLocalAddress(&server_address_), IsOk());
	server_->GetLocalAddress(&server_address_);
	server_url_ = std::string("http://") + server_address_.ToString();
}

void HttpServerRose::TearDown()
{
	// Run the event loop some to make sure that the memory handed over to
	// DeleteSoon gets fully freed.
	base::RunLoop().RunUntilIdle();
}

void HttpServerRose::OnConnect(int connection_id)
{
	DCHECK(connection_map_.find(connection_id) == connection_map_.end());
	connection_map_[connection_id] = true;
}

void HttpServerRose::OnHttpRequest(int connection_id, const HttpServerRequestInfo& info)
{
	twebrtc_send_helper::tlock lock(send_helper_);
	if (!lock.can_send()) {
		return;
	}

	tresponse_data resp;
	tmsg_data_request* pdata = new tmsg_data_request(info, resp);
	instance->sdl_thread().Send(RTC_FROM_HERE, this, POST_MSG_REQUET, pdata);
	server_->Send(connection_id, resp.status_code, resp.data, resp.content_type, TRAFFIC_ANNOTATION_FOR_TESTS);
}

void HttpServerRose::OnClose(int connection_id)
{
	std::unordered_map<int, bool>::iterator find_it = connection_map_.find(connection_id);
	DCHECK(find_it != connection_map_.end());
	connection_map_[connection_id] = false;
	connection_map_.erase(find_it);

	if (connection_id == quit_on_close_connection_)
		run_loop_quit_func_.Run();
}

void HttpServerRose::RunUntilConnectionIdClosed(int connection_id)
{
	quit_on_close_connection_ = connection_id;
	auto iter = connection_map_.find(connection_id);
	if (iter != connection_map_.end() && !iter->second) {
		// Already disconnected.
		return;
	}

	base::RunLoop run_loop;
	run_loop_quit_func_ = run_loop.QuitClosure();
	run_loop.Run();
	run_loop_quit_func_.Reset();
}
/*
void HttpServerRose::HandleAcceptResult(std::unique_ptr<StreamSocket> socket)
{
	server_->accepted_socket_ = std::move(socket);
	server_->HandleAcceptResult(OK);
}
*/

void HttpServerRose::OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case POST_MSG_REQUET:
		{
			tmsg_data_request* pdata = static_cast<tmsg_data_request*>(msg->pdata);
			instance->handle_http_request(pdata->info, pdata->resp);
		}
		break;
	}

	if (msg->pdata) {
		delete msg->pdata;
		msg->pdata = nullptr;
	}
}


thttpd_manager::thttpd_manager()
	: tserver_(server_httpd)
	, e_(false, false)
{}

thttpd_manager::~thttpd_manager()
{
	SDL_Log("---thttpd_manager::~thttpd_manager()---");
}

void thttpd_manager::did_set_event()
{
	e_.Set();
}

void thttpd_manager::start_internal(uint32_t ipaddr)
{
	tauto_destruct_executor destruct_executor(std::bind(&thttpd_manager::did_set_event, this));
	delegate_.reset(new HttpServerRose);
	delegate_->SetUp(ipaddr);
	delegate_->server_address().ToString();

	// net::HttpServer server(std::move(server_socket), &delegate);
	// delegate.set_server(&server);
}

void thttpd_manager::stop_internal()
{
	tauto_destruct_executor destruct_executor(std::bind(&thttpd_manager::did_set_event, this));
	VALIDATE(delegate_.get() != nullptr, null_str);
	delegate_.reset();
}

void thttpd_manager::start(uint32_t ipaddr)
{
	CHECK(thread_.get() == nullptr);
	thread_.reset(new base::Thread("HttpdThread"));

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&thttpd_manager::start_internal, base::Unretained(this), ipaddr));
	e_.Wait(rtc::Event::kForever);
}

void thttpd_manager::stop()
{
	if (thread_.get() == nullptr) {
		VALIDATE(delegate_.get() == nullptr, null_str);
		return;
	}

	CHECK(delegate_.get() != nullptr);
	// stop_internal will reset delegate_.
	delegate_->send_helper_.clear_msg();

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&thttpd_manager::stop_internal, base::Unretained(this)));
	e_.Wait(rtc::Event::kForever);

	thread_.reset();
}

}  // namespace net
