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

#include "gui/dialogs/window_setting.hpp"

#include "mkwin_controller.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/control_setting.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "unit.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, window_setting)

twindow_setting::twindow_setting(mkwin_controller& controller, const unit& u, const std::vector<std::string>& textdomains)
	: tsetting_dialog(u.cell())
	, controller_(controller)
	, u_(u)
	, textdomains_(textdomains)
	, current_page_(0)
	, menus_(controller.context_menus())
	, bar_(NULL)
	, menu_navigate_(NULL)
	, submenu_navigate_(NULL)
	, patch_bar_(NULL)
	, float_widget_at_(nposm)
{
	VALIDATE(!menus_.empty(), null_str);
}

void twindow_setting::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << _("Window setting");
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	label->set_label(ss.str());

	bar_panel_ = find_widget<tstack>(window_, "bar_panel", false, true);
	bar_panel_->set_radio_layer(BASE_PAGE);

	pre_base(*window_);
	pre_context_menu(*window_);
	pre_float_widget(*window_);

	// prepare navigate bar.
	bar_ = find_widget<treport>(window_, "bar", false, true);
	bar_->set_boddy(find_widget<twidget>(window_, "_bar_panel", false, true));
	bar_->set_did_item_pre_change(std::bind(&twindow_setting::did_item_pre_change_report, this, _1, _2, _3));
	bar_->set_did_item_changed(std::bind(&twindow_setting::did_item_changed_report, this, _1, _2));

	std::vector<std::string> labels;
	labels.push_back(_("Base"));
	labels.push_back(_("Context menu"));
	labels.push_back(_("Float widget"));
	int index = 0;
	for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
		tcontrol& widget = bar_->insert_item(null_str, *it);
		widget.set_cookie(index ++);
	}
	bar_->select_item(BASE_PAGE);
}

void twindow_setting::pre_base(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "id", false, true);
	text_box->set_label(cell_.id);

	set_app_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "app", false)
			, std::bind(
				&twindow_setting::set_app
				, this
				, std::ref(window)));

	set_orientation_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "orientation", false)
			, std::bind(
				&twindow_setting::set_orientation
				, this
				, std::ref(window)));

	// as if scene not use layout, but user maybe change scene runtime. so require prepare.
	layout_panel_ = find_widget<tstack>(&window, "layout", false, true);

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "automatic_placement", false, true);
	toggle->set_value(cell_.window.automatic_placement);
	toggle->set_did_state_changed(std::bind(&twindow_setting::automatic_placement_toggled, this, _1));

	if (cell_.window.automatic_placement) {
		swap_page(window, AUTOMATIC_PAGE, false);
	} else {
		swap_page(window, MANUAL_PAGE, false);
	}
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "set_definition", false)
			, std::bind(
				&twindow_setting::set_definition
				, this
				, std::ref(window)));

	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "_set_tile_shape", false)
			, std::bind(
				&twindow_setting::set_tile_shape
				, this
				, std::ref(window)));

	toggle = find_widget<ttoggle_button>(&window, "scene", false, true);
	toggle->set_value(cell_.window.scene);
	toggle->set_did_state_changed(std::bind(&twindow_setting::did_theme_toggled, this, _1));
	did_theme_toggled(*toggle);


	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, std::bind(
				&twindow_setting::save
				, this
				, std::ref(window)
				, _3, _4));
}

void twindow_setting::pre_context_menu(twindow& window)
{
	tbutton* button = find_widget<tbutton>(&window, "append_menu_item", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::append_menu_item
			, this
			, std::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "erase_menu_item", false)
			, std::bind(
				&twindow_setting::erase_menu_item
				, this
				, std::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "modify_menu_item", false)
			, std::bind(
				&twindow_setting::modify_menu_item
				, this
				, std::ref(window)));


	tlistbox& list = find_widget<tlistbox>(&window, "menu", false);
	list.set_did_row_changed(std::bind(&twindow_setting::item_selected, this, std::ref(window), _1));

	submenu_navigate_ = find_widget<treport>(&window, "submenu_navigate", false, true);
	submenu_navigate_->set_did_item_pre_change(std::bind(&twindow_setting::did_item_pre_change_report, this, _1, _2, _3));
	submenu_navigate_->set_did_item_changed(std::bind(&twindow_setting::did_item_changed_report, this, _1, _2));
	submenu_navigate_->set_boddy(find_widget<twidget>(&window, "submenu_panel", false, true));

	reload_submenu_navigate(menus_[0], window, NULL);
	reload_menu_table(menus_[0], window, 0);
}

