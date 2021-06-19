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

#include "gui/dialogs/browse.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"

using namespace std::placeholders;
// std::tolower
#include <cctype>

namespace gui2 {

REGISTER_DIALOG(rose, browse)

const char tbrowse::path_delim = '/';
const std::string tbrowse::path_delim_str = "/";

tbrowse::tfile2::tfile2(const std::string& name)
	: name(name)
	, method(tbrowse::METHOD_ALPHA)
{
	lower = utils::lowercase(name);
}
bool tbrowse::tfile2::operator<(const tbrowse::tfile2& that) const 
{
	return lower < that.lower; 
}

tbrowse::tbrowse(tparam& param, bool only_extra_entry)
	: param_(param)
	, only_extra_entry_(only_extra_entry)
	, navigate_(NULL)
	, goto_higher_(NULL)
	, filename_(NULL)
	, ok_(nullptr)
	, auto_select_(false)
	, current_dir_(get_path(param.initial))
{
	if (param.extra.valid()) {
		param.extra.path = get_path(param.extra.path);
		VALIDATE(SDL_IsDirectory(param.extra.path.c_str()), null_str);
	}
	if (only_extra_entry_) {
		VALIDATE(param_.extra.valid(), null_str);
	}
	if (!is_directory(current_dir_)) {
		if (!only_extra_entry_) {
			current_dir_ = get_path(game_config::preferences_dir);
		} else {
			current_dir_ = param.extra.path;
		}
	}
}

void tbrowse::pre_show()
{
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	}

	filename_ = find_widget<ttext_box>(window_, "filename", false, true);
	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	if (!param_.open.empty()) {
		ok_->set_label(param_.open);
	}
	ok_->set_active(false);

	goto_higher_ = find_widget<tbutton>(window_, "goto-higher", false, true);
	connect_signal_mouse_left_click(
			  *goto_higher_
			, std::bind(
				&tbrowse::goto_higher
				, this));

	navigate_ = find_widget<treport>(window_, "navigate", false, true);
	navigate_->set_did_item_click(std::bind(&tbrowse::click_navigate, this, std::ref(*window_), _2));

	if (param_.readonly) {
		filename_->set_active(false);
	}
	filename_->set_did_text_changed(std::bind(&tbrowse::text_changed_callback, this, _1));

	tlistbox& list = find_widget<tlistbox>(window_, "default", false);
	if (!game_config::mobile) {
		list.set_did_row_double_click(std::bind(&tbrowse::did_item_double_click, this, _1, _2));
	}
	list.set_did_row_changed(std::bind(&tbrowse::did_item_selected, this, _1, _2));

	window_->keyboard_capture(&list);

	init_entry(*window_);
}

void tbrowse::post_show()
{
	param_.result = calculate_result();
}

std::string tbrowse::calculate_result() const
{
	const std::string& label = filename_->label();
	if (current_dir_ != path_delim_str) {
		return current_dir_ + path_delim + label;
	} else {
		return current_dir_ + label;
	}
}

void tbrowse::init_entry(twindow& window)
{
	if (!only_extra_entry_) {
		entries_.push_back(tentry(game_config::preferences_dir, _("My Document"), "misc/documents.png"));
		if (game_config::os == os_windows) {
			entries_.push_back(tentry("c:", _("C:"), "misc/disk.png"));
		} else {
			entries_.push_back(tentry("/", _("Device"), "misc/disk.png"));
		}
		entries_.push_back(tentry(game_config::path, _("Sandbox"), "misc/dir_res.png"));
	}
	if (param_.extra.valid()) {
		entries_.push_back(param_.extra);
	}
	VALIDATE(!entries_.empty(), null_str);

	tlistbox& root = find_widget<tlistbox>(&window, "root", false);
	root.enable_select(false);

	std::map<std::string, std::string> data;
	for (std::vector<tentry>::const_iterator it = entries_.begin(); it != entries_.end(); ++ it) {
		const tentry& entry = *it;
		data.clear();
		data.insert(std::make_pair("label", entry.label));
		ttoggle_panel& row = root.insert_row(data);
		row.set_child_icon("label", entry.icon);
	}
	root.set_did_row_changed(std::bind(&tbrowse::did_root_changed, this, _1, _2));

	did_root_changed(root, root.row_panel(0));
}

