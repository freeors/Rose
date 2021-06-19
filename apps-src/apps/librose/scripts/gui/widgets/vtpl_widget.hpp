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

#ifndef GUI_WIDGETS_VTPL_WIDGET_HPP_INCLUDED
#define GUI_WIDGETS_VTPL_WIDGET_HPP_INCLUDED

#include "gui/widgets/window.hpp"

#include "lua/lua.h"

class rose_lua_kernel;

namespace gui2 {

class vtpl_widget
{
public:
	static const char metatableKey[];

	explicit vtpl_widget(lua_State* _L, int type, twindow& window, twidget& widget, const config& cfg);

	~vtpl_widget();

	tbase_tpl_widget& tpl_widget() { return *tpl_widget_; }
	const std::string& lua_window_id() const { return lua_window_id_; }

private:
	rose_lua_kernel& lua_;
	lua_State* L;
	twindow* window_;
	twidget* widget_;
	const config cfg_;
	const std::string lua_window_id_;
	tbase_tpl_widget* tpl_widget_;
};

void luaW_pushvtpl_widget(lua_State* L, int type, twindow& window, twidget& widget, const config& cfg);

} // namespace gui2

#endif

