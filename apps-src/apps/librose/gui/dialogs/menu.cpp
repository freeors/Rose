/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/menu.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/label.hpp"

using namespace std::placeholders;


namespace gui2 {

REGISTER_DIALOG(rose, menu)

void tmenu::titem::validate_unique_val(std::set<int>& vals) const
{
	VALIDATE(val == submeum_val, null_str);
	for (std::vector<titem>::const_iterator it = submenu.begin(); it != submenu.end(); ++ it) {
		const titem& item = *it;
		VALIDATE(item.submenu.empty(), null_str); // now only max tow serial.
		VALIDATE(vals.find(item.val) == vals.end(), null_str);
		vals.insert(item.val);
	}
}

tmenu::tmenu(const std::vector<titem>& items, int val, const tmenu* parent)
	: items_(items)
	, initial_val_(val)
	, cursel_(nposm)
	, parent_(parent)
	// , motion_result_popup_(!game_config::mobile && !game_config::gui_as_mobile)
	, motion_result_popup_(!game_config::mobile)
	, list_selectable_(true)
	, mouse_motioned_(false)
	, focused_panel_(NULL)
	, selected_val_(titem::submeum_val)
	, request_invoke_popup_menu_(false)
	, show_icon_(false)
	, show_submenu_icon_(false)
	, can_find_(false)
	, find_result_(false)
	, show_find_min_items_(10)
{
	VALIDATE(!items.empty(), null_str);
	validate_unique_val();

	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (cursel_ == nposm && val != nposm && it->val == val) {
			cursel_ = std::distance(items.begin(), it);
		}
		if (!item.icon.empty()) {
			show_icon_ = true;
		}
		if (!item.submenu.empty()) {
			show_submenu_icon_ = true;
		}
	}
	can_find_ = parent_ == nullptr && !show_icon_ && !show_submenu_icon_;
	set_timer_interval(game_config::min_longblock_threshold_s / 2 * 1000);
}

void tmenu::validate_unique_val() const
{
	std::set<int> vals;
	for (std::vector<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& item = *it;
		if (item.submenu.empty()) {
			VALIDATE(vals.find(item.val) == vals.end(), str_cast(item.val));
			vals.insert(item.val);
		} else {
			item.validate_unique_val(vals);
		}
	}
}

void tmenu::pre_show()
{
	window_->set_dismiss_outof_window(true);
	window_->set_border("menu_border");
	window_->set_margin(4 * twidget::hdpi_scale, 4 * twidget::hdpi_scale, 4 * twidget::hdpi_scale, 4 * twidget::hdpi_scale);

	if (parent_) {
		if (motion_result_popup_) {
			window_->set_leave_dismiss(true, std::bind(&tmenu::did_leave_dismiss, this, _1, _2));
		}
		window_->set_click_dismiss_except(std::bind(&tmenu::did_click_dismiss_except, this, _1, _2));
	}

	tlabel* title = find_widget<tlabel>(window_, "title", false, true);
	title->set_visible(twidget::INVISIBLE);

	tpanel* find_grid = find_widget<tpanel>(window_, "find_grid", false, true);
	if (can_find_ && (int)items_.size() >= show_find_min_items_) {
		ttext_box* text_box = find_widget<ttext_box>(window_, "key", false, true);
		// text_box->set_placeholder(_("person^Name"));
		text_box->set_did_text_changed(std::bind(&tmenu::did_key_changed, this, _1));

		tbutton* find = find_widget<tbutton>(window_, "find", false, true);
		connect_signal_mouse_left_click(
				*find
			, std::bind(
				&tmenu::click_find
				, this, std::ref(*find)));
		find->set_active(false);

	} else {
		find_grid->set_visible(twidget::INVISIBLE);
	}

	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	list.enable_select(list_selectable_);
	window_->keyboard_capture(&list);

	std::map<std::string, std::string> data;

	for (std::vector<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& item = *it;
		list_insert_row(item, list);
	}
	show_tip(items_.size(), list.rows());
	// make all item display focussed-background.
	if (cursel_ != nposm) {
		if (cursel_ < list.rows()) {
			list.select_row(cursel_);
		} else {
			cursel_ = nposm;
		}
	}

	list.set_did_row_changed(std::bind(&tmenu::did_item_changed, this, _1, _2));
	list.set_did_row_focus_changed(std::bind(&tmenu::did_item_focus_changed, this, _1, _2, _3));
}

