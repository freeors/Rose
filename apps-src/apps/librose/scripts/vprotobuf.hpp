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

#ifndef LIBROSE_SCRIPTS_VPROTOBUF_HPP
#define LIBROSE_SCRIPTS_VPROTOBUF_HPP

// #include "config.hpp"
#include "lua/lua.h"

#include "protobuf.hpp"
#include "lprotobuf.pb.h"

class rose_lua_kernel;

// all n32item's idx must be continuous.
#define VPROTOBUF_N32ITEM_COUNT	10
// all n64item's idx must be continuous.
#define VPROTOBUF_N64ITEM_COUNT	3
// all stritem's idx must be continuous.
#define VPROTOBUF_STRITEM_COUNT	5

// #define SET_N32ITEMX(n, n32)		item_->set_n32item##n(n32)
// #define SET_N64ITEMX(n, n64)		item_->set_n64item##n(n64)
// #define SET_STRITEMX(n, c_str, size)	item_->set_stritem##n(c_str, size)

enum {PBIDX_N32TOP0 = 0, PBIDX_N32TOP1 = 1, PBIDX_N32TOP2 = 2, PBIDX_N64TOP0 = 3, PBIDX_N64TOP1 = 4,
	PBIDX_N64TOP2 = 5, PBIDX_STRTOP0 = 6, PBIDX_STRTOP1 = 7, PBIDX_STRTOP2 = 8,
	PBIDX_N32ITEM0 = 10, PBIDX_N32ITEM1 = 11, PBIDX_N32ITEM2 = 12, PBIDX_N32ITEM3 = 13,
	PBIDX_N32ITEM4 = 14, PBIDX_N32ITEM5 = 15, PBIDX_N32ITEM6 = 16, PBIDX_N32ITEM7 = 17,
	PBIDX_N32ITEM8 = 18, PBIDX_N32ITEM9 = 19, PBIDX_N64ITEM0 = 20, PBIDX_N64ITEM1 = 21, 
	PBIDX_N64ITEM2 = 22, PBIDX_STRITEM0 = 23, PBIDX_STRITEM1 = 24, PBIDX_STRITEM2 = 25, 
	PBIDX_STRITEM3 = 26, PBIDX_STRITEM4 = 27};

class vpbitem
{
public:
	static const char metatableKey[];

	vpbitem(rose_lua_kernel& lua, pb2::titem& item, bool readonly);
	virtual ~vpbitem();

	void value(int idx) const;
	void set_value(int cuont, int first_stack_idx);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string filepath_;
	pb2::titem* item_;
	bool readonly_;
};

class vprotobuf
{
public:
	static const char metatableKey[];

	vprotobuf(rose_lua_kernel& lua, const std::string& filepath);
	virtual ~vprotobuf();

	void load_sha1pb(bool use_backup);
	void write_sha1pb(bool use_backup);
	int items_size() const;
	void item(int at, bool read_only);
	pb2::titem& add_item();
	void value(int idx) const;
	void set_value(int count_stack_idx);
	void delete_subrange(int start, int num);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string filepath_;
	pb2::titems pb_items_;
};

void luaW_pushvprotobuf(lua_State* L, const std::string& filepath);

#endif

