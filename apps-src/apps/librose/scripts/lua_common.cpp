/*
   Copyright (C) 2014 - 2018 by Chris Beck <render787@gmail.com>
   

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
 * Contains code common to the application and game lua kernels which
 * cannot or should not go into the lua kernel base files.
 *
 * Currently contains implementation functions related to vconfig and
 * gettext, also some macros to assist in writing C lua callbacks.
 */

#include "scripts/lua_common.hpp"

#include "config.hpp"
#include "tstring.hpp"                  // for t_string
#include "scripts/vconfig.hpp"
#include "gettext.hpp"
#include "lua_jailbreak_exception.hpp"

#include <cstring>
#include <iterator>                     // for distance, advance
#include <new>                          // for operator new
#include <string>                       // for string, basic_string

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include <SDL_log.h>

#include "config_cache.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>


static const char gettextKey[] = "gettext";
static const char vconfigKey[] = "vconfig";
static const char vconfigpairsKey[] = "vconfig pairs";
static const char vconfigipairsKey[] = "vconfig ipairs";
static const char tstringKey[] = "translatable string";
static const char executeKey[] = "err";

namespace lua_common {

/**
 * Creates a t_string object (__call metamethod).
 * - Arg 1: userdata containing the domain.
 * - Arg 2: string to translate.
 * - Ret 1: string containing the translatable string.
 */
static int impl_gettext(lua_State *L)
{
	char const *m = luaL_checkstring(L, 2);
	char const *d = static_cast<char *>(lua_touserdata(L, 1));
	// Hidden metamethod, so d has to be a string. Use it to create a t_string.
	if(lua_isstring(L, 3)) {
		const char* pl = luaL_checkstring(L, 3);
		int count = luaL_checkinteger(L, 4);
		luaW_pushtstring(L, t_string(m, pl, count, d));
	} else {
		luaW_pushtstring(L, t_string(m, d));
	}
	return 1;
}

/**
 * Creates an interface for gettext
 * - Arg 1: string containing the domain.
 * - Ret 1: a full userdata with __call pointing to lua_gettext.
 */
int intf_textdomain(lua_State *L)
{
	size_t l;
	char const *m = luaL_checklstring(L, 1, &l);

	void *p = lua_newuserdata(L, l + 1);
	memcpy(p, m, l + 1);

	luaL_setmetatable(L, gettextKey);
	return 1;
}

/**
 * Converts a Lua value at position @a src and appends it to @a dst.
 * @note This function is private to lua_tstring_concat. It expects two things.
 *       First, the t_string metatable is at the top of the stack on entry. (It
 *       is still there on exit.) Second, the caller hasn't any valuable object
 *       with dynamic lifetime, since they would be leaked on error.
 */
static void tstring_concat_aux(lua_State *L, t_string &dst, int src)
{
	switch (lua_type(L, src)) {
		case LUA_TNUMBER:
		case LUA_TSTRING:
			dst += lua_tostring(L, src);
			return;
		case LUA_TUSERDATA:
			// Compare its metatable with t_string's metatable.
			if (t_string * src_ptr = static_cast<t_string *> (luaL_testudata(L, src, tstringKey))) {
				dst += *src_ptr;
				return;
			}
			//intentional fall-through
		default:
			luaW_type_error(L, src, "string");
	}
}

/**
 * Appends a scalar to a t_string object (__concat metamethod).
 */
static int impl_tstring_concat(lua_State *L)
{
	// Create a new t_string.
	t_string *t = new(L) t_string;
	luaL_setmetatable(L, tstringKey);

	// Append both arguments to t.
	tstring_concat_aux(L, *t, 1);
	tstring_concat_aux(L, *t, 2);

	return 1;
}

static int impl_tstring_len(lua_State* L)
{
	t_string* t = static_cast<t_string*>(lua_touserdata(L, 1));
	lua_pushnumber(L, t->size());
	return 1;
}

/**
 * Destroys a t_string object before it is collected (__gc metamethod).
 */
static int impl_tstring_collect(lua_State *L)
{
	t_string *t = static_cast<t_string *>(lua_touserdata(L, 1));
	t->t_string::~t_string();
	return 0;
}

static int impl_tstring_lt(lua_State *L)
{
	t_string *t1 = static_cast<t_string *>(luaL_checkudata(L, 1, tstringKey));
	t_string *t2 = static_cast<t_string *>(luaL_checkudata(L, 2, tstringKey));
	bool val = t1->get() < t2->get();
	lua_pushboolean(L, val);
	return 1;
}

static int impl_tstring_le(lua_State *L)
{
	t_string *t1 = static_cast<t_string *>(luaL_checkudata(L, 1, tstringKey));
	t_string *t2 = static_cast<t_string *>(luaL_checkudata(L, 2, tstringKey));
	bool val = t1->get() < t2->get();
	if (!val) {
		val = t1->get() == t2->get();
	}
	lua_pushboolean(L, val);
	return 1;
}

static int impl_tstring_eq(lua_State *L)
{
	t_string *t1 = static_cast<t_string *>(lua_touserdata(L, 1));
	t_string *t2 = static_cast<t_string *>(lua_touserdata(L, 2));
	bool val = t1->get() == t2->get();
	lua_pushboolean(L, val);
	return 1;
}

/**
 * Converts a t_string object to a string (__tostring metamethod);
 * that is, performs a translation.
 */
static int impl_tstring_tostring(lua_State *L)
{
	t_string *t = static_cast<t_string *>(lua_touserdata(L, 1));
	lua_pushstring(L, t->c_str());
	return 1;
}

static int impl_vconfig_empty(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));
	lua_pushboolean(L, v->get_config().empty());
	return 1;
}

