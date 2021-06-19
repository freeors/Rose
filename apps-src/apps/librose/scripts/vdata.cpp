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

#include "base_instance.hpp"

#include "scripts/vdata.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lobject.h"

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */

static int impl_vjson_collect(lua_State *L)
{
	vjson *v = static_cast<vjson *>(lua_touserdata(L, 1));
	v->~vjson();
	return 0;
}

static int impl_vjson_get(lua_State *L)
{
	vjson *v = static_cast<vjson *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vjson_get_value(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	const char* field = luaL_checkstring(L, 2);
	int type = lua_tointeger(L, 3);
	v->json_value(field, type);
	return 1;
}

static int impl_vjson_array_size(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	int s = v->json_array_size();
	lua_pushinteger(L, s);
	return 1;
}

static int impl_vjson_array_at(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	int at = lua_tointeger(L, 2);
	v->json_array_at(at);
	return 1;
}

void luaW_pushvjson_bh(lua_State* L)
{
	tstack_size_lock lock(L, 0);
	VALIDATE(lua_type(L, -1) == LUA_TUSERDATA, null_str);
	if (luaL_newmetatable(L, vjson::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vjson_collect},
			{"__index", impl_vjson_get},
			{"value", impl_vjson_get_value},
			{"array_size", impl_vjson_array_size},
			{"array_at", impl_vjson_array_at},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vjson::metatableKey);
	}
	lua_setmetatable(L, -2);
}

void luaW_pushvjson(lua_State* L, const Json::Value& value)
{
	new(L) vjson(instance->lua(), value);
	luaW_pushvjson_bh(L);
}

const char vjson::metatableKey[] = "vjson";
vjson::vjson(rose_lua_kernel& lua)
	: L(lua.get_state())
	, lua_(lua)
{
}

vjson::vjson(rose_lua_kernel& lua, const Json::Value& value)
	: L(lua.get_state())
	, lua_(lua)
	, value_(value)
{}

vjson::~vjson()
{
	// SDL_Log("[lua.gc]---vjson::~vjson()---");
}

void vjson::json_value(const char* field, int type)
{
	VALIDATE(field != nullptr && field[0] != '\0', null_str);

	if (type == LUA_TBOOLEAN) {
		lua_pushboolean(L, value_[field].asBool());

	} else if (type == LUA_TSTRING) {
		lua_pushstring(L, value_[field].asString().c_str());

	} else if (type == LUA_VNUMFLT) {
		lua_pushnumber(L, value_[field].asDouble());

	} else if (type == LUA_VNUMINT) {
		lua_pushinteger(L, value_[field].asInt64());

	} else if (type == LUA_TUSERDATA) {
		Json::Value& result = value_[field];
		if (result.isObject() || result.isArray()) {
			luaW_pushvjson(L, result);
		} else {
			lua_pushnil(L);
		}

	} else {
		VALIDATE(false, nullptr);
	}
}

int vjson::json_array_size() const
{
	if (!value_.isArray()) {
		return nposm;
	}
	return value_.size();
}

void vjson::json_array_at(int at)
{
	Json::Value& results = value_[at];
	luaW_pushvjson(L, results);
}

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */
static int impl_vdata_collect(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));
	v->~vdata();
	return 0;
}

static int impl_vdata_get(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vdata_read_json(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = lua_tointeger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);

	v->read_json(offset, len);
	return 1;
}

static int impl_vdata_read_lstring(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = lua_tointeger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);

	v->read_lstring(offset, len);
	return 1;
}

static int impl_vdata_read_vdata(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = lua_tointeger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);
	bool ro = lua_isnoneornil(L, 4)? false: luaW_toboolean(L, 4);

	v->read_vdata(offset, len, ro);
	return 1;
}

