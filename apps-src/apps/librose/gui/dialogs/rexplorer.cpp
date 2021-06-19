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

#include "gui/dialogs/rexplorer.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "font.hpp"

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
#include "gui/widgets/stack.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"

// std::tolower
#include <cctype>

namespace gui2 {

REGISTER_DIALOG(rose, rexplorer)

const char trexplorer::path_delim = '/';
const std::string trexplorer::path_delim_str = "/";

trexplorer::tfile2::tfile2(const std::string& _name, bool dir)
	: name(_name)
	, dir(dir)
	, method(trexplorer::METHOD_ALPHA)
	, lower(utils::lowercase(_name))
{
}

bool trexplorer::tfile2::operator<(const trexplorer::tfile2& that) const 
{
	return lower < that.lower; 
}


trexplorer::trexplorer(tslot& slot, const std::string& initial, const tentry& extra, bool only_extra_entry, bool click_open_dir)
	: slot_(slot)
	, support_longpress_(true)
	, send_helper_(this)
	, initial_(initial)
	, extra_(extra)
	, only_extra_entry_(only_extra_entry)
	, navigate_(nullptr)
	, goto_higher_(nullptr)
	, file_list_(nullptr)
	, summary_(nullptr)
	, main_stack_(nullptr)
	, explorer_grid_(nullptr)
	, lt_stack_(nullptr)
	, current_dir_(get_path(initial))
	, click_open_dir_(click_open_dir)
{
	slot_.rexplorer_ = this;
	set_timer_interval(800);

	if (extra_.valid()) {
		extra_.path = get_path(extra_.path);
		VALIDATE(SDL_IsDirectory(extra_.path.c_str()), null_str);
	}
	if (only_extra_entry_) {
		VALIDATE(extra_.valid(), null_str);
	}
	if (!is_directory(current_dir_)) {
		if (!only_extra_entry_) {
			current_dir_ = get_path(game_config::preferences_dir);
		} else {
			current_dir_ = extra_.path;
		}
	}
}

void trexplorer::pre_show()
{
	// window_->set_label("misc/white_background.png");
	window_->set_label("misc/black_background.png");
	// window_->set_label("misc/decorate_background.png");
	// window_->set_label("misc/red_background.png");

	main_stack_ = find_widget<tstack>(window_, "main_stack", false, true);
	explorer_grid_ = main_stack_->layer(EXPLORER_LAYER);

	lt_stack_ = find_widget<tstack>(explorer_grid_, "lt_stack", false, true);
	pre_lt_cancel(*lt_stack_->layer(LT_CANCEL_LAYER));

	tbutton* button = find_widget<tbutton>(explorer_grid_, "multiselect", false, true);
	button->set_icon("misc/multiselect.png");
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trexplorer::click_multiselect
				, this
				, std::ref(*button)));

	button = find_widget<tbutton>(explorer_grid_, "edit", false, true);
	button->set_icon("misc/edit.png");
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trexplorer::click_edit
				, this
				, std::ref(*button)));

	button = find_widget<tbutton>(explorer_grid_, "new_folder", false, true);
	button->set_icon("misc/new-folder.png");
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&trexplorer::click_new_folder
				, this
				, std::ref(*button)));

	summary_  = find_widget<tlabel>(explorer_grid_, "summary", false, true);

	goto_higher_ = find_widget<tbutton>(explorer_grid_, "goto-higher", false, true);
	connect_signal_mouse_left_click(
			  *goto_higher_
			, std::bind(
				&trexplorer::goto_higher
				, this));

	find_widget<tpanel>(explorer_grid_, "navigate_panel", false, true)->set_border("textbox");

	navigate_ = find_widget<treport>(explorer_grid_, "navigate", false, true);
	navigate_->set_did_item_click(std::bind(&trexplorer::click_navigate, this, std::ref(*window_), _2));

	tlistbox& list = find_widget<tlistbox>(explorer_grid_, "default", false);
	if (!click_open_dir_) {
		list.set_did_row_double_click(std::bind(&trexplorer::did_item_double_click, this, _1, _2));
	}
	list.set_did_row_changed(std::bind(&trexplorer::did_item_selected, this, _1, _2));
	window_->keyboard_capture(&list);
	file_list_ = &list;

	init_entry(*window_);

	slot_.window_ = window_;
	slot_.main_stack_ = main_stack_;
	slot_.file_list_ = file_list_;
	slot_.explorer_grid_ = explorer_grid_;
	slot_.lt_stack_ = lt_stack_;
	slot_.rexplorer_pre_show(*window_);
}

