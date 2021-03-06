/* $Id: lobby_player_info.cpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/netdiag.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_panel.hpp"
#include "gui/widgets/settings.hpp"

#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, netdiag)

tnetdiag::tnetdiag()
	: log_(NULL)
{
}

tnetdiag::~tnetdiag()
{
}

void tnetdiag::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	log_ = &find_widget<tlabel>(window_, "log", false);
	log_->set_label(lobby->format_log_str());
	tscroll_panel& panel = find_widget<tscroll_panel>(window_, "scroll_panel", false);
	panel.set_scroll_to_end(true);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "connect", false)
			, std::bind(
				  &tnetdiag::connect_button_callback
				, this
				, std::ref(*window_)));
	find_widget<tbutton>(window_, "connect", false).set_active(false);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "clear", false)
			, std::bind(
				  &tnetdiag::clear_button_callback
				, this
				, std::ref(*window_)));

	join();
}

void tnetdiag::post_show()
{
}

void tnetdiag::handle(const tsock& s, const std::string& msg)
{
	log_->set_label(lobby->format_log_str());
	// log_->scroll_vertical_scrollbar(tscrollbar_::END);
}

void tnetdiag::connect_button_callback(twindow& window)
{
}

void tnetdiag::clear_button_callback(twindow& window)
{
	log_->set_label(null_str);
	lobby->clear_log();
}

} //end namespace gui2
