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

#ifndef LIBROSE_SCRIPTS_PREFERENCES_HPP
#define LIBROSE_SCRIPTS_PREFERENCES_HPP

#include "config.hpp"
#include "lua/lua.h"

class rose_lua_kernel;
class tfile;

class vpreferences
{
public:
	static const char metatableKey[];

	vpreferences(rose_lua_kernel& lua, const std::string& lua_bundleid);
	virtual ~vpreferences();

	const config& cfg() const { return cfg_; }

	void write_prefs();
	void value(const std::string& key) const;
	void set_value(int count, int first_key_stack_idx);

private:
	bool did_read_preferences_dat(tfile& file, int64_t fsize, bool bak);
	bool did_pre_write_preferences_dat();
	bool did_write_preferences_dat(tfile& file);
	void resize_data(int size, int vsize);
	
protected:
	const std::string signature_tag;
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string lua_bundleid_;
	const std::string prefs_file_;
	config cfg_;

	char* data_;
	int data_size_;
	int data_vsize_;
};

vpreferences* luaW_pushvpreferences(lua_State* L, const std::string& lua_bundleid);

#endif

