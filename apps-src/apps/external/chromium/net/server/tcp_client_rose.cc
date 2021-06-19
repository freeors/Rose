// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#define GETTEXT_DOMAIN "rose-lib"

#include "tcp_client_rose.h"

#include <utility>
#include "util_c.h"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "version.hpp"

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
#include "net/server/rdp_connection.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
// #include "net/dns/host_resolver_impl.h"
#include "net/base/host_port_pair.h"
#include <net/ssl/ssl_config_service_defaults.h>
#include <net/ssl/ssl_client_session_cache.h>

#include <SDL_stdinc.h>
#include <SDL_log.h>

#include "rose_config.hpp"
#include "base_instance.hpp"

#include "base/files/file_util.h"
#include "net/cert/x509_certificate.h"
#include "crypto/rsa_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include <net/socket/client_socket_factory.h>
#include <net/base/host_port_pair.h>
#include <net/socket/client_socket_handle.h>

#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "json/json.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "base/strings/utf_string_conversions.h"

#include "gui/dialogs/message.hpp"
using namespace std::placeholders;

namespace net {

TcpClient::TcpClient(const HostPortPair& host_port_pair, TcpClient::Delegate* delegate)
    : host_port_pair_(host_port_pair)
	, delegate_(delegate)
	, weak_ptr_factory_(this)
	, socket_factory_(ClientSocketFactory::GetDefaultFactory())
	, next_state_(STATE_NONE)
	, create_ticks_(SDL_GetTicks())
    , handshaked_ticks_(0)
	, max_read_buffer_size_(512 * 1024)
	, max_write_buffer_size_(512 * 1024)
	, read_buf_(new HttpConnection::ReadIOBuffer())
    , write_buf_(new HttpConnection::QueuedWriteIOBuffer())
	, ssl_config_service_(new SSLConfigServiceDefaults)
	, cert_verifier_(new MockCertVerifier)
    , transport_security_state_(new TransportSecurityState)
    , ct_verifier_(new DoNothingCTVerifier)
    , ct_policy_enforcer_(std::make_unique<DefaultCTPolicyEnforcer>())
	, ssl_client_session_cache_(std::make_unique<SSLClientSessionCache>(SSLClientSessionCache::Config()))
	, read_layer_ptr_(nullptr)
	, read_layer_size_(0)
	, read_layer_offset_(0)
	, tid_(SDL_ThreadID())
{
	cert_verifier_->set_default_result(OK);
    context_.reset(new SSLClientContext(ssl_config_service_.get(),
                                               cert_verifier_.get(),
                                               transport_security_state_.get(),
                                               ct_policy_enforcer_.get(),
                                               ssl_client_session_cache_.get(),
                                               nullptr));
/*
	EXPECT_CALL(*ct_policy_enforcer_, CheckCompliance(_, _, _))
        .WillRepeatedly(
            Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));
*/
}

TcpClient::~TcpClient()
{
	// here, delegate_->send_startup_msg cannot send successfully, because webrc-invoke is disable by send_helper_.clear_msg();
}

int TcpClient::SendData(const std::string& data)
{
	VALIDATE_CALLED_ON_THIS_THREAD(tid_);

    VALIDATE(!data.empty(), null_str);
    if (next_state_ == STATE_TRANSPORT_CONNECT_COMPLETE) {
        // cr_data = data;
        return data.size();
    }

    bool writing_in_progress = !write_buf_->IsEmpty();
    if (write_buf_->Append(data) && !writing_in_progress) {
        DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    }

    return data.size();
}

void TcpClient::OnIOComplete(int result)
{
	DoLoop(result);
}

int TcpClient::DoLoop(int result)
{
	DCHECK_NE(next_state_, STATE_NONE);

	int rv = result;
	do {
		State state = next_state_;
		next_state_ = STATE_NONE;
		switch (state) {
		case STATE_RESOLVE_HOST:
			DCHECK_EQ(net::OK, rv);
			rv = DoResolveHost();
			break;
		case STATE_RESOLVE_HOST_COMPLETE:
			rv = DoResolveHostComplete(rv);
			break;
		case STATE_TRANSPORT_CONNECT:
			DCHECK_EQ(net::OK, rv);
			rv = DoTransportConnect();
			break;
		case STATE_TRANSPORT_CONNECT_COMPLETE:
			rv = DoTransportConnectComplete(rv);
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
			rv = net::ERR_FAILED;
			break;
		}
	} while (rv != net::ERR_IO_PENDING && next_state_ != STATE_NONE);

	return rv;
}

int TcpClient::DoResolveHost()
{
  if (host_resolver_.get() == nullptr) {
	  host_resolver_ = HostResolver::CreateStandaloneResolver(nullptr);
  }
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = net::DEFAULT_PRIORITY;
  resolve_request_ = host_resolver_->CreateRequest(host_port_pair_,
                                            NetworkIsolationKey(),
                                            net_log_dns_, parameters);

  return resolve_request_->Start(base::BindOnce(&TcpClient::OnIOComplete,
                                        base::Unretained(this)));
}

int TcpClient::DoResolveHostComplete(int result)
{
  if (result == net::OK) {
    addresses_ = resolve_request_->GetAddressResults().value();
  }
  // Destroy HostResolverManager::RequestImpl must be in thread that it was created.
  resolve_request_.reset();

  if (result != net::OK) {
    return result;
  }

  next_state_ = STATE_TRANSPORT_CONNECT;
  return result;
}

int TcpClient::DoTransportConnect()
{
	next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;

	// We don't yet have a TCP socket (or we used to have one, but it got closed).  Set it up now.
	// ctrl_socket_ = socket_factory_->CreateTransportClientSocket(AddressList(server_address_), nullptr, net_log2_.net_log(), net_log2_.source());
	ctrl_socket_ = socket_factory_->CreateTransportClientSocket(addresses_, nullptr, nullptr, net_log2_.net_log(), net_log2_.source());

	if (ctrl_socket_.get() == nullptr) {
		delegate_->OnResult(net::ERR_FAILED);
		return ERR_FAILED;
	}
      
	// Connect to the remote endpoint:
	int connectResult = ctrl_socket_->Connect(base::Bind(&TcpClient::OnIOComplete, base::Unretained(this)));
	if (connectResult < 0 && connectResult != net::ERR_IO_PENDING) {
		delegate_->OnResult(connectResult);
		return connectResult;
	}
	return connectResult;
}

int TcpClient::DoTransportConnectComplete(int result)
{
	if (result != OK) {
		Close(result);
        return result;
    }
    next_state_ = STATE_READ_PDU;
	delegate_->OnConnect();
    return OK;
}

int TcpClient::DoHandshake()
{
    next_state_ = STATE_HANDSHAKE_COMPLETE;

	std::unique_ptr<StreamSocket> transport_socket;
	// const HostPortPair host_and_port = HostPortPair::FromIPEndPoint(server_address_);
	const HostPortPair host_and_port = host_port_pair_;
    const SSLConfig ssl_config;
/*
    std::unique_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    connection->SetSocket(std::move(ctrl_socket_));
    std::unique_ptr<SSLClientSocket> client(
		socket_factory_->CreateSSLClientSocket(std::move(connection), host_and_port, ssl_config, context_));
*/
	std::unique_ptr<SSLClientSocket> client(
		socket_factory_->CreateSSLClientSocket(context_.get(), std::move(ctrl_socket_), host_and_port, ssl_config));

	int rv = client->Connect(base::Bind(&TcpClient::OnHandshakeComplete, weak_ptr_factory_.GetWeakPtr()));
	ctrl_socket_ = std::move(client);

    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleHandshakeResult(rv);
    return rv;
}

void TcpClient::OnHandshakeComplete(int result)
{
    // SDL_Log("RdpConnection::OnIOComplete, result: %i", result);
    VALIDATE(next_state_ == STATE_HANDSHAKE_COMPLETE, null_str);

    result = HandleHandshakeResult(result);
    DoLoop(result);
}

int TcpClient::HandleHandshakeResult(int result)
{
    // 0(OK) is ok for SocketImpl::Handshake
    if (result < 0) {
		Close(result);
		return result == 0 ? ERR_CONNECTION_CLOSED : result;
	}
    return OK;
}

int TcpClient::DoHandshakeComplete(int result)
{
    if (result != OK) {
        return result;
    }
    next_state_ = STATE_READ_PDU; // STATE_WRITE_MCS_REQUEST

    handshaked_ticks_ = SDL_GetTicks();
    return OK;
}

int TcpClient::DoReadPDU()
{
    next_state_ = STATE_READ_PDU_COMPLETE;
    return DoReadLoop();
}

int TcpClient::DoReadPDUComplete(int result)
{
    if (result != OK) {
        return result;
    }
    return DoReadPDU();
}

int TcpClient::DoReadLoop()
{
    // Increases read buffer size if necessary.
    HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
    if (read_buf->RemainingCapacity() == 0 && !read_buf->IncreaseCapacity()) {
        Close(net::ERR_FILE_NO_SPACE);
        return ERR_FILE_NO_SPACE;
    }

    int buf_len = read_buf->RemainingCapacity();
    int rv = socket()->Read(
        read_buf,
        buf_len,
        base::Bind(&TcpClient::OnReadCompleted,
                    weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleReadResult(rv);
    return rv;
}

void TcpClient::OnReadCompleted(int rv)
{
    // SDL_Log("RdpConnection::OnReadCompleted, connection_id: %i, result: %i", connection_id, rv);
    rv = HandleReadResult(rv);
    DoLoop(rv);
}

int TcpClient::HandleReadResult(int rv)
{
	if (rv <= 0) {
		rv = rv == 0? ERR_CONNECTION_CLOSED: rv;
		Close(rv);
		return rv;
	}

	HttpConnection::ReadIOBuffer* read_buf = read_buf_.get();
	read_buf->DidRead(rv);

    const char* data_ptr = read_buf->StartOfBuffer();
    const int data_size = read_buf->GetSize();

	// SDL_Log("TcpClient::HandleReadResult, data_size: %i, next_state_: %i", data_size, next_state_);
    {
		int consumed_size = delegate_->OnTcpRequest(data_ptr, data_size);
        VALIDATE(consumed_size <= data_size, null_str);
        if (consumed_size != 0) {
            read_buf->DidConsume(consumed_size);
        }
    }
	return OK;
}

int TcpClient::DoWriteLoop(NetworkTrafficAnnotationTag traffic_annotation) {
    HttpConnection::QueuedWriteIOBuffer* write_buf = write_buf_.get();
    if (write_buf->GetSizeToWrite() == 0) {
        return OK;
    }

    int rv = socket()->Write(
        write_buf, write_buf->GetSizeToWrite(),
        base::Bind(&TcpClient::OnWriteCompleted,
                    weak_ptr_factory_.GetWeakPtr(),
                    traffic_annotation),
        traffic_annotation);
    if (rv == ERR_IO_PENDING) {
        return rv;
    }
    rv = HandleWriteResult(rv);
    return rv;
}

void TcpClient::OnWriteCompleted(NetworkTrafficAnnotationTag traffic_annotation, int rv)
{
	rv = HandleWriteResult(rv);
/*
	if (next_state_ == STATE_WRITE_CC_COMPLETE) {
		DoLoop(rv);
	}
*/
}

int TcpClient::HandleWriteResult(int rv)
{
	if (rv < 0) {
		Close(rv);
		return rv;
	}

    HttpConnection::QueuedWriteIOBuffer* write_buf = write_buf_.get();
	{
		// threading::lock lock2(write_buf_mutex_);
		write_buf->DidConsume(rv);
	}
    if (write_buf->GetSizeToWrite() != 0) {
        // If there's still data written, write it out.
        rv = DoWriteLoop(TRAFFIC_ANNOTATION_FOR_TESTS);
    } else {
        rv = OK;
    }
	return rv;
}

void TcpClient::Close(int rv)
{
	VALIDATE_CALLED_ON_THIS_THREAD(tid_);
	VALIDATE(rv != net::OK, null_str);

	if (ctrl_socket_.get() == nullptr) {
		return;
	}
	ctrl_socket_.reset();

	delegate_->OnResult(rv);
	delegate_->OnClose();
}

int TcpClient::did_read_layer(uint8_t* data, int bytes)
{
	int read = SDL_min(bytes, read_layer_size_ - read_layer_offset_);
	if (read > 0) {
		memcpy(data, read_layer_ptr_ + read_layer_offset_, read);
		read_layer_offset_ += read;
	}
	return read;
}

int TcpClient::did_write_layer(const uint8_t* data, int bytes)
{
	VALIDATE(bytes > 0, null_str);

	std::string data2((const char*)data, bytes);
	SDL_threadID id = SDL_ThreadID();
	if (tid_ != id) {
		// SDL_Log("TcpClient::did_write_layer, [main]tid: %lu, bytes: %i", id, bytes);
		// on android, as if in main-thread, thread_checker_.CalledOnValidThread() always return true!
		// I had to use specialized variable(in_main_thread) that current is in main thread.
		VALIDATE_IN_MAIN_THREAD();
		// Client Fast-Path Input Event PDU (TS_FP_INPUT_PDU)

		threading::lock lock2(main_thread_write_buf_mutex);
		main_thread_write_buf.push_back(data2);
		return bytes;

	} else {
		// SDL_Log("TcpClient::did_write_layer, [rdpthread]tid: %lu, bytes: %i", id, bytes);
		thread_checker_.CalledOnValidThread();
	}

	return SendData(data2);
}

void TcpClient::start_internal()
{
	write_buf_->set_max_buffer_size(max_write_buffer_size_);
	read_buf_->DisableAutoReduceBuffer();
	read_buf_->set_max_buffer_size(max_read_buffer_size_);
	read_buf_->SetCapacity(max_read_buffer_size_ / 2);
	next_state_ = STATE_RESOLVE_HOST; // STATE_TRANSPORT_CONNECT
	DoLoop(net::OK);
}


TcpClientRose::TcpClientRose(base::Thread& thread, base::WaitableEvent& e)
	: thread_(thread)
	, delete_pend_tasks_e_(e)
	, weak_ptr_factory_(this)
	, check_slice_timeout_(300) // 300 ms
	, slice_running_(false)
	, timeout_(nposm)
	, connected_(false)
	, rv_(net::OK)
	, should_disconnect_ticks_(0)
	, total_read_bytes_(0)
{
}

TcpClientRose::~TcpClientRose()
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);

	SDL_Log("TcpClientRose::~TcpClientRose()---");
	slice_running_ = false;
	// freerdp_disconnect(rdp_context_->instance);
	// freerdp_client_context_free(rdp_context_);
	thread_.SetRequireDeletePendingTasks(delete_pend_tasks_e_);
	SDL_Log("---TcpClientRose::~TcpClientRose() X");
}

void TcpClientRose::SetUp(const std::string& host, uint16_t port)
{
	// IPAddress address((const uint8_t*)&ipv4, 4);
	// server_address_ = IPEndPoint(address, port);
	// client_.reset(new TcpClient(server_address_, this));

	host_port_pair_ = HostPortPair(host, port);
	client_.reset(new TcpClient(host_port_pair_, this));

	VALIDATE(!slice_running_, null_str);
	SDL_Log("will run tcpc_slice");
	slice_running_ = true;
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&TcpClientRose::tcpc_slice, weak_ptr_factory_.GetWeakPtr(), check_slice_timeout_));

	client_->start_internal();
}

