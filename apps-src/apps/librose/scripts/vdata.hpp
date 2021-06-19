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

#ifndef LIBROSE_SCRIPTS_VDATA_HPP
#define LIBROSE_SCRIPTS_VDATA_HPP

#include "config.hpp"
#include "util.hpp"

#include "lua/lua.h"

class rose_lua_kernel;

// vjson mananger value_ myself, so vdata can free as if app will use vjson in future.
class vjson
{
public:
	static const char metatableKey[];

	vjson(rose_lua_kernel& lua);
	vjson(rose_lua_kernel& lua, const Json::Value& value);
	virtual ~vjson();

	Json::Value& value() { return value_; }
	void clear() { value_.clear(); }
	void json_value(const char* field, int type);
	int json_array_size() const;
	void json_array_at(int at);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	Json::Value value_;
};

// vdata don't malloc/free data{data_ptr_, data_len_}.
// caller must make ensure data valid when using vdata.
class vdata
{
public:
	static const int min_writeable_size = 4096;
	static const char metatableKey[];

	vdata(rose_lua_kernel& lua, const uint8_t* data, int _size);
	vdata(rose_lua_kernel& lua, int initial_size);
	virtual ~vdata();

	uint8_t* data_ptr() { return data_ptr_; }
	int data_vsize() const { return data_vsize_; }
	void set_data_vsize(int vsize);

	tuint8cdata get_data() const { return tuint8cdata{data_ptr_, data_vsize_}; }
	int validate_and_ajdust(int offset, int size) const;

	// read
	void read_json(int offset, int size) const;
	void read_lstring(int offset, int size) const;
	void read_vdata(int offset, int size, bool ro) const;

	// write
	int write_lstring(const char* data, int size);
	int write_json(const config& cfg);
	int write_align(int align, int val);
	int write_nbyte(int n, int val);

	// save data to xxx
	int to_file(int offset, int size, int type, const std::string& path) const;

private:
	void resize_data(int size, int vsize);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	uint8_t* data_ptr_;
	int data_size_;
	int data_vsize_;
	bool read_only_;
};

vdata* luaW_pushvrwdata(lua_State* L, int initial_size);
void luaW_pushvrodata(lua_State* L, const uint8_t* data, int size);

#endif

