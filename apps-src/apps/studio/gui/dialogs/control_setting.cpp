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

#include "gui/dialogs/control_setting.hpp"

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
#include "gui/widgets/report.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "gui/auxiliary/widget_definition/toggle_panel.hpp"
#include "gui/auxiliary/widget_definition/scroll_panel.hpp"
#include "unit.hpp"
#include "mkwin_controller.hpp"
#include "theme.hpp"
#include <cctype>

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(studio, control_setting)

const std::string untitled = "_untitled";

void show_id_error(const std::string& id, const std::string& errstr)
{
	std::stringstream err;
	utils::string_map symbols;

	symbols["id"] = id;
	err << vgettext2("Invalid '$id' value!", symbols);
	err << "\n\n" << errstr;
	gui2::show_message("", err.str());
}

bool verify_formula(const std::string& id, const std::string& expression2)
{
	if (expression2.empty()) {
		show_id_error(id, _("Must not empty."));
		return false;
	}
	try {
		std::string expression = "(" + expression2 + ")";
		game_logic::formula f(expression, nullptr);

	} catch (game_logic::formula_error& e)	{
		std::stringstream err;
		utils::string_map symbols;

		symbols["id"] = id;
		err << vgettext2("Error $id expression!", symbols);
		err << "\n\n" << e.what();
		gui2::show_message("", err.str());
		return false;
	}
	return true;
}

std::map<int, talign> horizontal_align;
std::map<int, talign> vertical_align;

std::map<int, tscroll_mode> horizontal_mode;
std::map<int, tscroll_mode> vertical_mode;

std::map<int, tparam3> orientations;

void init_layout_mode()
{
	if (horizontal_align.empty()) {
		horizontal_align.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_EDGE, 
			talign(tgrid::HORIZONTAL_ALIGN_EDGE, _("Double edge align. Maybe stretch"))));
		horizontal_align.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_LEFT, 
			talign(tgrid::HORIZONTAL_ALIGN_LEFT, _("Align left"))));
		horizontal_align.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_CENTER, 
			talign(tgrid::HORIZONTAL_ALIGN_CENTER, _("Align center"))));
		horizontal_align.insert(std::make_pair(tgrid::HORIZONTAL_ALIGN_RIGHT, 
			talign(tgrid::HORIZONTAL_ALIGN_RIGHT, _("Align right"))));
	}

	if (vertical_align.empty()) {
		vertical_align.insert(std::make_pair(tgrid::VERTICAL_ALIGN_EDGE, 
			talign(tgrid::VERTICAL_ALIGN_EDGE, _("Double edge align. Maybe stretch"))));
		vertical_align.insert(std::make_pair(tgrid::VERTICAL_ALIGN_TOP, 
			talign(tgrid::VERTICAL_ALIGN_TOP, _("Align top"))));
		vertical_align.insert(std::make_pair(tgrid::VERTICAL_ALIGN_CENTER, 
			talign(tgrid::VERTICAL_ALIGN_CENTER, _("Align center"))));
		vertical_align.insert(std::make_pair(tgrid::VERTICAL_ALIGN_BOTTOM, 
			talign(tgrid::VERTICAL_ALIGN_BOTTOM, _("Align bottom"))));
	}

	if (horizontal_mode.empty()) {
		horizontal_mode.insert(std::make_pair(tscroll_container::always_invisible, 
			tscroll_mode(tscroll_container::always_invisible, _("Always invisible"))));
		horizontal_mode.insert(std::make_pair(tscroll_container::auto_visible, 
			tscroll_mode(tscroll_container::auto_visible, _("Auto visible"))));
	}

	if (vertical_mode.empty()) {
		vertical_mode.insert(std::make_pair(tscroll_container::always_invisible, 
			tscroll_mode(tscroll_container::always_invisible, _("Always invisible"))));
		vertical_mode.insert(std::make_pair(tscroll_container::auto_visible, 
			tscroll_mode(tscroll_container::auto_visible, _("Auto visible"))));
	}

	std::stringstream err;
	utils::string_map symbols;
	
	if (orientations.empty()) {
		orientations.insert(std::make_pair(twidget::auto_orientation, tparam3(twidget::auto_orientation, "auto", _("Auto"))));
		orientations.insert(std::make_pair(twidget::landscape_orientation, tparam3(twidget::landscape_orientation, "landscape", _("Landscape"))));
		orientations.insert(std::make_pair(twidget::portrait_orientation, tparam3(twidget::portrait_orientation, "portrait", _("Portrait"))));
	}
}

tcontrol_setting::tcontrol_setting(mkwin_controller& controller, unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds, bool float_widget)
	: tsetting_dialog(u.cell())
	, controller_(controller)
	, u_(u)
	, textdomains_(textdomains)
	, linkeds_(linkeds)
	, float_widget_(float_widget)
	, bar_(NULL)
{
	VALIDATE(!controller_.preview(), null_str);
}

void tcontrol_setting::update_title(twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	if (!u_.cell().id.empty()) {
		ss << ht::generate_format(u_.cell().id, color_to_uint32(font::GOOD_COLOR)); 
	} else {
		ss << ht::generate_format("---", color_to_uint32(font::GOOD_COLOR)); 
	}
	ss << "    ";

	ss << widget.first;
	if (widget.second.get()) {
		ss << "(" << ht::generate_format(widget.second->id, color_to_uint32(font::BLUE_COLOR)) << ")"; 
	}
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	update_title(*window_);

	// prepare navigate bar.
	std::vector<std::string> labels;
	labels.push_back(_("Base"));
	labels.push_back(_("Best size"));
	labels.push_back(_("Advanced"));

	bar_ = find_widget<treport>(window_, "bar", false, true);
	bar_->set_did_item_pre_change(std::bind(&tcontrol_setting::did_item_pre_change_report, this, _1, _2, _3));
	bar_->set_did_item_changed(std::bind(&tcontrol_setting::did_item_changed_report, this, _1, _2));
	bar_->set_boddy(find_widget<twidget>(window_, "bar_panel", false, true));
	int index = 0;
	for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
		tcontrol& widget = bar_->insert_item(null_str, *it);
		widget.set_cookie(index ++);
	}

	page_panel_ = find_widget<tstack>(window_, "panel", false, true);
	page_panel_->set_radio_layer(BASE_PAGE);
	
	pre_base();

	pre_size();
	if (u_.has_size() || u_.has_mtwusc()) {
		// pre_size();
	} else {
		bar_->set_item_visible(SIZE_PAGE, false);
		page_panel_->layer(SIZE_PAGE)->set_visible(twidget::INVISIBLE);
	}

	pre_advanced(*window_);
	if (u_.widget().second.get()) {
		// pre_advanced(window);
	} else {
		bar_->set_item_visible(ADVANCED_PAGE, false);
		page_panel_->layer(ADVANCED_PAGE)->set_visible(twidget::INVISIBLE);
	}

	// until
	bar_->select_item(BASE_PAGE);

	connect_signal_mouse_left_click(
		 find_widget<tbutton>(window_, "save", false)
		, std::bind(
			&tcontrol_setting::save
			, this
			, std::ref(*window_)
			, _3, _4));
}

