// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/rdp_server.h"

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
#include "net/server/rdp_connection.h"
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
/*
static scoped_refptr<X509Certificate> ImportCertFromFile(
    const base::FilePath& certs_dir,
    const std::string& cert_file) {
  // base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath cert_path = certs_dir.AppendASCII(cert_file);
  std::string cert_data;
  if (!base::ReadFileToString(cert_path, &cert_data))
    return nullptr;

  CertificateList certs_in_file =
      X509Certificate::CreateCertificateListFromBytes(
          cert_data.data(), cert_data.size(), X509Certificate::FORMAT_AUTO);
  if (certs_in_file.empty())
    return nullptr;
  return certs_in_file[0];
}
*/

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

RdpServer::RdpServer(std::unique_ptr<ServerSocket> server_socket,
                       RdpServer::Delegate* delegate)
    : server_socket_(std::move(server_socket))
	, delegate_(delegate)
	, last_id_(0)
	, weak_ptr_factory_(this)
	, fRequestBufferBytesLeftMinus1_(1023)
{
	DCHECK(server_socket_);

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

	// Start accepting connections in next run loop in case when delegate is not
	// ready to get callbacks.
	base::ThreadTaskRunnerHandle::Get()->PostTask(
		FROM_HERE,
		base::Bind(&RdpServer::DoAcceptLoop, weak_ptr_factory_.GetWeakPtr()));
}

RdpServer::~RdpServer()
{
    VALIDATE(id_to_connection_.empty(), null_str);
}

void RdpServer::CloseAllConnection()
{
    std::vector<RdpConnection*> ptrs;
    {
        threading::lock lock(connections_mutex_);
        // Return as quickly as possible, the subthread may be waiting for connections_mutex_
        // and delegate_->OnClose(*connection) maybe wait subthread.
        for (std::map<int, std::unique_ptr<RdpConnection> >::iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ) {
            std::unique_ptr<RdpConnection> connection = std::move(it->second);
            id_to_connection_.erase(it ++);
            ptrs.push_back(connection.release());
        }
    }

    for (std::vector<RdpConnection*>::const_iterator it = ptrs.begin(); it != ptrs.end(); ++ it) {
        RdpConnection* connection = *it;
        delegate_->OnClose(*connection);
        delete connection;
    }
}

void RdpServer::Close(int connection_id)
{
	SDL_Log("RdpServer::Close(%i)--- id_to_connection_.size: %i", connection_id, id_to_connection_.size());
	auto it = id_to_connection_.find(connection_id);
	if (it == id_to_connection_.end()) {
        SDL_Log("------RdpServer::Close(%i) X cannot find this conneciton, do nothing", connection_id);
        return;
    }

    std::unique_ptr<RdpConnection> connection;
    {
        threading::lock lock(connections_mutex_);
	    connection = std::move(it->second);
	    id_to_connection_.erase(it);
	    SDL_Log("RdpServer::Close(%i), post erase, id_to_connection_.size: %i", connection_id, id_to_connection_.size());
    }
    connection->set_closing();
	delegate_->OnClose(*connection.get());

    SDL_Log("RdpServer::Close(%i), post delegate_->OnClose", connection_id);

	// The call stack might have callbacks which still have the pointer of
	// connection. Instead of referencing connection with ID all the time,
	// destroys the connection in next run loop to make sure any pending
	// callbacks in the call stack return.
	base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, connection.release());

    SDL_Log("RdpServer::Close(%i) X", connection_id);
}

