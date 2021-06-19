// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/tcp_server.h"

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
#include "net/server/tcp_connection.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

#include <SDL_stdinc.h>
#include <SDL_log.h>

#include "rose_config.hpp"
#include "base_instance.hpp"
using namespace std::placeholders;
#include "base/files/file_util.h"
#include "net/cert/x509_certificate.h"
#include "crypto/rsa_private_key.h"
#include "net/ssl/ssl_server_config.h"

#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "json/json.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "base/strings/utf_string_conversions.h"

namespace net {

static scoped_refptr<X509Certificate> ImportCertFromFile(const std::string& cert_file)
{
  // base::ScopedAllowBlockingForTesting allow_blocking;
  tfile file(cert_file, GENERIC_READ, OPEN_EXISTING);
  int fsize = file.read_2_data();
  if (fsize == 0) {
	SDL_Log("ImportCertFromFile, read data from %s fail", cert_file.c_str());
    return nullptr;
  }
  SDL_Log("ImportCertFromFile, cert_data.size: %i", fsize);

  CertificateList certs_in_file =
      X509Certificate::CreateCertificateListFromBytes(
          file.data, fsize, X509Certificate::FORMAT_AUTO);
  if (certs_in_file.empty()) {
    return nullptr;
  }
  return certs_in_file[0];
}

static bssl::UniquePtr<EVP_PKEY> LoadEVP_PKEY(const std::string& filepath)
{
  tfile file(filepath, GENERIC_READ, OPEN_EXISTING);
  int fsize = file.read_2_data();
  if (fsize == 0) {
    SDL_Log("LoadEVP_PKEY, read data from %s fail", filepath.c_str());
    return nullptr;
  }
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(const_cast<char*>(file.data),
                                           static_cast<int>(fsize)));
  if (!bio) {
    SDL_Log("Could not allocate BIO for buffer?");
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> result(
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr));
  if (!result) {
    SDL_Log("Could not decode private key file: %s", filepath.c_str());
    return nullptr;
  }
  return result;
}

TcpServer::TcpServer(std::unique_ptr<ServerSocket> server_socket,
                       TcpServer::Delegate* delegate)
    : server_socket_(std::move(server_socket))
	, delegate_(delegate)
    , ssl_(false)
	, last_id_(0)
	, weak_ptr_factory_(this)
	, fRequestBufferBytesLeftMinus1_(1023)
    , tid_(SDL_ThreadID())
{
	DCHECK(server_socket_);

    if (ssl_) {
	    const std::string certs_dir = game_config::path + "/" + game_config::app_dir + "/cert";

        // Set up an SSL server.
        scoped_refptr<net::X509Certificate> cert = ImportCertFromFile(certs_dir + "/ok_cert.pem");
	    VALIDATE(cert.get() != nullptr, null_str);
        bssl::UniquePtr<EVP_PKEY> pkey = LoadEVP_PKEY(certs_dir + "/ok_cert.pem");
	    VALIDATE(pkey.get() != nullptr, null_str);
        std::unique_ptr<crypto::RSAPrivateKey> key = crypto::RSAPrivateKey::CreateFromKey(pkey.get());
        VALIDATE(key.get() != nullptr, null_str);
        server_context_ =
          CreateSSLServerContext(cert.get(), *key.get(), SSLServerConfig());
    }

	// Start accepting connections in next run loop in case when delegate is not
	// ready to get callbacks.
	base::ThreadTaskRunnerHandle::Get()->PostTask(
		FROM_HERE,
		base::Bind(&TcpServer::DoAcceptLoop, weak_ptr_factory_.GetWeakPtr()));
}

TcpServer::~TcpServer()
{
    VALIDATE(id_to_connection_.empty(), null_str);
}

void TcpServer::CloseAllConnection()
{
    std::vector<TcpConnection*> ptrs;
    {
        threading::lock lock(connections_mutex_);
        // Return as quickly as possible, the subthread may be waiting for connections_mutex_
        // and delegate_->OnClose(*connection) maybe wait subthread.
        for (std::map<int, std::unique_ptr<TcpConnection> >::iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ) {
            std::unique_ptr<TcpConnection> connection = std::move(it->second);
            id_to_connection_.erase(it ++);
            ptrs.push_back(connection.release());
        }
    }

    for (std::vector<TcpConnection*>::const_iterator it = ptrs.begin(); it != ptrs.end(); ++ it) {
        TcpConnection* connection = *it;
        delegate_->OnClose(*connection);
        delete connection;
    }
}