void tcontrol_setting::post_show()
{
	// tcontrol_setting::save_size isn't only called when save not "size" page.
	if (cell_.widget.mtwusc) {
		cell_.widget.width.clear();
		cell_.widget.width_is_max = false;
		cell_.widget.height.clear();
		cell_.widget.height_is_max = false;
		cell_.widget.min_text_width.clear();
	}
}

void tcontrol_setting::pre_base()
{
	tgrid* base_page = page_panel_->layer(BASE_PAGE);

	// horizontal layout
	set_horizontal_layout_label();
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(base_page, "_set_horizontal_layout", false)
			, std::bind(
				&tcontrol_setting::set_horizontal_layout
				, this));

	// vertical layout
	set_vertical_layout_label();
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(base_page, "_set_vertical_layout", false)
			, std::bind(
				&tcontrol_setting::set_vertical_layout
				, this));

	// linked_group
	set_linked_group_label();
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(base_page, "_set_linked_group", false)
			, std::bind(
				&tcontrol_setting::set_linked_group
				, this));

	// linked_group
	set_textdomain_label(true);
	set_textdomain_label(false);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(base_page, "textdomain_label", false)
			, std::bind(
				&tcontrol_setting::set_textdomain
				, this
				, true));
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(base_page, "textdomain_tooltip", false)
			, std::bind(
				&tcontrol_setting::set_textdomain
				, this
				, false));

	// border size
	ttext_box* text_box = find_widget<ttext_box>(base_page, "_border", false, true);
	text_box->set_label(str_cast(cell_.widget.cell.border_size_));
	if (cell_.widget.cell.flags_ & tgrid::BORDER_LEFT) {
		find_widget<ttoggle_button>(base_page, "_border_left", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_RIGHT) {
		find_widget<ttoggle_button>(base_page, "_border_right", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_TOP) {
		find_widget<ttoggle_button>(base_page, "_border_top", false, true)->set_value(true);
	}
	if (cell_.widget.cell.flags_ & tgrid::BORDER_BOTTOM) {
		find_widget<ttoggle_button>(base_page, "_border_bottom", false, true)->set_value(true);
	}

	text_box = find_widget<ttext_box>(base_page, "_id", false, true);
	text_box->set_maximum_chars(max_id_len);
	text_box->set_label(cell_.id);
	if (float_widget_) {
		VALIDATE(!cell_.id.empty(), null_str);
		text_box->set_active(false);
	}

	find_widget<tscroll_text_box>(base_page, "_label", false, true)->set_label(cell_.widget.label);
	find_widget<tscroll_text_box>(base_page, "_tooltip", false, true)->set_label(cell_.widget.tooltip);
}

void tcontrol_setting::pre_size()
{
	tgrid* size_page = page_panel_->layer(SIZE_PAGE);

	find_widget<tgrid>(size_page, "_size_grid", false, true)->set_visible(u_.has_size()? twidget::VISIBLE: twidget::INVISIBLE);
	if (u_.has_size()) {
		if (u_.has_size_is_max()) {
			connect_signal_mouse_left_click(
				  find_widget<tbutton>(size_page, "width_is_max", false)
				, std::bind(
					&tcontrol_setting::click_size_is_max
					, this
					, true));
			set_size_is_max_label(true);

			connect_signal_mouse_left_click(
				  find_widget<tbutton>(size_page, "height_is_max", false)
				, std::bind(
					&tcontrol_setting::click_size_is_max
					, this
					, false));
			set_size_is_max_label(false);
		}

		ttext_box* text_box = find_widget<ttext_box>(size_page, "_width", false, true);
		text_box->set_label(cell_.widget.width);
		// default width maybe is null_str, for safe, call did_best_size_changed explicted.
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_best_size_changed, this, _1, true));
		did_best_size_changed(*text_box, true);

		text_box = find_widget<ttext_box>(size_page, "_height", false, true);
		text_box->set_label(cell_.widget.height);
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_best_size_changed, this, _1, false));
		did_best_size_changed(*text_box, false);

		text_box = find_widget<ttext_box>(size_page, "_min_text_width", false, true);
		text_box->set_label(cell_.widget.min_text_width);
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_min_text_width_changed, this, _1));
		did_min_text_width_changed(*text_box);
	}

	find_widget<tgrid>(size_page, "_mtwusc_grid", false, true)->set_visible(u_.has_mtwusc()? twidget::VISIBLE: twidget::INVISIBLE);
	if (u_.has_mtwusc()) {
		find_widget<ttoggle_button>(size_page, "mtwusc", false, true)->set_value(cell_.widget.mtwusc);
	}
}