void twindow_setting::pre_float_widget(twindow& window)
{
	const std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();

	tgrid* float_widget_page = bar_panel_->layer(FLOAT_WIDGET_PAGE);

	patch_bar_ = find_widget<treport>(float_widget_page, "navigate", false, true);
	patch_bar_->set_did_item_pre_change(std::bind(&twindow_setting::did_item_pre_change_report, this, _1, _2, _3));
	patch_bar_->set_did_item_changed(std::bind(&twindow_setting::did_item_changed_report, this, _1, _2));
	reload_float_widget(*patch_bar_, window);
	patch_bar_->set_boddy(find_widget<twidget>(float_widget_page, "navigate_panel", false, true));

	tbutton* button = find_widget<tbutton>(float_widget_page, "_append_patch", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::append_float_widget
			, this
			, std::ref(window)));

	button = find_widget<tbutton>(float_widget_page, "_erase_patch", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::erase_float_widget
			, this
			, std::ref(window)));
	if (float_widgets.empty()) {
		button->set_active(false);
	}

	button = find_widget<tbutton>(float_widget_page, "_rename_patch", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::rename_float_widget
			, this
			, std::ref(window)));
	if (float_widgets.empty()) {
		button->set_active(false);
	}

	button = find_widget<tbutton>(float_widget_page, "float_widget_type", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::change_float_widget_type
			, this
			, std::ref(window)));
	if (float_widgets.empty()) {
		button->set_active(false);
	}

	button = find_widget<tbutton>(float_widget_page, "float_widget", false, true);
	connect_signal_mouse_left_click(
		  *button
		, std::bind(
			&twindow_setting::edit_float_widget
			, this
			, std::ref(window)));
	if (float_widgets.empty()) {
		button->set_active(false);
	}

	ttext_box* text_box = find_widget<ttext_box>(float_widget_page, "float_widget_width_formula", false, true);
	text_box->set_placeholder(_("Usable variable: svga, vga, mobile, ref_width, ref_height"));

	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_height_formula", false, true);
	text_box->set_placeholder(_("Usable variable: svga, vga, mobile, ref_width, ref_height"));

	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_x_formula", false, true);
	text_box->set_placeholder(_("Usable variable: svga, vga, mobile, ref_width, ref_height, width, height"));

	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_y_formula", false, true);
	text_box->set_placeholder(_("Usable variable: svga, vga, mobile, ref_width, ref_height, width, height"));

	if (!float_widgets.empty()) {
		float_widget_at_ = 0;
		patch_bar_->select_item(float_widget_at_);
		switch_float_widget(window);
	}
}

bool twindow_setting::save_base(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "id", false, true);
	cell_.id = text_box->label();
	if (!utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
		show_id_error("id", utils::errstr);
		return false;
	}

	cell_.window.scene = find_widget<ttoggle_button>(&window, "scene", false, true)->get_value();

	if (cell_.window.scene) {
		VALIDATE(!cell_.window.tile_shape.empty(), null_str);
		cell_.window.definition = "null";
		cell_.window.click_dismiss = false;
		cell_.window.automatic_placement = false;
		cell_.window.x = "0";
		cell_.window.y = "0";
		cell_.window.width = "screen_width";
		cell_.window.height = "screen_height";

	} else {
		cell_.window.click_dismiss = find_widget<ttoggle_button>(&window, "click_dismiss1", false, true)->get_value();
		cell_.window.automatic_placement = find_widget<ttoggle_button>(&window, "automatic_placement", false, true)->get_value();
		if (!cell_.window.automatic_placement) {
			text_box = find_widget<ttext_box>(&window, "x", false, true);
			cell_.window.x = text_box->label();
			utils::lowercase2(cell_.window.x);
			if (!cell_.window.x.empty() && !verify_formula("x's formula", cell_.window.x)) {
				return false;
			}
			text_box = find_widget<ttext_box>(&window, "y", false, true);
			cell_.window.y = text_box->label();
			utils::lowercase2(cell_.window.y);
			if (!cell_.window.y.empty() && !verify_formula("y's formula", cell_.window.y)) {
				return false;
			}
			text_box = find_widget<ttext_box>(&window, "width", false, true);
			cell_.window.width = text_box->label();
			utils::lowercase2(cell_.window.width);
			if (!cell_.window.width.empty() && !verify_formula("width's formula", cell_.window.width)) {
				return false;
			}
			text_box = find_widget<ttext_box>(&window, "height", false, true);
			cell_.window.height = text_box->label();
			utils::lowercase2(cell_.window.height);
			if (!cell_.window.height.empty() && !verify_formula("height's formula", cell_.window.height)) {
				return false;
			}
		}
	}
	return true;
}

bool twindow_setting::save_context_menu(twindow& window)
{
	tmenu2& top = *current_submenu()->top_menu();

	if (top.items.empty()) {
		return true;
	}

	tgrid* context_menu_page = bar_panel_->layer(CONTEXT_MENU_PAGE);

	ttext_box* text_box = find_widget<ttext_box>(context_menu_page, "report_id", false, true);
	std::string report_id = text_box->label();
	utils::lowercase2(report_id);
	if (!utils::isvalid_id(report_id, false, min_id_len, max_id_len)) {
		show_id_error("report_id", utils::errstr);
		return false;
	}
	top.report = report_id;
	return true;
}