void TcpClientRose::OnConnect()
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);
	connected_ = true;
	should_disconnect_ticks_ = 0;
}

void TcpClientRose::OnClose()
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);
	connected_ = false;
}

void TcpClientRose::OnResult(int rv)
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);
	rv_ = rv;
}

int TcpClientRose::OnTcpRequest(const char* data, int len)
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);

	total_read_bytes_ += len;
	if (timeout_ != nposm) {
		should_disconnect_ticks_ = SDL_GetTicks() + timeout_ * 1000;
	}
	return app_OnTcpRequest(data, len);
}

void TcpClientRose::did_slice_quited(int timeout1)
{
	VALIDATE(client_.get() != nullptr, null_str);
	if (!slice_running_) {
		SDL_Log("will stop tcpc_slice");
		return;
	}

	// base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&RdpServerRose::rdpd_slice, weak_ptr_factory_.GetWeakPtr(), timeout1));

	const base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(20); // 20 ms.
	base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::Bind(&TcpClientRose::tcpc_slice, weak_ptr_factory_.GetWeakPtr(), timeout1), timeout);
}

void TcpClientRose::tcpc_slice(int timeout)
{
	VALIDATE_CALLED_ON_THIS_THREAD(client_->tid_);
	tauto_destruct_executor destruct_executor(std::bind(&TcpClientRose::did_slice_quited, this, timeout));

	if (!slice_running_) {
		return;
	}

	TcpClient* client_ptr = client_.get();

	const uint32_t now = SDL_GetTicks();

	if (client_ptr != nullptr && client_ptr->socket() != nullptr) {
		// whay 12 seconds? base on rtt_threshold is 5 second in launcher's rdp_server_rose.cc. 
		// so 12 at least tow rtt-request.
		if (should_disconnect_ticks_ != 0 && now >= should_disconnect_ticks_) {
			// Within capture_thread_threshold, handshaked connection must start the capture thread (which means entering activeed, etc.), 
			// otherwise an exception is considered, and force to disconnect.
			client_ptr->Close(net::ERR_TIMED_OUT);
			return;
		}

		{
			threading::lock lock2(client_ptr->main_thread_write_buf_mutex);
			std::vector<std::string>& write_buf = client_ptr->main_thread_write_buf;
			for (std::vector<std::string>::const_iterator it  = write_buf.begin(); it != write_buf.end(); ++ it) {
				const std::string& data = *it;
				client_ptr->SendData(data);
			}
			write_buf.clear();
		}
/*
		std::vector<std::string> main_thread_write_buf;
		freerdp_channels_check_fds(rdp_context_->channels, rdp_context_->instance);
*/
	}
}