/**
 * Gets the parsed field of a vconfig object (_index metamethod).
 * Special fields __literal, __shallow_literal, __parsed, and
 * __shallow_parsed, return Lua tables.
 */
static int impl_vconfig_get(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));

	if (lua_isnumber(L, 2))
	{
		vconfig::all_children_iterator i = v->ordered_begin();
		unsigned len = std::distance(i, v->ordered_end());
		unsigned pos = lua_tointeger(L, 2) - 1;
		if (pos >= len) return 0;
		std::advance(i, pos);

		lua_createtable(L, 2, 0);
		lua_pushstring(L, i.get_key().c_str());
		lua_rawseti(L, -2, 1);
		luaW_pushvconfig(L, i.get_child());
		lua_rawseti(L, -2, 2);
		return 1;
	}

	char const *m = luaL_checkstring(L, 2);
	int msize = strlen(m);
	if (msize > 2 && m[0] == '_' && m[1] == '_') {
		if (strcmp(m, "__literal") == 0) {
			luaW_pushconfig(L, v->get_config());
			return 1;
		} 
		if (strcmp(m, "__parsed") == 0) {
			luaW_pushconfig(L, v->get_parsed_config());
			return 1;
		}
		if (strcmp(m, "__empty") == 0) {
			lua_pushboolean(L, v->get_config().empty());
			return 1;
		}

		bool shallow_literal = strcmp(m, "__shallow_literal") == 0;
		if (shallow_literal || strcmp(m, "__shallow_parsed") == 0)
		{
			lua_newtable(L);
			for (const config::attribute &a : v->get_config().attribute_range()) {
				if (shallow_literal)
					luaW_pushscalar(L, a.second);
				else
					luaW_pushscalar(L, v->expand(a.first));
				lua_setfield(L, -2, a.first.c_str());
			}
			vconfig::all_children_iterator i = v->ordered_begin(),
				i_end = v->ordered_end();
			if (shallow_literal) {
				i.disable_insertion();
				i_end.disable_insertion();
			}
			for (int j = 1; i != i_end; ++i, ++j)
			{
				lua_createtable(L, 2, 0);
				lua_pushstring(L, i.get_key().c_str());
				lua_rawseti(L, -2, 1);
				luaW_pushvconfig(L, i.get_child());
				lua_rawseti(L, -2, 2);
				lua_rawseti(L, -2, j);
			}
			return 1;
		}
	}

	if (v->null() || !v->has_attribute(m)) return 0;
	luaW_pushscalar(L, (*v)[m]);
	return 1;
}

