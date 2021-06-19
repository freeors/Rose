/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// A generic media server class, used to implement a RTSP server, and any other server that uses
//  "ServerMediaSession" objects to describe media to be served.
// Implementation

#include "liveMedia/include/GenericMediaServer.hh"
#include <groupsock/include/GroupsockHelper.hh>
#if defined(__WIN32__) || defined(_WIN32) || defined(_QNX4)
#define snprintf _snprintf
#endif

#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "util.hpp"
#include "live555d.hpp"

////////// GenericMediaServer implementation //////////

void GenericMediaServer::addServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  char const* sessionName = serverMediaSession->streamName();
  if (sessionName == NULL) sessionName = "";
  removeServerMediaSession(sessionName); // in case an existing "ServerMediaSession" with this name already exists
  
  fServerMediaSessions->Add(sessionName, (void*)serverMediaSession);
}

ServerMediaSession* GenericMediaServer
::lookupServerMediaSession(char const* streamName, Boolean /*isFirstLookupInSession*/) {
  // Default implementation:
  return (ServerMediaSession*)(fServerMediaSessions->Lookup(streamName));
}

void GenericMediaServer::removeServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;

  ServerMediaSession* found_sms = (ServerMediaSession*)(fServerMediaSessions->Lookup(serverMediaSession->streamName()));
  if (found_sms == serverMediaSession) {
    fServerMediaSessions->Remove(serverMediaSession->streamName());
  }
  if (serverMediaSession->referenceCount() == 0) {
    Medium::close(serverMediaSession);
  } else {
    serverMediaSession->deleteWhenUnreferenced() = True;
  }
}

void GenericMediaServer::removeServerMediaSession(char const* streamName) {
  removeServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

void GenericMediaServer::closeAllClientSessionsForServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  HashTable::Iterator* iter = HashTable::Iterator::create(*fClientSessions);
  GenericMediaServer::ClientSession* clientSession;
  char const* key; // dummy
  while ((clientSession = (GenericMediaServer::ClientSession*)(iter->next(key))) != NULL) {
    if (clientSession->fOurServerMediaSession == serverMediaSession) {
      delete clientSession;
    }
  }
  delete iter;
}