ttcpc_manager::ttcpc_manager(const std::function<TcpClientRose* (base::Thread&, base::WaitableEvent&)>& create)
	: create_delegate_(create)
	, e_(base::WaitableEvent::ResetPolicy::AUTOMATIC, base::WaitableEvent::InitialState::NOT_SIGNALED)
{}

ttcpc_manager::~ttcpc_manager()
{
	stop();
}

void ttcpc_manager::did_set_event()
{
	e_.Signal();
}

void ttcpc_manager::start_internal(const std::string& host, uint16_t port, int timeout)
{	
	tauto_destruct_executor destruct_executor(std::bind(&ttcpc_manager::did_set_event, this));
	// delegate_.reset(app_create_delegate(*thread_.get(), e_));
	delegate_.reset(create_delegate_(*thread_.get(), e_));
	delegate_->timeout_ = timeout;
	if (timeout != nposm) {
		delegate_->should_disconnect_ticks_ = SDL_GetTicks() + timeout;
	}
	
	// Setup maybe call app_OnTcpRequest, in order to avoid deallock, 
	// main-thread must not lock mutex if app_OnTcpRequest will lcok it.
	delegate_->SetUp(host, port);
}

void ttcpc_manager::stop_internal()
{
	// thread_.SetRequireDeletePendingTasks in TcpClientRose::~TcpClientRose() signal e_, don't use tauto_destruct_executor.
	// tauto_destruct_executor destruct_executor(std::bind(&ttcp_manager::did_set_event, this));
	VALIDATE(delegate_.get() != nullptr, null_str);
	delegate_.reset();
}