tspace4 get_cfg_margin(const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget)
{
	const tresolution_definition_const_ptr& resolution = widget.second->resolutions.front();

	if (widget.first == "panel") {
		boost::intrusive_ptr<const tpanel_definition::tresolution> conf =
			boost::dynamic_pointer_cast<const tpanel_definition::tresolution>(resolution);	

		return tspace4{os_size_2_cfg(conf->left_margin), os_size_2_cfg(conf->right_margin), 
			os_size_2_cfg(conf->top_margin), os_size_2_cfg(conf->bottom_margin)};

	} else if (widget.first == "toggle_panel") {
		boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
			boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(resolution);	

		return tspace4{os_size_2_cfg(conf->left_margin), os_size_2_cfg(conf->right_margin), 
			os_size_2_cfg(conf->top_margin), os_size_2_cfg(conf->bottom_margin)};

	}
	VALIDATE(widget.first == "scroll_panel", null_str);

	boost::intrusive_ptr<const tscroll_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tscroll_panel_definition::tresolution>(resolution);	

	return tspace4{os_size_2_cfg(conf->left_margin), os_size_2_cfg(conf->right_margin), 
		os_size_2_cfg(conf->top_margin), os_size_2_cfg(conf->bottom_margin)};
}

void tcontrol_setting::pre_advanced(twindow& window)
{
	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);

	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ttext_box* text_box;
	tbutton* button;
	ttoggle_button* toggle;
	if (!u_.is_tree()) {
		tgrid* grid = find_widget<tgrid>(advanced_page, "_grid_tree_view", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		find_widget<ttoggle_button>(advanced_page, "foldable", false, true)->set_value(cell_.widget.tree_view.foldable);
		text_box = find_widget<ttext_box>(advanced_page, "indention_step_size", false, true);
		text_box->set_label(str_cast(cell_.widget.tree_view.indention_step_size));
		text_box = find_widget<ttext_box>(advanced_page, "node_id", false, true);
		text_box->set_label(cell_.widget.tree_view.node_id);
	}

	if (!u_.is_slider()) {
		tgrid* grid = find_widget<tgrid>(advanced_page, "_grid_slider", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		text_box = find_widget<ttext_box>(advanced_page, "minimum_value", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.minimum_value));
		text_box = find_widget<ttext_box>(advanced_page, "maximum_value", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.maximum_value));
		text_box = find_widget<ttext_box>(advanced_page, "step_size", false, true);
		text_box->set_label(str_cast(cell_.widget.slider.step_size));
	}

	if (!u_.is_text_box()) {
		tgrid* grid = find_widget<tgrid>(advanced_page, "_grid_text_box", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		toggle = find_widget<ttoggle_button>(advanced_page, "text_box_desensitize", false, true);
		toggle->set_value(cell_.widget.text_box.desensitize);
	}

	if (!u_.is_label()) {
		tgrid* grid = find_widget<tgrid>(advanced_page, "_grid_label", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		button = find_widget<tbutton>(advanced_page, "set_label_state", false, true);
		connect_signal_mouse_left_click(
			  *button
			, std::bind(
				&tcontrol_setting::set_label_state
				, this
				, std::ref(*button)));
		set_label_state_label(*button);
	}

	if (!u_.is_report()) {
		tgrid* grid = find_widget<tgrid>(advanced_page, "_grid_report", false, true);
		grid->set_visible(twidget::INVISIBLE);

	} else {
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "multi_line", false)
			, std::bind(
				&tcontrol_setting::set_multi_line
				, this
				, std::ref(window)));

		text_box = find_widget<ttext_box>(advanced_page, "report_fixed_cols", false, true);
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_report_fixed_cols_changed, this, _1));
		set_multi_line_label(window);

		button = find_widget<tbutton>(advanced_page, "report_toggle", false, true);
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "report_toggle", false)
			, std::bind(
				&tcontrol_setting::click_report_toggle
				, this
				, std::ref(*advanced_page)));
		button->set_label(cell_.widget.report.toggle? _("Toggle button"): _("Button"));

		connect_signal_mouse_left_click(
			  find_widget<tbutton>(advanced_page, "report_definition", false)
			, std::bind(
				&tcontrol_setting::click_report_definition
				, this
				, std::ref(*advanced_page)));
		set_report_definition_label(*advanced_page);

		// multi select
		toggle = find_widget<ttoggle_button>(&window, "report_multi_select", false, true);
		toggle->set_value(cell_.widget.report.multi_select);
		toggle->set_visible(cell_.widget.report.toggle? twidget::VISIBLE: twidget::INVISIBLE);

		text_box = find_widget<ttext_box>(advanced_page, "unit_width", false, true);
		text_box->set_placeholder(_("Usable variable: screen_width, screen_height"));
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_report_unit_size_changed, this, _1, true));
		text_box->set_label(cell_.widget.report.unit_width);

		text_box = find_widget<ttext_box>(advanced_page, "unit_height", false, true);
		text_box->set_placeholder(_("Usable variable: screen_width, screen_height"));
		text_box->set_did_text_changed(std::bind(&tcontrol_setting::did_report_unit_size_changed, this, _1, false));
		text_box->set_label(cell_.widget.report.unit_height);

		text_box = find_widget<ttext_box>(advanced_page, "gap", false, true);
		text_box->set_label(str_cast(cell_.widget.report.gap));
	}

	// if (u_.is_scroll()) {
	if (false) {
		// horizontal mode
		set_horizontal_mode_label(window);
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "_set_horizontal_mode", false)
				, std::bind(
					&tcontrol_setting::set_horizontal_mode
					, this
					, std::ref(window)));

		// vertical layout
		set_vertical_mode_label(window);
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "_set_vertical_mode", false)
				, std::bind(
					&tcontrol_setting::set_vertical_mode
					, this
					, std::ref(window)));
	} else {
		find_widget<tgrid>(&window, "_grid_scrollbar", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.has_atom_markup()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(advanced_page, "atom_markup", false, true);
		toggle->set_value(cell_.widget.atom_markup);

	} else {
		find_widget<tgrid>(advanced_page, "_grid_atom_markup", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.has_margin()) {
		// margin
		tspace4 margin = get_cfg_margin(widget);

		ttext_box* text_box = find_widget<ttext_box>(advanced_page, "_left_margin", false, true);
		text_box->set_placeholder(str_cast(margin.left));
		if (cell_.widget.margin.left != nposm) {
			text_box->set_label(str_cast(cell_.widget.margin.left));	
		}

		text_box = find_widget<ttext_box>(advanced_page, "_right_margin", false, true);
		text_box->set_placeholder(str_cast(margin.right));
		if (cell_.widget.margin.right != nposm) {
			text_box->set_label(str_cast(cell_.widget.margin.right));
		}

		text_box = find_widget<ttext_box>(advanced_page, "_top_margin", false, true);
		text_box->set_placeholder(str_cast(margin.top));
		if (cell_.widget.margin.top != nposm) {
			text_box->set_label(str_cast(cell_.widget.margin.top));
		}

		text_box = find_widget<ttext_box>(advanced_page, "_bottom_margin", false, true);
		text_box->set_placeholder(str_cast(margin.bottom));
		if (cell_.widget.margin.bottom != nposm) {
			text_box->set_label(str_cast(cell_.widget.margin.bottom));
		}
	} else {
		find_widget<tgrid>(advanced_page, "_grid_margin", false, true)->set_visible(twidget::INVISIBLE);
	}

	if (u_.is_stack()) {
		find_widget<tbutton>(advanced_page, "set_stack_mode", false).set_label(gui2::implementation::form_stack_mode_str(cell_.widget.stack.mode));
		connect_signal_mouse_left_click(
				  find_widget<tbutton>(&window, "set_stack_mode", false)
				, std::bind(
					&tcontrol_setting::set_stack_mode
					, this
					, std::ref(window)));
	} else {
		find_widget<tgrid>(advanced_page, "_grid_stacked_widget", false).set_visible(twidget::INVISIBLE);
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_definition", false)
			, std::bind(
				&tcontrol_setting::set_definition
				, this
				, std::ref(window)));

	if (u_.widget().second && u_.widget().second->label_is_text) {
		connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_text_font_size", false)
			, std::bind(
				&tcontrol_setting::set_text_font_size
				, this));
		set_text_font_size_label();

		connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_text_color_tpl", false)
			, std::bind(
				&tcontrol_setting::set_text_color_tpl
				, this
				, std::ref(window)));
		set_text_color_tpl_label(window);

	} else {
		find_widget<tbutton>(&window, "_set_text_font_size", false).set_visible(twidget::INVISIBLE);
		find_widget<tlabel>(&window, "_text_font_size", false).set_visible(twidget::INVISIBLE);
		find_widget<tbutton>(&window, "_set_text_color_tpl", false).set_visible(twidget::INVISIBLE);
	}

	if (u_.widget().second) {
		const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
		const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(u_.widget().first)->second;

		tbutton* button = find_widget<tbutton>(&window, "_set_definition", false, true);
		button->set_active(definitions.size() > 1);
	
		set_definition_label(window, utils::generate_app_prefix_id(u_.widget().second->app, u_.widget().second->id));
	}
}

