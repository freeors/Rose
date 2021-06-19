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

#include "scripts/vprotobuf.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"

#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"

const char vpbitem::metatableKey[] = "vpbitem";

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */
static int impl_vpbitem_collect(lua_State *L)
{
	vpbitem *v = static_cast<vpbitem *>(lua_touserdata(L, 1));
	v->~vpbitem();
	return 0;
}

static int impl_vpbitem_get(lua_State *L)
{
	vpbitem *v = static_cast<vpbitem *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vpbitem_value(lua_State *L)
{
	vpbitem *v = static_cast<vpbitem *>(lua_touserdata(L, 1));

	// below v->value(idx) will increase stack. so require calculate count first.
	int count = 0;
	while (true) {
		if (lua_type(L, 2 + count) == LUA_TNONE) {
			// no more key.
			break;
		}
		count ++;
	}

	tstack_size_lock lock(L, count);
	for (int at = 0; at < count; at ++) {
		int idx = lua_tointeger(L, 2 + at);
		v->value(idx);
	}
	return count;
}

static int impl_vpbitem_set_value(lua_State *L)
{
	vpbitem *v = static_cast<vpbitem *>(lua_touserdata(L, 1));

	v->set_value(nposm, 2);
	return 0;
}

static vpbitem* luaW_pushvpbitem(lua_State* L, pb2::titem& notification, bool readonly)
{
	// tstack_size_lock lock(L, 1);
	vpbitem* v = new(L) vpbitem(instance->lua(), notification, readonly);
	if (luaL_newmetatable(L, vpbitem::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vpbitem_collect},
			{"__index", impl_vpbitem_get},
			{"value", impl_vpbitem_value},
			{"set_value", impl_vpbitem_set_value},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vpbitem::metatableKey);
	}
	lua_setmetatable(L, -2);
	return v;
}

vpbitem::vpbitem(rose_lua_kernel& lua, pb2::titem& item, bool readonly)
	: L(lua.get_state())
	, lua_(lua)
	, item_(&item)
	, readonly_(readonly)
{
}

vpbitem::~vpbitem()
{
	// SDL_Log("[lua.gc]---vpbitem::~vpbitem()---");
}