void ttcpc_manager::start(const std::string& host, uint16_t port, int timeout)
{
	CHECK(thread_.get() == nullptr);
	thread_.reset(new base::Thread("TcpcThread"));

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&ttcpc_manager::start_internal, base::Unretained(this), host, port, timeout));
	e_.Wait();
}

void ttcpc_manager::stop()
{
	if (thread_.get() == nullptr) {
		VALIDATE(delegate_.get() == nullptr, null_str);
		return;
	}

	CHECK(delegate_.get() != nullptr);
	// stop_internal will reset delegate_.

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&ttcpc_manager::stop_internal, base::Unretained(this)));
	e_.Wait();

	thread_.reset();
}

bool ttcpc_manager::xmit_transaction_internal(gui2::tprogress_& progress, const std::string& host, uint16_t port, const tuint8cdata& req, int timeout)
{
	std::string err_msg;
	utils::string_map symbols;
	symbols["host"] = host;
	progress.set_message(vgettext2("Connect to $host", symbols));

	VALIDATE(!host.empty() && port != 0, null_str);
	start(host, port, timeout);
	TcpClientRose& rose = *delegate_.get();
	while (!rose.connected_ && rose.rv_ == net::OK) {
		progress.show_slice();
	}
	if (rose.connected_) {
		progress.set_message(_("In processing command"));

		rose.client_->did_write_layer(req.ptr, req.len);
		while (rose.total_read_bytes_ == 0 && rose.rv_ == net::OK) {
			progress.show_slice();
		}
	}
	int rv = rose.rv_;
	stop();


	if (rv != net::OK) {
		symbols.clear();
		symbols["err"] = str_cast(rv);
		symbols["desc"] = _("Error");
		err_msg = vgettext2("[$err] $desc", symbols);
		gui2::show_message(null_str, err_msg);
	}
	return rv == net::OK;
}

bool ttcpc_manager::xmit_transaction(const std::string& host, uint16_t port, const tuint8cdata& req, int timeout)
{
	gui2::tprogress_default_slot slot(std::bind(&ttcpc_manager::xmit_transaction_internal, this, _1, host, port, std::ref(req), timeout), null_str);
	return gui2::run_with_progress(slot, null_str, null_str, 500);
}

//
// default impletement
//
TcpClientDefault::TcpClientDefault(base::Thread& thread, base::WaitableEvent& e)
	: TcpClientRose(thread, e)
{}

int TcpClientDefault::app_OnTcpRequest(const char* data, int len)
{
	return len;
}

TcpClientRose* app_create_delegate_default(base::Thread& thread, base::WaitableEvent& e)
{
	return new TcpClientDefault(thread, e);
}

}  // namespace net
