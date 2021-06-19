/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_SCRIPTS_VNET_HPP
#define LIBROSE_SCRIPTS_VNET_HPP

#include "config.hpp"
#include "lua/lua.h"

#include <net/url_request/url_request_http_job_rose.hpp>
#include <net/server/tcp_client_rose.h>

class rose_lua_kernel;

class vhttp_agent
{
public:
	static const char metatableKey[];

	vhttp_agent(rose_lua_kernel& lua, const config& cfg);
	virtual ~vhttp_agent();

	bool run();

private:
	bool did_pre(net::URLRequest& request, net::HttpRequestHeaders& headers, std::string& body);
	bool did_post(const net::URLRequest& request, const net::RoseDelegate& delegate, int status, std::string& err);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const config cfg_;
};

class vtcpc_manager
{
public:
	static const char metatableKey[];

	vtcpc_manager(rose_lua_kernel& lua);
	virtual ~vtcpc_manager();

	bool xmit_transaction(const std::string& host, uint16_t port, const tuint8cdata& req, int timeout);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	net::ttcpc_manager tcpc_mgr_;
};

void luaW_pushvhttp_agent(lua_State* L, const config& cfg);
void luaW_pushvtcpcmgr(lua_State* L);

#endif