static int impl_vdata_write_lstring(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	size_t size = 0;
	const char* data = luaL_checklstring(L, 2, &size);

	int wrote = v->write_lstring(data, size);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_json(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	vconfig vcfg = luaW_checkvconfig(L, 2);

	int wrote = v->write_json(vcfg.get_config());
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_align(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int align = lua_tointeger(L, 2);
	int val = luaL_optinteger(L, 3, '\0');

	int wrote = v->write_align(align, val);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_nbyte(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int n = lua_tointeger(L, 2);
	int val = luaL_optinteger(L, 3, '\0');

	int wrote = v->write_nbyte(n, val);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_to_file(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = lua_tointeger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);
	int type = luaL_checkinteger(L, 4);
	const char* path = luaL_checkstring(L, 5);

	int ret = v->to_file(offset, len, type, path);
	lua_pushinteger(L, ret);
	return 1;
}

static void luaW_pushvdata_bh(lua_State* L)
{
	// tstack_size_lock lock(L, 0);
	if (luaL_newmetatable(L, vdata::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vdata_collect},
			{"__index", impl_vdata_get},
			{"read_json", impl_vdata_read_json},
			{"read_lstring", impl_vdata_read_lstring},
			{"read_vdata", impl_vdata_read_vdata},
			{"write_lstring", impl_vdata_write_lstring},
			{"write_json", impl_vdata_write_json},
			{"write_align", impl_vdata_write_align},
			{"write_nbyte", impl_vdata_write_nbyte},

			{"to_file", impl_vdata_to_file},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vdata::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vdata* luaW_pushvrwdata(lua_State* L, int initial_size)
{
	vdata* v = new(L) vdata(instance->lua(), initial_size);
	luaW_pushvdata_bh(L);
	return v;
}

void luaW_pushvrodata(lua_State* L, const uint8_t* data, int len)
{
	new(L) vdata(instance->lua(), data, len);
	luaW_pushvdata_bh(L);
}

const char vdata::metatableKey[] = "vdata";
vdata::vdata(rose_lua_kernel& lua, const uint8_t* data, int size)
	: L(lua.get_state())
	, lua_(lua)
	, data_ptr_(const_cast<uint8_t*>(data))
	, data_size_(size)
	, data_vsize_(size)
	, read_only_(true)
{
}

vdata::vdata(rose_lua_kernel& lua, int initial_size)
	: L(lua.get_state())
	, lua_(lua)
	, data_ptr_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
	, read_only_(false)
{
	if (initial_size != nposm) {
		VALIDATE(initial_size >= min_writeable_size, null_str);
	} else {
		initial_size = min_writeable_size;
	}
	resize_data(initial_size, 0);
}

vdata::~vdata()
{
	// SDL_Log("[lua.gc]---vdata::~vdata()---");
	if (!read_only_) {
		free(data_ptr_);
	}
}

int vdata::validate_and_ajdust(int offset, int size) const
{
	VALIDATE(offset >= 0 && offset < data_vsize_, null_str);
	if (size != nposm) {
		VALIDATE(size > 0 && offset + size <= data_vsize_, null_str);
	} else {
		size = data_vsize_ - offset;
	}
	return size;
}

void vdata::set_data_vsize(int vsize)
{
	VALIDATE(vsize >= 0 && vsize < data_size_, null_str);
	data_vsize_ = vsize;
}

void vdata::read_json(int offset, int size) const
{
	size = validate_and_ajdust(offset, size);

	tstack_size_lock lock(L, 1);
	std::stringstream err;
	try {
		Json::Reader reader;
		
		vjson* json = new(L) vjson(instance->lua());
		if (!reader.parse((const char*)data_ptr_ + offset, (const char*)data_ptr_ + offset + size, json->value())) {
			err << "Invalid json request";
			lua_pop(L, 1);
		} else {
			luaW_pushvjson_bh(L);
			return;
		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	// as if fail, return nil, let lua know fail.
	lua_pushnil(L);
}

void vdata::read_lstring(int offset, int size) const
{
	size = validate_and_ajdust(offset, size);
	lua_pushlstring(L, (const char*)data_ptr_ + offset, size);
}

void vdata::read_vdata(int offset, int size, bool ro) const
{
	size = validate_and_ajdust(offset, size);
	if (ro) {
		luaW_pushvrodata(L, data_ptr_ + offset, size);
	} else {
		vdata* sub = luaW_pushvrwdata(L, nposm);
		sub->write_lstring((const char*)data_ptr_ + offset, size);
	}
}

void vdata::resize_data(int size, int vsize)
{
	VALIDATE(!read_only_, null_str);

	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (data_ptr_) {
			if (vsize) {
				memcpy(tmp, data_ptr_, vsize);
			}
			free(data_ptr_);
		}
		data_ptr_ = tmp;
		data_size_ = size;
	}
}

int vdata::write_lstring(const char* data, int size)
{
	VALIDATE(size > 0, null_str);
	resize_data(data_vsize_ + size, data_vsize_);
	memcpy(data_ptr_ + data_vsize_, data, size);
	data_vsize_ += size;
	return size;
}

int vdata::write_json(const config& cfg)
{
	Json::Value json;
	cfg.to_json(json);

	Json::FastWriter writer;
	const std::string str = writer.write(json);
	return write_lstring(str.c_str(), str.size());
}

#define is_power_2(x)	((x > 0) && (0 == (x & (x - 1))))

int vdata::write_align(int align, int val)
{
	VALIDATE(align > 0, null_str);
	int should_vsize;
	if (is_power_2(align)) {
		should_vsize = posix_align_ceil(data_vsize_, align);
	} else {
		should_vsize = posix_align_ceil2(data_vsize_, align);
	}

	int expend = should_vsize - data_vsize_;
	if (expend != 0) {
		VALIDATE(expend > 0, null_str);
		resize_data(should_vsize, data_vsize_);
		memset(data_ptr_ + data_vsize_, val, expend);
		data_vsize_ += expend;
	}
	return expend;
}

int vdata::write_nbyte(int n, int val)
{
	VALIDATE(n > 0, null_str);

	resize_data(data_vsize_ + n, data_vsize_);
	memset(data_ptr_ + data_vsize_, val, n);
	data_vsize_ += n;
	return n;
}

int vdata::to_file(int offset, int size, int type, const std::string& path) const
{
	size = validate_and_ajdust(offset, size);
	if (type == PATHTYPE_ABS) {
		VALIDATE(game_config::os == os_windows, null_str);
		write_file(path, (const char*)data_ptr_ + offset, size);

	} else {
		VALIDATE(false, null_str);
	}
	return size;
}