bool twindow_setting::save_float_widget(twindow& window, const int at)
{
	if (at == nposm) {
		return true;
	}

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	::tfloat_widget& item = float_widgets[at];
	tgrid* float_widget_page = bar_panel_->layer(FLOAT_WIDGET_PAGE);

	ttext_box* text_box = find_widget<ttext_box>(float_widget_page, "float_widget_ref", false, true);
	item.ref = text_box->label();
	utils::lowercase2(item.ref);
	if (!utils::isvalid_id(item.ref, false, min_id_len, max_id_len)) {
		show_id_error("ref", utils::errstr);
		return false;
	}

	// width
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_width_formula", false, true);
	item.w.first = text_box->label();
	if (!item.w.first.empty()) {
		utils::lowercase2(item.w.first);
		if (!verify_formula("width's fomula", item.w.first)) {
			return false;
		}
		item.w.first = unit::formual_fill_str(item.w.first);

		text_box = find_widget<ttext_box>(float_widget_page, "float_widget_width_offset", false, true);
		if (!utils::isinteger(text_box->label())) {
			show_id_error("width's offset", utils::errstr);
			return false;
		}
		item.w.second = atoi(text_box->label().c_str());
	}

	// height
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_height_formula", false, true);
	item.h.first = text_box->label();
	if (!item.h.first.empty()) {
		utils::lowercase2(item.h.first);
		if (!verify_formula("height's fomula", item.h.first)) {
			return false;
		}
		item.h.first = unit::formual_fill_str(item.h.first);

		text_box = find_widget<ttext_box>(float_widget_page, "float_widget_height_offset", false, true);
		if (!utils::isinteger(text_box->label())) {
			show_id_error("height's offset", utils::errstr);
			return false;
		}
		item.h.second = atoi(text_box->label().c_str());
	}

	// first x
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_x_formula", false, true);
	item.x[0].first = text_box->label();
	utils::lowercase2(item.x[0].first);
	if (!verify_formula("x's fomula", item.x[0].first)) {
		return false;
	}
	item.x[0].first = unit::formual_fill_str(item.x[0].first);

	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_x_offset", false, true);
	if (!utils::isinteger(text_box->label())) {
		show_id_error("x's offset", utils::errstr);
		return false;
	}
	item.x[0].second = atoi(text_box->label().c_str());

	// first y
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_y_formula", false, true);
	item.y[0].first = text_box->label();
	utils::lowercase2(item.y[0].first);
	if (!verify_formula("y's fomula", item.y[0].first)) {
		return false;
	}
	item.y[0].first = unit::formual_fill_str(item.y[0].first);

	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_y_offset", false, true);
	if (!utils::isinteger(text_box->label())) {
		show_id_error("y's offset", utils::errstr);
		return false;
	}
	item.y[0].second = atoi(text_box->label().c_str());

	return true;
}