void vpbitem::value(int idx) const
{
	// PBIDX_ERRCODE = utils.PBIDX_N32ITEM0,
	// PBIDX_CHANNEL = utils.PBIDX_N32ITEM1,
	// PBIDX_DEVICEID = utils.PBIDX_N32ITEM2,
	// PBIDX_TIMESTAMP = utils.PBIDX_N64ITEM0,
	// PBIDX_CTIME = utils.PBIDX_N64ITEM1,
	// PBIDX_UUID = utils.PBIDX_STRITEM0,
	// PBIDX_MSG = utils.PBIDX_STRITEM1,
	// PBIDX_ACCOUNT = utils.PBIDX_STRITEM2,

	if (idx >= PBIDX_N32ITEM0 && idx < PBIDX_N32ITEM0 + VPROTOBUF_N32ITEM_COUNT) {
		if (idx == PBIDX_N32ITEM0) {
			lua_pushinteger(L, item_->n32item0());
		} else if (idx == PBIDX_N32ITEM1) {
			lua_pushinteger(L, item_->n32item1());
		} else if (idx == PBIDX_N32ITEM2) {
			lua_pushinteger(L, item_->n32item2());
		} else if (idx == PBIDX_N32ITEM3) {
			lua_pushinteger(L, item_->n32item3());
		} else if (idx == PBIDX_N32ITEM4) {
			lua_pushinteger(L, item_->n32item4());
		} else if (idx == PBIDX_N32ITEM5) {
			lua_pushinteger(L, item_->n32item5());
		} else if (idx == PBIDX_N32ITEM6) {
			lua_pushinteger(L, item_->n32item6());
		} else if (idx == PBIDX_N32ITEM7) {
			lua_pushinteger(L, item_->n32item7());
		} else if (idx == PBIDX_N32ITEM8) {
			lua_pushinteger(L, item_->n32item8());
		} else if (idx == PBIDX_N32ITEM9) {
			lua_pushinteger(L, item_->n32item9());
		} else {
			VALIDATE(false, null_str);
		}

	} else if (idx >= PBIDX_N64ITEM0 && idx < PBIDX_N64ITEM0 + VPROTOBUF_N64ITEM_COUNT) {
		if (idx == PBIDX_N64ITEM0) {
			lua_pushinteger(L, item_->n64item0());
		} else if (idx == PBIDX_N64ITEM1) {
			lua_pushinteger(L, item_->n64item1());
		} else if (idx == PBIDX_N64ITEM2) {
			lua_pushinteger(L, item_->n64item2());
		} else {
			VALIDATE(false, null_str);
		}

	} else if (idx >= PBIDX_STRITEM0 && idx < PBIDX_STRITEM0 + VPROTOBUF_STRITEM_COUNT) {
		if (idx == PBIDX_STRITEM0) {
			const std::string& str = item_->stritem0();
			lua_pushlstring(L, str.c_str(), str.size());
		} else if (idx == PBIDX_STRITEM1) {
			const std::string& str = item_->stritem1();
			lua_pushlstring(L, str.c_str(), str.size());
		} else if (idx == PBIDX_STRITEM2) {
			const std::string& str = item_->stritem2();
			lua_pushlstring(L, str.c_str(), str.size());
		} else if (idx == PBIDX_STRITEM3) {
			const std::string& str = item_->stritem3();
			lua_pushlstring(L, str.c_str(), str.size());
		} else if (idx == PBIDX_STRITEM4) {
			const std::string& str = item_->stritem4();
			lua_pushlstring(L, str.c_str(), str.size());
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}
}

void vpbitem::set_value(int count, int first_stack_idx)
{
	// stack#1 is vpbitem
	VALIDATE(count > 0 || count == nposm, null_str);
	if (count == nposm) {
		count = INT32_MAX;
	}
	int stack_idx = first_stack_idx;
	size_t size = 0;
	const char* c_str = nullptr;
	for (int at = 0; at < count; at ++, stack_idx ++) {
		if (lua_type(L, stack_idx) == LUA_TNONE) {
			// nore item
			break;
		}
		int idx = lua_tointeger(L, stack_idx);
		stack_idx ++;

		if (idx >= PBIDX_N32ITEM0 && idx < PBIDX_N32ITEM0 + VPROTOBUF_N32ITEM_COUNT) {
			int n32 = lua_tointeger(L, stack_idx);
			if (idx == PBIDX_N32ITEM0) {
				item_->set_n32item0(n32);
			} else if (idx == PBIDX_N32ITEM1) {
				item_->set_n32item1(n32);
			} else if (idx == PBIDX_N32ITEM2) {
				item_->set_n32item2(n32);
			} else if (idx == PBIDX_N32ITEM3) {
				item_->set_n32item3(n32);
			} else if (idx == PBIDX_N32ITEM4) {
				item_->set_n32item4(n32);
			} else if (idx == PBIDX_N32ITEM5) {
				item_->set_n32item5(n32);
			} else if (idx == PBIDX_N32ITEM6) {
				item_->set_n32item6(n32);
			} else if (idx == PBIDX_N32ITEM7) {
				item_->set_n32item7(n32);
			} else if (idx == PBIDX_N32ITEM8) {
				item_->set_n32item8(n32);
			} else if (idx == PBIDX_N32ITEM9) {
				item_->set_n32item9(n32);
			} else {
				VALIDATE(false, null_str);
			}

		} else if (idx >= PBIDX_N64ITEM0 && idx < PBIDX_N64ITEM0 + VPROTOBUF_N64ITEM_COUNT) {
			int64_t n64 = lua_tointeger(L, stack_idx);
			if (idx == PBIDX_N64ITEM0) {
				item_->set_n64item0(n64);
			} else if (idx == PBIDX_N64ITEM1) {
				item_->set_n64item1(n64);
			} else if (idx == PBIDX_N64ITEM2) {
				item_->set_n64item2(n64);
			} else {
				VALIDATE(false, null_str);
			}

		} else if (idx >= PBIDX_STRITEM0 && idx < PBIDX_STRITEM0 + VPROTOBUF_STRITEM_COUNT) {
			c_str = luaL_checklstring(L, stack_idx, &size);
			if (idx == PBIDX_STRITEM0) {
				item_->set_stritem0(c_str, size);
			} else if (idx == PBIDX_STRITEM1) {
				item_->set_stritem1(c_str, size);
			} else if (idx == PBIDX_STRITEM2) {
				item_->set_stritem2(c_str, size);
			} else if (idx == PBIDX_STRITEM3) {
				item_->set_stritem3(c_str, size);
			} else if (idx == PBIDX_STRITEM4) {
				item_->set_stritem4(c_str, size);
			}
		} else {
			VALIDATE(false, null_str);
		}
	}
}

const char vprotobuf::metatableKey[] = "vprotobuf";

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */
static int impl_vprotobuf_collect(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));
	v->~vprotobuf();
	return 0;
}