/**
 * Returns the number of a child of a vconfig object.
 */
static int impl_vconfig_size(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));
	lua_pushinteger(L, v->null() ? 0 :
		std::distance(v->ordered_begin(), v->ordered_end()));
	return 1;
}

/**
 * Destroys a vconfig object before it is collected (__gc metamethod).
 */
static int impl_vconfig_collect(lua_State *L)
{
	vconfig *v = static_cast<vconfig *>(lua_touserdata(L, 1));
	v->~vconfig();
	return 0;
}

/**
 * Iterate through the attributes of a vconfig
 */
static int impl_vconfig_pairs_iter(lua_State *L)
{
	vconfig vcfg = luaW_checkvconfig(L, 1);
	void* p = luaL_checkudata(L, lua_upvalueindex(1), vconfigpairsKey);
	config::const_attr_itors& range = *static_cast<config::const_attr_itors*>(p);
	if (range.empty()) {
		return 0;
	}
	config::attribute value = range.front();
	range.pop_front();
	lua_pushlstring(L, value.first.c_str(), value.first.length());
	luaW_pushscalar(L, vcfg[value.first]);

	return 2;
}

/**
 * Destroy a vconfig pairs iterator
 */
static int impl_vconfig_pairs_collect(lua_State *L)
{
	typedef config::const_attr_itors const_attr_itors;
	void* p = lua_touserdata(L, 1);
	const_attr_itors* cai = static_cast<const_attr_itors*>(p);
	cai->~const_attr_itors();
	return 0;
}

/**
 * Construct an iterator to iterate through the attributes of a vconfig
 */
static int impl_vconfig_pairs(lua_State *L)
{
	vconfig vcfg = luaW_checkvconfig(L, 1);
	new(L) config::const_attr_itors(vcfg.get_config().attribute_range());
	luaL_newmetatable(L, vconfigpairsKey);
	lua_setmetatable(L, -2);
	lua_pushcclosure(L, &impl_vconfig_pairs_iter, 1);
	lua_pushvalue(L, 1);
	return 2;
}

typedef std::pair<vconfig::all_children_iterator, vconfig::all_children_iterator> vconfig_child_range;

/**
 * Iterate through the subtags of a vconfig
 */
static int impl_vconfig_ipairs_iter(lua_State *L)
{
	luaW_checkvconfig(L, 1);
	int i = luaL_checkinteger(L, 2);
	void* p = luaL_checkudata(L, lua_upvalueindex(1), vconfigipairsKey);
	vconfig_child_range& range = *static_cast<vconfig_child_range*>(p);
	if (range.first == range.second) {
		return 0;
	}
	std::pair<std::string, vconfig> value = *range.first++;
	lua_pushinteger(L, i + 1);
	lua_createtable(L, 2, 0);
	lua_pushlstring(L, value.first.c_str(), value.first.length());
	lua_rawseti(L, -2, 1);
	luaW_pushvconfig(L, value.second);
	lua_rawseti(L, -2, 2);
	return 2;
}

/**
 * Destroy a vconfig ipairs iterator
 */
static int impl_vconfig_ipairs_collect(lua_State *L)
{
	void* p = lua_touserdata(L, 1);
	vconfig_child_range* vcr = static_cast<vconfig_child_range*>(p);
	vcr->~vconfig_child_range();
	return 0;
}

/**
 * Construct an iterator to iterate through the subtags of a vconfig
 */
static int impl_vconfig_ipairs(lua_State *L)
{
	vconfig cfg = luaW_checkvconfig(L, 1);
	new(L) vconfig_child_range(cfg.ordered_begin(), cfg.ordered_end());
	luaL_newmetatable(L, vconfigipairsKey);
	lua_setmetatable(L, -2);
	lua_pushcclosure(L, &impl_vconfig_ipairs_iter, 1);
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 0);
	return 3;
}