void tbrowse::reload_navigate()
{
	VALIDATE(!current_dir_.empty(), null_str);

	std::vector<std::string> ids;

	if (current_dir_ != path_delim_str) {
		ids = utils::split(current_dir_, path_delim);

		if (current_dir_.at(0) == path_delim) {
			// it is prefix by '/'.
			ids.insert(ids.begin(), path_delim_str);
		}

	} else {
		ids.push_back(path_delim_str);
	}

	int childs = navigate_->items();
	while (childs > (int)ids.size()) {
		// require reverse erase.
		navigate_->erase_item(nposm);
		childs = navigate_->items();
	}
	
	cookie_paths_.clear();

	int n = 0;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it, n ++) {
		const std::string& label = *it;
		if (n < childs) {
			gui2::tbutton* widget = dynamic_cast<tbutton*>(&navigate_->item(n));
			VALIDATE(widget->label() == label, null_str);

		} else {
			// password_char_(UCS2_to_UTF8(0x25b6)), 0x25ba
			navigate_->insert_item(null_str, label);
		}
		if (!cookie_paths_.empty()) {
			if (cookie_paths_.back() != path_delim_str) {
				cookie_paths_.push_back(cookie_paths_.back() + path_delim_str + label);
			} else {
				cookie_paths_.push_back(cookie_paths_.back() + label);
			}
		} else {
			cookie_paths_.push_back(label);
		}
	}

	goto_higher_->set_active(ids.size() >= 2);
}

void tbrowse::click_navigate(twindow& window, tbutton& widget)
{
	if (only_extra_entry_ && cookie_paths_[widget.at()].size() < param_.extra.path.size()) {
		return;
	}
	current_dir_ = cookie_paths_[widget.at()];
	update_file_lists();

	reload_navigate();
}

void tbrowse::did_root_changed(tlistbox& list, ttoggle_panel& widget)
{
	// reset navigate
	navigate_->clear();

	current_dir_ = entries_[widget.at()].path;
	update_file_lists();

	reload_navigate();
}

static const std::string& get_browse_icon(bool dir)
{
	static const std::string dir_icon = "misc/folder.png";
	static const std::string file_icon = "misc/file.png";
	return dir? dir_icon: file_icon;
}

void tbrowse::add_row(twindow& window, tlistbox& list, const std::string& name, bool dir)
{
	std::map<std::string, std::string> list_item_item;

	list_item_item.insert(std::make_pair("name", name));

	list_item_item.insert(std::make_pair("date", "---"));

	list_item_item.insert(std::make_pair("size", dir? null_str: "---"));

	ttoggle_panel& row = list.insert_row(list_item_item);
	row.set_child_icon("name", get_browse_icon(dir));

	if (dir) {
		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&tbrowse::signal_handler_longpress_item
				, this
				, _4, _5, std::ref(row))
				, event::tdispatcher::back_child);
		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&tbrowse::signal_handler_longpress_item
				, this
				, _4, _5, std::ref(row))
				, event::tdispatcher::back_post_child);
	}
}

void tbrowse::click_directory(ttoggle_panel& row)
{
	std::vector<gui2::tmenu::titem> items;
	std::string message;

	enum {select};
	
	items.push_back(gui2::tmenu::titem(_("Select"), select, "misc/select.png"));

	int selected;
	{
		int x, y;
		SDL_GetMouseState(&x, &y);
		gui2::tmenu dlg(items, nposm);
		dlg.show(x, y + 16 * twidget::hdpi_scale);
		// int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		selected = dlg.selected_val();
	}
	if (selected == select) {
		tlistbox* list = find_widget<tlistbox>(window_, "default", false, true);
		tauto_select_lock lock(*this);
		list->select_row(row.at());
	}
}

void tbrowse::signal_handler_longpress_item(bool& halt, const tpoint& coordinate, ttoggle_panel& row)
{
	halt = true;

	// network maybe disconnect except.
	rtc::Thread::Current()->Post(RTC_FROM_HERE, this, tbrowse::MSG_POPUP_DIRECTORY, reinterpret_cast<rtc::MessageData*>(&row));
}

void tbrowse::app_OnMessage(rtc::Message* msg)
{
	ttoggle_panel* row = nullptr;
	switch (msg->message_id) {
	case MSG_POPUP_DIRECTORY:
		row = reinterpret_cast<ttoggle_panel*>(msg->pdata);
		click_directory(*row);
		break;

	case MSG_OPEN_DIRECTORY:
		row = reinterpret_cast<ttoggle_panel*>(msg->pdata);
		open(*window_, row->at());
		break;
	}
}