void trexplorer::post_show()
{
	slot_.rexplorer_post_show();
}

void trexplorer::pre_lt_cancel(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "cancel", false, true);
	button->set_icon("misc/back.png");
}

void trexplorer::init_entry(twindow& window)
{
	if (!only_extra_entry_) {
		entries_.push_back(tentry(game_config::preferences_dir, _("UserData"), "misc/documents.png"));

		if (game_config::os == os_windows) {
			entries_.push_back(tentry("c:", _("C:"), "misc/disk.png"));
		} else if (game_config::os != os_ios) {
			entries_.push_back(tentry("/", _("Device"), "misc/disk.png"));
		}
		if (game_config::os != os_android) {
			entries_.push_back(tentry(game_config::path, _("Sandbox"), "misc/dir_res.png"));
		}
	}
	if (extra_.valid()) {
		entries_.push_back(extra_);
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
	root.set_did_row_changed(std::bind(&trexplorer::did_root_changed, this, _1, _2));

	did_root_changed(root, root.row_panel(0));
}

void trexplorer::reload_navigate()
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

void trexplorer::click_navigate(twindow& window, tbutton& widget)
{
	if (only_extra_entry_ && cookie_paths_[widget.at()].size() < extra_.path.size()) {
		return;
	}
	current_dir_ = cookie_paths_[widget.at()];
	update_file_lists();

	reload_navigate();
}

void trexplorer::did_root_changed(tlistbox& list, ttoggle_panel& widget)
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

void trexplorer::add_row(twindow& window, tlistbox& list, const std::string& name, bool dir)
{
	std::map<std::string, std::string> list_item_item;

	list_item_item.insert(std::make_pair("name", name));

	list_item_item.insert(std::make_pair("date", "---"));

	list_item_item.insert(std::make_pair("size", dir? null_str: "---"));

	ttoggle_panel& row = list.insert_row(list_item_item);
	row.set_child_icon("name", get_browse_icon(dir));

	if (support_longpress_) {
		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&trexplorer::signal_handler_longpress_item
				, this
				, _4, _5, std::ref(row))
				, event::tdispatcher::back_child);
		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&trexplorer::signal_handler_longpress_item
				, this
				, _4, _5, std::ref(row))
				, event::tdispatcher::back_post_child);
	}
}

void trexplorer::click_directory(ttoggle_panel& row)
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
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		selected = dlg.selected_val();
	}

	if (selected == select) {
		tlistbox* list = find_widget<tlistbox>(window_, "default", false, true);
		int ii = 0;
		// tauto_select_lock lock(*this);
		// list->select_row(row.at());
	}

}

void trexplorer::app_first_drawn()
{
	slot_.rexplorer_first_drawn();
}

void trexplorer::app_resize_screen()
{
	slot_.rexplorer_resize_screen();
}

void trexplorer::app_handler_mouse_motion(const tpoint& coordinate)
{
	slot_.rexplorer_handler_mouse_motion(coordinate.x, coordinate.y);
}

void trexplorer::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_OPEN_DIRECTORY:
	{
		tmsg_open_directory* pdata = static_cast<tmsg_open_directory*>(msg->pdata);
		open(*window_, pdata->row);
		break;
	}
	default:
		slot_.rexplorer_OnMessage(msg);
	}
	if (msg->pdata) {
		delete msg->pdata;
		msg->pdata = nullptr;
	}
}

void trexplorer::reload_file_table(int cursel)
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
	set_summary_label();
}

