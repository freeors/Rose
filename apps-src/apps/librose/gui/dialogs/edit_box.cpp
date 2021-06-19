/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
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

#include "gui/dialogs/edit_box.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"

#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, edit_box)

tedit_box* tedit_box::instance = nullptr;

tedit_box::tedit_box(tedit_box_param& param)
	: param_(param)
	, ok_(nullptr)
	, cancel_(nullptr)
	, cancel_label_(_("Cancel"))
	, will_close_ticks_(0)
	, sync_with_keyboard_(false)

{
	VALIDATE(tprogress_::instance == nullptr, "only allows a maximum of one tedit_box");
	VALIDATE(param_.cancel_strategy == tedit_box_param::hide_cancel || param_.cancel_strategy >= tedit_box_param::show_cancel, null_str);
	set_timer_interval(400);
}

tedit_box::~tedit_box()
{
	tedit_box::instance = nullptr;
}

void tedit_box::pre_show()
{
	tedit_box::instance = this;
	window_->set_canvas_variable("border", variant("default_border"));

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	label = find_widget<tlabel>(window_, "prefix", false, true);
	if (!param_.prefix.empty()) {
		label->set_label(param_.prefix);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	label = find_widget<tlabel>(window_, "remark", false, true);
	if (!param_.remark.empty()) {
		label->set_label(param_.remark);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	tbutton* button = find_widget<tbutton>(window_, "reset", false, true);
	if (!param_.reset.empty()) {
		connect_signal_mouse_left_click(
				*button
			, std::bind(
				&tedit_box::click_reset
				, this));
	} else {
		button->set_visible(twidget::INVISIBLE);
	}

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	int cancel_strategy = param_.cancel_strategy;
	if (!param_.ok.empty()) {
		ok_->set_label(param_.ok);
	} else {
		ok_->set_visible(twidget::INVISIBLE);
		// forbit cancel_strategy to hide_cancel.
		cancel_strategy = tedit_box_param::hide_cancel;
	}

	cancel_ = find_widget<tbutton>(window_, "cancel", true, true);
	if (cancel_strategy == tedit_box_param::hide_cancel) {
		window_->set_escape_disabled(true);
		cancel_->set_visible(twidget::INVISIBLE);
	} else {
		VALIDATE(ok_->get_visible() == twidget::VISIBLE, null_str);
		if (cancel_strategy > tedit_box_param::show_cancel) {
			will_close_ticks_ = SDL_GetTicks() + (cancel_strategy - tedit_box_param::show_cancel) * 1000;
		}
	}

	txt_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "txt", false, true));
	txt_->text_box()->set_border("textbox");
	window_->keyboard_capture(txt_->text_box());
	// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

	txt_->text_box()->set_placeholder(param_.placeholder);
	if (param_.max_chars != nposm) {
		txt_->text_box()->set_maximum_chars(param_.max_chars);
	}
	txt_->set_did_text_changed(std::bind(&tedit_box::text_changed_callback, this, _1));
	if (param_.initial.empty()) {
		text_changed_callback(*txt_->text_box());
	} else {
		txt_->text_box()->set_label(param_.initial);
	}

	sync_with_keyboard_ = ok_->get_visible() != twidget::VISIBLE;
	if (sync_with_keyboard_) {
		connect_signal_pre_key_press(*txt_->text_box(), std::bind(&tedit_box::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));
		keyboard::set_visible(true);
	}
}

void tedit_box::post_show()
{
}

void tedit_box::click_reset()
{
	VALIDATE(!param_.reset.empty(), null_str);
	txt_->text_box()->set_label(param_.reset);
}

void tedit_box::signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
	VALIDATE(ok_->get_visible() != twidget::VISIBLE, null_str);
	if (key == SDLK_RETURN) {
		window_->set_retval(twindow::OK);
	}
}

void tedit_box::text_changed_callback(ttext_box& widget)
{
	param_.result = widget.label();
	bool active = true;
	if (active && param_.did_text_changed != NULL) {
		active = param_.did_text_changed(param_.result);
	}
	ok_->set_active(active);
}

void tedit_box::app_did_keyboard_hidden(int reason)
{
	if (reason == keyboard::REASON_CLOSE) {
		if (sync_with_keyboard_) {
			window_->set_retval(twindow::OK);
		}
	}
}

void tedit_box::app_timer_handler(uint32_t now)
{
	if (will_close_ticks_ != 0) {
		std::stringstream label_ss;
		if (now < will_close_ticks_) {
			int remainder = (will_close_ticks_ - now) / 1000;
			if (remainder == 0) {
				remainder = 1;
			}
			label_ss << cancel_label_ << "(" << remainder << ")";
			cancel_->set_label(label_ss.str());

		} else {
			will_close_ticks_ = 0;
			window_->set_retval(twindow::CANCEL);
		}
	}
}

} // namespace gui2

