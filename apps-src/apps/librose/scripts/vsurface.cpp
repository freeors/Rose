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

#include "scripts/vsurface.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"

#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"

const char vsurface::metatableKey[] = "vsurface";

/**
 * Destroys a vsurface object before it is collected (__gc metamethod).
 */
static int impl_vsurface_collect(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(luaL_checkudata(L, 1, vsurface::metatableKey));
	v->~vsurface();
	return 0;
}

static int impl_vsurface_get(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vsurface_reset(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(lua_touserdata(L, 1));

	int type = lua_type(L, 2);
	if (type == LUA_TNIL || type == LUA_TNONE) {
		v->reset(nullptr);
	} else if (type == LUA_TSTRING) {
		surface surf(image::get_image(luaL_checkstring(L, 2)));
		v->reset(&surf);
	} else {
		VALIDATE(false, null_str);
	}
	return 0;
}

static int impl_vsurface_valid(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(lua_touserdata(L, 1));

	lua_pushboolean(L, v->valid());
	return 1;
}

static int impl_vsurface_size(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(lua_touserdata(L, 1));

	SDL_Surface* surf = v->get();
	VALIDATE(surf != nullptr, null_str);
	lua_pushinteger(L, surf->w);
	lua_pushinteger(L, surf->h);
	return 2;
}

static int impl_vsurface_imwrite(lua_State *L)
{
	vsurface *v = static_cast<vsurface *>(lua_touserdata(L, 1));

	const char* path = luaL_checkstring(L, 2);
	v->imwrite(path);
	return 0;
}

void luaW_pushvsurface(lua_State* L, const std::string& image)
{
	tstack_size_lock lock(L, 1);
	new(L) vsurface(instance->lua(), image);
	if (luaL_newmetatable(L, vsurface::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vsurface_collect},
			{"__index", impl_vsurface_get},
			{"reset", impl_vsurface_reset},
			{"valid", impl_vsurface_valid},
			{"size", impl_vsurface_size},
			{"imwrite", impl_vsurface_imwrite},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vsurface::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vsurface::vsurface(rose_lua_kernel& lua)
	: L(lua.get_state())
	, lua_(lua)
{
	// use this.tmp_surf_
	VALIDATE(!gui2::twidget::disable_lua_unlimited_mem, null_str);
}

vsurface::vsurface(rose_lua_kernel& lua, const std::string& image)
	: vsurface(lua)
{
	if (!image.empty()) {
		surf_ = image::get_image(image);
	}
}

vsurface::~vsurface()
{
	// SDL_Log("[lua.gc]---vsurface::~vsurface()---");
}

void vsurface::reset(surface* surf)
{
	if (surf != nullptr) {
		surf_ = *surf;
	} else {
		surf_ = nullptr;
	}
}

bool vsurface::valid() const
{
	return surf_.get() != nullptr;
}

void vsurface::imwrite(const std::string& path)
{
	::imwrite(surf_, path);
}

//
// ---vtexture
//
const char vtexture::metatableKey[] = "vtexture";

/**
 * Destroys a vtexture object before it is collected (__gc metamethod).
 */
static int impl_vtexture_collect(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(luaL_checkudata(L, 1, vtexture::metatableKey));
	v->~vtexture();
	return 0;
}

static int impl_vtexture_get(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vtexture_reset(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(lua_touserdata(L, 1));

	int type = lua_type(L, 2);
	if (type == LUA_TNIL || type == LUA_TNONE) {
		v->reset(nullptr);
	} else if (type == LUA_TUSERDATA) {
		VALIDATE(false, null_str);

		vtexture* v2 = static_cast<vtexture*>(lua_touserdata(L, 2));
		v->reset(&v2->get());
	} else {
		VALIDATE(false, null_str);
	}

	return 0;
}

static int impl_vtexture_set_surf(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(lua_touserdata(L, 1));

	surface surf;
	int type = lua_type(L, 2);
	if (type == LUA_TSTRING) {
		surf = image::get_image(luaL_checkstring(L, 2));
	} else if (type == LUA_TUSERDATA) {
		vsurface *vsurf = static_cast<vsurface *>(lua_touserdata(L, 2));
		surf = vsurf->get();
	} else {
		VALIDATE(false, null_str);
	}

	v->set_surf(surf);
	return 0;
}

static int impl_vtexture_valid(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(lua_touserdata(L, 1));

	lua_pushboolean(L, v->valid());
	return 1;
}

static int impl_vtexture_query(lua_State *L)
{
	vtexture *v = static_cast<vtexture *>(lua_touserdata(L, 1));

	SDL_Texture* tex = v->get().get();
	VALIDATE(tex != nullptr, null_str);

	int width, height;
	SDL_QueryTexture(tex, NULL, NULL, &width, &height);
	lua_pushinteger(L, width);
	lua_pushinteger(L, height);
	return 2;
}

static void luaW_pushvtexture_bh(lua_State* L)
{
	if (luaL_newmetatable(L, vtexture::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vtexture_collect},
			{"__index", impl_vtexture_get},
			{"reset", impl_vtexture_reset},
			{"set_surf", impl_vtexture_set_surf},
			{"valid", impl_vtexture_valid},
			{"query", impl_vtexture_query},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vtexture::metatableKey);
	}
	lua_setmetatable(L, -2);
}

void luaW_pushvtexture(lua_State* L, surface& surf)
{
	tstack_size_lock lock(L, 1);
	new(L) vtexture(instance->lua(), surf);
	luaW_pushvtexture_bh(L);
}

void luaW_pushvtexture(lua_State* L)
{
	tstack_size_lock lock(L, 1);
	new(L) vtexture(instance->lua());
	luaW_pushvtexture_bh(L);
}

vtexture::vtexture(rose_lua_kernel& lua)
	: L(lua.get_state())
	, lua_(lua)
{
	// use this.tmp_tex_
	VALIDATE(!gui2::twidget::disable_lua_unlimited_mem, null_str);
}

vtexture::vtexture(rose_lua_kernel& lua, const surface& surf)
	: vtexture(lua)
{
	set_surf(surf);
}

vtexture::~vtexture()
{
	// SDL_Log("[lua.gc]---vtexture::~vtexture()---");
}

void vtexture::reset(texture* tex)
{
	if (tex != nullptr) {
		tex_ = *tex;
	} else {
		tex_ = nullptr;
	}
}

bool vtexture::valid() const
{
	return tex_.get() != nullptr;
}

void vtexture::set_surf(const surface& surf)
{
	if (surf.get() != nullptr) {
		tex_ = SDL_CreateTextureFromSurface(get_renderer(), surf.get());
	}
}