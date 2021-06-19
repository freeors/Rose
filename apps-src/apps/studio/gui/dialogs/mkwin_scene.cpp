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
#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include "formula_string_utils.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "hotkeys.hpp"

#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, mkwin_scene);

tmkwin_scene::tmkwin_scene(mkwin_display& disp, mkwin_controller& controller)
	: tdialog(&controller)
	, controller_(controller)
	, current_widget_page_(nposm)
{
	tdomains = controller.get_app_tdomains();
}

void tmkwin_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(UNIT_NAME, "unit_name"));
	reports_.insert(std::make_pair(UNIT_TYPE, "unit_type"));
	reports_.insert(std::make_pair(UNIT_STATUS, "unit_status"));
	reports_.insert(std::make_pair(UNIT_HP, "unit_hp"));
	reports_.insert(std::make_pair(UNIT_XP, "unit_xp"));
	reports_.insert(std::make_pair(UNIT_SECOND, "unit_second"));
	reports_.insert(std::make_pair(UNIT_IMAGE, "unit_image"));
	reports_.insert(std::make_pair(TIME_OF_DAY, "time_of_day"));
	reports_.insert(std::make_pair(TURN, "turn"));
	reports_.insert(std::make_pair(GOLD, "gold"));
	reports_.insert(std::make_pair(VILLAGES, "villages"));
	reports_.insert(std::make_pair(UPKEEP, "upkeep"));
	reports_.insert(std::make_pair(INCOME, "income"));
	reports_.insert(std::make_pair(TECH_INCOME, "tech_income"));
	reports_.insert(std::make_pair(TACTIC, "tactic"));
	reports_.insert(std::make_pair(POSITION, "position"));
	reports_.insert(std::make_pair(STRATUM, "stratum"));
	reports_.insert(std::make_pair(MERITORIOUS, "meritorious"));
	reports_.insert(std::make_pair(SIDE_PLAYING, "side_playing"));
	reports_.insert(std::make_pair(OBSERVERS, "observers"));
	reports_.insert(std::make_pair(REPORT_COUNTDOWN, "report_countdown"));
	reports_.insert(std::make_pair(REPORT_CLOCK, "report_clock"));
	reports_.insert(std::make_pair(EDITOR_SELECTED_TERRAIN, "selected_terrain"));
	reports_.insert(std::make_pair(EDITOR_LEFT_BUTTON_FUNCTION, "edit_left_button_function"));
	reports_.insert(std::make_pair(EDITOR_TOOL_HINT, "editor_tool_hint"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_STATUS, "status", _("Status"));
	hotkey::insert_hotkey(HOTKEY_RCLICK, "rclick", _("Right Click"));

	std::stringstream err;
	utils::string_map symbols;
	symbols["window"] = controller_.preview()? _("scene"): _("dialog");

	hotkey::insert_hotkey(HOTKEY_BUILD, "build", _("Build gui.bin"));
	hotkey::insert_hotkey(HOTKEY_PREVIEW, "preview", _("Verify scripts"));
	hotkey::insert_hotkey(HOTKEY_SETTING, "settings", _("Settings"));
	hotkey::insert_hotkey(HOTKEY_SPECIAL_SETTING, "special_setting", _("Special Setting"));
	hotkey::insert_hotkey(HOTKEY_LINKED_GROUP, "linked_group", _("Linked group"));
	hotkey::insert_hotkey(HOTKEY_ERASE, "erase", _("Erase"));

	hotkey::insert_hotkey(HOTKEY_INSERT_TOP, "insert_top", _("Insert top"));
	hotkey::insert_hotkey(HOTKEY_INSERT_BOTTOM, "insert_bottom", _("Inert bottom"));
	hotkey::insert_hotkey(HOTKEY_ERASE_ROW, "erase_row", _("Erase row"));
	hotkey::insert_hotkey(HOTKEY_INSERT_LEFT, "insert_left", _("Insert left"));
	hotkey::insert_hotkey(HOTKEY_INSERT_RIGHT, "insert_right", _("Insert right"));
	hotkey::insert_hotkey(HOTKEY_ERASE_COLUMN, "erase_column", _("Erase column"));

	hotkey::insert_hotkey(HOTKEY_INSERT_CHILD, "insert_child", _("Insert child"));
	hotkey::insert_hotkey(HOTKEY_ERASE_CHILD, "erase_child", _("Erase child"));

	hotkey::insert_hotkey(HOTKEY_UNPACK, "unpack", _("Unpack"));
	hotkey::insert_hotkey(HOTKEY_PACK, "pack", _("Pack"));

	// widget page
	tbutton* widget = dynamic_cast<tbutton*>(get_object("status"));
	widget->set_blits(gui2::tformula_blit("buttons/status.png", null_str, null_str, "(width)", "(height)"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("rclick"));
	widget->set_label("misc/rclick.png");
	click_generic_handler(*widget, null_str);

	std::stringstream ss;
	ss.str("");
	tlabel* label = dynamic_cast<tlabel*>(get_object("title"));
	if (controller_.preview()) {
		ss << _("Theme");
	} else {
		ss << _("Dialog");
	}
	label->set_label(ss.str());

	tlistbox* list = dynamic_cast<tlistbox*>(get_object("object-list"));
	list->set_did_row_changed(std::bind(&tmkwin_scene::object_selected, this, std::ref(*window_), _1));

	page_panel_ = find_widget<tstack>(window_, "page_panel", false, true);
	load_widget_page();

	context_menu_panel_ = find_widget<tstack>(window_, "context_menu_panel", false, true);
	context_menu_panel_->set_radio_layer(LAYER_CONTEXT_MENU);

	treport* report = find_widget<treport>(window_, "scroll-header", false, true);
	report->set_did_item_changed(std::bind(&mkwin_controller::did_scroll_header_item_changed, &controller_, _2));

	tbuild::pre_show(find_widget<ttrack>(context_menu_panel_, "task_status", false));
}

void tmkwin_scene::did_first_layouted(twindow& window)
{
	if (get_object("_float_lb")) {
		window.find_float_widget("_float_lb")->set_visible(true);
	}
}

void tmkwin_scene::load_widget_page()
{
	page_panel_->set_radio_layer(WIDGET_PAGE);
	current_widget_page_ = WIDGET_PAGE;
}

void tmkwin_scene::load_object_page(const unit_map& units)
{
	page_panel_->set_radio_layer(OBJECT_PAGE);
	current_widget_page_ = OBJECT_PAGE;

	fill_object_list(units);
}

void tmkwin_scene::object_selected(twindow& window, tlistbox& list)
{
	ttoggle_panel* panel = list.cursel();
	if (!panel) {
		return;
	}

	unit* u = reinterpret_cast<unit*>(panel->cookie());
	controller_.select_unit(*u);
}

void tmkwin_scene::fill_object_list(const unit_map& units)
{
	if (current_widget_page_ != OBJECT_PAGE) {
		return;
	}

	tlistbox* list = dynamic_cast<tlistbox*>(get_object("object-list"));
	list->clear();

	const map_location& selected_hex = controller_.selected_hex();
	std::stringstream ss;
	int index = 0;
	int cursel = 0;
	for (unit_map::const_iterator it = units.begin(); it != units.end(); ++ it) {
		unit& u = *dynamic_cast<unit*>(&*it);
		if (u.type() != unit::WIDGET) {
			continue;
		}
		if (u.get_location() == selected_hex) {
			cursel = index;
		}
		std::map<std::string, std::string> list_item_item;

		ss.str("");
		ss << (index ++);
		ss << "(" << ht::generate_format(u.get_map_index(), color_to_uint32(font::BLUE_COLOR));
		list_item_item.insert(std::make_pair("number", ss.str()));

		list_item_item.insert(std::make_pair("id", u.cell().id));

		ss.str("");
		const SDL_Rect& rect = u.get_rect();
		// ss << rect.x << "," << rect.y << "," << rect.w << "," << rect.h;
		ss << rect.x << "," << rect.y;
		list_item_item.insert(std::make_pair("rect", ss.str()));

		twidget& panel = list->insert_row(list_item_item);

		panel.set_cookie(reinterpret_cast<size_t>(&u));
	}
	if (list->rows()) {
		list->select_row(cursel);
	}
}

bool tmkwin_scene::require_build()
{
	std::vector<teditor_::BIN_TYPE> system_bins;
	system_bins.push_back(teditor_::GUI);
	editor_.get_wml2bin_desc_from_wml(system_bins);

	std::vector<std::pair<teditor_::BIN_TYPE, teditor_::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	VALIDATE(descs.size() == 1 && descs[0].first == teditor_::GUI, null_str);
	teditor_::wml2bin_desc& desc = descs[0].second;

	return desc.wml_nfiles != desc.bin_nfiles || desc.wml_sum_size != desc.bin_sum_size || desc.wml_modified != desc.bin_modified;
}

void tmkwin_scene::do_build()
{
	VALIDATE(editor_.wml2bin_descs().size() == 1, null_str);

	tbuild::do_build2();
}

void tmkwin_scene::app_work_start()
{
	std::vector<std::pair<teditor_::BIN_TYPE, teditor_::wml2bin_desc> >& descs = editor_.wml2bin_descs();
	VALIDATE(descs.size() == 1 && descs[0].first == teditor_::GUI, null_str);
	teditor_::wml2bin_desc& desc = descs[0].second;
	desc.require_build = true;

	context_menu_panel_->set_radio_layer(LAYER_TASK_STATUS);
}

void tmkwin_scene::app_work_done()
{
	context_menu_panel_->set_radio_layer(LAYER_CONTEXT_MENU);
	controller_.gui().show_context_menu();
}

} //end namespace gui2