static int impl_vprotobuf_get(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
	return 1;
}

static int impl_vprotobuf_load_sha1pb(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(luaL_checkudata(L, 1, vprotobuf::metatableKey));

	bool use_backup = luaW_toboolean(L, 2);
	v->load_sha1pb(use_backup);
	return 0;
}

static int impl_vprotobuf_write_sha1pb(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(luaL_checkudata(L, 1, vprotobuf::metatableKey));

	bool use_backup = luaW_toboolean(L, 2);
	v->write_sha1pb(use_backup);
	return 0;
}

static int impl_vprotobuf_item_size(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	lua_pushinteger(L, v->items_size());
	return 1;
}

static int impl_vprotobuf_item(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	int at = lua_tointeger(L, 2);
	bool read_only = luaW_toboolean(L, 3);
	v->item(at, read_only);
	return 1;
}

static int impl_vprotobuf_add_item(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	pb2::titem& item = v->add_item();
	if (lua_type(L, 2) != LUA_TNONE) {
		int count = 0;
		while (true) {
			if (lua_type(L, 2 + count * 2) == LUA_TNONE) {
				// nore item
				break;
			}
			count ++;
		}

		// below luaW_pushvpbitem will increase stack. so require calculate count first.
		vpbitem* vi = luaW_pushvpbitem(L, item, false);
		vi->set_value(count, 2);
	}
	return 1;
}

static int impl_vprotobuf_value(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	// below v->value(idx) will increase stack. so require calculate count first.
	int count = 0;
	while (true) {
		if (lua_type(L, 2 + count) == LUA_TNONE) {
			// no more key.
			break;
		}
		count ++;
	}

	tstack_size_lock lock(L, count);
	for (int at = 0; at < count; at ++) {
		int idx = lua_tointeger(L, 2 + at);
		v->value(idx);
	}
	return count;
}

static int impl_vprotobuf_set_value(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	v->set_value(2);
	return 0;
}

static int impl_vprotobuf_delete_subrange(lua_State *L)
{
	vprotobuf *v = static_cast<vprotobuf *>(lua_touserdata(L, 1));

	int start = lua_tointeger(L, 2);
	int num = lua_tointeger(L, 3);
	v->delete_subrange(start, num);
	return 0;
}