void tmenu::post_show()
{
	if (selected_val_ != titem::submeum_val) {
		return;
	}
	if (cursel_ != nposm) {
		tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
		ttoggle_panel& row = list.row_panel(cursel_);
		selected_val_ = (long)(row.cookie()); 
	}
}

void tmenu::list_insert_row(const titem& item, tlistbox& list)
{
	const int theory_max_show_items = 500;
	const int max_show_items = SDL_max(show_find_min_items_ * 2, theory_max_show_items);
	if (can_find_ && list.rows() >= max_show_items) {
		VALIDATE(list.rows() == max_show_items, null_str);
		return;
	}

	std::map<std::string, std::string> data;

	data["label"] = item.str;
	ttoggle_panel& row = list.insert_row(data);
	row.set_cookie(item.val);

	tcontrol* widget = dynamic_cast<tcontrol*>(row.find("icon", false));
	if (!show_icon_) {
		widget->set_visible(twidget::INVISIBLE);
	} else if (!item.icon.empty()) {
		widget->set_label(item.icon);
	}

	widget = dynamic_cast<tcontrol*>(row.find("submenu", false));
	if (!show_submenu_icon_) {
		widget->set_visible(twidget::INVISIBLE);
	} else if (item.submenu.empty()) {
		widget->set_visible(twidget::HIDDEN);
	}

	if (!item.separator) {
		widget = dynamic_cast<tcontrol*>(row.find("separator", false));
		widget->set_visible(twidget::HIDDEN);
	}
}

const ttoggle_panel& tmenu::focused_panel() const
{
	return *focused_panel_;
}

bool tmenu::did_leave_dismiss(const int x, const int y)
{
	VALIDATE(parent_, null_str);

	if (point_in_rect(x, y, window_->get_rect())) {
		mouse_motioned_ = true;
		return false;
	}

	const ttoggle_panel& focus = parent_->focused_panel();
	const SDL_Rect focus_rect = focus.get_rect();
	if (!mouse_motioned_) {
		return !point_in_rect(x, y, focus_rect);
	} else {
		const twindow* parent_window = focus.get_window();
		const SDL_Rect rect = parent_window->get_rect();
		SDL_Rect top = empty_rect, bottom = empty_rect;
		if (focus.at()) {
			// maybe exist top gap.
			top = ::create_rect(rect.x, rect.y, rect.w, focus_rect.y - rect.y);
		}
		if (focus.at() != parent_->items().size() - 1) {
			// maybe exist bottom gap.
			bottom = ::create_rect(rect.x, focus_rect.y + focus_rect.h, rect.w, rect.h - (focus_rect.y + focus_rect.h- rect.y));
		}
		return point_in_rect(x, y, top) || point_in_rect(x, y, bottom);
	}
}

bool tmenu::did_click_dismiss_except(const int x, const int y)
{
	VALIDATE(parent_, null_str);

	const ttoggle_panel& focus = parent_->focused_panel();
	return point_in_rect(x, y, focus.get_rect());
}

void tmenu::did_key_changed(ttext_box& widget)
{
	const std::string& label = widget.label();
	find_widget<tbutton>(window_, "find", false, true)->set_active(last_key_ != label);
}

void tmenu::did_item_changed(tlistbox& list, ttoggle_panel& widget)
{
	if (find_result_) {
		return;
	}
	VALIDATE(focused_panel_ == &widget, null_str);

	const titem& item = items_[focused_panel_->at()];
	VALIDATE(focused_panel_ == &widget, null_str);
	if (!item.submenu.empty()) {
		return;
	}
	cursel_ = widget.at();
	window_->set_retval(twindow::OK);
}