/**
 * Creates a vconfig containing the WML table.
 * - Arg 1: WML table.
 * - Ret 1: vconfig userdata.
 */
int intf_tovconfig(lua_State *L)
{
	if (lua_type(L, 1) == LUA_TSTRING) {
		const std::string path = luaL_checkstring(L, 1);

		config cfg;
		config_cache& cache = config_cache::instance();
		cache.get_config(path, cfg);

		vconfig vcfg(cfg, true);
		luaW_pushvconfig(L, vcfg);

	} else {
		vconfig vcfg = luaW_checkvconfig(L, 1);
		luaW_pushvconfig(L, vcfg);
	}
	return 1;
}

/**
 * Adds the gettext metatable
 */
std::string register_gettext_metatable(lua_State *L)
{
	luaL_newmetatable(L, gettextKey);

	static luaL_Reg const callbacks[] {
		{ "__call", 	    &impl_gettext},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	lua_pushstring(L, "message domain");
	lua_setfield(L, -2, "__metatable");

	return "Adding gettext metatable...\n";
}

/**
 * Adds the tstring metatable
 */
std::string register_tstring_metatable(lua_State *L)
{
	luaL_newmetatable(L, tstringKey);

	static luaL_Reg const callbacks[] {
		{ "__concat", 	    &impl_tstring_concat},
		{ "__gc",           &impl_tstring_collect},
		{ "__tostring",	    &impl_tstring_tostring},
		{ "__len",          &impl_tstring_len},
		{ "__lt",	        &impl_tstring_lt},
		{ "__le",	        &impl_tstring_le},
		{ "__eq",	        &impl_tstring_eq},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	lua_pushstring(L, "translatable string");
	lua_setfield(L, -2, "__metatable");

	return "Adding tstring metatable...\n";
}

/**
 * Adds the vconfig metatable
 */
std::string register_vconfig_metatable(lua_State *L)
{
	luaL_newmetatable(L, vconfigKey);

	static luaL_Reg const callbacks[] {
		{ "__gc",           &impl_vconfig_collect},
		{ "__index",        &impl_vconfig_get},
		{ "__len",          &impl_vconfig_size},
		{ "__pairs",        &impl_vconfig_pairs},
		{ "__ipairs",       &impl_vconfig_ipairs},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	lua_pushstring(L, "wml object");
	lua_setfield(L, -2, "__metatable");

	// Metatables for the iterator userdata

	// I don't bother setting __metatable because this
	// userdata is only ever stored in the iterator's
	// upvalues, so it's never visible to the user.
	luaL_newmetatable(L, vconfigpairsKey);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, &impl_vconfig_pairs_collect);
	lua_rawset(L, -3);

	luaL_newmetatable(L, vconfigipairsKey);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, &impl_vconfig_ipairs_collect);
	lua_rawset(L, -3);

	lua_pop(L, 3); // vconfigKey, vconfigpairsKey and vconfigipairsKey

	return "Adding vconfig metatable...\n";
}

} // end namespace lua_common

void* operator new(size_t sz, lua_State *L)
{
	return lua_newuserdata(L, sz);
}

void operator delete(void*, lua_State *L)
{
	// Not sure if this is needed since it's a no-op
	// It's only called if a constructor throws while using the above operator new
	// By removing the userdata from the stack, this should ensure that Lua frees it
	lua_pop(L, 1);
}

bool luaW_getmetafield(lua_State *L, int idx, const char* key)
{
	if(key == nullptr) {
		return false;
	}
	int n = strlen(key);
	if(n == 0) {
		return false;
	}
	if(n >= 2 && key[0] == '_' && key[1] == '_') {
		return false;
	}
	return luaL_getmetafield(L, idx, key) != 0;
}