void trexplorer::update_file_lists()
{
	canel_multiselect();
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();

	std::vector<std::string> files, dirs;
	get_files_in_dir(current_dir_, &files, &dirs);

	// files and dirs of get_files_in_dir returned are unicode16 format
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& str = *it;
		files_in_current_dir_.insert(tfile2(str, false));
	}
	for (std::vector<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
		const std::string& str = *it;
		dirs_in_current_dir_.insert(tfile2(str, true));
	}

	reload_file_table(0);
}

void trexplorer::open(twindow& window, const int at)
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

void trexplorer::goto_higher()
{
	if (only_extra_entry_ && current_dir_ == extra_.path) {
		return;
	}

	VALIDATE(cookie_paths_.size() >= 2, null_str);
	SDL_Log("goto_higher, cookie_paths_.back: %s, current_dir_: %s", cookie_paths_.back().c_str(), current_dir_.c_str());

	VALIDATE(cookie_paths_.back() == current_dir_, null_str);

	current_dir_ = cookie_paths_[cookie_paths_.size() - 2];
	update_file_lists();

	reload_navigate();
}

std::string trexplorer::get_path(const std::string& file_or_dir) const 
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

void trexplorer::did_item_selected(tlistbox& list, ttoggle_panel& widget)
{
	bool dir = false;

	std::set<tfile2>::const_iterator it;
	int row = widget.at();
	if (row < (int)dirs_in_current_dir_.size()) {
		dir = true;
		it = dirs_in_current_dir_.begin();

		if (!list.multiselect() && click_open_dir_) {
			tmsg_open_directory* pdata = new tmsg_open_directory(widget.at());
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, trexplorer::MSG_OPEN_DIRECTORY, pdata);
			return;
		}
	} else {
		it = files_in_current_dir_.begin();
		row -= dirs_in_current_dir_.size();
	}
	std::advance(it, row);
}

std::string trexplorer::row_full_name(ttoggle_panel& widget, int* type_ptr)
{
	bool dir = false;

	std::set<tfile2>::const_iterator it;
	int row = widget.at();
	if (row < (int)dirs_in_current_dir_.size()) {
		dir = true;
		it = dirs_in_current_dir_.begin();

	} else {
		it = files_in_current_dir_.begin();
		row -= dirs_in_current_dir_.size();
	}
	std::advance(it, row);

	const std::string ret = current_dir_ + path_delim + it->name;
	if (type_ptr) {
		*type_ptr = dir? TYPE_DIR: TYPE_FILE;
	}
	return ret;
}

std::string trexplorer::selected_full_name(int* type_ptr)
{
	ttoggle_panel* cursel = file_list_->cursel();
	VALIDATE(cursel != nullptr, null_str);

	return row_full_name(*cursel, type_ptr);
}

std::vector<std::string> trexplorer::selected_full_multinames()
{
	VALIDATE(file_list_->multiselect(), null_str);
	const std::set<int>& rows = file_list_->multiselected_rows();
	VALIDATE(!rows.empty(), null_str);

	std::vector<std::string> ret;
	for (std::set<int>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
		int row_at = *it;
		// ttoggle_panel& row = file_list_->row_panel(row_at);
		bool dir = false;
		std::set<tfile2>::const_iterator it2;
		if (row_at < (int)dirs_in_current_dir_.size()) {
			dir = true;
			it2 = dirs_in_current_dir_.begin();

		} else {
			it2 = files_in_current_dir_.begin();
			row_at -= dirs_in_current_dir_.size();
		}
		std::advance(it2, row_at);
		ret.push_back(current_dir_ + path_delim + it2->name);
	}
	return ret;
}

trexplorer::tfile2 trexplorer::row_2_file2(int row) const
{
	VALIDATE(row >= 0 && row < file_list_->rows(), null_str);

	std::set<tfile2>::const_iterator it;
	if (row < (int)dirs_in_current_dir_.size()) {
		it = dirs_in_current_dir_.begin();

	} else {
		it = files_in_current_dir_.begin();
		row -= dirs_in_current_dir_.size();
	}
	std::advance(it, row);

	return *it;
}