bool twindow_setting::did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& to)
{
	twindow& window = *to.get_window();

	if (&report == menu_navigate_) {
		return menu_pre_toggle_tabbar(&to, &from);

	} else if (&report == submenu_navigate_) {
		return submenu_pre_toggle_tabbar(&to, &from);

	} else if (&report == patch_bar_) {
		return save_float_widget(window, float_widget_at_);
	}

	bool ret = true;
	int previous_page = from.at();
	if (previous_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (previous_page == CONTEXT_MENU_PAGE) {
		ret = save_context_menu(window);
	} else if (previous_page == FLOAT_WIDGET_PAGE) {
		return save_float_widget(window, float_widget_at_);
	}
	return ret;
}

void twindow_setting::did_item_changed_report(treport& report, ttoggle_button& widget)
{
	if (&report == menu_navigate_) {
		menu_toggle_tabbar(&widget);
		return;
	} else if (&report == submenu_navigate_) {
		submenu_toggle_tabbar(&widget);
		return;
	} else if (&report == patch_bar_) {
		patch_toggle_tabbar(widget);
		return;
	}

	int page = widget.at();
	bar_panel_->set_radio_layer(page);
}

void twindow_setting::set_tile_shape(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<std::string> ids;

	const config& core_config = controller_.app_cfg();

	int initial_at = 0;
	BOOST_FOREACH (const config &c, core_config.child_range("tb")) {
		const std::string& id = c["id"].str();
		ss.str("");
		ss << ht::generate_format(id, color_to_uint32(font::BLUE_COLOR));
		items.push_back(ss.str());
		ids.push_back(id);
		if (id == cell_.window.tile_shape) {
			initial_at = items.size() - 1;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.window.tile_shape = ids[dlg.cursel()];

	set_tile_shape_label(window);
}

void twindow_setting::set_tile_shape_label(twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "tile_shape", false, true);
	label->set_label(cell_.window.tile_shape);
}

void twindow_setting::set_definition(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& windows = controls.find("window")->second;

	int index = 0;
	int initial_at = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = windows.begin(); it != windows.end(); ++ it) {
		ss.str("");
		ss << ht::generate_format(it->first, color_to_uint32(font::BLUE_COLOR)) << "(" << it->second->description.str() << ")";
		items.push_back(ss.str());
		if (it->first == cell_.window.definition) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = windows.begin();
	std::advance(it, dlg.cursel());
	cell_.window.definition = it->first;

	set_definition_label(window);
}

void twindow_setting::set_definition_label(twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "definition", false, true);
	label->set_label(cell_.window.definition);
}

void twindow_setting::set_horizontal_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = nposm;
	for (std::map<int, talign>::const_iterator it = horizontal_align.begin(); it != horizontal_align.end(); ++ it) {
		if (it->second.val == gui2::tgrid::HORIZONTAL_ALIGN_EDGE) {
			continue;
		}
		ss.str("");
		ss << ht::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (values.back() == cell_.window.horizontal_placement) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.window.horizontal_placement = values[dlg.cursel()];

	set_horizontal_layout_label(window);
}

void twindow_setting::set_vertical_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = nposm;
	for (std::map<int, talign>::const_iterator it = vertical_align.begin(); it != vertical_align.end(); ++ it) {
		if (it->second.val == gui2::tgrid::VERTICAL_ALIGN_EDGE) {
			continue;
		}
		ss.str("");
		ss << ht::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (values.back() == cell_.window.vertical_placement) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.window.vertical_placement = values[dlg.cursel()];

	set_vertical_layout_label(window);
}

void twindow_setting::set_horizontal_layout_label(twindow& window)
{
	std::stringstream ss;

	ss << horizontal_align.find(cell_.window.horizontal_placement)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_layout", true, true);
	label->set_label(ss.str());
}

void twindow_setting::set_vertical_layout_label(twindow& window)
{
	std::stringstream ss;

	ss << vertical_align.find(cell_.window.vertical_placement)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_layout", true, true);
	label->set_label(ss.str());
}

void twindow_setting::did_theme_toggled(twidget& widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(&widget);
	twindow* window = toggle->get_window();
	if (toggle->get_value()) {
		find_widget<tgrid>(window, "grid_base_dialog", false).set_visible(twidget::INVISIBLE);
		find_widget<tgrid>(window, "grid_base_scene", false).set_visible(twidget::VISIBLE);
		set_tile_shape_label(*window);

	} else {
		find_widget<tgrid>(window, "grid_base_dialog", false).set_visible(twidget::VISIBLE);
		find_widget<tgrid>(window, "grid_base_scene", false).set_visible(twidget::INVISIBLE);
		set_definition_label(*window);

		toggle = find_widget<ttoggle_button>(window, "click_dismiss1", false, true);
		toggle->set_value(cell_.window.click_dismiss);

		toggle = find_widget<ttoggle_button>(window, "automatic_placement", false, true);
		if (toggle->get_value()) {
			swap_page(*window, AUTOMATIC_PAGE, true);
		} else {
			swap_page(*window, MANUAL_PAGE, true);
		}
	}
}

void twindow_setting::automatic_placement_toggled(twidget& widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(&widget);
	twindow* window = toggle->get_window();
	if (toggle->get_value()) {
		swap_page(*window, AUTOMATIC_PAGE, true);
	} else {
		swap_page(*window, MANUAL_PAGE, true);
	}
}

std::string extract_app_from_textdomain(const std::string& textdomain)
{
	// "-lib"
	return textdomain.substr(0, textdomain.size() - 4);
}


void twindow_setting::set_app(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;

	int index = 0;
	int initial_at = 0;
	for (std::vector<std::string>::const_iterator it = textdomains_.begin(); it != textdomains_.end(); ++ it) {
		std::string app = extract_app_from_textdomain(*it);
		ss.str("");
		ss << app;
		items.push_back(ss.str());
		if (app == cell_.window.app) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.window.app = extract_app_from_textdomain(textdomains_[dlg.cursel()]);

	set_app_label(window);
}

void twindow_setting::set_app_label(twindow& window)
{
	std::stringstream ss;
	ss << cell_.window.app;

	tbutton* button = find_widget<tbutton>(&window, "app", false, true);
	button->set_label(ss.str());

	tlabel* label = find_widget<tlabel>(&window, "textdomain", false, true);
	label->set_label(cell_.window.app + "-lib");
}

void twindow_setting::set_orientation(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = 0;
	for (std::map<int, tparam3>::const_iterator it = orientations.begin(); it != orientations.end(); ++ it) {
		const tparam3& param = it->second;
		ss.str("");
		ss << param.description;
		items.push_back(ss.str());
		values.push_back(param.val);
		if (values.back() == cell_.window.orientation) {
			initial_at = param.val;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.window.orientation = (twidget::torientation)values[dlg.cursel()];

	set_orientation_label(window);
}

void twindow_setting::set_orientation_label(twindow& window)
{
	const tparam3& param = orientations.find(cell_.window.orientation)->second;

	tbutton* button = find_widget<tbutton>(&window, "orientation", false, true);
	button->set_label(param.description);
}

void twindow_setting::save(twindow& window, bool& handled, bool& halt)
{
	bool ret = true;
	int current_page = (int)bar_->cursel()->at();
	if (current_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (current_page == CONTEXT_MENU_PAGE) {
		ret = save_context_menu(window);
	} else if (current_page == FLOAT_WIDGET_PAGE) {
		ret = save_float_widget(window, float_widget_at_);
	}
	if (!ret) {
		handled = true;
		halt = true;
		return;
	}
	window.set_retval(twindow::OK);
}

void twindow_setting::fill_automatic_page(twindow& window)
{
	// horizontal layout
	set_horizontal_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_horizontal_layout", false)
			, std::bind(
				&twindow_setting::set_horizontal_layout
				, this
				, std::ref(window)));

	// vertical layout
	set_vertical_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_vertical_layout", false)
			, std::bind(
				&twindow_setting::set_vertical_layout
				, this
				, std::ref(window)));
}

void twindow_setting::fill_manual_page(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "x", false, true);
	text_box->set_label(cell_.window.x);

	text_box = find_widget<ttext_box>(&window, "y", false, true);
	text_box->set_label(cell_.window.y);

	text_box = find_widget<ttext_box>(&window, "width", false, true);
	text_box->set_label(cell_.window.width);

	text_box = find_widget<ttext_box>(&window, "height", false, true);
	text_box->set_label(cell_.window.height);
}

void twindow_setting::swap_page(twindow& window, int page, bool swap)
{
	if (!layout_panel_) {
		return;
	}
	if (!swap) {
		fill_automatic_page(window);
		fill_manual_page(window);
	}
	layout_panel_->set_radio_layer(page);
}

//
// context menu page
//
void twindow_setting::reload_menu_table(tmenu2& menu, twindow& window, int cursel)
{
	tlistbox* list = find_widget<tlistbox>(&window, "menu", false, true);
	list->clear();

	int index = 0;
	for (std::vector<tmenu2::titem>::const_iterator it = menu.items.begin(); it != menu.items.end(); ++ it) {
		const tmenu2::titem& item = *it;

		std::map<std::string, std::string> list_item_item;

		list_item_item.insert(std::make_pair("number", str_cast(index ++)));

		list_item_item.insert(std::make_pair("id", item.id));

		list_item_item.insert(std::make_pair("submenu", item.submenu? _("Yes"): "-"));

		list_item_item.insert(std::make_pair("hide", item.hide? _("Yes"): "-"));

		list_item_item.insert(std::make_pair("param", item.param? _("Yes"): "-"));

		list->insert_row(list_item_item);
	}
	if (cursel >= (int)menu.items.size()) {
		cursel = (int)menu.items.size() - 1;
	}
	if (!menu.items.empty()) {
		list->select_row(cursel);
		map_menu_item_to(window, menu, cursel);
	}
}

class tmenu2_item_id_lock
{
public:
	tmenu2_item_id_lock(tmenu2& menu, int n)
		: menu_(menu)
		, n_(n)
		, original_id_(null_str)
	{
		if (n_ >= 0 && n_ < (int)menu_.items.size()) {
			tmenu2::titem& item = menu_.items[n_];
			original_id_ = item.id;
			item.id = null_str;
		}
	}
	~tmenu2_item_id_lock()
	{
		if (n_ >= 0 && n_ < (int)menu_.items.size()) {
			tmenu2::titem& item = menu_.items[n_];
			item.id = original_id_;
		}
	}

private:
	tmenu2& menu_;
	int n_;
	std::string original_id_;
};


std::string twindow_setting::get_menu_item_id(tmenu2& menu, twindow& window, int exclude)
{
	std::string id = find_widget<ttext_box>(&window, "_id", false, true)->label();
	if (!utils::isvalid_id(id, true, min_id_len, max_id_len)) {
		gui2::show_message("", utils::errstr);
		return null_str;
	}
	utils::lowercase2(id);

	tmenu2_item_id_lock lock(menu, exclude);

	if (menu.top_menu()->id_existed(id)) {
		std::stringstream err;
		err << _("String is using.");
		gui2::show_message("", err.str());
		return null_str;
	}
	return id;
}

void twindow_setting::reload_submenu_navigate(tmenu2& menu, twindow& window, const tmenu2* cursel)
{
	submenu_navigate_->clear();

	std::vector<tmenu2*> menus;
	menus.push_back(&menu);
	menu.submenus(menus);

	std::stringstream ss;
	int selected = 0;
	for (std::vector<tmenu2*>::iterator it = menus.begin(); it != menus.end(); ++ it) {
		tmenu2* m = *it;
		if (m == cursel) {
			selected = std::distance(menus.begin(), it);
		}
		tcontrol& widget = submenu_navigate_->insert_item(null_str, m->id);
		widget.set_cookie(reinterpret_cast<size_t>(m));
	}
	submenu_navigate_->select_item(selected);

	refresh_parent_desc(window, *menus[selected]);
}

tmenu2* twindow_setting::current_submenu() const
{
	tcontrol* widget = submenu_navigate_->cursel();
	return reinterpret_cast<tmenu2*>(widget->cookie());
}

bool twindow_setting::menu_pre_toggle_tabbar(twidget* widget, twidget* previous)
{
	return true;
}

void twindow_setting::menu_toggle_tabbar(twidget* widget)
{
	twindow* window = widget->get_window();
	tmenu2* menu = reinterpret_cast<tmenu2*>(widget->cookie());
}

bool twindow_setting::submenu_pre_toggle_tabbar(twidget* widget, twidget* previous)
{
	return true;
}

void twindow_setting::submenu_toggle_tabbar(twidget* widget)
{
	twindow* window = widget->get_window();
	tmenu2* menu = reinterpret_cast<tmenu2*>(widget->cookie());

	refresh_parent_desc(*window, *menu);
	reload_menu_table(*menu, *window, 0);
}

void twindow_setting::append_menu_item(twindow& window)
{
	tmenu2& menu = *current_submenu();

	std::string id = get_menu_item_id(menu, window, -1);
	if (id.empty()) {
		return;
	}
	
	tmenu2* submenu = NULL;
	if (find_widget<ttoggle_button>(&window, "_submenu", false, true)->get_value()) {
		submenu = new tmenu2(null_str, id, &menu);
	}
	bool hide = find_widget<ttoggle_button>(&window, "_hide", false, true)->get_value();
	bool param = find_widget<ttoggle_button>(&window, "_param", false, true)->get_value();
	menu.items.push_back(tmenu2::titem(id, submenu, hide, param));

	if (submenu) {
		reload_submenu_navigate(menus_[0], window, current_submenu());
	}

	reload_menu_table(menu, window, menus_[0].items.size() - 1);
	// find_widget<tlistbox>(&window, "menu", false).scroll_vertical_scrollbar(tscrollbar_::END);
}

void twindow_setting::modify_menu_item(twindow& window)
{
	tmenu2& menu = *current_submenu();
	tlistbox& list = find_widget<tlistbox>(&window, "menu", false);
	int row = list.cursel()->at();

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	std::string id = get_menu_item_id(menu, window, row);
	if (id.empty()) {
		return;
	}
	bool submenu = find_widget<ttoggle_button>(&window, "_submenu", false, true)->get_value();
	bool hide = find_widget<ttoggle_button>(&window, "_hide", false, true)->get_value();
	bool param = find_widget<ttoggle_button>(&window, "_param", false, true)->get_value();

	tmenu2::titem& item = menu.items[row];
	if (id == item.id && submenu == !!item.submenu && hide == item.hide && param == item.param) {
		return;
	}

	bool require_navigate = false;
	if (submenu != !!item.submenu) {
		if (item.submenu) {
			delete item.submenu;
			item.submenu = NULL;
		} else {
			item.submenu = new tmenu2(null_str, id, &menu);
		}
		require_navigate = true;
	}
	item.id = id;
	item.hide = hide;
	item.param = param;

	if (require_navigate) {
		reload_submenu_navigate(*menu.top_menu(), window, &menu);
	}
	reload_menu_table(menu, window, row);
}

void twindow_setting::erase_menu_item(twindow& window)
{
	tmenu2& menu = *current_submenu();
	tlistbox& list = find_widget<tlistbox>(&window, "menu", false);
	int row = list.cursel()->at();

	std::vector<tmenu2::titem>::iterator it = menu.items.begin();
	std::advance(it, row);
	bool require_navigate = it->submenu;
	menu.items.erase(it);

	if (require_navigate) {
		reload_submenu_navigate(*menu.top_menu(), window, &menu);
	}
	reload_menu_table(menu, window, row);
}

void twindow_setting::item_selected(twindow& window, tlistbox& list)
{
	tmenu2& menu = *current_submenu();
	int row = list.cursel()->at();
	map_menu_item_to(window, menu, row);
}

void twindow_setting::map_menu_item_to(twindow& window, tmenu2& menu, int row)
{
	tmenu2::titem& item = menu.items[row];

	find_widget<ttext_box>(&window, "_id", false, true)->set_label(item.id);
	find_widget<ttoggle_button>(&window, "_submenu", false, true)->set_value(item.submenu);
	find_widget<ttoggle_button>(&window, "_hide", false, true)->set_value(item.hide);
	find_widget<ttoggle_button>(&window, "_param", false, true)->set_value(item.param);
}

void twindow_setting::refresh_parent_desc(twindow& window, tmenu2& menu)
{
	tgrid& grid = find_widget<tgrid>(&window, "_grid_set_report", false);
	tlabel& label = find_widget<tlabel>(&window, "parent_menu", false);

	if (!menu.parent) {
		grid.set_visible(twidget::VISIBLE);
		label.set_visible(twidget::INVISIBLE);
		find_widget<ttext_box>(&window, "report_id", false).set_label(menu.report);

	} else {
		grid.set_visible(twidget::INVISIBLE);
		label.set_visible(twidget::VISIBLE);

		std::stringstream ss;
		utils::string_map symbols;
		symbols["menu"] = ht::generate_format(menu.parent->id, color_to_uint32(font::GOOD_COLOR));

		std::string msg = vgettext2("Parent menu $menu", symbols);
		label.set_label(msg);
	}
}

//
// patch page
//
void twindow_setting::switch_float_widget(twindow& window)
{
	VALIDATE(float_widget_at_ != nposm && float_widget_at_ < (int)controller_.float_widgets().size(), null_str);
	const ::tfloat_widget& item = controller_.float_widgets()[float_widget_at_];

	tgrid* float_widget_page = bar_panel_->layer(FLOAT_WIDGET_PAGE);

	ttext_box* text_box = find_widget<ttext_box>(float_widget_page, "float_widget_ref", false, true);
	text_box->set_label(item.ref);

	// width
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_width_formula", false, true);
	text_box->set_label(unit::formual_extract_str(item.w.first));
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_width_offset", false, true);
	text_box->set_maximum_chars(5);
	text_box->set_label(str_cast(item.w.second));

	// height
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_height_formula", false, true);
	text_box->set_label(unit::formual_extract_str(item.h.first));
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_height_offset", false, true);
	text_box->set_maximum_chars(5);
	text_box->set_label(str_cast(item.h.second));

	// first x
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_x_formula", false, true);
	text_box->set_label(unit::formual_extract_str(item.x[0].first));
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_x_offset", false, true);
	text_box->set_maximum_chars(5);
	text_box->set_label(str_cast(item.x[0].second));

	// first y
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_y_formula", false, true);
	text_box->set_label(unit::formual_extract_str(item.y[0].first));
	text_box = find_widget<ttext_box>(float_widget_page, "float_widget_y_offset", false, true);
	text_box->set_maximum_chars(5);
	text_box->set_label(str_cast(item.y[0].second));
}

void twindow_setting::append_float_widget(twindow& window)
{
	treport& navigate = *patch_bar_;

	const std::string default_float_widget_id = "flt_untitled";
	gui2::tedit_box_param param(_("New float widget"), null_str, _("ID"), default_float_widget_id, null_str, null_str, _("OK"), nposm);
	{
		param.did_text_changed = std::bind(&mkwin_controller::verify_new_float_widget, &controller_, _1, nposm);
		gui2::tedit_box dlg(param);
		try {
			dlg.show();
		} catch(twml_exception& e) {
			e.show();
		}
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
	}

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	int original_size = (int)float_widgets.size();
	controller_.insert_float_widget(param.result);

	std::vector<::tfloat_widget>::const_iterator it = float_widgets.begin();
	std::advance(it, original_size);
	for (int index = original_size; it != float_widgets.end(); ++ it) {
		navigate.insert_item(param.result, form_tab_label(navigate, index));
	}

	if (!original_size) {
		navigate.select_item(0);

		tgrid* float_widget_page = bar_panel_->layer(FLOAT_WIDGET_PAGE);
		find_widget<tbutton>(float_widget_page, "_append_patch", false, true)->set_active(true);
		find_widget<tbutton>(float_widget_page, "_erase_patch", false, true)->set_active(true);
		find_widget<tbutton>(float_widget_page, "_rename_patch", false, true)->set_active(true);
		find_widget<tbutton>(float_widget_page, "float_widget_type", false, true)->set_active(true);
		find_widget<tbutton>(float_widget_page, "float_widget", false, true)->set_active(true);
	}
}

void twindow_setting::erase_float_widget(twindow& window)
{
	int res = gui2::show_message2("", _("Do you want to erase this float widget?"), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	treport& navigate = *patch_bar_;

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	std::vector<::tfloat_widget>::iterator it = float_widgets.begin();
	std::advance(it, float_widget_at_);

	float_widgets.erase(it);
	navigate.erase_item(float_widget_at_);

	reload_float_widget_label(navigate);

	if (float_widget_at_ == (int)float_widgets.size()) {
		float_widget_at_ --;
	}
	if (float_widgets.empty()) {
		tgrid* float_widget_page = bar_panel_->layer(FLOAT_WIDGET_PAGE);
		// find_widget<tbutton>(float_widget_page, "_append_patch", false, true)->set_active(false);
		find_widget<tbutton>(float_widget_page, "_erase_patch", false, true)->set_active(false);
		find_widget<tbutton>(float_widget_page, "_rename_patch", false, true)->set_active(false);
		find_widget<tbutton>(float_widget_page, "float_widget_type", false, true)->set_active(false);
		find_widget<tbutton>(float_widget_page, "float_widget", false, true)->set_active(false);
	}
}

void twindow_setting::rename_float_widget(twindow& window)
{
	treport& navigate = *patch_bar_;

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	::tfloat_widget& item = float_widgets[float_widget_at_];
	const std::string original_id = item.u->cell().id;

	std::stringstream ss;
	ss << _("Original ID") << " " << ht::generate_format(original_id, color_to_uint32(font::GOOD_COLOR));
	ss << "\n";

	gui2::tedit_box_param param(_("Rename float widget"), ss.str(), _("ID"), item.u->cell().id, null_str, null_str, _("OK"), nposm);
	{
		param.did_text_changed = std::bind(&mkwin_controller::verify_new_float_widget, &controller_, _1, float_widget_at_);
		gui2::tedit_box dlg(param);
		try {
			dlg.show();
		} catch(twml_exception& e) {
			e.show();
		}
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		if (original_id == param.result) {
			return;
		}
	}

	item.u->cell().id = param.result;

	reload_float_widget_label(navigate);
}

void twindow_setting::change_float_widget_type(twindow& window)
{
	if (float_widget_at_ == nposm) {
		return;
	}

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	::tfloat_widget& item = float_widgets[float_widget_at_];

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;

	static std::set<std::string> allow;
	if (allow.empty()) {
		allow.insert("label");
		allow.insert("button");
		allow.insert("image");
		allow.insert("slider");
		allow.insert("toggle_button");
		allow.insert("text_box");
		allow.insert("track");
	}

	std::vector<std::string> items;
	std::vector<int> values;
	std::stringstream ss;

	int at = 0;
	int initial_val = nposm;
	for (gui2::tgui_definition::tcontrol_definition_map::const_iterator it = controls.begin(); it != controls.end(); ++ it, at ++) {
		const std::string& type = it->first;
		if (allow.find(type) == allow.end()) {
			continue;
		}
		ss.str("");
		ss << type;
		items.push_back(ss.str());
		values.push_back(at);
		if (type == item.u->widget().first) {
			initial_val = items.size() - 1;
		}
	}

	int selected_val;
	{
		gui2::tcombo_box dlg(items, initial_val);
		dlg.show();
		if (dlg.cursel() == initial_val) {
			return;
		}
		selected_val = values[dlg.cursel()];
	}

	gui2::tgui_definition::tcontrol_definition_map::const_iterator it = controls.begin();
	std::advance(it, selected_val);

	const std::string id = item.u->cell().id;
	const std::string width = item.u->cell().widget.width;
	const std::string height = item.u->cell().widget.height;
	gui2::tcontrol_definition_ptr ptr = mkwin_display::find_widget(it->first, "default", null_str);
	item.u.reset(new unit(controller_, controller_.get_display(), controller_.get_units(), std::make_pair(it->first, ptr), nullptr, -1));
	item.u->cell().id = id;
	item.u->cell().widget.width = width;
	item.u->cell().widget.height = height;

	reload_float_widget_label(*patch_bar_);
}

void twindow_setting::edit_float_widget(twindow& window)
{
	if (float_widget_at_ == nposm) {
		return;
	}

	const std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	const ::tfloat_widget& item = float_widgets[float_widget_at_];

	gui2::tcontrol_setting dlg(controller_, *item.u.get(), textdomains_, controller_.linked_groups(), true);
	dlg.show();
	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}
	item.u->set_cell(dlg.cell());
}

bool twindow_setting::float_widget_pre_toggle_tabbar(twidget* widget, twidget* previous)
{
	return true;
}

void twindow_setting::patch_toggle_tabbar(ttoggle_button& widget)
{
	float_widget_at_ = widget.at();

	twindow* window = widget.get_window();
	switch_float_widget(*window);
}

std::string twindow_setting::form_tab_label(treport& navigate, int at) const
{
	const std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	const ::tfloat_widget& item = float_widgets[at];

	std::stringstream ss;
	ss << item.u->cell().id;

	ss << "\n(" << item.u->widget().first << ")";

	return ss.str();
}

void twindow_setting::reload_float_widget_label(treport& navigate) const
{
	const tgrid::tchild* children = navigate.child_begin();
	int childs = navigate.items();
	for (int i = 0; i < childs; i ++) {
		tcontrol* widget = dynamic_cast<tcontrol*>(children[i].widget_);
		widget->set_label(form_tab_label(navigate, i));
	}
}

void twindow_setting::reload_float_widget(treport& navigate, twindow& window)
{
	navigate.clear();

	std::vector<::tfloat_widget>& float_widgets = controller_.float_widgets();
	std::stringstream ss;
	int at = 0;
	for (std::vector<::tfloat_widget>::const_iterator it = float_widgets.begin(); it != float_widgets.end(); ++ it, at ++) {
		navigate.insert_item(it->u->cell().id, form_tab_label(navigate, at));
	}
	if (!float_widgets.empty()) {
		navigate.select_item(0);
	}
}

}