void luaW_pushvconfig(lua_State *L, const vconfig& cfg)
{
	new(L) vconfig(cfg);
	luaL_setmetatable(L, vconfigKey);
}

void luaW_pushtstring(lua_State *L, const t_string& v)
{
	new(L) t_string(v);
	luaL_setmetatable(L, tstringKey);
}

void luaW_pushscalar(lua_State *L, const config::attribute_value& v)
{
	int type = v.type();
	if (type == config::attribute_value::type_bool) {
		lua_pushboolean(L, v.to_bool());
	} else if (type == config::attribute_value::type_integer) {
		lua_pushinteger(L, v.to_long_long());
	} else if (type == config::attribute_value::type_double) {
		lua_pushnumber(L, v.to_double());
	} else if (type == config::attribute_value::type_string) {
		lua_pushstring(L, v.str().c_str());
	} else if (type == config::attribute_value::type_tstring) {
		luaW_pushtstring(L, v.t_str());
	} else {
		VALIDATE(type == config::attribute_value::type_blank, null_str);
		lua_pushnil(L);
	}
}

bool luaW_toscalar(lua_State *L, int index, config::attribute_value& v)
{
	switch (lua_type(L, index)) {
		case LUA_TBOOLEAN:
			v = luaW_toboolean(L, -1);
			break;
		case LUA_TNUMBER:
			v = lua_tonumber(L, -1);
			break;
		case LUA_TSTRING:
			v.from_string(lua_tostring(L, -1), true);
			break;
		case LUA_TUSERDATA:
		{
			if (t_string * tptr = static_cast<t_string *>(luaL_testudata(L, -1, tstringKey))) {
				v = *tptr;
				break;
			} else {
				return false;
			}
		}
		default:
			return false;
	}
	return true;
}

bool luaW_totstring(lua_State *L, int index, t_string &str)
{
	switch (lua_type(L, index)) {
		case LUA_TBOOLEAN:
			str = lua_toboolean(L, index) ? "yes" : "no";
			break;
		case LUA_TNUMBER:
		case LUA_TSTRING:
			str = lua_tostring(L, index);
			break;
		case LUA_TUSERDATA:
		{
			if (t_string * tstr = static_cast<t_string *> (luaL_testudata(L, index, tstringKey))) {
				str = *tstr;
				break;
			} else {
				return false;
			}
		}
		default:
			return false;
	}
	return true;
}

t_string luaW_checktstring(lua_State *L, int index)
{
	t_string result;
	if (!luaW_totstring(L, index, result))
		luaW_type_error(L, index, "translatable string");
	return result;
}

bool luaW_iststring(lua_State* L, int index)
{
	if(lua_isstring(L, index)) {
		return true;
	}
	if(lua_isuserdata(L, index) && luaL_testudata(L, index, tstringKey)) {
		return true;
	}
	return false;
}

