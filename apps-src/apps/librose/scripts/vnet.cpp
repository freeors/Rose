/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "scripts/vnet.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"

#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"

const char vhttp_agent::metatableKey[] = "vhttp_agent";

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */
static int impl_vhttp_agent_collect(lua_State *L)
{
	vhttp_agent *v = static_cast<vhttp_agent *>(lua_touserdata(L, 1));
	v->~vhttp_agent();
	return 0;
}

static int impl_vhttp_agent_get(lua_State *L)
{
	vhttp_agent *v = static_cast<vhttp_agent *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
	return 1;
}

static int impl_vhttp_agent_run(lua_State *L)
{
	vhttp_agent *v = static_cast<vhttp_agent *>(luaL_checkudata(L, 1, vhttp_agent::metatableKey));

	bool ret = v->run();
	lua_pushboolean(L, ret);
	return 1;
}

void luaW_pushvhttp_agent(lua_State* L, const config& cfg)
{
	tstack_size_lock lock(L, 1);
	new(L) vhttp_agent(instance->lua(), cfg);
	if (luaL_newmetatable(L, vhttp_agent::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vhttp_agent_collect},
			{"__index", impl_vhttp_agent_get},
			{"run", impl_vhttp_agent_run},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vhttp_agent::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vhttp_agent::vhttp_agent(rose_lua_kernel& lua, const config& cfg)
	: L(lua.get_state())
	, lua_(lua)
	, cfg_(cfg)
{
}

vhttp_agent::~vhttp_agent()
{
	// SDL_Log("[lua.gc]---vhttp_agent::~vhttp_agent()---");
}

bool vhttp_agent::did_pre(net::URLRequest& request, net::HttpRequestHeaders& headers, std::string& body)
{
	// If you execute the following statement, it makes it more difficult to find lua script errors.
	// for example lua function: http_agent_did_pre 
	// tstack_size_lock lock(L, 0);

	luaW_getglobal_2joined(L, cfg_["did_pre"].str().c_str());
	luaW_pushvconfig(L, vconfig(cfg_));
	lua_.protected_call(1, 2);

	// ret0: json request
	vconfig vcfg = luaW_checkvconfig(L, -2);
	Json::Value json;
	vcfg.get_config().to_json(json);
	Json::FastWriter writer;
	body = writer.write(json);

	// ret1: kContentType
	std::string content_type = "application/json; charset=UTF-8";
	if (lua_type(L, -1) == LUA_TSTRING) {
		content_type = luaL_checkstring(L, -1);
	}
	headers.SetHeader(net::HttpRequestHeaders::kContentType, content_type);

	lua_pop(L, 2);

	return true;
}

bool vhttp_agent::did_post(const net::URLRequest& request, const net::RoseDelegate& delegate, int status, std::string& err)
{
	tstack_size_lock lock(L, 0);

	const std::string& data_received = delegate.data_received();

	luaW_getglobal_2joined(L, cfg_["did_post"].str().c_str());
	luaW_pushvconfig(L, vconfig(cfg_));
	lua_pushinteger(L, status);
	luaW_pushvrodata(L, (const uint8_t*)data_received.c_str(), data_received.size());
	lua_.protected_call(3, 1);

	bool ret = luaW_toboolean(L, -1);

	// if (lua_type(L, -1) != LUA_TNIL) {
	//	err = luaL_checkstring(L, -1);
	// }
	lua_pop(L, 1);

	return ret;
}

bool vhttp_agent::run()
{
	std::string err;
	net::thttp_agent agent(cfg_["url"].str(), cfg_["method"].str(), null_str, cfg_["timeout"].to_int());
	agent.did_pre = std::bind(&vhttp_agent::did_pre, this, _1, _2, _3);
	agent.did_post = std::bind(&vhttp_agent::did_post, this, _1, _2, _3, std::ref(err));
	return net::handle_http_request(agent);
}


const char vtcpc_manager::metatableKey[] = "vtcpc_manager";

/**
 * Destroys a vtcpc_manager object before it is collected (__gc metamethod).
 */
static int impl_vtcpc_manager_collect(lua_State *L)
{
	vtcpc_manager *v = static_cast<vtcpc_manager *>(lua_touserdata(L, 1));
	v->~vtcpc_manager();
	return 0;
}

static int impl_vtcpc_manager_get(lua_State *L)
{
	vtcpc_manager *v = static_cast<vtcpc_manager *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
	return 1;
}

static int impl_vtcpc_manager_xmit_transaction(lua_State *L)
{
	vtcpc_manager *v = static_cast<vtcpc_manager *>(luaL_checkudata(L, 1, vtcpc_manager::metatableKey));

	const char* host = luaL_checkstring(L, 2);
	uint16_t port = lua_tointeger(L, 3);
	vdata* data = static_cast<vdata *>(luaL_checkudata(L, 4, vdata::metatableKey));
	int timeout = lua_tointeger(L, 5);

	bool ret = v->xmit_transaction(host, port, data->get_data(), timeout);
	lua_pushboolean(L, ret);
	return 1;
}

void luaW_pushvtcpcmgr(lua_State* L)
{
	tstack_size_lock lock(L, 1);
	new(L) vtcpc_manager(instance->lua());
	if (luaL_newmetatable(L, vtcpc_manager::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vtcpc_manager_collect},
			{"__index", impl_vtcpc_manager_get},
			{"xmit_transaction", impl_vtcpc_manager_xmit_transaction},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vtcpc_manager::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vtcpc_manager::vtcpc_manager(rose_lua_kernel& lua)
	: L(lua.get_state())
	, lua_(lua)
	, tcpc_mgr_(std::bind(&net::app_create_delegate_default, _1, _2))
{
}

vtcpc_manager::~vtcpc_manager()
{
	// SDL_Log("[lua.gc]---vtcpc_manager::~vtcpc_manager()---");
}

bool vtcpc_manager::xmit_transaction(const std::string& host, uint16_t port, const tuint8cdata& req, int timeout)
{
	return tcpc_mgr_.xmit_transaction(host, port, req, timeout);
}