void tmenu::did_item_focus_changed(tlistbox& list, twidget& panel, const bool enter)
{
	if (!enter) {
		return;
	}
	if (request_invoke_popup_menu_) {
		return;
	}

	focused_panel_ = static_cast<ttoggle_panel*>(&panel);
	const titem& item = items_[focused_panel_->at()];
	if (!item.submenu.empty()) {
		request_invoke_popup_menu_ = true;
		rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_POPUP_MENU, nullptr);
	}
}

void tmenu::request_close(int retval, int selected_val)
{
	selected_val_ = selected_val;
	window_->set_retval(retval);
}

void tmenu::click_find(tbutton& widget)
{
	ttext_box& key_widget = find_widget<ttext_box>(window_, "key", false);
	const std::string& key = key_widget.label();

	focused_panel_ = nullptr;
	cursel_ = nposm;

	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	list.set_best_size_1th(nposm, list.get_width_is_max(), list.get_height(), false);
	list.clear();
	std::map<std::string, std::string> data;
	for (std::vector<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& item = *it;
		
		if (!key.empty() && !chinese::key_matched(item.str, key)) {
			continue;
		}
		if (cursel_ == nposm && item.val == initial_val_) {
			cursel_ = list.rows();
		}
		list_insert_row(item, list);
	}
	show_tip(items_.size(), list.rows());

	// make all item display focussed-background.
	if (cursel_ != nposm) {
		if (cursel_ < list.rows()) {
			tfind_result_lock lock(*this);
			list.select_row(cursel_);
			focused_panel_ = list.cursel();
		} else {
			cursel_ = nposm;
		}
	}

	last_key_ = key;
	widget.set_active(false);
}

void tmenu::show_tip(int items, int list_rows)
{
	tlabel& tip = find_widget<tlabel>(window_, "tip", false);

	utils::string_map symbols;
	symbols["items"] = str_cast(items);
	symbols["list_rows"] = str_cast(list_rows);
	tip.set_label(vgettext2("Total $items, display $list_rows", symbols));
}

void tmenu::app_OnMessage(rtc::Message* msg)
{
	const uint32_t now = SDL_GetTicks();

	switch (msg->message_id) {
	case MSG_POPUP_MENU:
		VALIDATE(focused_panel_, null_str);
		const titem& item = items_[focused_panel_->at()];
		VALIDATE(!item.submenu.empty(), null_str);
		{
			request_invoke_popup_menu_ = false;
			{
				// this focus set but not draw, so require one absolute_draw
				absolute_draw();

				const SDL_Rect panel_rect = focused_panel_->get_rect();
				const int x = panel_rect.x + panel_rect.w;
				const int y = panel_rect.y;

				gui2::tmenu dlg(item.submenu, nposm, this);
				dlg.show(x, y);
				int retval = dlg.get_retval();
				if (dlg.get_retval() == gui2::twindow::OK) {
					request_close(gui2::twindow::OK, dlg.selected_val());
					break;
				}
				if (motion_result_popup_) {
					break;
				}
			}

			// if press in parent item, and this item has submenu, 
			int mousex, mousey;
			SDL_GetMouseState(&mousex, &mousey);
			// if exist
			tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
			if (!point_in_rect(mousex, mousey, list.get_dirty_rect())) {
				break;
			}
			const int rows = list.rows();
			for (int i = 0; i < rows; i ++) {
				ttoggle_panel& row = list.row_panel(i);
				if (point_in_rect(mousex, mousey, row.get_rect())) {
					if (&row != focused_panel_) {
						focused_panel_ = &row;
						const titem& item2 = items_[focused_panel_->at()];
						if (!item2.submenu.empty()) {
							// pupup this item's submenu continue.
							request_invoke_popup_menu_ = true;
							rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_POPUP_MENU, nullptr);
						}
						break;
					}
				}
			}
		}
		break;
	}
}

int tmenu::selected_val() const 
{ 
	VALIDATE(selected_val_ != titem::submeum_val, null_str);
	return selected_val_;
}

}