void luaW_filltable(lua_State *L, const config& cfg)
{
	if (!lua_checkstack(L, LUA_MINSTACK))
		return;

	int k = 1;
	BOOST_FOREACH (const config::any_child &ch, cfg.all_children_range())
	{
		lua_createtable(L, 2, 0);
		lua_pushstring(L, ch.key.c_str());
		lua_rawseti(L, -2, 1);
		lua_newtable(L);
		luaW_filltable(L, ch.cfg);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, k++);
	}
	for (const config::attribute &attr : cfg.attribute_range())
	{
		luaW_pushscalar(L, attr.second);
		lua_setfield(L, -2, attr.first.c_str());
	}
}
/*
void luaW_pushlocation(lua_State *L, const map_location& ml)
{
	lua_createtable(L, 2, 0);

	lua_pushinteger(L, ml.wml_x());
	lua_rawseti(L, -2, 1);

	lua_pushinteger(L, ml.wml_y());
	lua_rawseti(L, -2, 2);
}

bool luaW_tolocation(lua_State *L, int index, map_location& loc) {
	if (!lua_checkstack(L, LUA_MINSTACK)) {
		return false;
	}
	if (lua_isnoneornil(L, index)) {
		// Need this special check because luaW_tovconfig returns true in this case
		return false;
	}

	vconfig dummy_vcfg = vconfig::unconstructed_vconfig();

	index = lua_absindex(L, index);

	if (lua_istable(L, index) || luaW_tounit(L, index) || luaW_tovconfig(L, index, dummy_vcfg)) {
		map_location result;
		int x_was_num = 0, y_was_num = 0;
		lua_getfield(L, index, "x");
		result.set_wml_x(lua_tonumberx(L, -1, &x_was_num));
		lua_getfield(L, index, "y");
		result.set_wml_y(lua_tonumberx(L, -1, &y_was_num));
		lua_pop(L, 2);
		if (!x_was_num || !y_was_num) {
			// If we get here and it was userdata, checking numeric indices won't help
			// (It won't help if it was a config either, but there's no easy way to check that.)
			if (lua_isuserdata(L, index)) {
				return false;
			}
			lua_rawgeti(L, index, 1);
			result.set_wml_x(lua_tonumberx(L, -1, &x_was_num));
			lua_rawgeti(L, index, 2);
			result.set_wml_y(lua_tonumberx(L, -1, &y_was_num));
			lua_pop(L, 2);
		}
		if (x_was_num && y_was_num) {
			loc = result;
			return true;
		}
	} else if (lua_isnumber(L, index) && lua_isnumber(L, index + 1)) {
		// If it's a number, then we consume two elements on the stack
		// Since we have no way of notifying the caller that we have
		// done this, we remove the first number from the stack.
		loc.set_wml_x(lua_tonumber(L, index));
		lua_remove(L, index);
		loc.set_wml_y(lua_tonumber(L, index));
		return true;
	}
	return false;
}

map_location luaW_checklocation(lua_State *L, int index)
{
	map_location result;
	if (!luaW_tolocation(L, index, result))
		luaW_type_error(L, index, "location");
	return result;
}
*/
void luaW_pushconfig(lua_State *L, const config& cfg)
{
	lua_newtable(L);
	luaW_filltable(L, cfg);
}




#define return_misformed() \
  do { lua_settop(L, initial_top); return false; } while (0)

bool luaW_toconfig(lua_State *L, int index, config &cfg)
{
	if (!lua_checkstack(L, LUA_MINSTACK))
		return false;

	// Get the absolute index of the table.
	index = lua_absindex(L, index);
	int initial_top = lua_gettop(L);

	switch (lua_type(L, index))
	{
		case LUA_TTABLE:
			break;
		case LUA_TUSERDATA:
		{
			if (vconfig * ptr = static_cast<vconfig *> (luaL_testudata(L, index, vconfigKey))) {
				cfg = ptr->get_parsed_config();
				return true;
			} else {
				return false;
			}
		}
		case LUA_TNONE:
		case LUA_TNIL:
			return true;
		default:
			return false;
	}

	// First convert the children (integer indices).
	for (int i = 1, i_end = lua_rawlen(L, index); i <= i_end; ++i)
	{
		lua_rawgeti(L, index, i);
		if (!lua_istable(L, -1)) return_misformed();
		lua_rawgeti(L, -1, 1);
		char const *m = lua_tostring(L, -1);
		if (!m || !config::valid_tag(m) || m[0] == '_') return_misformed();
		lua_rawgeti(L, -2, 2);
		if (!luaW_toconfig(L, -1, cfg.add_child(m)))
			return_misformed();
		lua_pop(L, 3);
	}

	// Then convert the attributes (string indices).
	for (lua_pushnil(L); lua_next(L, index); lua_pop(L, 1))
	{
		int indextype = lua_type(L, -2);
		if (indextype == LUA_TNUMBER) continue;
		if (indextype != LUA_TSTRING) return_misformed();
		config::attribute_value &v = cfg[lua_tostring(L, -2)];
		if (lua_istable(L, -1)) {
			int subindex = lua_absindex(L, -1);
			std::ostringstream str;
			for (int i = 1, i_end = lua_rawlen(L, subindex); i <= i_end; ++i, lua_pop(L, 1)) {
				lua_rawgeti(L, -1, i);
				config::attribute_value item;
				if (!luaW_toscalar(L, -1, item)) return_misformed();
				if (i > 1) str << ',';
				str << item;
			}
			// If there are any string keys, it's malformed
			for (lua_pushnil(L); lua_next(L, subindex); lua_pop(L, 1)) {
				if (lua_type(L, -2) != LUA_TNUMBER) return_misformed();
			}
			v = str.str();
		} else if (!luaW_toscalar(L, -1, v)) return_misformed();
	}

	lua_settop(L, initial_top);
	return true;
}