void TcpServer::Close(int connection_id)
{
	SDL_Log("TcpServer::Close(%i)--- id_to_connection_.size: %i", connection_id, id_to_connection_.size());
	auto it = id_to_connection_.find(connection_id);
	if (it == id_to_connection_.end()) {
        SDL_Log("------TcpServer::Close(%i) X cannot find this conneciton, do nothing", connection_id);
        return;
    }

    std::unique_ptr<TcpConnection> connection;
    {
        threading::lock lock(connections_mutex_);
	    connection = std::move(it->second);
	    id_to_connection_.erase(it);
	    SDL_Log("TcpServer::Close(%i), post erase, id_to_connection_.size: %i", connection_id, id_to_connection_.size());
    }
    connection->set_closing();
	delegate_->OnClose(*connection.get());

    SDL_Log("TcpServer::Close(%i), post delegate_->OnClose", connection_id);

	// The call stack might have callbacks which still have the pointer of
	// connection. Instead of referencing connection with ID all the time,
	// destroys the connection in next run loop to make sure any pending
	// callbacks in the call stack return.
	base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, connection.release());

    SDL_Log("TcpServer::Close(%i) X", connection_id);
}

int TcpServer::GetLocalAddress(IPEndPoint* address) {
  return server_socket_->GetLocalAddress(address);
}
/*
void TcpServer::SetReceiveBufferSize(int connection_id, int32_t size) {
  RdpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->read_buf()->set_max_buffer_size(size);
}

void TcpServer::SetSendBufferSize(int connection_id, int32_t size) {
  RdpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->write_buf()->set_max_buffer_size(size);
}
*/
void TcpServer::DoAcceptLoop() {
  int rv;
  do {
    rv = server_socket_->Accept(&accepted_socket_,
                                base::Bind(&TcpServer::OnAcceptCompleted,
                                           weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleAcceptResult(rv);
  } while (rv == OK);

  SDL_Log("TcpServer::DoAcceptLoop, will exit, rv: %i", rv);
}

void TcpServer::OnAcceptCompleted(int rv) {
  if (HandleAcceptResult(rv) == OK)
    DoAcceptLoop();
}

#define SUPPORTED_MAX_CLIENTS		1

int TcpServer::HandleAcceptResult(int rv)
{
	if (rv < 0) {
		LOG(ERROR) << "HandleAcceptResult, Accept error: rv=" << rv;
		return rv;
	}

	if (last_id_ == INT_MAX) {
		last_id_ = 0;
	}
	++ last_id_;
	SDL_Log("TcpServer::HandleAcceptResult, connection's id: %i, existed connections: %i", last_id_, (int)id_to_connection_.size());

	std::unique_ptr<TcpConnection> connection_ptr = std::make_unique<TcpConnection>(*this, *server_context_.get(), last_id_, std::move(accepted_socket_), ssl_);
	TcpConnection* connection = connection_ptr.get();
    {
        threading::lock lock(connections_mutex_);
	    id_to_connection_[connection->id()] = std::move(connection_ptr);
    }

    connection->start_internal();

	return OK;
}

void TcpServer::OnConnectionHandshakeComplete(int connection_id, int result)
{
    TcpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnHandshakeComplete(result);
}

void TcpServer::OnConnectionReadCompleted(int connection_id, int rv)
{
    TcpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnReadCompleted(connection_id, rv);
}

void TcpServer::OnConnectionWriteCompleted(int connection_id, NetworkTrafficAnnotationTag traffic_annotation, int rv)
{
    TcpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnWriteCompleted(connection_id, traffic_annotation, rv);
}

void TcpServer::PostClose(TcpConnection& connection)
{
    SDL_Log("TcpServer::PostClose(%i)---", connection.id());

	connection.set_closing();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&TcpServer::Close, base::Unretained(this), connection.id()));	
}

int TcpServer::normal_connection_count() const
{ 
    int ret = 0;
    for (std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        TcpConnection* connection = it->second.get();
        if (!connection->closing_) {
            ret ++;
        }
    }
    return ret; 
}

TcpConnection* TcpServer::FindFirstNormalConnection() {
    for (std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        TcpConnection* connection = it->second.get();
        if (!connection->closing_) {
            return connection;
        }
    }
    return nullptr;
}

TcpConnection* TcpServer::FindNormalConnection(int connection_id) {
    std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = id_to_connection_.find(connection_id);
    if (it == id_to_connection_.end()) {
        return nullptr;
    }
    TcpConnection* connection = it->second.get();
    if (connection->closing_) {
        return nullptr;
    }
    return connection;
}

TcpConnection* TcpServer::FindFirstHandshakedConnection() {
    for (std::map<int, std::unique_ptr<TcpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        TcpConnection* connection = it->second.get();
        if (!connection->closing_ && connection->handshaked()) {
            return connection;
        }
    }
    return nullptr;
}

bool TcpServer::ip_is_connected(uint32_t ipv4)
{
    VALIDATE(ipv4 != 0, null_str);
	threading::lock lock(connections_mutex_);

	for (std::map<int, std::unique_ptr<net::TcpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        const net::TcpConnection& connection = *it->second.get();
		if (connection.peer_ipv4_ == ipv4) {
			return true;
		}
	}
    return false;
}

bool TcpServer::sessionid_is_valid(int connection_id) const
{
	return id_to_connection_.count(connection_id) != 0;
}

}  // namespace net
