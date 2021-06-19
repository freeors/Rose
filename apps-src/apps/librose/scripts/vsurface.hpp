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

#ifndef LIBROSE_SCRIPTS_VSURFACE_HPP
#define LIBROSE_SCRIPTS_VSURFACE_HPP

#include "lua/lua.h"

#include "sdl_utils.hpp"

class rose_lua_kernel;

class vsurface
{
public:
	static const char metatableKey[];

	vsurface(rose_lua_kernel& lua);
	vsurface(rose_lua_kernel& lua, const std::string& image);
	virtual ~vsurface();

	void reset(surface* surf);
	bool valid() const;
	void imwrite(const std::string& path);
	surface& get() { return surf_; }

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	surface surf_;
};

class vtexture
{
public:
	static const char metatableKey[];

	vtexture(rose_lua_kernel& lua);
	vtexture(rose_lua_kernel& lua, const surface& surf);
	virtual ~vtexture();

	void set_surf(const surface& surf);
	void reset(texture* tex);
	bool valid() const;
	texture& get() { return tex_; }

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	texture tex_;
};

void luaW_pushvsurface(lua_State* L, const std::string& image);
void luaW_pushvtexture(lua_State* L);
void luaW_pushvtexture(lua_State* L, surface& surf);

#endif