int RdpServer::GetLocalAddress(IPEndPoint* address) {
  return server_socket_->GetLocalAddress(address);
}
/*
void RdpServer::SetReceiveBufferSize(int connection_id, int32_t size) {
  RdpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->read_buf()->set_max_buffer_size(size);
}

void RdpServer::SetSendBufferSize(int connection_id, int32_t size) {
  RdpConnection* connection = FindConnection(connection_id);
  if (connection)
    connection->write_buf()->set_max_buffer_size(size);
}
*/
void RdpServer::DoAcceptLoop() {
  int rv;
  do {
    rv = server_socket_->Accept(&accepted_socket_,
                                base::Bind(&RdpServer::OnAcceptCompleted,
                                           weak_ptr_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING)
      return;
    rv = HandleAcceptResult(rv);
  } while (rv == OK);

  SDL_Log("RdpServer::DoAcceptLoop, will exit, rv: %i", rv);
}

void RdpServer::OnAcceptCompleted(int rv) {
  if (HandleAcceptResult(rv) == OK)
    DoAcceptLoop();
}

#define SUPPORTED_MAX_CLIENTS		1

int RdpServer::HandleAcceptResult(int rv)
{
	if (rv < 0) {
		LOG(ERROR) << "HandleAcceptResult, Accept error: rv=" << rv;
		return rv;
	}


    // Client hope to a separate error code to know this error occurred,
    // If it's disconnected here, client will confused what's error.

    // Still does not understand freerdp-related structure clearly, 
    // RdpServerRose::OnClose cleans up the client-related structure still has problem. for example subsystem-thread.
    // Reject connection in this crude way until this is resolved.
    if (id_to_connection_.size() >= SUPPORTED_MAX_CLIENTS) {
        SDL_Log("HandleAcceptResult, so far, up to %i client is supported", SUPPORTED_MAX_CLIENTS);
        accepted_socket_.reset();
        return OK;
    }

	if (last_id_ == INT_MAX) {
		last_id_ = 0;
	}
	++ last_id_;
	SDL_Log("RdpServer::HandleAcceptResult, connection's id: %i, existed connections: %i", last_id_, (int)id_to_connection_.size());

	std::unique_ptr<RdpConnection> connection_ptr = std::make_unique<RdpConnection>(*this, *server_context_.get(), last_id_, std::move(accepted_socket_));
	RdpConnection* connection = connection_ptr.get();
    {
        threading::lock lock(connections_mutex_);
	    id_to_connection_[connection->id()] = std::move(connection_ptr);
    }

    connection->start_internal();

	return OK;
}

void RdpServer::OnConnectionHandshakeComplete(int connection_id, int result)
{
    RdpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnHandshakeComplete(result);
}

void RdpServer::OnConnectionReadCompleted(int connection_id, int rv)
{
    RdpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnReadCompleted(connection_id, rv);
}

void RdpServer::OnConnectionWriteCompleted(int connection_id, NetworkTrafficAnnotationTag traffic_annotation, int rv)
{
    RdpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
    connection->OnWriteCompleted(connection_id, traffic_annotation, rv);
}

void RdpServer::PostClose(RdpConnection& connection)
{
    SDL_Log("RdpServer::PostClose(%i)---", connection.id());

	connection.set_closing();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&RdpServer::Close, base::Unretained(this), connection.id()));	
}
/*
void RdpServer::PostClose(int connection_id)
{
	RdpConnection* connection = FindNormalConnection(connection_id);
	if (connection == nullptr) {
        return;
    }
	connection->set_closing();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&RdpServer::Close, base::Unretained(this), connection->id()));	
}
*/

int RdpServer::normal_connection_count() const
{ 
    int ret = 0;
    for (std::map<int, std::unique_ptr<RdpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        RdpConnection* connection = it->second.get();
        if (!connection->closing_) {
            ret ++;
        }
    }
    return ret; 
}

RdpConnection* RdpServer::FindFirstNormalConnection() {
    for (std::map<int, std::unique_ptr<RdpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        RdpConnection* connection = it->second.get();
        if (!connection->closing_) {
            return connection;
        }
    }
    return nullptr;
}

RdpConnection* RdpServer::FindNormalConnection(int connection_id) {
    std::map<int, std::unique_ptr<RdpConnection> >::const_iterator it = id_to_connection_.find(connection_id);
    if (it == id_to_connection_.end()) {
        return nullptr;
    }
    RdpConnection* connection = it->second.get();
    if (connection->closing_) {
        return nullptr;
    }
    return connection;
}

RdpConnection* RdpServer::FindFirstHandshakedConnection() {
    for (std::map<int, std::unique_ptr<RdpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        RdpConnection* connection = it->second.get();
        if (!connection->closing_ && connection->handshaked()) {
            return connection;
        }
    }
    return nullptr;
}

RdpConnection* RdpServer::FindFirstConnectionfinishedConnection() {
    for (std::map<int, std::unique_ptr<RdpConnection> >::const_iterator it = id_to_connection_.begin(); it != id_to_connection_.end(); ++ it) {
        RdpConnection* connection = it->second.get();
        if (!connection->closing_ && connection->connectionfinished()) {
            return connection;
        }
    }
    return nullptr;
}

/*
StreamSocket* RdpServer::FindSocket(int connection_id) {
	RdpConnection* connection = FindConnection(connection_id);
	DCHECK(connection != nullptr);
	return connection->socket();
}
*/
bool RdpServer::sessionid_is_valid(int connection_id) const
{
	return id_to_connection_.count(connection_id) != 0;
}

// This is called after any delegate callbacks are called to check if Close()
// has been called during callback processing. Using the pointer of connection,
// |connection| is safe here because Close() deletes the connection in next run
// loop.
/*
bool RdpServer::HasClosedConnection(RdpConnection* connection) {
  return FindConnection(connection->id()) != connection;
}
*/
}  // namespace net