bool tcontrol_setting::did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& to)
{
	twindow& window = *to.get_window();
	bool ret = true;
	int previous_page = (int)from.cookie();
	if (previous_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (previous_page == SIZE_PAGE) {
		ret = save_size(window);
	} else if (previous_page == ADVANCED_PAGE) {
		ret = save_advanced(window);
	}
	return ret;
}

void tcontrol_setting::did_item_changed_report(treport& report, ttoggle_button& widget)
{
	int page = (int)widget.cookie();
	page_panel_->set_radio_layer(page);
}

void tcontrol_setting::save(twindow& window, bool& handled, bool& halt)
{
	bool ret = true;
	int current_page = (int)bar_->cursel()->cookie();
	if (current_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (current_page == SIZE_PAGE) {
		ret = save_size(window);
	} else if (current_page == ADVANCED_PAGE) {
		ret = save_advanced(window);
	}
	if (!ret) {
		handled = true;
		halt = true;
		return;
	}
	window.set_retval(twindow::OK);
}

bool tcontrol_setting::save_base(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "_border", false, true);
	int border = lexical_cast_default<int>(text_box->label());
	if (border < 0 || border > 50) {
		return false;
	}
	cell_.widget.cell.border_size_ = border;

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "_border_left", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_LEFT;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_LEFT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_right", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_RIGHT;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_RIGHT;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_top", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_TOP;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_TOP;
	}
	toggle = find_widget<ttoggle_button>(&window, "_border_bottom", false, true);
	if (toggle->get_value()) {
		cell_.widget.cell.flags_ |= tgrid::BORDER_BOTTOM;
	} else {
		cell_.widget.cell.flags_ &= ~tgrid::BORDER_BOTTOM;
	}

	text_box = find_widget<ttext_box>(&window, "_id", false, true);
	cell_.id = text_box->label();
	// id maybe empty.
	if (!cell_.id.empty()) {
		if (!utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
			gui2::show_message("", _("id error"));
			return false;
		}
	}
	utils::lowercase2(cell_.id);

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&window, "_label", false, true);
	cell_.widget.label = scroll_text_box->label();
	if (!cell_.widget.label.empty()) {
		// git ride of \r\n\t at end.
		const char* c_str = cell_.widget.label.c_str();
		const int size = cell_.widget.label.size();
		int at = size - 1;
		for (at = size - 1; at >= 0; at --){
			if (c_str[at] != '\r' && c_str[at] != '\n' && c_str[at] != '\t' && c_str[at] != ' ') {
				break;
			}
		}
		if (at != size - 1) {
			cell_.widget.label = cell_.widget.label.substr(0, at + 1);
		}
	}
	
	
	scroll_text_box = find_widget<tscroll_text_box>(&window, "_tooltip", false, true);
	cell_.widget.tooltip = scroll_text_box->label();

	return true;
}