void tbrowse::reload_file_table(int cursel)
{
	tlistbox* list = find_widget<tlistbox>(window_, "default", false, true);
	list->clear();

	int size = int(dirs_in_current_dir_.size() + files_in_current_dir_.size());
	for (std::set<tfile2>::const_iterator it = dirs_in_current_dir_.begin(); it != dirs_in_current_dir_.end(); ++ it) {
		const tfile2& file = *it;
		add_row(*window_, *list, file.name, true);

	}
	for (std::set<tfile2>::const_iterator it = files_in_current_dir_.begin(); it != files_in_current_dir_.end(); ++ it) {
		const tfile2& file = *it;
		add_row(*window_, *list, file.name, false);
	}
	if (size) {
		if (cursel >= size) {
			cursel = size - 1;
		}
		// tauto_select_lock lock(*this);
		// list->select_row(cursel);
	}
	set_filename(null_str);
}

void tbrowse::update_file_lists()
{
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();

	std::vector<std::string> files, dirs;
	get_files_in_dir(current_dir_, &files, &dirs);

	// files and dirs of get_files_in_dir returned are unicode16 format
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& str = *it;
		files_in_current_dir_.insert(str);
	}
	for (std::vector<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
		const std::string& str = *it;
		dirs_in_current_dir_.insert(str);
	}

	reload_file_table(0);
}

void tbrowse::open(twindow& window, const int at)
{
	std::set<tfile2>::const_iterator it = dirs_in_current_dir_.begin();
	std::advance(it, at);

	if (current_dir_ != path_delim_str) {
		current_dir_ = current_dir_ + path_delim + it->name;
	} else {
		current_dir_ = current_dir_ + it->name;
	}
	update_file_lists();

	reload_navigate();
}

void tbrowse::goto_higher()
{
	if (only_extra_entry_ && current_dir_ == param_.extra.path) {
		return;
	}

	VALIDATE(cookie_paths_.size() >= 2, null_str);
	SDL_Log("goto_higher, cookie_paths_.back: %s, current_dir_: %s", cookie_paths_.back().c_str(), current_dir_.c_str());

	VALIDATE(cookie_paths_.back() == current_dir_, null_str);

	current_dir_ = cookie_paths_[cookie_paths_.size() - 2];
	update_file_lists();

	reload_navigate();
}

std::string tbrowse::get_path(const std::string& file_or_dir) const 
{
	std::string res_path = utils::normalize_path(file_or_dir);

	// get rid of all path_delim at end.
	size_t s = res_path.size();
	while (s > 1 && res_path.at(s - 1) == path_delim) {
		res_path.erase(s - 1);
		s = res_path.size();
	}

	if (!::is_directory(res_path)) {
		size_t index = file_or_dir.find_last_of(path_delim);
		if (index != std::string::npos) {
			res_path = res_path.substr(0, index);
		}
	}
	return res_path;
}

void tbrowse::did_item_selected(tlistbox& list, ttoggle_panel& widget)
{
	bool dir = false;

	std::set<tfile2>::const_iterator it;
	int row = widget.at();
	if (row < (int)dirs_in_current_dir_.size()) {
		dir = true;
		it = dirs_in_current_dir_.begin();
		if (game_config::mobile && !auto_select_) {
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, tbrowse::MSG_OPEN_DIRECTORY, reinterpret_cast<rtc::MessageData*>(&widget));
			return;
		}

	} else {
		it = files_in_current_dir_.begin();
		row -= dirs_in_current_dir_.size();
	}
	std::advance(it, row);

	set_filename(it->name);
}

void tbrowse::did_item_double_click(tlistbox& list, ttoggle_panel& widget)
{
	int row = widget.at();
	if (row >= (int)dirs_in_current_dir_.size()) {
		return;
	}

	open(*window_, row);
}

void tbrowse::text_changed_callback(ttext_box& widget)
{
	const std::string& label = widget.label();
	bool active = !label.empty();
	if (active) {
		const std::string path = calculate_result();
		if (SDL_IsFile(path.c_str())) {
			if (!(param_.flags & TYPE_FILE)) {
				active = false;
			}
		} else if (SDL_IsDirectory(path.c_str())) {
			if (!(param_.flags & TYPE_DIR)) {
				active = false;
			}
		}
		if (active && did_result_changed_) {
			active = did_result_changed_(calculate_result(), label);
		}
	}
	ok_->set_active(active);
}

void tbrowse::set_filename(const std::string& label)
{
	filename_->set_label(label);
}

}