#undef return_misformed

config luaW_checkconfig(lua_State *L, int index)
{
	config result;
	if (!luaW_toconfig(L, index, result))
		luaW_type_error(L, index, "WML table");
	return result;
}

config luaW_checkconfig(lua_State *L, int index, const vconfig*& vcfg)
{
	config result = luaW_checkconfig(L, index);
	if(void* p = luaL_testudata(L, index, vconfigKey)) {
		vcfg = static_cast<vconfig*>(p);
	}
	return result;
}

bool luaW_tovconfig(lua_State *L, int index, vconfig &vcfg)
{
	switch (lua_type(L, index))
	{
		case LUA_TTABLE:
		{
			config cfg;
			bool ok = luaW_toconfig(L, index, cfg);
			if (!ok) return false;
			vcfg = vconfig(std::move(cfg));
			break;
		}
		case LUA_TUSERDATA:
			if (vconfig * ptr = static_cast<vconfig *> (luaL_testudata(L, index, vconfigKey))) {
				vcfg = *ptr;
			} else {
				return false;
			}
		case LUA_TNONE:
		case LUA_TNIL:
			break;
		default:
			return false;
	}
	return true;
}

vconfig luaW_checkvconfig(lua_State *L, int index, bool allow_missing)
{
	vconfig result = vconfig::unconstructed_vconfig();
	if (!luaW_tovconfig(L, index, result) || (!allow_missing && result.null()))
		luaW_type_error(L, index, "WML table");
	return result;
}

bool luaW_getglobal(lua_State *L, const std::vector<std::string>& path)
{
	lua_pushglobaltable(L);
	for (const std::string& s : path)
	{
		if (!lua_istable(L, -1)) goto discard;
		lua_pushlstring(L, s.c_str(), s.size());
		lua_rawget(L, -2);
		// 
		lua_remove(L, -2);
	}

	if (lua_isnil(L, -1)) {
		VALIDATE(false , null_str);
	discard:
		lua_pop(L, 1);
		return false;
	}

	return true;
}

static bool luaW_getglobal_internal(lua_State *L, const char** paths, int size)
{
	lua_pushglobaltable(L);
	for (int n = 0; n < size; n ++) {
		const char* path = paths[n];
		// stack#-1 isn't path, is before stack top value.
		if (!lua_istable(L, -1)) {
			goto discard;
		}
		lua_pushlstring(L, path, strlen(path));
		lua_rawget(L, -2);
		// // stack#-2 is last table. since it no use, pop it.
		lua_remove(L, -2);
	}

	if (lua_isnil(L, -1)) {
		VALIDATE(false , null_str);
discard:
		lua_pop(L, 1);
		return false;
	}

	return true;
}

void luaW_getglobal_2joined(lua_State* L, const char* path)
{
	const char* ptr = strchr(path, '.');
	VALIDATE(ptr != nullptr, null_str);

	int len = strlen(path);

	char* buf = (char*)malloc(len + 1);
	memset(buf, 0, len + 1);
	memcpy(buf, path, len);
	buf[ptr - path] = 0;
	const char* paths[2] = {buf, buf + (ptr - path) + 1};
	luaW_getglobal_internal(L, paths, 2);
	free(buf);
}