bool tcontrol_setting::save_size(twindow& window)
{
	tgrid* size_page = page_panel_->layer(SIZE_PAGE);

	if (u_.has_size()) {
		ttext_box* text_box = find_widget<ttext_box>(size_page, "_width", false, true);
		cell_.widget.width = text_box->label();
		utils::lowercase2(cell_.widget.width);
		if (!cell_.widget.width.empty() && !verify_formula("width's formula", cell_.widget.width)) {
			return false;
		}

		text_box = find_widget<ttext_box>(size_page, "_height", false, true);
		cell_.widget.height = text_box->label();
		utils::lowercase2(cell_.widget.height);
		if (!cell_.widget.height.empty() && !verify_formula("height's formula", cell_.widget.height)) {
			return false;
		}

		text_box = find_widget<ttext_box>(size_page, "_min_text_width", false, true);
		cell_.widget.min_text_width = text_box->label();
		utils::lowercase2(cell_.widget.min_text_width);
		if (!cell_.widget.min_text_width.empty() && !verify_formula("min_text_width's formula", cell_.widget.min_text_width)) {
			return false;
		}
	}
	if (u_.has_mtwusc()) {
		cell_.widget.mtwusc = find_widget<ttoggle_button>(size_page, "mtwusc", false, true)->get_value();
	}
	return true;
}

static bool save_margin(tgrid& grid, const std::string& id, int& ret)
{
	ttext_box* text_box = find_widget<ttext_box>(&grid, id, false, true);
	std::string label = text_box->label();
	if (label.empty()) {
		ret = nposm;
		return true;
	}
	if (!utils::isinteger(label)) {
		show_id_error(id, _("must be integer."));
		return false;
	}

	ret = utils::to_int(label);
	if (ret < 0) {
		show_id_error(id, _("must be >= 0."));
		return false;
	}
	return true;
}

bool tcontrol_setting::save_advanced(twindow& window)
{
	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);

	if (u_.is_tree()) {
		cell_.widget.tree_view.foldable = find_widget<ttoggle_button>(advanced_page, "foldable", false, true)->get_value();

		ttext_box* text_box = find_widget<ttext_box>(advanced_page, "indention_step_size", false, true);
		cell_.widget.tree_view.indention_step_size = lexical_cast_default<int>(text_box->label());
		text_box = find_widget<ttext_box>(advanced_page, "node_id", false, true);
		cell_.widget.tree_view.node_id = text_box->label();
		if (cell_.widget.tree_view.node_id.empty()) {
			show_id_error("node's id", _("Can not empty!"));
			return false;
		}
	}

	if (u_.is_slider()) {
		ttext_box* text_box = find_widget<ttext_box>(advanced_page, "minimum_value", false, true);
		cell_.widget.slider.minimum_value = lexical_cast_default<int>(text_box->label());
		text_box = find_widget<ttext_box>(advanced_page, "maximum_value", false, true);
		cell_.widget.slider.maximum_value = lexical_cast_default<int>(text_box->label());
		if (cell_.widget.slider.minimum_value >= cell_.widget.slider.maximum_value) {
			show_id_error("Minimum or Maximum value", _("Maximum value must large than minimum value!"));
			return false;
		}
		text_box = find_widget<ttext_box>(&window, "step_size", false, true);
		cell_.widget.slider.step_size = lexical_cast_default<int>(text_box->label());
		if (cell_.widget.slider.step_size <= 0) {
			show_id_error("Step size", _("Step size must > 0!"));
			return false;
		}
		if (cell_.widget.slider.step_size >= cell_.widget.slider.maximum_value - cell_.widget.slider.minimum_value) {
			show_id_error("Step size", _("Step size must < maximum_value - minimum_value!"));
			return false;
		}
	}

	if (u_.is_text_box()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "text_box_desensitize", false, true);
		cell_.widget.text_box.desensitize = toggle->get_value();
	}

	if (u_.is_report()) {
		if (cell_.widget.report.multi_line) {
			ttext_box* text_box = find_widget<ttext_box>(&window, "report_fixed_cols", false, true);
			cell_.widget.report.fixed_cols = lexical_cast_default<int>(text_box->label());
			if (cell_.widget.report.fixed_cols < 0) {
				show_id_error(_("Fixed clos"), _("must not be less than 0!"));
				return false;
			}

		} else {
			cell_.widget.report.segment_switch = find_widget<ttoggle_button>(&window, "report_segment_switch", false, true)->get_value();
		}
		if (cell_.widget.report.toggle) {
			cell_.widget.report.multi_select = find_widget<ttoggle_button>(&window, "report_multi_select", false, true)->get_value();
		} else {
			cell_.widget.report.multi_select = false;
		}

		ttext_box* text_box = find_widget<ttext_box>(advanced_page, "unit_width", false, true);
		cell_.widget.report.unit_width = text_box->label();
		utils::lowercase2(cell_.widget.report.unit_width);
		if (!verify_formula("unit_width's formula", cell_.widget.report.unit_width)) {
			return false;
		}
		
		text_box = find_widget<ttext_box>(advanced_page, "unit_height", false, true);
		cell_.widget.report.unit_height = text_box->label();
		utils::lowercase2(cell_.widget.report.unit_height);
		if (!verify_formula("unit_height's formula", cell_.widget.report.unit_height)) {
			return false;
		}
		text_box = find_widget<ttext_box>(&window, "gap", false, true);
		int gap = lexical_cast_default<int>(text_box->label());
		if (gap < 0) {
			show_id_error(_("Gap"), _("must not be less than 0!"));
			return false;
		}

		cell_.widget.report.gap = gap;
	}

	if (u_.has_atom_markup()) {
		ttoggle_button* toggle = find_widget<ttoggle_button>(advanced_page, "atom_markup", false, true);
		cell_.widget.atom_markup = toggle->get_value();
	}

	if (u_.has_margin()) {
		if (!save_margin(*advanced_page, "_left_margin", cell_.widget.margin.left)) {
			return false;
		}
		if (!save_margin(*advanced_page, "_right_margin", cell_.widget.margin.right)) {
			return false;
		}
		if (!save_margin(*advanced_page, "_top_margin", cell_.widget.margin.top)) {
			return false;
		}
		if (!save_margin(*advanced_page, "_bottom_margin", cell_.widget.margin.bottom)) {
			return false;
		}
	}

	return true;
}

