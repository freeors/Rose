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

#include "scripts/vSDL.hpp"

#include <lua/lauxlib.h>
#include <lua/lua.h>

#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"
#include "scripts/vconfig.hpp"

#include "base_instance.hpp"

using namespace std::placeholders;


static const char SDLKey[] = "SDL";

static int impl_SDL_IsFile(lua_State *L)
{
	const char* file = luaL_checkstring(L, 1);
	SDL_bool ret = SDL_IsFile(file);
	lua_pushboolean(L, ret);
	return 1;
}

static int impl_SDL_IsDirectory(lua_State *L)
{
	const char* file = luaL_checkstring(L, 1);
	SDL_bool ret = SDL_IsDirectory(file);
	lua_pushboolean(L, ret);
	return 1;
}

void register_SDL_metatable(lua_State *L)
{
	tstack_size_lock lock(L, 0);

	lua_getglobal(L, SDLKey);
	if (!lua_istable(L, -1)) {
		// if SDLKey table isn't existed, create it.
		lua_pop(L, 1);
		lua_newtable(L);
		lua_setglobal(L, SDLKey);

		lua_getglobal(L, SDLKey);
	}

	static luaL_Reg const callbacks[] {
		{ "IsFile", 		    &impl_SDL_IsFile},
		{ "IsDirectory", 	    &impl_SDL_IsDirectory},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	// lua_pushstring(L, SDLKey);
	// lua_setfield(L, -2, "__metatable");

	lua_pop(L, 1);
}