SDL_Point trexplorer::analysis_selected() const
{
	SDL_Point ret{0, 0};
	if (!file_list_->multiselect()) {
		ttoggle_panel* cursel = file_list_->cursel();
		if (cursel != nullptr) {
			tfile2 file = row_2_file2(cursel->at());
			if (file.dir) {
				ret.x ++;
			} else {
				ret.y ++;
			}
		}
	} else {
		const std::set<int>& rows = file_list_->multiselected_rows();
		for (std::set<int>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
			int row_at = *it;
			tfile2 file = row_2_file2(row_at);
			if (file.dir) {
				ret.x ++;
			} else {
				ret.y ++;
			}
		}
	}
	return ret;
}

void trexplorer::did_item_double_click(tlistbox& list, ttoggle_panel& widget)
{
	int row = widget.at();
	if (row >= (int)dirs_in_current_dir_.size()) {
		return;
	}

	open(*window_, row);
}

void trexplorer::set_summary_label()
{
	utils::string_map symbols;
	symbols["items"] = str_cast(files_in_current_dir_.size() + dirs_in_current_dir_.size());

	summary_->set_label(vgettext2("$items items", symbols));
}

void trexplorer::click_multiselect(tbutton& widget)
{
	click_multiselect_internal(widget);
}

void trexplorer::click_multiselect_internal(tbutton& widget)
{	
	bool cur_multiselect = file_list_->multiselect();
	if (cur_multiselect) {
		// will cancel multiselect
		file_list_->enable_multiselect(false);
		file_list_->enable_select(true);

	} else {
		// will execult multiselect
		file_list_->enable_select(false);
		file_list_->enable_multiselect(true);
	}

	widget.set_icon(file_list_->multiselect()? "misc/multiselect_selected.png": "misc/multiselect.png");
}

bool trexplorer::verify_edit_item(const std::string& label, const std::string& def, int type) const
{
	if (label.empty()) {
		return false;
	}
	if (label == def) {
		return false;
	}
	if (type == edittype_newfolder) {
		if (!utils::isvalid_filename(label)) {
			return false;
		}
		const tfile2 file2(label, true);
		if (dirs_in_current_dir_.count(file2) != 0) {
			return false;
		}
		if (files_in_current_dir_.count(file2) != 0) {
			return false;
		}
	}
	return true;
}

void trexplorer::handle_edit_item(int type)
{
	VALIDATE(type >= 0 && type < edittype_count, null_str);
	utils::string_map symbols;
	std::string title;
	std::string prefix;
	std::string placeholder;
	std::string def;
	std::string remark;
	std::string reset;
	size_t max_chars = 0;
	std::string result;
	int cancel_seconds = 0; // always show
	{
		// tpaper_keyboard_focus_lock keyboard_focus_lock(*this);
		if (type == edittype_newfolder) {
			title = _("Input floder name");
			placeholder = null_str;
			def = null_str;
			max_chars = 20;

		} else {
			VALIDATE(false, null_str);
		}
		VALIDATE(max_chars > 0, null_str);
		gui2::tedit_box_param param(title, prefix, placeholder, def, remark, reset, _("OK"), max_chars, gui2::tedit_box_param::show_cancel + cancel_seconds);
		param.did_text_changed = std::bind(&trexplorer::verify_edit_item, this, _1, def, type);
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 4);
			if (dlg.get_retval() != twindow::OK) {
				return;
			}
		}
		result = param.result;
	}

	if (type == edittype_newfolder) {
		const std::string dir = current_dir_ + path_delim + result;
		SDL_bool ret = SDL_MakeDirectory(dir.c_str());
		if (!ret) {
			symbols["folder"] = result;
			gui2::show_message(null_str, vgettext2("Create $folder fail", symbols));
		}
		update_file_lists();

	} else {
		VALIDATE(false, null_str);
	}
}

