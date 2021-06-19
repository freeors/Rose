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

#include "scripts/vpreferences.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"

#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"
#include "serialization/parser.hpp"
using namespace std::placeholders;

const char vpreferences::metatableKey[] = "vpreferences";

/**
 * Destroys a vpreferences object before it is collected (__gc metamethod).
 */
static int impl_vpreferences_collect(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));
	v->~vpreferences();
	return 0;
}

static int impl_vpreferences_get(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	if (strcmp(m, "_set_value") == 0 || strcmp(m, "_value") == 0) {
		bool ret = luaW_getmetafield(L, 1, m);
		return ret? 1: 0;
	}
	v->value(m);
	return 1;
}

static int impl_vpreferences_set(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));
	v->set_value(1, 2);
	v->write_prefs();
	return 0;
}

static int impl_vpreferences_value(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(luaL_checkudata(L, 1, vpreferences::metatableKey));

	// below v->value(key) will increase stack. so require calculate count first.
	int count = 0;
	while (true) {
		if (lua_type(L, 2 + count) == LUA_TNONE) {
			// no more key.
			break;
		}
		count ++;
	}

	for (int at = 0; at < count; at ++) {
		const char* key = luaL_checkstring(L, 2 + at);
		v->value(key);
	}
	return count;
}

static int impl_vpreferences_set_value(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(luaL_checkudata(L, 1, vpreferences::metatableKey));

	v->set_value(nposm, 2);
	v->write_prefs();
	return 0;
}

vpreferences* luaW_pushvpreferences(lua_State* L, const std::string& lua_bundleid)
{
	tstack_size_lock lock(L, 1);
	vpreferences* v = new(L) vpreferences(instance->lua(), lua_bundleid);
	if (luaL_newmetatable(L, vpreferences::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vpreferences_collect},
			{"__index", impl_vpreferences_get},
			{"__newindex", impl_vpreferences_set},
			{"_value", impl_vpreferences_value},
			{"_set_value", impl_vpreferences_set_value},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vpreferences::metatableKey);
	}
	lua_setmetatable(L, -2);
	return v;
}

bool vpreferences::did_read_preferences_dat(tfile& file, int64_t fsize, bool bak)
{
	fsize = file.read_2_data();
	std::string stream;
	stream.assign(file.data, fsize);
	try {
		read(cfg_, stream);
		const config& signature_cfg = cfg_.child(signature_tag.c_str());
		if (!signature_cfg) {
			// exist preferences, but isn't valid
			cfg_.clear();
		} else {
			if (!cfg_.empty()) {
				resize_data(fsize, 0);
				memcpy(data_, file.data, fsize);
				data_vsize_ = fsize;
			}
		}
	} catch (twml_exception& ) {
		cfg_.clear();
	}

	return !cfg_.empty();
}

// nposm: write fail. delete this file
//
bool vpreferences::did_pre_write_preferences_dat()
{
	std::stringstream out;
	// wirte full prefs
	try {
		write(out, cfg_);

	} catch (io_exception&) {
		// error writing to preferences file '$get_prefs_file()'
		return false; 
	}

	if (data_vsize_ == out.str().size()) {
		if (memcmp(data_, out.str().c_str(), data_vsize_) == 0) {
			// no change, do nothing.
			return false;
		}
	}

	data_vsize_ = out.str().size();
	if (data_vsize_ != 0) {
		resize_data(data_vsize_, 0);
		memcpy(data_, out.str().c_str(), data_vsize_);
	}

	return true;
}

bool vpreferences::did_write_preferences_dat(tfile& file)
{
	if (data_vsize_ != 0) {
		posix_fwrite(file.fp, data_, data_vsize_);
	}
	return true;
}

vpreferences::vpreferences(rose_lua_kernel& lua, const std::string& lua_bundleid)
	: signature_tag("signature")
	, L(lua.get_state())
	, lua_(lua)
	, lua_bundleid_(lua_bundleid)
	, prefs_file_(get_aplt_prefs_file(lua_bundleid))
	, data_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
{
	{
		tbakreader file(prefs_file_, true, std::bind(&vpreferences::did_read_preferences_dat, this, _1, _2, _3));
		file.read();
	}
	if (!cfg_.has_child(signature_tag.c_str())) {
		cfg_.add_child(signature_tag.c_str());
	}
}

vpreferences::~vpreferences()
{
	if (data_ != nullptr) {
		free(data_);
	}
	// SDL_Log("[lua.gc]---vpreferences::~vpreferences()---");
}

void vpreferences::write_prefs()
{
	tbakwriter bakfile(prefs_file_, true, std::bind(&vpreferences::did_write_preferences_dat, this, _1), std::bind(&vpreferences::did_pre_write_preferences_dat, this));
	bakfile.write();
}

void vpreferences::value(const std::string& key) const
{
	// use must make sure this key existed!
	VALIDATE(cfg_.has_attribute(key), null_str);

	const config::attribute_value& v = cfg_[key];

	config::attribute_value::vtype type = v.type();
	if (type == config::attribute_value::type_blank) {
		lua_pushnil(L);
	} else if (type == config::attribute_value::type_bool) {
		lua_pushboolean(L, v.to_bool());
	} else if (type == config::attribute_value::type_integer) {
		lua_pushinteger(L, v.to_long_long());
	} else if (type == config::attribute_value::type_double) {
		lua_pushnumber(L, v.to_double());
	} else if (type == config::attribute_value::type_string) {
		lua_pushstring(L, v.str().c_str());
	} else if (type == config::attribute_value::type_tstring) {
		VALIDATE(false, null_str);
		luaW_pushtstring(L, v.t_str());
	} else {
		VALIDATE(false, null_str);
	}
}

void vpreferences::set_value(int count, int first_key_stack_idx)
{
	VALIDATE(count > 0 || count == nposm, null_str);
	if (count == nposm) {
		count = INT32_MAX;
	}

	int stack_idx = first_key_stack_idx;
	const char* c_str = nullptr;
	for (int at = 0; at < count; at ++, stack_idx ++) {
		if (lua_type(L, stack_idx) == LUA_TNONE) {
			// no more key-value
			break;
		}
		const std::string key = luaL_checkstring(L, stack_idx);
		stack_idx ++;
		int val_type = lua_type(L, stack_idx);
		if (val_type == LUA_TNIL) {
			cfg_.remove_attribute(key);

		} else if (val_type == LUA_TBOOLEAN) {
			cfg_[key] = luaW_toboolean(L, stack_idx);

		} else if (val_type == LUA_TNUMBER) {
			if (lua_isinteger(L, stack_idx)) {
				cfg_[key] = lua_tointeger(L, stack_idx);
			} else {
				cfg_[key] = lua_tonumber(L, stack_idx);
			}

		} else if (val_type == LUA_TSTRING) {
			cfg_[key].from_string(luaL_checkstring(L, stack_idx), true);

		} else {
			VALIDATE(false, null_str);
		}
	}
}

void vpreferences::resize_data(int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size_) {
		char* tmp = (char*)malloc(size);
		if (data_ != nullptr) {
			if (vsize) {
				memcpy(tmp, data_, vsize);
			}
			free(data_);
		}
		data_ = tmp;
		data_size_ = size;
	}
}