void tcontrol_setting::set_horizontal_layout()
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;
	unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;

	int initial_at = nposm;
	for (std::map<int, talign>::const_iterator it = horizontal_align.begin(); it != horizontal_align.end(); ++ it) {
		ss.str("");
		ss << ht::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (h_flag == it->first) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	h_flag = values[dlg.cursel()];
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::HORIZONTAL_MASK) | h_flag;

	set_horizontal_layout_label();
}

void tcontrol_setting::set_vertical_layout()
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;
	unsigned v_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;

	int initial_at = nposm;
	for (std::map<int, talign>::const_iterator it = vertical_align.begin(); it != vertical_align.end(); ++ it) {
		ss.str("");
		ss << ht::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (v_flag == it->first) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	v_flag = values[dlg.cursel()];
	cell_.widget.cell.flags_ = (cell_.widget.cell.flags_ & ~tgrid::VERTICAL_MASK) | v_flag;

	set_vertical_layout_label();
}

void tcontrol_setting::set_horizontal_layout_label()
{
	tgrid* base_page = page_panel_->layer(BASE_PAGE);

	const unsigned h_flag = cell_.widget.cell.flags_ & tgrid::HORIZONTAL_MASK;
	std::stringstream ss;

	ss << horizontal_align.find(h_flag)->second.description;
	tlabel* label = find_widget<tlabel>(base_page, "_horizontal_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_vertical_layout_label()
{
	tgrid* base_page = page_panel_->layer(BASE_PAGE);

	const unsigned v_flag = cell_.widget.cell.flags_ & tgrid::VERTICAL_MASK;
	std::stringstream ss;

	ss << vertical_align.find(v_flag)->second.description;
	tlabel* label = find_widget<tlabel>(base_page, "_vertical_layout", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_linked_group()
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int index = -1;
	int initial_at = nposm;
	items.push_back("");
	values.push_back(index ++);
	for (std::vector<tlinked_group>::const_iterator it = linkeds_.begin(); it != linkeds_.end(); ++ it) {
		const tlinked_group& linked = *it;
		ss.str("");
		ss << linked.id;
		if (linked.fixed_width) {
			ss << "  " << ht::generate_format("width", color_to_uint32(font::BLUE_COLOR));
		}
		if (linked.fixed_height) {
			ss << "  " << ht::generate_format("height", color_to_uint32(font::BLUE_COLOR));
		}
		if (cell_.widget.linked_group == linked.id) {
			initial_at = index;
		}
		items.push_back(ss.str());
		values.push_back(index ++);
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	index = values[dlg.cursel()];
	if (index >= 0) {
		cell_.widget.linked_group = linkeds_[index].id;
	} else {
		cell_.widget.linked_group.clear();
	}

	set_linked_group_label();
}

void tcontrol_setting::set_linked_group_label()
{
	tgrid* base_page = page_panel_->layer(BASE_PAGE);
	std::stringstream ss;

	ss << cell_.widget.linked_group;
	ttext_box* text_box = find_widget<ttext_box>(base_page, "_linked_group", false, true);
	text_box->set_label(ss.str());
	text_box->set_active(false);

	if (linkeds_.empty()) {
		find_widget<tbutton>(base_page, "_set_linked_group", false, true)->set_active(false);
	}
}

void tcontrol_setting::set_textdomain(bool label)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;
	std::string& textdomain = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	
	int index = -1;
	int initial_at = nposm;
	items.push_back("");
	values.push_back(index ++);
	for (std::vector<std::string>::const_iterator it = textdomains_.begin(); it != textdomains_.end(); ++ it) {
		ss.str("");
		ss << *it;
		items.push_back(ss.str());
		values.push_back(index ++);
		if (*it == textdomain) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	index = values[dlg.cursel()];
	if (index >= 0) {
		textdomain = textdomains_[index];
	} else {
		textdomain.clear();
	}

	set_textdomain_label(label);
}

void tcontrol_setting::set_textdomain_label(bool label)
{
	tgrid* base_page = page_panel_->layer(BASE_PAGE);
	std::stringstream ss;

	const std::string id = label? "textdomain_label": "textdomain_tooltip";
	std::string str = label? cell_.widget.label_textdomain: cell_.widget.tooltip_textdomain;
	if (str.empty()) {
		str = "---";
	}

	tbutton* button = find_widget<tbutton>(base_page, id, false, true);
	button->set_label(str);

	if (textdomains_.empty()) {
		button->set_active(false);
	}
}

//
// size page
//
void tcontrol_setting::click_size_is_max(bool width)
{
	tgrid* size_page = page_panel_->layer(SIZE_PAGE);

	bool& value = width? cell_.widget.width_is_max: cell_.widget.height_is_max;

	if (value) {
		value = false;
	} else {
		value = true;
	}
	set_size_is_max_label(width);

	did_best_size_changed(find_widget<ttext_box>(size_page, width? "_width": "_height", false), true);
}

void tcontrol_setting::set_size_is_max_label(bool width)
{
	bool& value = width? cell_.widget.width_is_max: cell_.widget.height_is_max;

	std::string type_str;
	if (value) {
		type_str = width? _("Max width"): _("Max height");
	} else {
		type_str = width? _("Best width"): _("Best height");
	}

	tgrid* size_page = page_panel_->layer(SIZE_PAGE);
	tbutton& button = find_widget<tbutton>(size_page, width? "width_is_max": "height_is_max", false);
	button.set_label(type_str);
}

void tcontrol_setting::did_best_size_changed(ttext_box& widget, const bool width)
{
	const std::string label = widget.label();
	std::string description;
	utils::string_map symbols;

	bool is_max = width? cell_.widget.width_is_max: cell_.widget.height_is_max;
	symbols["field"] = width? _("width"): _("height");
	symbols["size"] = label;

	if (label.empty()) {
		description = vgettext2("The $field calculated according to the content at that time is the best $field.", symbols);

	} else {
		if (is_max) {
			if (utils::isinteger(label)) {
				description = vgettext2("The $field calculated according to the content at that time is the best $field, but maximum should not exceed cfg_2_os_size($size).", symbols);
			} else {
				description = vgettext2("The $field calculated according to the content at that time is the best $field, but maximum should not exceed ($size) * hdpi_scale.", symbols);
			}
		} else {
			if (utils::isinteger(label)) {
				description = vgettext2("Best $field is fixed to cfg_2_os_size($size).", symbols);
			} else {
				description = vgettext2("Best $field is fixed to ($size) * hdpi_scale.", symbols);
			}
		}
	}

	tgrid* size_page = page_panel_->layer(SIZE_PAGE);
	find_widget<tlabel>(size_page, width? "_width_description": "_height_description", false).set_label(description);
}

void tcontrol_setting::did_min_text_width_changed(ttext_box& widget)
{
	std::string label = widget.label();
	if (label.empty()) {
		label = "0";
	}
	std::string description;
	utils::string_map symbols;

	symbols["field"] = _("min_text_width");
	symbols["size"] = label;

	if (utils::isinteger(label)) {
		description = vgettext2("$field is fixed to cfg_2_os_size(max(cfg->min_width, $size)).", symbols);
	} else {
		description = vgettext2("$field is fixed to max(cfg_2_os_size(cfg->min_width), ($size) * hdpi_scale).", symbols);
	}

	tgrid* size_page = page_panel_->layer(SIZE_PAGE);
	find_widget<tlabel>(size_page, "_min_text_width_description", false).set_label(description);
}

//
// avcanced page
//
void tcontrol_setting::set_horizontal_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;
	int initial_at = nposm;

	for (std::map<int, tscroll_mode>::const_iterator it = horizontal_mode.begin(); it != horizontal_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (it->first == cell_.widget.horizontal_mode) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.widget.horizontal_mode = (tscroll_container::tscrollbar_mode)values[dlg.cursel()];

	set_horizontal_mode_label(window);
}

void tcontrol_setting::set_horizontal_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << horizontal_mode.find(cell_.widget.horizontal_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_mode", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_vertical_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;
	int initial_at = nposm;

	for (std::map<int, tscroll_mode>::const_iterator it = vertical_mode.begin(); it != vertical_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(ss.str());
		values.push_back(it->first);
		if (it->first == cell_.widget.vertical_mode) {
			initial_at = items.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	cell_.widget.vertical_mode = (tscroll_container::tscrollbar_mode)values[dlg.cursel()];

	set_vertical_mode_label(window);
}

void tcontrol_setting::set_vertical_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << vertical_mode.find(cell_.widget.vertical_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_mode", false, true);
	label->set_label(ss.str());
}

void tcontrol_setting::set_definition(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	const std::string current_definition = utils::generate_app_prefix_id(u_.widget().second->app, u_.widget().second->id);

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(u_.widget().first)->second;

	int index = 0;
	int def = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin(); it != definitions.end(); ++ it) {
		ss.str("");
		std::pair<std::string, std::string> pair = utils::split_app_prefix_id(it->first);
		ss << ht::generate_format(pair.second, color_to_uint32(font::BLUE_COLOR));
		if (!pair.first.empty()) {
			ss << " [" << pair.first << "]";
		}
		if (!it->second->description.str().empty()) {
			ss << " (" << it->second->description.str() << ")";
		}
		if (it->first == current_definition) {
			def = index;
		}
		items.push_back(ss.str());
		values.push_back(index ++);
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show();

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin();
	std::advance(it, values[dlg.cursel()]);
	u_.set_widget_definition(it->second);
	update_title(window);

	set_definition_label(window, it->first);
}

void tcontrol_setting::set_definition_label(twindow& window, const std::string& id2)
{
	std::stringstream ss;

	std::pair<std::string, std::string> pair = utils::split_app_prefix_id(id2);
	ss.str("");
	ss << pair.second;
	if (!pair.first.empty()) {
		ss << "[" << pair.first << "]";
	}
	find_widget<tlabel>(&window, "_definition", false).set_label(ss.str());
}

static const char* text_font_size_names[] = {
	"smaller",
	"small",
	"default",
	"large(title)",
	"larger"
};

void tcontrol_setting::set_text_font_size()
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	VALIDATE(font_max_cfg_size_diff == 2, null_str);
	int initial_at = nposm;
	for (int diff = -2; diff <= 2; diff ++) {
		items.push_back(text_font_size_names[diff + 2]);
		values.push_back(font_cfg_reference_size + diff);
		if (cell_.widget.text_font_size == values.back()) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	
	cell_.widget.text_font_size = values[dlg.cursel()];
	set_text_font_size_label();
}

void tcontrol_setting::set_text_font_size_label()
{
	int diff = cell_.widget.text_font_size - font_cfg_reference_size;
	VALIDATE(posix_abs(diff) <= font_max_cfg_size_diff, null_str);

	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);
	std::stringstream ss;
	ss << text_font_size_names[diff + font_max_cfg_size_diff];
	find_widget<tlabel>(advanced_page, "_text_font_size", false).set_label(ss.str());
}

void tcontrol_setting::set_text_color_tpl(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = 0;
	for (int n = theme::default_tpl; n < theme::color_tpls; n ++) {
		items.push_back(theme::color_tpl_name(n));
		values.push_back(n);
		if (cell_.widget.text_color_tpl == values.back()) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}
	
	cell_.widget.text_color_tpl = values[dlg.cursel()];
	set_text_color_tpl_label(window);
}

void tcontrol_setting::set_text_color_tpl_label(twindow& window)
{
	find_widget<tbutton>(&window, "_set_text_color_tpl", false).set_label(theme::color_tpl_name(cell_.widget.text_color_tpl));
}

void tcontrol_setting::set_stack_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	int initial_at = nposm;
	for (int n = 0; n < tstack::modes; n ++) {
		ss.str("");
		ss << implementation::form_stack_mode_str((tstack::tmode)n);
		items.push_back(ss.str());
		values.push_back(n);
		if (cell_.widget.stack.mode == values.back()) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	tstack::tmode ret = (tstack::tmode)values[dlg.cursel()];
	if (ret == cell_.widget.stack.mode) {
		return;
	}
	cell_.widget.stack.mode = ret;
	find_widget<tbutton>(&window, "set_stack_mode", false).set_label(implementation::form_stack_mode_str(ret));
}

void tcontrol_setting::set_multi_line(twindow& window)
{
	cell_.widget.report.multi_line = !cell_.widget.report.multi_line;
	set_multi_line_label(window);
}

void tcontrol_setting::set_multi_line_label(twindow& window)
{
	tbutton* button = find_widget<tbutton>(&window, "multi_line", false, true);
	button->set_label(cell_.widget.report.multi_line? _("Multi line"): _("Tabbar"));

	if (cell_.widget.report.multi_line) {
		find_widget<tgrid>(&window, "report_multiline_grid", false, true)->set_visible(twidget::VISIBLE);
		find_widget<tgrid>(&window, "report_tabbar_grid", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<ttext_box>(&window, "report_fixed_cols", false, true)->set_label(str_cast(cell_.widget.report.fixed_cols));
	} else {
		find_widget<tgrid>(&window, "report_multiline_grid", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<tgrid>(&window, "report_tabbar_grid", false, true)->set_visible(twidget::VISIBLE);
		find_widget<ttoggle_button>(&window, "report_segment_switch", false, true)->set_value(cell_.widget.report.segment_switch);
	}
}

void tcontrol_setting::set_label_state(tbutton& widget)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	std::map<int, std::string> options;
	options.insert(std::make_pair(tlabel::ENABLED, "Enabled"));
	options.insert(std::make_pair(tlabel::DISABLED, "Disabled"));

	int at = tlabel::ENABLED;
	int initial_at = nposm;
	for (std::map<int, std::string>::const_iterator it = options.begin(); it != options.end(); ++ it, at ++) {
		ss.str("");
		ss << it->second;
		items.push_back(it->second);
		if (at == cell_.widget.label_widget.state) {
			initial_at = at;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();
	int retval = dlg.get_retval();
	if (retval != twindow::OK) {
		return;
	}

	cell_.widget.label_widget.state = tlabel::ENABLED + dlg.cursel();
	set_label_state_label(widget);
}

void tcontrol_setting::set_label_state_label(tbutton& widget)
{
	std::string label = cell_.widget.label_widget.state == gui2::tlabel::ENABLED? "Enabled": "Disabled";
	widget.set_label(label);
}

void tcontrol_setting::click_report_toggle(tgrid& grid)
{
	cell_.widget.report.toggle = !cell_.widget.report.toggle;

	tbutton* button = find_widget<tbutton>(&grid, "report_toggle", false, true);
	button->set_label(cell_.widget.report.toggle? _("Toggle button"): _("Button"));

	find_widget<ttoggle_button>(&grid, "report_multi_select", false, true)->set_visible(cell_.widget.report.toggle? twidget::VISIBLE: twidget::INVISIBLE);

	cell_.widget.report.definition = "default";
	set_report_definition_label(grid);
}

void tcontrol_setting::click_report_definition(tgrid& grid)
{
	std::stringstream ss;
	std::vector<std::string> items;
	std::vector<int> values;

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::gui.control_definition;
	const std::string control = cell_.widget.report.toggle? "toggle_button": "button";
	const std::map<std::string, gui2::tcontrol_definition_ptr>& definitions = controls.find(control)->second;

	int index = 0;
	int initial_at = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin(); it != definitions.end(); ++ it) {
		ss.str("");
		ss << ht::generate_format(it->first, color_to_uint32(font::BLUE_COLOR)) << "(" << it->second->description.str() << ")";
		items.push_back(ss.str());
		values.push_back(index ++);
		if (it->first == cell_.widget.report.definition) {
			initial_at = values.size() - 1;
		}
	}

	gui2::tcombo_box dlg(items, initial_at);
	dlg.show();

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = definitions.begin();
	std::advance(it, values[dlg.cursel()]);
	cell_.widget.report.definition = it->first;

	set_report_definition_label(grid);
}

void tcontrol_setting::set_report_definition_label(tgrid& grid)
{
	find_widget<tlabel>(&grid, "report_definition_label", false, true)->set_label(cell_.widget.report.definition);
}

void tcontrol_setting::did_report_fixed_cols_changed(ttext_box& widget)
{
	const std::string label = widget.label();
	int value = nposm;
	if (!label.empty() && utils::isinteger(label)) {
		value = lexical_cast<int>(label);
	}
	if (value >= 0) {
		cell_.widget.report.fixed_cols = value;
	}

	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);
	ttext_box* text_box = find_widget<ttext_box>(advanced_page, "unit_width", false, true);
	did_report_unit_size_changed(*text_box, true);
}

void tcontrol_setting::did_report_unit_size_changed(ttext_box& widget, const bool width)
{
	const std::string label = widget.label();
	std::string description;
	utils::string_map symbols;

	symbols["field"] = width? _("width"): _("height");
	symbols["size"] = label;

	bool best_size_as_size = label.empty();
	if (!label.empty() && utils::isinteger(label)) {
		best_size_as_size = lexical_cast<int>(label) == 0;
	}

	if (width && cell_.widget.report.multi_line && cell_.widget.report.fixed_cols) {
		symbols["fixed_cols"] = str_cast(cell_.widget.report.fixed_cols);
		description = vgettext2("The report control's current $field is divided by $fixed_cols as the unit $field.", symbols);

	} else if (best_size_as_size) {
		description = vgettext2("The best $field calculated by the item at that time is the unit $field.", symbols);

	} else {
		if (utils::isinteger(label)) {
			description = vgettext2("Unit $field is fixed to cfg_2_os_size($size).", symbols);
		} else {
			description = vgettext2("Unit $field is fixed to ($size) * hdpi_scale.", symbols);
		}
	}

	tgrid* advanced_page = page_panel_->layer(ADVANCED_PAGE);
	find_widget<tlabel>(advanced_page, width? "unit_width_description": "unit_height_description", false).set_label(description);
}

}