static bool luaW_checkglobal_internal(lua_State* L, const char** paths, int size)
{
	lua_pushglobaltable(L);
	bool ret = true;
	for (int n = 0; n < size; n ++) {
		const char* path = paths[n];
		// stack#-1 isn't path, is before stack top value.
		if (!lua_istable(L, -1)) {
			ret = false;
			break;
		}
		lua_pushlstring(L, path, strlen(path));
		lua_rawget(L, -2);
		// stack#-2 is last table. since it no use, pop it.
		lua_remove(L, -2);
	}

	if (ret && lua_isnil(L, -1)) {
		ret = false;
	}
	lua_pop(L, 1);
	return ret;
}

bool luaW_checkglobal2(lua_State* L, const char* path1, const char* path2)
{
	const char* paths[2] = {path1, path2};
	return luaW_checkglobal_internal(L, paths, 2);
}

void chat_message(const std::string& caption, const std::string& msg)
{
	return;
/*
	if (!game_display::get_singleton()) return;
	game_display::get_singleton()->get_chat_manager().add_chat_message(time(nullptr), caption, 0, msg,
														   events::chat_handler::MESSAGE_PUBLIC, false);
*/
}

void push_error_handler(lua_State *L)
{
	luaW_getglobal(L, "debug", "traceback");
	lua_setfield(L, LUA_REGISTRYINDEX, executeKey);
}

int luaW_pcall_internal(lua_State *L, int nArgs, int nRets)
{
	// Load the error handler before the function and its arguments.
	lua_getfield(L, LUA_REGISTRYINDEX, executeKey);
	lua_insert(L, -2 - nArgs);

	int error_handler_index = lua_gettop(L) - nArgs - 1;

	// Call the function.
	int errcode = lua_pcall(L, nArgs, nRets, -2 - nArgs);

	lua_jailbreak_exception::rethrow();

	// Remove the error handler.
	lua_remove(L, error_handler_index);

	return errcode;
}

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4706)
#endif
bool luaW_pcall(lua_State *L, int nArgs, int nRets, bool allow_wml_error)
{
	int res = luaW_pcall_internal(L, nArgs, nRets);

	if (res)
	{
		/*
		 * When an exception is thrown which doesn't derive from
		 * std::exception m will be nullptr pointer.
		 */
		char const *m = lua_tostring(L, -1);
		if(m) {
			if (allow_wml_error && strncmp(m, "~wml:", 5) == 0) {
				m += 5;
				char const *e = strstr(m, "stack traceback");
				// SDL_Log(std::string(m, e ? e - m : strlen(m)).c_str());
			} else if (allow_wml_error && strncmp(m, "~lua:", 5) == 0) {
				m += 5;
				char const *e = nullptr, *em = m;
				while (em[0] && ((em = strstr(em + 1, "stack traceback"))))
#ifdef _MSC_VER
#pragma warning (pop)
#endif
					e = em;
				chat_message("Lua error", std::string(m, e ? e - m : strlen(m)));
			} else {
				// SDL_Log(m);
				chat_message("Lua error", m);
			}
		} else {
			chat_message("Lua caught unknown exception", "");
		}
		lua_pop(L, 1);
		return false;
	}

	return true;
}

// Originally luaL_typerror, now deprecated.
// Easier to define it for Wesnoth and not have to worry about it if we update Lua.
int luaW_type_error(lua_State *L, int narg, const char *tname) {
	const char *msg = lua_pushfstring(L, "%s expected, got %s", tname, luaL_typename(L, narg));
	return luaL_argerror(L, narg, msg);
}

// An alternate version which raises an error for a key in a table.
// In this version, narg should refer to the stack index of the table rather than the stack index of the key.
// kpath should be the key name or a string such as "key[idx].key2" specifying a path to the key.
int luaW_type_error (lua_State *L, int narg, const char* kpath, const char *tname) {
	const char *msg = lua_pushfstring(L, "%s expected for '%s', got %s", tname, kpath, luaL_typename(L, narg));
	return luaL_argerror(L, narg, msg);
}
