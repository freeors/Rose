/* $Id: message.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#include "gui/dialogs/message.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "rose_config.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "base_instance.hpp"

#include <boost/foreach.hpp>
using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, simple_message)
REGISTER_DIALOG(rose, portrait_message)

/**
 * Helper to implement private functions without modifying the header.
 *
 * The class is a helper to avoid recompilation and only has static
 * functions.
 */
struct tmessage_implementation
{
	/**
	 * Initialiazes a button.
	 *
	 * @param window              The window that contains the button.
	 * @param button_status       The button status to modify.
	 * @param id                  The id of the button.
	 */
	static void
	init_button(twindow& window, tmessage::tbutton_status& button_status,
			const std::string& id)
	{
		button_status.button = find_widget<tbutton>(
				&window, id, false, true);
		button_status.button->set_visible(button_status.visible);

		if(!button_status.caption.empty()) {
			button_status.button->set_label(button_status.caption);
		}

		if(button_status.retval != twindow::NONE) {
			button_status.button->set_retval(button_status.retval);
		}
	}
};

void tmessage::pre_show()
{
	// Validate the required buttons
	tmessage_implementation::init_button(*window_, buttons_[cancel], "cancel");
	tmessage_implementation::init_button(*window_, buttons_[ok] ,"ok");

	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	if (!title_.empty()) {
		find_widget<tlabel>(window_, "title", false).set_label(title_);
	} else {
		find_widget<tlabel>(window_, "title", false).set_visible(twidget::INVISIBLE);
	}

	tcontrol& label = find_widget<tcontrol>(window_, "label", false);
	label.set_label(message_);

	// The label might not always be a scroll_label but the capturing
	// shouldn't hurt.
	window_->keyboard_capture(&label);

	// Override the user value, to make sure it's set properly.
	window_->set_click_dismiss(auto_close_);
}

void tmessage::post_show()
{
	BOOST_FOREACH (tbutton_status& button_status, buttons_) {
		button_status.button = NULL;
	}
}

void tmessage::set_button_caption(const tbutton_id button,
		const std::string& caption)
{
	buttons_[button].caption = caption;
	if(buttons_[button].button) {
		buttons_[button].button->set_label(caption);
	}
}

void tmessage::set_button_visible(const tbutton_id button,
		const twidget::tvisible visible)
{
	buttons_[button].visible = visible;
	if(buttons_[button].button) {
		buttons_[button].button->set_visible(visible);
	}
}

void tmessage::set_button_retval(const tbutton_id button,
		const int retval)
{
	buttons_[button].retval = retval;
	if(buttons_[button].button) {
		buttons_[button].button->set_retval(retval);
	}
}

tmessage::tbutton_status::tbutton_status()
	: button(NULL)
	, caption()
	, visible(twidget::INVISIBLE)
	, retval(twindow::NONE)
{
}

void tsimple_message::pre_show()
{
	tmessage::pre_show();

	bool all_button_invisible = true;
	for (std::vector<tbutton_status>::const_iterator it = buttons_.begin(); it != buttons_.end(); ++ it) {
		if (it->visible != twidget::INVISIBLE) {
			all_button_invisible = false;
		}
	}

	if (countdown_close_ && all_button_invisible) {
		will_close_ticks_ = SDL_GetTicks() + will_close_threshold_;
	}
}

void tsimple_message::app_timer_handler(uint32_t now)
{
	if (will_close_ticks_ != 0) {
		std::stringstream ss;
		if (now < will_close_ticks_) {
			int remainder = (will_close_ticks_ - now) / 1000 + 1;

			utils::string_map symbols;
			int elapse = will_close_ticks_ - now;
			symbols["elapse"] = format_elapse_hms(elapse / 1000);
			// std::string msg = vgettext2("Will close after $elapse", symbols);
			std::string msg = vgettext2("($elapse)", symbols);
			// find_widget<tlabel>(window_, "close_msg", false, true)->set_label(msg);

		} else {
			will_close_ticks_ = 0;
			window_->set_retval(twindow::CANCEL);
		}
	}
}

void tportrait_message::pre_show()
{
	tmessage::pre_show();

	tfloat_widget* item = window_->find_float_widget("float_portrait");
	item->set_visible(true);
	item->widget->set_label(portrait_);
	item->widget->disable_allow_handle_event();

	item = window_->find_float_widget("float_incident");
	item->set_visible(true);
	item->widget->set_label(incident_);
	item->widget->disable_allow_handle_event();
}

void show_message_onlycountdownclose(const std::string& title, const std::string& message)
{
	if (!gui2::settings::actived) {
		SDL_SimplerMB("%s", message.c_str());
		return;
	}

	tsimple_message dlg(title, message, false, true);
	dlg.show();
}

void show_message(const std::string& title, const std::string& message, bool countdown_close)
{
	if (!gui2::settings::actived) {
		SDL_SimplerMB("%s", message.c_str());
		return;
	}

	tsimple_message dlg(title, message, true, countdown_close);
	dlg.show();
}

int show_message2(const std::string& title,
	const std::string& message, const tmessage::tbutton_style button_style,
	const std::string& portrait, const std::string& incident)
{
	if (game_config::no_messagebox && button_style == tmessage::auto_close) {
		return twindow::OK;
	}

	tmessage* dlg;
	
	if (portrait.empty()) {
		dlg = new tsimple_message(title, message, button_style == tmessage::auto_close, false);
	} else {
		dlg = new tportrait_message(title, message, portrait, incident, button_style == tmessage::auto_close);
	}

	switch (button_style) {
		case tmessage::auto_close :
			break;
		case tmessage::ok_button :
			dlg->set_button_visible(tmessage::ok, twidget::VISIBLE);
			dlg->set_button_caption(tmessage::ok, _("OK"));
			break;
		case tmessage::close_button :
			dlg->set_button_visible(tmessage::ok, twidget::VISIBLE);
			break;
		case tmessage::ok_cancel_buttons :
			dlg->set_button_visible(tmessage::ok, twidget::VISIBLE);
			dlg->set_button_caption(tmessage::ok, _("OK"));
			/* FALL DOWN */
		case tmessage::cancel_button :
			dlg->set_button_visible(tmessage::cancel, twidget::VISIBLE);
			break;
		case tmessage::yes_no_buttons :
			dlg->set_button_visible(tmessage::ok, twidget::VISIBLE);
			dlg->set_button_caption(tmessage::ok, _("Yes"));
			dlg->set_button_visible(tmessage::cancel, twidget::VISIBLE);
			dlg->set_button_caption(tmessage::cancel, _("No"));
			break;
	}

	dlg->show();
	int retval = dlg->get_retval();
	delete dlg;

	// When dialog is closed, some scenes immediately display the operating system's built-in boxes, 
	// for example a copy dialog that shows the progress of copy files under MS-Windows. 
	// Result to no time to perform a rose refresh in the middle. For UI clean, here calling a rose refresh explicited.
	absolute_draw();
	return retval;
}

void show_error_message(const std::string& message)
{
	show_message2(_("Error"), message, tmessage::ok_button, "hero-256/0.png", "");
}

} // namespace gui2

