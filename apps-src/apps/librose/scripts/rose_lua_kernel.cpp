/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Provides a Lua interpreter, to be embedded in WML.
 *
 * @note Naming conventions:
 *   - intf_ functions are exported in the wesnoth domain,
 *   - impl_ functions are hidden inside metatables,
 *   - cfun_ functions are closures,
 *   - luaW_ functions are helpers in Lua style.
 */

#include "gettext.hpp"
#include "filesystem.hpp"
#include "version.hpp"

#include "scripts/rose_lua_kernel.hpp"

#include "scripts/lua_common.hpp"

// #include <boost/range/algorithm/copy.hpp>    // boost::copy
// #include <boost/range/adaptors.hpp>     // boost::adaptors::filtered
#include <cassert>                      // for assert
#include <cstring>                      // for strcmp
#include <iterator>                     // for distance, advance
#include <map>                          // for map, map<>::value_type, etc
#include <new>                          // for operator new
#include <set>                          // for set
#include <sstream>                      // for operator<<, basic_ostream, etc
#include <utility>                      // for pair
#include <algorithm>
#include <vector>                       // for vector, etc
#include "lua/lauxlib.h"                // for luaL_checkinteger, etc
#include "lua/lua.h"                    // for lua_setfield, etc

#include "serialization/string_utils.hpp"
#include "scripts/gui/dialogs/rldialog.hpp"
#include "scripts/gui/widgets/vwidget.hpp"
#include "scripts/vSDL.hpp"
#include "scripts/vsurface.hpp"
#include "scripts/vpreferences.hpp"


#ifdef DEBUG_LUA
#include "scripts/debug_lua.hpp"
#endif

/*
static int impl_vaplt_gettext(lua_State *L)
{
	const char* textdomain = luaL_checkstring(L, 1);
	const char* msgid = luaL_checkstring(L, 2);

	lua_getglobal(L, lua_bundleid.c_str());

	lua_pushstring(L, dsgettext(textdomain, msgid));
	return 1;
}
*/

rose_lua_kernel::rose_lua_kernel()
	: lua_kernel_base()
{
	lua_State *L = mState;

	lua_settop(L, 0);
	tstack_size_lock lock(L, 0);

	// Create the vconfig metatable.
	lua_common::register_vconfig_metatable(L);

	load_core();

	load_lua("lua/std.lua");
	load_lua("lua/gui2.lua");
	gui2::register_gui2_metatable(L);
	register_SDL_metatable(L);
}

void rose_lua_kernel::preprocess_aplt_table(lua_State *L, trapplet& aplt, const version_info& version)
{
	tstack_size_lock lock(L, 0);

	lua_getglobal(L, aplt.lua_bundleid.c_str());
	VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_aplt_table");

	// Although I'd love to have lua call member function of a class directly, unfortunately, no method was found.
/*
	static luaL_Reg const callbacks[] {
		{ "gettext2", 	    impl_vaplt_gettext},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);
*/
	lua_pushstring(L, aplt.lua_bundleid.c_str());
	lua_setfield(L, -2, "TAG");

	lua_pushstring(L, (aplt.lua_bundleid + "-lib").c_str());
	lua_setfield(L, -2, "GETTEXT_DOMAIN");

	lua_pushstring(L, utils::replace_all(aplt.lua_bundleid, "_", ".").c_str());
	lua_setfield(L, -2, "bundleid");

	lua_pushstring(L, version.str(true).c_str());
	lua_setfield(L, -2, "version");

	lua_pushstring(L, version.str(false).c_str());
	lua_setfield(L, -2, "version3");

	vpreferences* vpref = luaW_pushvpreferences(L, aplt.lua_bundleid);
	lua_setfield(L, -2, "preferences");

	// preferences_def
	lua_getfield(L, -1, "preferences_def");
	int tbl_idx = lua_absindex(L, -1);
	bool changed = false;
	for (lua_pushnil(L); lua_next(L, tbl_idx); lua_pop(L, 1)) {
		// -1: value
		// -2: key
		const std::string key = luaL_checkstring(L, -2);
		if (!vpref->cfg().has_attribute(key)) {
			vpref->set_value(1, -2);
			changed = true;
		}
	}
	if (changed) {
		vpref->write_prefs();
	}

	lua_pop(L, 1); // table: preferences_def
	 
	// files
	aplt.files.clear();
	lua_getfield(L, -1, "files");

	tbl_idx = -1;
	for (int i = 1, i_end = lua_rawlen(L, tbl_idx); i <= i_end; ++i) {
		lua_rawgeti(L, tbl_idx, i);
		const char* file = luaL_checkstring(L, -1);
		VALIDATE(file[0] != '\0', null_str);

		aplt.files.push_back(file);

		// Attempt to call load_lua("lua/file.lua") here, resulting in an exception exit
		// load_lua("lua/" + file);

		lua_pop(L, 1); // 1file
	}
	lua_pop(L, 1); // table: files

	lua_pop(L, 1); // table: lua_bundleid[aplt.leagor.iaccess]
}