void luaW_pushvprotobuf(lua_State* L, const std::string& filepath)
{
	// tstack_size_lock lock(L, 1);
	new(L) vprotobuf(instance->lua(), filepath);
	if (luaL_newmetatable(L, vprotobuf::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vprotobuf_collect},
			{"__index", impl_vprotobuf_get},
			{"load_sha1pb", impl_vprotobuf_load_sha1pb},
			{"write_sha1pb", impl_vprotobuf_write_sha1pb},
			{"item_size", impl_vprotobuf_item_size},
			{"item", impl_vprotobuf_item},
			{"add_item", impl_vprotobuf_add_item},
			{"value", impl_vprotobuf_value},
			{"set_value", impl_vprotobuf_set_value},
			{"delete_subrange", impl_vprotobuf_delete_subrange},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vprotobuf::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vprotobuf::vprotobuf(rose_lua_kernel& lua, const std::string& filepath)
	: L(lua.get_state())
	, lua_(lua)
	, filepath_(filepath)
{}

vprotobuf::~vprotobuf()
{
	// SDL_Log("[lua.gc]---vprotobuf::~vprotobuf()---");
}

void vprotobuf::load_sha1pb(bool use_backup)
{
	protobuf::load_sha1pb(filepath_, &pb_items_, use_backup);
}

void vprotobuf::write_sha1pb(bool use_backup)
{
	protobuf::write_sha1pb(filepath_, &pb_items_, use_backup);
}

int vprotobuf::items_size() const
{
	return pb_items_.items_size();
}

void vprotobuf::item(int at, bool read_only)
{
	pb2::titem* notification = nullptr;
	if (read_only) {
		notification = const_cast<pb2::titem*>(&pb_items_.items(at));
	} else {
		notification = pb_items_.mutable_items(at);
	}
	luaW_pushvpbitem(L, *notification, read_only);
}

pb2::titem& vprotobuf::add_item()
{
	// stack#1 is vprotobuf
	return *pb_items_.add_items();
}

void vprotobuf::value(int idx) const
{
	// PBIDX_VERSION = utils.PBIDX_N32TOP0,
	// PBIDX_NOTIFICATION_TS = utils.PBIDX_N64TOP0,	

	if (idx == PBIDX_N32TOP0) {
		lua_pushinteger(L, pb_items_.n32top0());
	} else if (idx == PBIDX_N32TOP1) {
		lua_pushinteger(L, pb_items_.n32top1());
	} else if (idx == PBIDX_N32TOP2) {
		lua_pushinteger(L, pb_items_.n32top2());

	} else if (idx == PBIDX_N64TOP0) {
		lua_pushinteger(L, pb_items_.n64top0());
	} else if (idx == PBIDX_N64TOP1) {
		lua_pushinteger(L, pb_items_.n64top1());
	} else if (idx == PBIDX_N64TOP2) {
		lua_pushinteger(L, pb_items_.n64top2());

	} else if (idx == PBIDX_STRTOP0) {
		const std::string& str = pb_items_.strtop0();
		lua_pushlstring(L, str.c_str(), str.size());
	} else if (idx == PBIDX_STRTOP1) {
		const std::string& str = pb_items_.strtop1();
		lua_pushlstring(L, str.c_str(), str.size());
	} else if (idx == PBIDX_STRTOP2) {
		const std::string& str = pb_items_.strtop2();
		lua_pushlstring(L, str.c_str(), str.size());
	} else {
		VALIDATE(false, null_str);
	}
}

void vprotobuf::set_value(int first_stack_idx)
{
	int count = INT32_MAX;
	int stack_idx = first_stack_idx;
	size_t size = 0;
	const char* c_str = nullptr;
	for (int at = 0; at < count; at ++, stack_idx ++) {
		if (lua_type(L, stack_idx) == LUA_TNONE) {
			// nore item
			break;
		}
		int idx = lua_tointeger(L, stack_idx);
		stack_idx ++;
		if (idx == PBIDX_N32TOP0) {
			int n32 = lua_tointeger(L, stack_idx);
			pb_items_.set_n32top0(n32);
		} else if (idx == PBIDX_N32TOP1) {
			int n32 = lua_tointeger(L, stack_idx);
			pb_items_.set_n32top1(n32);
		} else if (idx == PBIDX_N32TOP2) {
			int n32 = lua_tointeger(L, stack_idx);
			pb_items_.set_n32top2(n32);

		} else if (idx == PBIDX_N64TOP0) {
			pb_items_.set_n64top0(lua_tointeger(L, stack_idx));
		} else if (idx == PBIDX_N64TOP1) {
			pb_items_.set_n64top1(lua_tointeger(L, stack_idx));
		} else if (idx == PBIDX_N64TOP2) {
			pb_items_.set_n64top2(lua_tointeger(L, stack_idx));

		} else if (idx == PBIDX_STRTOP0) {
			c_str = luaL_checklstring(L, stack_idx, &size);
			pb_items_.set_strtop0(c_str, size);
		} else if (idx == PBIDX_STRTOP1) {
			c_str = luaL_checklstring(L, stack_idx, &size);
			pb_items_.set_strtop1(c_str, size);
		} else if (idx == PBIDX_STRTOP2) {
			c_str = luaL_checklstring(L, stack_idx, &size);
			pb_items_.set_strtop2(c_str, size);
		} else {
			VALIDATE(false, null_str);
		}
	}
}

void vprotobuf::delete_subrange(int start, int num)
{
	pb_items_.mutable_items()->DeleteSubrange(start, num);
}