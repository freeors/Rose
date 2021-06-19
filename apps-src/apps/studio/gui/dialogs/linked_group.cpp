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

#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/linked_group.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "unit.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"

// std::tolower
#include <cctype>

using namespace std::placeholders;

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

REGISTER_DIALOG(studio, linked_group2)

tlinked_group2::tlinked_group2(const unit& u, const std::vector<tlinked_group>& linked_groups)
	: u_(u)
	, orignal_(linked_groups)
	, linked_groups_(linked_groups)
{
}

void tlinked_group2::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	std::stringstream ss;
	utils::string_map symbols;

	symbols["owner"] = u_.is_listbox()? "Listbox(" + u_.cell().id + ")": "Window";

	ss.str("");
	ss << vgettext2("$owner|'s Linked group", symbols); 
	find_widget<tlabel>(window_, "title", false).set_label(ss.str());

	tlistbox& list = find_widget<tlistbox>(window_, "default", false);
	// window.keyboard_capture(&list);


	std::map<std::string, utils::string_map> data;

	reload_linked_group_table(*window_, 0);
	list.set_did_row_changed(std::bind(&tlinked_group2::item_selected, this, std::ref(*window_), _1));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "append", false)
			, std::bind(
				&tlinked_group2::click_append
				, this
				, std::ref(*window_)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "modify", false)
			, std::bind(
				&tlinked_group2::modify
				, this
				, std::ref(*window_)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(window_, "delete", false)
			, std::bind(
				&tlinked_group2::erase
				, this
				, std::ref(*window_)));

}

void tlinked_group2::reload_linked_group_table(twindow& window, int cursel)
{
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	int index = 0;
	for (std::vector<tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it) {
		const tlinked_group& linked = *it;

		std::map<std::string, std::string> list_item_item;

		list_item_item.insert(std::make_pair("number", str_cast(index ++)));

		list_item_item.insert(std::make_pair("id", linked.id));

		list_item_item.insert(std::make_pair("width", linked.fixed_width? "yes": "-"));

		list_item_item.insert(std::make_pair("height", linked.fixed_height? "yes": "-"));

		list->insert_row(list_item_item);
	}
	if (cursel >= (int)linked_groups_.size()) {
		cursel = (int)linked_groups_.size() - 1;
	}
	if (!linked_groups_.empty()) {
		list->select_row(cursel);
	}

	map_linked_group_to(window, cursel);
}

std::string tlinked_group2::get_id(twindow& window, int exclude)
{
	std::string id = find_widget<ttext_box>(&window, "_id", false, true)->label();
	if (!utils::isvalid_id(id, true, min_id_len, max_id_len)) {
		gui2::show_message("", utils::errstr);
		return null_str;
	}
	utils::lowercase2(id);

	int index = 0;
	for (std::vector<tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it, index ++) {
		if (index == exclude) {
			continue;
		}
		if (id == linked_groups_[index].id) {
			std::stringstream err;
			err << _("String is using.");
			gui2::show_message("", err.str());
			return null_str;
		}
	}
	return id;
}

bool tlinked_group2::verify_other_param(bool width, bool height)
{
	if (!width && !height) {
		std::stringstream err;
		err << _("Both width and height must not be all No!");
		gui2::show_message("", err.str());
		return false;
	}
	return true;
}

void tlinked_group2::map_linked_group_to(twindow& window, int index)
{
	static const tlinked_group null_linked_group;

	bool valid = !linked_groups_.empty();
	const tlinked_group& linked = valid? linked_groups_[index]: null_linked_group;
	
	ttext_box* id = find_widget<ttext_box>(&window, "_id", false, true);
	id->set_label(linked.id);

	ttoggle_button* width = find_widget<ttoggle_button>(&window, "_width", false, true);
	width->set_value(linked.fixed_width);

	ttoggle_button* height = find_widget<ttoggle_button>(&window, "_height", false, true);
	height->set_value(linked.fixed_height);

	find_widget<tcontrol>(&window, "modify", false, true)->set_active(valid);
	find_widget<tcontrol>(&window, "delete", false, true)->set_active(valid);
}

void tlinked_group2::click_append(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	std::string id = get_id(window, -1);
	if (id.empty()) {
		return;
	}
	bool width = find_widget<ttoggle_button>(&window, "_width", false, true)->get_value();
	bool height = find_widget<ttoggle_button>(&window, "_height", false, true)->get_value();
	if (!verify_other_param(width, height)) {
		return;
	}
	linked_groups_.push_back(tlinked_group(id, width, height));

	reload_linked_group_table(window, linked_groups_.size() - 1);
}

void tlinked_group2::modify(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.cursel()->at();
	tlinked_group& linked = linked_groups_[row];

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	std::string id = get_id(window, row);
	if (id.empty()) {
		return;
	}
	bool width = find_widget<ttoggle_button>(&window, "_width", false, true)->get_value();
	bool height = find_widget<ttoggle_button>(&window, "_height", false, true)->get_value();
	if (!verify_other_param(width, height)) {
		return;
	}
	if (id != linked.id || width != linked.fixed_width || height != linked.fixed_height) {
		linked_groups_[row] = tlinked_group(id, width, height);
		reload_linked_group_table(window, row);
	}
}

void tlinked_group2::erase(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.cursel()->at();

	std::vector<tlinked_group>::iterator it = linked_groups_.begin();
	std::advance(it, row);
	linked_groups_.erase(it);
	reload_linked_group_table(window, row);
}

void tlinked_group2::item_selected(twindow& window, tlistbox& list)
{
	int row = list.cursel()->at();
	map_linked_group_to(window, row);
}

}