void rose_lua_kernel::preprocess_window_table(lua_State *L, trapplet& aplt)
{
	tstack_size_lock lock(L, 0);

	for (std::set<std::string>::const_iterator it = aplt.window_ids.begin(); it != aplt.window_ids.end(); ++ it) {
		const std::string tbl_name = utils::generate_app_prefix_id(aplt.lua_bundleid, *it);

		lua_getglobal(L, tbl_name.c_str());
		VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_aplt_table");


		luaW_pushvsurface(L, null_str);
		lua_setfield(L, -2, "tmp_surf_");

		luaW_pushvtexture(L);
		lua_setfield(L, -2, "tmp_tex_");

		lua_pop(L, 1); // table: aplt_leagor_iaccess__terminal
	}

}

void rose_lua_kernel::register_applet(const std::string& lua_bundleid, const version_info& version, const config& cfg)
{
	VALIDATE(raplts_.count(lua_bundleid) == 0, null_str);
	std::pair<std::map<std::string, trapplet>::iterator, bool> ins = raplts_.insert(std::make_pair(lua_bundleid, trapplet(lua_bundleid)));
	trapplet& aplt = ins.first->second;

	lua_State *L = mState;

	// load main.lua
	load_lua("lua/main.lua");

	// 1. fill TAG/GETTEXT_DOMAIN/version/version3 etc
	// 2. get files
	preprocess_aplt_table(L, aplt, version);

	// load other *.lua
	std::set<std::string> files_set;
	for (std::vector<std::string>::const_iterator it = aplt.files.begin(); it != aplt.files.end(); ++ it) {
		const std::string& file = *it;
		load_lua("lua/" + file);
		files_set.insert(file_main_name(file));
	}

	BOOST_FOREACH (const config &w, cfg.child_range("window")) {
		const std::string& id = w["id"].str();
		VALIDATE(files_set.count(id) != 0, null_str);
		aplt.window_ids.insert(id);
	}
	preprocess_window_table(L, aplt);
}

void rose_lua_kernel::unregister_applet(const std::string& lua_bundleid)
{
	std::map<std::string, trapplet>::iterator it = raplts_.find(lua_bundleid);
	VALIDATE(it != raplts_.end(), null_str);
	const trapplet& aplt = it->second;

	lua_State *L = mState;

	// delete this applet's table
	tstack_size_lock lock(L, 0);
	for (std::vector<std::string>::const_iterator it = aplt.files.begin(); it != aplt.files.end(); ++ it) {
		const std::string tbl_name = utils::generate_app_prefix_id(lua_bundleid, extract_file(*it));

		lua_pushnil(L);
		lua_setglobal(L, tbl_name.c_str());
	}

	lua_pushnil(L);
	lua_setglobal(L, lua_bundleid.c_str());

	raplts_.erase(it);
}

void rose_lua_kernel::post_show(const std::string& lua_bundleid, const std::string& window_id)
{
	std::map<std::string, trapplet>::const_iterator it = raplts_.find(lua_bundleid);
	VALIDATE(it != raplts_.end(), null_str);
	const trapplet& aplt = it->second;

	lua_State *L = mState;
	tstack_size_lock lock(L, 0);

	const std::string tbl_name = utils::generate_app_prefix_id(aplt.lua_bundleid, window_id);
	lua_getglobal(L, tbl_name.c_str());
	VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_aplt_table");

	// tmp_surf_.reset(nullptr);
	lua_getfield(L, -1, "tmp_surf_");
	vsurface* vsurf = static_cast<vsurface *>(lua_touserdata(L, -1));
	vsurf->reset(nullptr);
	lua_pop(L, 1);

	// tmp_tex_.reset(nullptr);
	lua_getfield(L, -1, "tmp_tex_");
	vtexture* vtex = static_cast<vtexture *>(lua_touserdata(L, -1));
	vtex->reset(nullptr);
	lua_pop(L, 1);

	lua_pop(L, 1); // table: aplt_leagor_iaccess__terminal
}