void trexplorer::click_new_folder(tbutton& widget)
{
	handle_edit_item(edittype_newfolder);
}

void trexplorer::canel_multiselect()
{
	if (file_list_->multiselect()) {
		click_multiselect_internal(*find_widget<tbutton>(window_, "multiselect", false, true));
	}
}

void trexplorer::click_edit(tbutton& widget)
{
	slot_.rexplorer_click_edit(widget);
}

void trexplorer::app_timer_handler(uint32_t now)
{
	slot_.rexplorer_timer_handler(now);
}

//
// drag-copy
//

surface trexplorer::get_drag_surface(int files, bool has_folder, const std::string& path)
{
	VALIDATE(files > 0, null_str);

	uint32_t key = posix_mku32(files, posix_mku16(path.empty()? 1: 0, has_folder? 1: 0));
	if (drag_surfs_.count(key) != 0) {
		return drag_surfs_.find(key)->second;
	}

	const int icon_size = 64 * twidget::hdpi_scale;
	surface bg_surf = create_neutral_surface(icon_size, icon_size);
	VALIDATE(bg_surf.get(), null_str);

	// const uint32_t bg_color = 0xffffc04d;
	const uint32_t bg_color = path.empty()? 0xff800000: 0xff008000;
	SDL_Rect dst_rect{0, 0, bg_surf->w, bg_surf->h};
	sdl_fill_rect(bg_surf, &dst_rect, bg_color);

	const std::string image_png = has_folder? "misc/folder.png": "misc/file.png";
	surface surf = image::get_image(image_png);
	surf = scale_surface(surf, icon_size, icon_size);

	dst_rect = ::create_rect(0, 0, surf->w, surf->h);
	sdl_blit(surf, nullptr, bg_surf, &dst_rect);

	surf = font::get_rendered_text(str_cast(files), 0, font::SIZE_LARGEST, font::BLACK_COLOR);
	dst_rect = ::create_rect(0, 0, surf->w, surf->h);
	sdl_blit(surf, nullptr, bg_surf, &dst_rect);

	drag_surfs_.insert(std::make_pair(key, bg_surf));
	return bg_surf;
}

void trexplorer::start_drag2(const tpoint& coordinate, int files, bool has_folder)
{
	VALIDATE(files > 0, null_str);
	surface surf = get_drag_surface(files, has_folder, null_str);

	window_->set_drag_surface(surf, false);

	window_->start_drag(coordinate, std::bind(&trexplorer::did_drag_mouse_motion, this, _1, _2),
		std::bind(&trexplorer::did_drag_mouse_leave, this, _1, _2, _3));

	slot_.rexplorer_drag_started(coordinate.x, coordinate.y);
}

void trexplorer::signal_handler_longpress_item(bool& halt, const tpoint& coordinate, ttoggle_panel& widget)
{
	halt = true;
	slot_.rexplorer_handler_longpress_item(coordinate, widget);
}

bool trexplorer::did_drag_mouse_motion(const int x, const int y)
{
	VALIDATE(window_->draging(), null_str);

	slot_.rexplorer_did_drag_mouse_motion(x, y);
/*
	tgrid& explorer_grid = *main_stack_->layer(EXPLORER_LAYER);
	SDL_Rect rect = explorer_grid.get_rect();
	if (point_in_rect(x, y, rect)) {
		if (!window_->is_drag_surface_visible()) {
			window_->set_drag_surface_visible(true);
		}
	} else if (window_->is_drag_surface_visible()) {
		window_->set_drag_surface_visible(false);
	}
*/
/*
	tcontrol* icon = find_widget<tcontrol>(window_, "userdata", false, true);
	bool hit = point_in_rect(x, y, icon->get_rect());
	set_remark_label(hit);
*/
	return true;
}

void trexplorer::did_drag_mouse_leave(const int x, const int y, bool up_result)
{
	VALIDATE(!window_->draging(), null_str);

	slot_.rexplorer_drag_stoped(x, y, up_result);
	drag_surfs_.clear();
	// set_remark_label(false);
}

}
