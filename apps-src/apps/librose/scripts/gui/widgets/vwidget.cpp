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

#include "scripts/gui/widgets/vwidget.hpp"
#include "scripts/gui/widgets/vtpl_widget.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/listbox.hpp"

#include <lua/lauxlib.h>
#include <lua/lua.h>

#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"
#include "scripts/vconfig.hpp"

#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gui/dialogs/menu.hpp"

#include "base_instance.hpp"

using namespace std::placeholders;

namespace gui2 {

static const char gui2Key[] = "gui2";
static const char vwidgetMetatableKey[] = "gui2.vwidget";

static int impl_gui2_cfg_2_os_size(lua_State *L)
{
	int cfg_size = lua_tointeger(L, 1);
	lua_pushinteger(L, cfg_2_os_size(cfg_size));
	return 1;
}

static int impl_gui2_find_widget(lua_State *L)
{
	twidget* widget = *static_cast<twidget **>(luaL_checkudata(L, 1, vwidgetMetatableKey));
	char const* id = luaL_checkstring(L, 2);
	bool must_be_active = luaW_toboolean(L, 3);
	bool must_exist = luaW_toboolean(L, 4);

	twidget* result = find_widget<twidget>(widget, id, must_be_active, must_exist);
	if (result != nullptr) {
		luaW_pushvwidget(L, *result);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int impl_gui2_tovtpl_widget(lua_State *L)
{
	int tpl_type = lua_tointeger(L, 1);
	twindow* window = *static_cast<twindow **>(luaL_checkudata(L, 2, vwidgetMetatableKey));
	twidget* widget = *static_cast<twidget **>(luaL_checkudata(L, 3, vwidgetMetatableKey));
	vconfig vcfg = luaW_checkvconfig(L, 4);

	luaW_pushvtpl_widget(L, tpl_type, *window, *widget, vcfg.get_config());
	return 1;
}

static bool lua_slot_func(lua_State* L, const vconfig& vcfg)
{
	tstack_size_lock lock(L, 0);

	const config& cfg = vcfg.get_config();

	luaW_getglobal_2joined(L, cfg["method"].str().c_str());
	luaW_pushvconfig(L, vcfg);
	instance->lua().protected_call(1, 1);

	bool ret = luaW_toboolean(L, -1);
	lua_pop(L, 1);
	return ret;
}

static int impl_gui2_run_with_progress(lua_State *L)
{
	vconfig vcfg = luaW_checkvconfig(L, 1);
	const config& cfg = vcfg.get_config();

	gui2::tprogress_default_slot slot(std::bind(&lua_slot_func, L, std::ref(vcfg)));
	std::string title;
	if (cfg.has_attribute("title")) {
		title = cfg["title"].str();
	}
	std::string message;
	if (cfg.has_attribute("message")) {
		message = cfg["message"].str();
	}
	VALIDATE(cfg.has_attribute("hidden_ms"), null_str);
	int hidden_ms = cfg["hidden_ms"].to_int();

	SDL_Rect rect = SDL_Rect{nposm, nposm, nposm, nposm};
	if (cfg.has_attribute("rect")) {
		rect = lua_unpack_rect(cfg["rect"].to_long_long());
	}

	bool ret = run_with_progress(slot, title, message, hidden_ms, rect);
	lua_pushboolean(L, ret);
	return 1;
}

/**
 * Displays a simple message box.
 */
int impl_gui2_show_message(lua_State* L)
{
	const std::string title = lua_type(L, 1) == LUA_TSTRING? luaL_checkstring(L, 1): null_str;
	const std::string message = lua_type(L, 2) == LUA_TSTRING? luaL_checkstring(L, 2): null_str;
	int style = luaL_optinteger(L, 3, gui2::tmessage::auto_close);

	int result = gui2::show_message2(title, message, (gui2::tmessage::tbutton_style)style);
	if (style == gui2::tmessage::ok_cancel_buttons || style == gui2::tmessage::yes_no_buttons) {
		lua_pushboolean(L, result == gui2::twindow::OK);
		return 1;
	}
	
	return 0;
}

int impl_gui2_show_messagefs(lua_State* L)
{
	const t_string message = luaL_checkstring(L, 1);
	const t_string ok_caption = lua_type(L, 2) == LUA_TSTRING? luaL_checkstring(L, 2): null_str;
	const t_string cancel_caption = lua_type(L, 3) == LUA_TSTRING? luaL_checkstring(L, 3): null_str;

	int result = gui2::show_messagefs(message, ok_caption, cancel_caption);
	lua_pushboolean(L, result == gui2::twindow::OK);
	return 1;
}

int impl_gui2_show_menu(lua_State* L)
{
	int explicit_x = lua_tointeger(L, 1);
	int explicit_y = lua_tointeger(L, 2);
	std::vector<gui2::tmenu::titem> items;

	{
		tstack_size_lock lock(L, 0);
		int items_index = 3;
		for (int i = 1, i_end = lua_rawlen(L, items_index); i <= i_end; ++i) {
			lua_rawgeti(L, items_index, i);
			VALIDATE(lua_istable(L, -1), null_str);

			lua_rawgeti(L, -1, 1);
			const char* label = luaL_checkstring(L, -1);
			VALIDATE(label[0] != '\0', null_str);

			// field#1 push to stack, so table's index changes to -2.
			lua_rawgeti(L, -2, 2);
			int value = lua_tointeger(L, -1);
			VALIDATE(value >= 0, null_str);

			// field#1 and field#2 push to stack, so table's index changes to -3.
			lua_rawgeti(L, -3, 3);
			// icon maybe nil.
			std::string icon;
			if (lua_type(L, -1) == LUA_TSTRING) {
				icon = luaL_checkstring(L, -1);
			}

			items.push_back(gui2::tmenu::titem(label, value, icon));
			lua_pop(L, 4); // 1table + 3fields
		}
	}

	int initial_sel = lua_tointeger(L, 4);

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(explicit_x, explicit_y);
	int retval = dlg.get_retval() == gui2::twindow::OK? dlg.selected_val(): nposm;

	lua_pushinteger(L, retval);
	return 1;
}

void register_gui2_metatable(lua_State *L)
{
	tstack_size_lock lock(L, 0);

	// luaL_newmetatable(L, gui2Key);
	lua_getglobal(L, "gui2");
	VALIDATE(lua_istable(L, -1), "must load lua/gui2.lua before call register_gui2_metatable");

	static luaL_Reg const callbacks[] {
		{ "cfg_2_os_size", 	    &impl_gui2_cfg_2_os_size},
		{ "find_widget", 	    &impl_gui2_find_widget},
		{ "tovtpl_widget",      &impl_gui2_tovtpl_widget},
		{ "run_with_progress",  &impl_gui2_run_with_progress},
		{ "show_message",       &impl_gui2_show_message},
		{ "show_messagefs",     &impl_gui2_show_messagefs},
		{ "show_menu",          &impl_gui2_show_menu},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	lua_pushstring(L, gui2Key);
	lua_setfield(L, -2, "__metatable");

	std::map<std::string, int> values;
	values.insert(std::make_pair("tmessage_auto_close", tmessage::auto_close));
	values.insert(std::make_pair("tmessage_ok_button", tmessage::ok_button));
	values.insert(std::make_pair("tmessage_close_button", tmessage::close_button));
	values.insert(std::make_pair("tmessage_ok_cancel_buttons", tmessage::ok_cancel_buttons));
	values.insert(std::make_pair("tmessage_cancel_button", tmessage::cancel_button));
	values.insert(std::make_pair("tmessage_yes_no_buttons", tmessage::yes_no_buttons));

	for (std::map<std::string, int>::const_iterator it = values.begin(); it != values.end(); ++ it) {
		lua_pushinteger(L, it->second);
		lua_setfield(L, -2, it->first.c_str());
	}
	lua_pop(L, 1);
}

/**
 * Destroys a vwidget object before it is collected (__gc metamethod).
 */
static int impl_vwidget_collect(lua_State *L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_vwidget_get(lua_State *L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vwidget_set_active(lua_State *L)
{
	twidget* widget = *static_cast<twidget **>(lua_touserdata(L, 1));

	bool active = luaW_toboolean(L, 2);
	widget->set_active(active);

	return 0;
}

static int impl_vwidget_cookie(lua_State *L)
{
	twidget* widget = *static_cast<twidget **>(lua_touserdata(L, 1));

	lua_pushinteger(L, widget->cookie());
	return 1;
}

static int impl_vwidget_set_cookie(lua_State *L)
{
	twidget* widget = *static_cast<twidget **>(lua_touserdata(L, 1));

	widget->set_cookie(lua_tointeger(L, 2));
	return 0;
}

static int impl_vwidget_set_visible(lua_State *L)
{
	twidget* widget = *static_cast<twidget **>(lua_touserdata(L, 1));

	int visible = lua_tointeger(L, 2);
	widget->set_visible((twidget::tvisible)visible);
	
	return 0;
}

static int impl_vwidget_label(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	lua_pushstring(L, widget->label().c_str());
	return 1;
}

static int impl_vwidget_set_label(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	char const* m = nullptr;
	if (lua_type(L, 2) == LUA_TSTRING) {
		m = luaL_checkstring(L, 2);
	}
	widget->set_label(m != nullptr? m: null_str);

	return 0;
}

static int impl_vwidget_set_border(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	char const* m = luaL_checkstring(L, 2);
	widget->set_border(m);

	return 0;
}

static int impl_vwidget_set_icon(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	char const* m = luaL_checkstring(L, 2);
	widget->set_icon(m);

	return 0;
}

static int impl_vwidget_at(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	lua_pushinteger(L, widget->at());
	return 1;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
static void lua_did_left_click(lua_State* L, const std::string& window_id, const std::string& method, tcontrol* widget)
{
	tstack_size_lock lock(L, 0);
	luaW_getglobal(L, window_id.c_str(), method);
	luaW_pushvwidget(L, *widget);
	instance->lua().protected_call(1, 0);
}

static int impl_vwidget_set_did_left_click(lua_State *L)
{
	tcontrol* widget = *static_cast<tcontrol **>(lua_touserdata(L, 1));

	char const* m = luaL_checkstring(L, 2);
	VALIDATE(m != nullptr && m[0] != '\0', null_str);

	const std::string method = m;
	connect_signal_mouse_left_click(
		*widget
		, std::bind(
			&lua_did_left_click
			, L, widget->get_window()->id(), method, widget));
	
	return 0;
}

static int impl_vwidget_cipher(lua_State *L)
{
	ttext_box* widget = *static_cast<ttext_box **>(lua_touserdata(L, 1));

	lua_pushboolean(L, widget->cipher());
	return 1;
}

static int impl_vwidget_set_cipher(lua_State *L)
{
	ttext_box* widget = *static_cast<ttext_box **>(lua_touserdata(L, 1));

	bool cipher = lua_toboolean(L, 2);
	widget->set_cipher(cipher);
	return 0;
}

static int impl_vwidget_set_maximum_chars(lua_State *L)
{
	ttext_box* widget = *static_cast<ttext_box **>(lua_touserdata(L, 1));

	int maximum_chars = lua_tointeger(L, 2);
	widget->set_maximum_chars(maximum_chars);
	return 0;
}

static int impl_vwidget_set_placeholder(lua_State *L)
{
	ttext_box* widget = *static_cast<ttext_box **>(lua_touserdata(L, 1));

	const char* c_str = lua_tostring(L, 2);
	const std::string label = c_str != nullptr? c_str: null_str;
	widget->set_placeholder(label);
	return 0;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
void lua_did_text_changed(ttext_box& widget, lua_State* L, const std::string& window_id, const std::string& method)
{
	tstack_size_lock lock(L, 0);
	
	luaW_getglobal(L, window_id.c_str(), method.c_str());
	luaW_pushvwidget(L, widget);
	instance->lua().protected_call(1, 0);
}

static int impl_vwidget_set_did_text_changed(lua_State *L)
{
	ttext_box* widget = *static_cast<ttext_box **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	const char* m = luaL_checkstring(L, 2);
	VALIDATE(m[0] != '\0', null_str);

	const std::string method = m;
	widget->set_did_text_changed(std::bind(&lua_did_text_changed, _1, L, widget->get_window()->id(), method));
	return 0;
}

static int impl_vwidget_set_child_label(lua_State *L)
{
	ttoggle_panel* widget = *static_cast<ttoggle_panel **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	const char* id = luaL_checkstring(L, 2);
	const char* label = luaL_checkstring(L, 3);
	widget->set_child_label(id, label);
	return 0;
}

static int impl_vwidget_set_timer_interval(lua_State *L)
{
	ttrack* widget = *static_cast<ttrack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int interval = lua_tointeger(L, 2);
	widget->set_timer_interval(interval);
	return 0;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
void lua_track_did_draw(ttrack& widget, const SDL_Rect& rect, bool bg_drawn, lua_State* L, const std::string& window_id, const std::string& method)
{
	gui2::twidget::tdisable_lua_unlimited_mem_lock lock;
	// tstack_size_lock lock(L, 0);
	
	luaW_getglobal(L, window_id.c_str(), method.c_str());
	luaW_pushvwidget(L, widget);
	lua_pushinteger(L, lua_pack_rect(rect.x, rect.y, rect.w, rect.h));
	lua_pushboolean(L, bg_drawn);
	instance->lua().protected_call(3, 0);
}

static int impl_vwidget_set_did_draw(lua_State *L)
{
	ttrack* widget = *static_cast<ttrack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	const char* m = luaL_checkstring(L, 2);
	const std::string method = m;
	widget->set_did_draw(std::bind(&lua_track_did_draw, _1, _2, _3, L, widget->get_window()->id(), method));
	return 0;
}

static int impl_vwidget_get_draw_rect(lua_State *L)
{
	ttrack* widget = *static_cast<ttrack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	SDL_Rect rect = widget->get_draw_rect();
	lua_pushinteger(L, lua_pack_rect(rect.x, rect.y, rect.w, rect.h));
	return 1;
}

static int impl_vwidget_immediate_draw(lua_State *L)
{
	ttrack* widget = *static_cast<ttrack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	widget->immediate_draw();
	return 0;
}

static int impl_vwidget_clear(lua_State *L)
{
	tscroll_container* widget = *static_cast<tscroll_container **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	widget->clear();
	return 0;
}

static int impl_vwidget_layer(lua_State *L)
{
	tstack* widget = *static_cast<tstack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int at = lua_tointeger(L, 2);
	luaW_pushvwidget(L, *widget->layer(at));
	return 1;
}

static int impl_vwidget_set_radio_layer(lua_State *L)
{
	tstack* widget = *static_cast<tstack **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int at = lua_tointeger(L, 2);
	widget->set_radio_layer(at);
	return 0;
}

static int impl_vwidget_insert_item(lua_State *L)
{
	treport* widget = *static_cast<treport **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	const std::string id = luaL_checkstring(L, 2);
	const std::string label = luaL_checkstring(L, 3);
	int at = nposm;
	if (lua_type(L, 4) == LUA_TNUMBER) {
		at = lua_tointeger(L, 4);
	}

	tcontrol& item = widget->insert_item(id, label, at);
	luaW_pushvwidget(L, item);
	return 1;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
void lua_did_item_changed(treport& report, ttoggle_button& widget, lua_State* L, const std::string& window_id, const std::string& method)
{
	tstack_size_lock lock(L, 0);
	
	luaW_getglobal(L, window_id.c_str(), method.c_str());
	luaW_pushvwidget(L, report);
	luaW_pushvwidget(L, widget);
	instance->lua().protected_call(2, 0);
}

static int impl_vwidget_set_did_item_changed(lua_State *L)
{
	treport* widget = *static_cast<treport **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	const char* m = luaL_checkstring(L, 2);
	VALIDATE(m[0] != '\0', null_str);

	const std::string method = m;
	widget->set_did_item_changed(std::bind(&lua_did_item_changed, _1, _2, L, widget->get_window()->id(), method));
	return 0;
}

static int impl_vwidget_select_item(lua_State *L)
{
	treport* widget = *static_cast<treport **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int at = lua_tointeger(L, 2);
	widget->select_item(at);
	return 0;
}

static int impl_vwidget_enable_select(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	widget->enable_select(luaW_toboolean(L, 2));
	return 0;
}

static int impl_vwidget_insert_row(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	vconfig vcfg = luaW_checkvconfig(L, 2);
	const config& cfg = vcfg.get_config();
	std::map<std::string, std::string> data;
	for (const config::attribute& a: cfg.attribute_range()) {
		data.insert(std::make_pair(a.first, a.second));
	}
	int at = nposm;
	if (lua_type(L, 3) == LUA_TNUMBER) {
		at = lua_tointeger(L, 3);
	}
	ttoggle_panel& row = widget->insert_row(data, at);
	luaW_pushvwidget(L, row);
	return 1;
}

static int impl_vwidget_erase_row(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int at = lua_tointeger(L, 2);
	widget->erase_row(at);
	return 0;
}

static int impl_vwidget_cookie_2_at(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	size_t cookie = lua_tointeger(L, 2);
	bool validate = lua_tointeger(L, 3);
	int at = widget->cookie_2_at(cookie, validate);
	lua_pushinteger(L, at);
	return 1;
}

static int impl_vwidget_row_panel(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	int at = lua_tointeger(L, 2);
	luaW_pushvwidget(L, widget->row_panel(at));
	return 1;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
void lua_list_did_pullrefresh(ttrack& widget, const SDL_Rect& rect, lua_State* L, const std::string& window_id, const std::string& method, const config& cfg)
{
	tstack_size_lock lock(L, 0);
	
	luaW_getglobal_2joined(L, method.c_str());
	luaW_pushvwidget(L, widget);
	lua_pushinteger(L, lua_pack_rect(rect.x, rect.y, rect.w, rect.h));
	luaW_pushvconfig(L, vconfig(cfg));
	instance->lua().protected_call(3, 0);
}

static int impl_vwidget_set_did_pullrefresh(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	vconfig vcfg = luaW_checkvconfig(L, 2);
	const config& cfg = vcfg.get_config();
	widget->set_did_pullrefresh(cfg["refresh_height"].to_int(), 
		std::bind(&lua_list_did_pullrefresh, _1, _2, L, widget->get_window()->id(), cfg["did_refresh"].str(), cfg));
	return 0;
}

// must not use vwidget's member function, becuase vwidget maybe destructed.
void lua_list_did_left_drag_widget_click(tlistbox& list, ttoggle_panel& row, lua_State* L, const std::string& window_id, const std::string& method, const config& cfg)
{
	tstack_size_lock lock(L, 0);
	
	luaW_getglobal(L, window_id.c_str(), method.c_str());
	luaW_pushvwidget(L, list);
	luaW_pushvwidget(L, row);
	luaW_pushvconfig(L, vconfig(cfg));
	instance->lua().protected_call(3, 0);
}

static int impl_vwidget_set_did_left_drag_widget_click(lua_State *L)
{
	tlistbox* widget = *static_cast<tlistbox **>(lua_touserdata(L, 1));
	VALIDATE(widget != nullptr, null_str);

	vconfig vcfg = luaW_checkvconfig(L, 2);
	const config& cfg = vcfg.get_config();
	tbutton& control = widget->set_did_left_drag_widget_click(cfg["id"].str(),
		std::bind(&lua_list_did_left_drag_widget_click, _1, _2, L, widget->get_window()->id(), cfg["method"].str(), cfg));
	luaW_pushvwidget(L, control);
	return 1;
}

static int impl_vwidget_set_retval(lua_State *L)
{
	twindow* window = *static_cast<twindow **>(lua_touserdata(L, 1));

	int retval = lua_tointeger(L, 2);
	window->set_retval(retval);
	
	return 0;
}

void luaW_pushvwidget(lua_State* L, twidget& widget)
{
	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	twidget** v = (twidget**)lua_newuserdata(L, sizeof(twidget*));
	*v = &widget;
	if (luaL_newmetatable(L, vwidgetMetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vwidget_collect},
			{"__index", impl_vwidget_get},
			// twidget
			{"set_visible", impl_vwidget_set_visible},
			{"set_active", impl_vwidget_set_active},
			{"cookie", impl_vwidget_cookie},
			{"set_cookie", impl_vwidget_set_cookie},

			// tcontrol
			{"label", impl_vwidget_label},
			{"set_label", impl_vwidget_set_label},
			{"set_border", impl_vwidget_set_border},
			{"set_icon", impl_vwidget_set_icon},
			{"at", impl_vwidget_at},
			{"set_did_left_click", impl_vwidget_set_did_left_click},

			// text_box
			{"cipher", impl_vwidget_cipher},
			{"set_cipher", impl_vwidget_set_cipher},
			{"set_maximum_chars", impl_vwidget_set_maximum_chars},
			{"set_placeholder", impl_vwidget_set_placeholder},
			{"set_did_text_changed", impl_vwidget_set_did_text_changed},

			// toggle_panel
			{"set_child_label", impl_vwidget_set_child_label},

			// track
			{"set_timer_interval", impl_vwidget_set_timer_interval},
			{"set_did_draw", impl_vwidget_set_did_draw},
			{"get_draw_rect", impl_vwidget_get_draw_rect},
			{"immediate_draw", impl_vwidget_immediate_draw},

			// tscroll_container
			{"clear", impl_vwidget_clear},

			// stack
			{"layer", impl_vwidget_layer},
			{"set_radio_layer", impl_vwidget_set_radio_layer},

			// report
			{"insert_item", impl_vwidget_insert_item},
			{"set_did_item_changed", impl_vwidget_set_did_item_changed},
			{"select_item", impl_vwidget_select_item},

			// listbox
			{"enable_select", impl_vwidget_enable_select},
			{"insert_row", impl_vwidget_insert_row},
			{"erase_row", impl_vwidget_erase_row},
			{"cookie_2_at", impl_vwidget_cookie_2_at},
			{"row_panel", impl_vwidget_row_panel},
			{"set_did_pullrefresh", impl_vwidget_set_did_pullrefresh},
			{"set_did_left_drag_widget_click", impl_vwidget_set_did_left_drag_widget_click},

			// twindow
			{"set_retval", impl_vwidget_set_retval},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vwidgetMetatableKey);
	}
	lua_setmetatable(L, -2);
}


} // namespace gui2
