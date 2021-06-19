/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_CONTROL_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include "unit.hpp"

class unit;
class mkwin_controller;

namespace gui2 {

struct tlinked_group;
class tstack;
class ttext_box;
class tbutton;

class tcontrol_setting : public tsetting_dialog
{
public:
	enum {BASE_PAGE, SIZE_PAGE, ADVANCED_PAGE};

	explicit tcontrol_setting(mkwin_controller& controller, unit& u, const std::vector<std::string>& textdomains, const std::vector<tlinked_group>& linkeds, bool float_widget);

protected:
	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void update_title(twindow& window);

	void save(twindow& window, bool& handled, bool& halt);

	void set_horizontal_layout();
	void set_vertical_layout();
	
	void set_horizontal_layout_label();
	void set_vertical_layout_label();

	void set_linked_group();
	void set_linked_group_label();

	void set_textdomain(bool label);
	void set_textdomain_label(bool label);

	bool did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_item_changed_report(treport& report, ttoggle_button& widget);

	void pre_base();
	void pre_size();
	void pre_advanced(twindow& window);

	bool save_base(twindow& window);
	bool save_size(twindow& window);
	bool save_advanced(twindow& window);

	//
	// size page
	//
	void click_size_is_max(bool width);
	void set_size_is_max_label(bool width);
	void did_best_size_changed(ttext_box& widget, const bool width);
	void did_min_text_width_changed(ttext_box& widget);

	//
	// advanced page
	//
	void set_horizontal_mode(twindow& window);
	void set_horizontal_mode_label(twindow& window);
	void set_vertical_mode(twindow& window);
	void set_vertical_mode_label(twindow& window);
	void set_definition(twindow& window);
	void set_definition_label(twindow& window, const std::string& id2);
	void set_text_font_size();
	void set_text_font_size_label();
	void set_text_color_tpl(twindow& window);
	void set_text_color_tpl_label(twindow& window);
	void set_multi_line(twindow& window);
	void set_multi_line_label(twindow& window);
	void set_label_state(tbutton& widget);
	void set_label_state_label(tbutton& widget);
	void click_report_toggle(tgrid& grid);
	void click_report_definition(tgrid& grid);
	void set_report_definition_label(tgrid& grid);
	void did_report_fixed_cols_changed(ttext_box& widget);
	void did_report_unit_size_changed(ttext_box& widget, const bool width);
	void set_stack_mode(twindow& window);

protected:
	mkwin_controller& controller_;
	unit& u_;
	const std::vector<std::string>& textdomains_;
	const std::vector<tlinked_group>& linkeds_;
	bool float_widget_;
	treport* bar_;
	tstack* page_panel_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
