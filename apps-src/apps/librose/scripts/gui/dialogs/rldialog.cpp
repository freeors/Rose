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

#include "scripts/gui/dialogs/rldialog.hpp"
#include "scripts/gui/widgets/vwidget.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"

#include "gui/widgets/panel.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

namespace gui2 {

const char vdialog::metatableKey[] = "gui2.dialog";

/**
 * Destroys a vdialog object before it is collected (__gc metamethod).
 */
static int impl_vdialog_collect(lua_State *L)
{
	vdialog *v = static_cast<vdialog *>(lua_touserdata(L, 1));
	v->~vdialog();
	return 0;
}

static int impl_vdialog_get(lua_State *L)
{
	vdialog *v = static_cast<vdialog *>(lua_touserdata(L, 1));

	VALIDATE(false, null_str);
	lua_pushnil(L);
	return 1;
}

void luaW_pushvdialog(lua_State* L, trldialog& dlg)
{
	tstack_size_lock lock(L, 1);
	new(L) vdialog(dlg);
	if (luaL_newmetatable(L, vdialog::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vdialog_collect},
			{"__index", impl_vdialog_get},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vdialog::metatableKey);
	}
	lua_setmetatable(L, -2);
}

trldialog::trldialog(rose_lua_kernel& lua, const std::string& aplt, const std::string& id)
	: L(lua.get_state())
	, lua_(lua)
	, aplt_(aplt)
	, id_(id)
	, joined_id_(utils::generate_app_prefix_id(aplt, id))
	, has_timer_handler_(false)
{
	tstack_size_lock lock(L, 0);
	luaW_getglobal(L, joined_id_.c_str(), "construct");
	lua_.protected_call(0, 1);
	int interval = lua_tointegerx(L, -1, nullptr);
	lua_pop(L, 1);

	// int interval = lua_.construct_dialog(joined_id_);
	if (interval != 0) {
		set_timer_interval(interval);
		has_timer_handler_ = luaW_checkglobal2(L, joined_id_.c_str(), "timer_handler");
		VALIDATE(has_timer_handler_, "since enable timer, must implement timer_handler");
	}
}

trldialog::~trldialog()
{
	// Enforce a complete garbage collection
	lua_gc(L, LUA_GCCOLLECT);
}

const std::string& trldialog::window_id() const
{
	return joined_id_;
}

void trldialog::pre_show()
{
	tpanel* aplt = find_widget<tpanel>(window_, "aplt", false, true);
	aplt->set_border("blue_ellipse");

	tstack_size_lock lock(L, 0);
	luaW_getglobal(L, joined_id_.c_str(), "pre_show");
	luaW_pushvdialog(L, *this);
	luaW_pushvwidget(L, *window_);
	lua_.protected_call(2, 0);
}

void trldialog::post_show()
{
	tstack_size_lock lock(L, 0);
	luaW_getglobal(L, joined_id_.c_str(), "post_show");
	lua_.protected_call(0, 0);
}

void trldialog::app_timer_handler(uint32_t now)
{
	if (!has_timer_handler_) {
		return;
	}

	gui2::twidget::tdisable_lua_unlimited_mem_lock lock;

	luaW_getglobal(L, joined_id_.c_str(), "timer_handler");
	lua_pushinteger(L, now);
	lua_.protected_call(1, 0);
}

void trldialog::app_OnMessage(rtc::Message* msg)
{
	luaW_getglobal(L, joined_id_.c_str(), "app_OnMessage");
	lua_pushinteger(L, msg->message_id);
	tmsg_data_cfg* pdata = static_cast<tmsg_data_cfg*>(msg->pdata);
	luaW_pushvconfig(L, vconfig(pdata->cfg));
	lua_.protected_call(2, 0);

	if (msg->pdata) {
		delete msg->pdata;
	}
}

} // namespace gui2