void GenericMediaServer::closeAllClientSessionsForServerMediaSession(char const* streamName) {
  closeAllClientSessionsForServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

void GenericMediaServer::deleteServerMediaSession(ServerMediaSession* serverMediaSession) {
  if (serverMediaSession == NULL) return;
  
  closeAllClientSessionsForServerMediaSession(serverMediaSession);
  removeServerMediaSession(serverMediaSession);
}

void GenericMediaServer::deleteServerMediaSession(char const* streamName) {
  deleteServerMediaSession((ServerMediaSession*)(fServerMediaSessions->Lookup(streamName)));
}

GenericMediaServer* GenericMediaServer::instance = nullptr;

GenericMediaServer::GenericMediaServer(UsageEnvironment& env, int ourSocket, Port ourPort,
		     unsigned reclamationSeconds)
	: Medium(env)
	, fServerSocket(ourSocket)
	, fServerPort(ourPort), fReclamationSeconds(reclamationSeconds),
    fServerMediaSessions(HashTable::create(STRING_HASH_KEYS)),
    fClientConnections(HashTable::create(ONE_WORD_HASH_KEYS)),
    fClientSessions(HashTable::create(STRING_HASH_KEYS)) {
  ignoreSigPipeOnSocket(fServerSocket); // so that clients on the same host that are killed don't also kill us
  
  // Arrange to handle connections from others:
  env.taskScheduler().turnOnBackgroundReadHandling(fServerSocket, incomingConnectionHandler, this);
  instance = this;
}

GenericMediaServer::~GenericMediaServer() {
  // Turn off background read handling:
  envir().taskScheduler().turnOffBackgroundReadHandling(fServerSocket);
  ::closeSocket(fServerSocket);
  instance = nullptr;
}

void GenericMediaServer::cleanup()
{
  // This member function must be called in the destructor of any subclass of
  // "GenericMediaServer".  (We don't call this in the destructor of "GenericMediaServer" itself,
  // because by that time, the subclass destructor will already have been called, and this may
  // affect (break) the destruction of the "ClientSession" and "ClientConnection" objects, which
  // themselves will have been subclassed.)

  // Close all client connection objects:
  GenericMediaServer::ClientConnection* connection;
  while ((connection = (GenericMediaServer::ClientConnection*)fClientConnections->getFirst()) != NULL) {
    delete connection;
  }
  delete fClientConnections;
  fClientConnections = nullptr;

  // if ClientSession is "full active", above delete connection should delete relative ClientSession.
  // but if ClientSession isn't "full active", delete connection doesn't delete relative ClientSession, delete now.
  GenericMediaServer::ClientSession* clientSession;
  while ((clientSession = (GenericMediaServer::ClientSession*)fClientSessions->getFirst()) != NULL) {
    delete clientSession;
  }
  delete fClientSessions;
  fClientSessions = nullptr;
  
  // Delete all server media sessions
  ServerMediaSession* serverMediaSession;
  while ((serverMediaSession = (ServerMediaSession*)fServerMediaSessions->getFirst()) != NULL) {
    removeServerMediaSession(serverMediaSession); // will delete it, because it no longer has any 'client session' objects using it
  }
  delete fServerMediaSessions;
  fServerMediaSessions = nullptr;
}

#define LISTEN_BACKLOG_SIZE 20

int GenericMediaServer::setUpOurSocket(UsageEnvironment& env, Port& ourPort) {
  int ourSocket = -1;
  
  do {
    // The following statement is enabled by default.
    // Don't disable it (by defining ALLOW_SERVER_PORT_REUSE) unless you know what you're doing.
#if !defined(ALLOW_SERVER_PORT_REUSE) && !defined(ALLOW_RTSP_SERVER_PORT_REUSE)
    // ALLOW_RTSP_SERVER_PORT_REUSE is for backwards-compatibility #####
    NoReuse dummy(env); // Don't use this socket if there's already a local server using it
#endif
    
    ourSocket = setupStreamSocket(env, ourPort, True, True);
    if (ourSocket < 0) break;
    
    // Make sure we have a big send buffer:
    if (!increaseSendBufferTo(env, ourSocket, 50*1024)) break;
    
    // Allow multiple simultaneous connections:
    if (listen(ourSocket, LISTEN_BACKLOG_SIZE) < 0) {
      env.setResultErrMsg("listen() failed: ");
      break;
    }
    
    if (ourPort.num() == 0) {
      // bind() will have chosen a port for us; return it also:
      if (!getSourcePort(env, ourSocket, ourPort)) break;
    }
    
    return ourSocket;
  } while (0);
  
  if (ourSocket != -1) ::closeSocket(ourSocket);
  return -1;
}

void GenericMediaServer::incomingConnectionHandler(void* instance, int /*mask*/) {
  GenericMediaServer* server = (GenericMediaServer*)instance;
  server->incomingConnectionHandler();
}
void GenericMediaServer::incomingConnectionHandler() {
  incomingConnectionHandlerOnSocket(fServerSocket);
}

void GenericMediaServer::incomingConnectionHandlerOnSocket(int serverSocket) {
  struct sockaddr_in clientAddr;
  SOCKLEN_T clientAddrLen = sizeof clientAddr;
  int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
  if (clientSocket < 0) {
    int err = envir().getErrno();
    if (err != EWOULDBLOCK) {
      envir().setResultErrMsg("accept() failed: ");
    }
    return;
  }
  ignoreSigPipeOnSocket(clientSocket); // so that clients on the same host that are killed don't also kill us
  makeSocketNonBlocking(clientSocket);
  increaseSendBufferTo(envir(), clientSocket, 50*1024);
  
#ifdef DEBUG
  envir() << "accept()ed connection from " << AddressString(clientAddr).val() << "\n";
#endif
  
  // Create a new object for handling this connection:
  (void)createNewClientConnection(clientSocket, clientAddr, nullptr, nposm);
}

bool sendDataOverTCP_chromium(net::StreamSocket* netSock, u_int8_t const* data, unsigned dataSize)
{
	if (GenericMediaServer::instance == nullptr) {
		return false;
	}
	net::RtspServer& server = GenericMediaServer::instance->rtsp_server();
	std::string data_str((const char*)data, dataSize);
	return server.SendRawBySocket(netSock, data_str, TRAFFIC_ANNOTATION_FOR_TESTS);
}

bool sessionid_is_valid_chromium(int sessionid)
{
	if (GenericMediaServer::instance == nullptr) {
		return false;
	}
	net::RtspServer& server = GenericMediaServer::instance->rtsp_server();
	return server.sessionid_is_valid(sessionid);
}

void GenericMediaServer::setup_chromium(uint32_t ipaddr)
{
	std::unique_ptr<net::ServerSocket> server_socket(new net::TCPServerSocket(NULL, net::NetLogSource()));
	// server_socket->ListenWithAddressAndPort("127.0.0.1", 8080, 1);

	// Must not use 127.0.0.1
	// make sure we have a value in a known byte order: big endian
	uint32_t addrNBO = htonl(ipaddr);
	std::stringstream addr_ss;
	addr_ss << (int)((addrNBO >> 24) & 0xFF) << "." << (int)((addrNBO>>16) & 0xFF);
	addr_ss << "." << (int)((addrNBO>>8)&0xFF) << "." << (int)(addrNBO & 0xFF);
	server_socket->ListenWithAddressAndPort(addr_ss.str(), htons(fServerPort.num()), 1);

	server_.reset(new net::RtspServer(std::move(server_socket), this));
	// ASSERT_THAT(server_->GetLocalAddress(&server_address_), IsOk());
	server_->GetLocalAddress(&server_address_);
}

////////// GenericMediaServer::ClientConnection implementation //////////
GenericMediaServer::ClientConnection
::ClientConnection(GenericMediaServer& ourServer, int clientSocket, struct sockaddr_in clientAddr, net::StreamSocket* netSock, int sessionid)
	: fOurServer(ourServer)
	, fOurSocket(clientSocket)
	, fClientAddr(clientAddr)
	, fNetSock(netSock)
	, fSessionid(sessionid)
{
	// Add ourself to our 'client connections' table:
	fOurServer.fClientConnections->Add((char const*)this, this);
  
	// Arrange to handle incoming requests:
	resetRequestBuffer();
	envir().taskScheduler().setBackgroundHandling(fOurSocket, SOCKET_READABLE|SOCKET_EXCEPTION, incomingRequestHandler, this);

	{
		threading::lock lock(live555d::tmanager::vidcap_mutex);
		if (live555d::tmanager::vidcap) {
			live555d::tmanager::vidcap->setup(fSessionid);
		}
	}
}

GenericMediaServer::ClientConnection::~ClientConnection()
{
	{
		threading::lock lock(live555d::tmanager::vidcap_mutex);
		if (live555d::tmanager::vidcap) {
			live555d::tmanager::vidcap->teardown(fSessionid);
		}
	}
	SDL_Log("---ClientConnection::~ClientConnection()---");

	// Remove ourself from the server's 'client connections' hash table before we go:
	fOurServer.fClientConnections->Remove((char const*)this);
  
	closeSockets();
}

void GenericMediaServer::ClientConnection::closeSockets() {
  // Turn off background handling on our socket:
  envir().taskScheduler().disableBackgroundHandling(fOurSocket);
  if (fOurSocket>= 0) ::closeSocket(fOurSocket);

  fOurSocket = -1;
}

void GenericMediaServer::ClientConnection::incomingRequestHandler(void* instance, int /*mask*/) {
  ClientConnection* connection = (ClientConnection*)instance;
  connection->incomingRequestHandler();
}

void GenericMediaServer::ClientConnection::incomingRequestHandler() {
  struct sockaddr_in dummy; // 'from' address, meaningless in this case
  
  int bytesRead = readSocket(envir(), fOurSocket, &fRequestBuffer[fRequestBytesAlreadySeen], fRequestBufferBytesLeft, dummy);
  handleRequestBytes(bytesRead);
}

void GenericMediaServer::ClientConnection::resetRequestBuffer() {
  fRequestBytesAlreadySeen = 0;
  fRequestBufferBytesLeft = sizeof fRequestBuffer;
}

////////// GenericMediaServer::ClientSession implementation //////////

GenericMediaServer::ClientSession
::ClientSession(GenericMediaServer& ourServer, u_int32_t sessionId)
  : fOurServer(ourServer), fOurSessionId(sessionId), fOurServerMediaSession(NULL),
    fLivenessCheckTask(NULL) {
  noteLiveness();
}

GenericMediaServer::ClientSession::~ClientSession()
{
	SDL_Log("ClientSession::~ClientSession(), E");
  // Turn off any liveness checking:
  envir().taskScheduler().unscheduleDelayedTask(fLivenessCheckTask);

  // Remove ourself from the server's 'client sessions' hash table before we go:
  char sessionIdStr[8+1];
  sprintf(sessionIdStr, "%08X", fOurSessionId);
  fOurServer.fClientSessions->Remove(sessionIdStr);
  
  if (fOurServerMediaSession != NULL) {
    fOurServerMediaSession->decrementReferenceCount();
    if (fOurServerMediaSession->referenceCount() == 0
	&& fOurServerMediaSession->deleteWhenUnreferenced()) {
      fOurServer.removeServerMediaSession(fOurServerMediaSession);
      fOurServerMediaSession = NULL;
    }
  }
  SDL_Log("ClientSession::~ClientSession(), X");
}

void GenericMediaServer::ClientSession::noteLiveness() {
#ifdef DEBUG
  char const* streamName
    = (fOurServerMediaSession == NULL) ? "???" : fOurServerMediaSession->streamName();
  fprintf(stderr, "Client session (id \"%08X\", stream name \"%s\"): Liveness indication\n",
	  fOurSessionId, streamName);
#endif
  if (fOurServerMediaSession != NULL) fOurServerMediaSession->noteLiveness();

  if (fOurServer.fReclamationSeconds > 0) {
    envir().taskScheduler().rescheduleDelayedTask(fLivenessCheckTask,
						  fOurServer.fReclamationSeconds*1000000,
						  (TaskFunc*)livenessTimeoutTask, this);
  }
}

void GenericMediaServer::ClientSession::noteClientLiveness(ClientSession* clientSession) {
  clientSession->noteLiveness();
}

void GenericMediaServer::ClientSession::livenessTimeoutTask(ClientSession* clientSession) {
  // If this gets called, the client session is assumed to have timed out, so delete it:
#ifdef DEBUG
  char const* streamName
    = (clientSession->fOurServerMediaSession == NULL) ? "???" : clientSession->fOurServerMediaSession->streamName();
  fprintf(stderr, "Client session (id \"%08X\", stream name \"%s\") has timed out (due to inactivity)\n",
	  clientSession->fOurSessionId, streamName);
#endif
  delete clientSession;
}

GenericMediaServer::ClientSession* GenericMediaServer::createNewClientSessionWithId() {
  u_int32_t sessionId;
  char sessionIdStr[8+1];

  // Choose a random (unused) 32-bit integer for the session id
  // (it will be encoded as a 8-digit hex number).  (We avoid choosing session id 0,
  // because that has a special use by some servers.)
  do {
    sessionId = (u_int32_t)our_random32();
    snprintf(sessionIdStr, sizeof sessionIdStr, "%08X", sessionId);
  } while (sessionId == 0 || lookupClientSession(sessionIdStr) != NULL);

  ClientSession* clientSession = createNewClientSession(sessionId);
  if (clientSession != NULL) fClientSessions->Add(sessionIdStr, clientSession);

  return clientSession;
}

GenericMediaServer::ClientSession*
GenericMediaServer::lookupClientSession(u_int32_t sessionId) {
  char sessionIdStr[8+1];
  snprintf(sessionIdStr, sizeof sessionIdStr, "%08X", sessionId);
  return lookupClientSession(sessionIdStr);
}

GenericMediaServer::ClientSession*
GenericMediaServer::lookupClientSession(char const* sessionIdStr) {
  return (GenericMediaServer::ClientSession*)fClientSessions->Lookup(sessionIdStr);
}


////////// ServerMediaSessionIterator implementation //////////

GenericMediaServer::ServerMediaSessionIterator
::ServerMediaSessionIterator(GenericMediaServer& server)
  : fOurIterator((server.fServerMediaSessions == NULL)
		 ? NULL : HashTable::Iterator::create(*server.fServerMediaSessions)) {
}

GenericMediaServer::ServerMediaSessionIterator::~ServerMediaSessionIterator() {
  delete fOurIterator;
}

ServerMediaSession* GenericMediaServer::ServerMediaSessionIterator::next() {
  if (fOurIterator == NULL) return NULL;

  char const* key; // dummy
  return (ServerMediaSession*)(fOurIterator->next(key));
}


////////// UserAuthenticationDatabase implementation //////////

UserAuthenticationDatabase::UserAuthenticationDatabase(char const* realm,
						       Boolean passwordsAreMD5)
  : fTable(HashTable::create(STRING_HASH_KEYS)),
    fRealm(strDup(realm == NULL ? "LIVE555 Streaming Media" : realm)),
    fPasswordsAreMD5(passwordsAreMD5) {
}

UserAuthenticationDatabase::~UserAuthenticationDatabase() {
  delete[] fRealm;
  
  // Delete the allocated 'password' strings that we stored in the table, and then the table itself:
  char* password;
  while ((password = (char*)fTable->RemoveNext()) != NULL) {
    delete[] password;
  }
  delete fTable;
}

void UserAuthenticationDatabase::addUserRecord(char const* username,
					       char const* password) {
  fTable->Add(username, (void*)(strDup(password)));
}

void UserAuthenticationDatabase::removeUserRecord(char const* username) {
  char* password = (char*)(fTable->Lookup(username));
  fTable->Remove(username);
  delete[] password;
}

char const* UserAuthenticationDatabase::lookupPassword(char const* username) {
  return (char const*)(fTable->Lookup(username));
}
