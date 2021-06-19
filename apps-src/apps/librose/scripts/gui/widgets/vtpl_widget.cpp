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

#include "scripts/gui/widgets/vtpl_widget.hpp"
#include "scripts/gui/widgets/vwidget.hpp"

#include <lua/lauxlib.h>
#include <lua/lua.h>

#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"

#include "base_instance.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/widgets/button.hpp"

using namespace std::placeholders;


namespace gui2 {

/**
 * Destroys a vwidget object before it is collected (__gc metamethod).
 */
static int impl_vtpl_widget_collect(lua_State *L)
{
	vtpl_widget *v = static_cast<vtpl_widget *>(lua_touserdata(L, 1));
	v->~vtpl_widget();
	return 0;
}

static int impl_vtpl_widget_get(lua_State *L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vtpl_widget_set_did_text_changed(lua_State *L)
{
	vtpl_widget *v = static_cast<vtpl_widget *>(lua_touserdata(L, 1));
	const char* m = luaL_checkstring(L, 2);
	VALIDATE(m[0] != '\0', null_str);

	const std::string method = m;
	tbase_tpl_widget& widget = v->tpl_widget();
	widget.set_did_text_changed(std::bind(&lua_did_text_changed, _1, L, v->lua_window_id(), method));
	return 0;
}

static int impl_vtpl_widget_button(lua_State *L)
{
	vtpl_widget *v = static_cast<vtpl_widget *>(lua_touserdata(L, 1));
	
	twidget* widget = v->tpl_widget().button();
	VALIDATE(widget != nullptr, null_str);

	luaW_pushvwidget(L, *widget);
	return 1;
}

static int impl_vtpl_widget_text_box(lua_State *L)
{
	vtpl_widget *v = static_cast<vtpl_widget *>(lua_touserdata(L, 1));
	
	twidget* widget = v->tpl_widget().text_box();
	VALIDATE(widget != nullptr, null_str);

	luaW_pushvwidget(L, *widget);
	return 1;
}

void luaW_pushvtpl_widget(lua_State* L, int type, twindow& window, twidget& widget, const config& cfg)
{
	tstack_size_lock lock(L, 1);
	new(L) vtpl_widget(L, type, window, widget, cfg);
	if (luaL_newmetatable(L, vtpl_widget::metatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vtpl_widget_collect},
			{"__index", impl_vtpl_widget_get},

			{"set_did_text_changed", impl_vtpl_widget_set_did_text_changed},
			{"button", impl_vtpl_widget_button},
			{"text_box", impl_vtpl_widget_text_box},

			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vtpl_widget::metatableKey);
	}
	lua_setmetatable(L, -2);
}

const char vtpl_widget::metatableKey[] = "gui2.vtpl_widget";

vtpl_widget::vtpl_widget(lua_State* _L, int type, twindow& window, twidget& widget, const config& cfg)
	: lua_(instance->lua())
	, L(_L)
	, window_(&window)
	, widget_(&widget)
	, cfg_(cfg)
	, lua_window_id_(widget.get_window()->id())
	, tpl_widget_(nullptr)
{
	if (type == tbase_tpl_widget::type_text_box2) {
		std::string panel_border = cfg_.has_attribute("panel_border")? cfg_["panel_border"].str(): null_str;
		std::string image_label = cfg_.has_attribute("image_label")? cfg_["image_label"].str(): null_str;
		bool desensitize = cfg_.has_attribute("desensitize")? cfg_["desensitize"].to_bool(): false;
		std::string button_label = cfg_.has_attribute("button_label")? cfg_["button_label"].str(): "misc/delete.png";
		bool clear = cfg_.has_attribute("clear")? cfg_["clear"].to_bool(): true;

		tpl_widget_ = new ttext_box2(*window_, *widget_, panel_border, image_label, desensitize, button_label, clear);
	} else {
		VALIDATE(false, null_str);
	}
}

vtpl_widget::~vtpl_widget()
{
	// SDL_Log("[lua.gc]---vtpl_widget::~vtpl_widget()---");
}

} // namespace gui2
