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

#ifndef GUI_DIALOGS_WINDOW_SETTING_HPP_INCLUDED
#define GUI_DIALOGS_WINDOW_SETTING_HPP_INCLUDED

#include "gui/dialogs/cell_setting.hpp"
#include "mkwin_controller.hpp"
#include <vector>

class mkwin_controller;
class unit;

namespace gui2 {

class tstack;
class tlistbox;
class treport;
class ttoggle_button;

class twindow_setting : public tsetting_dialog
{
public:
	enum {BASE_PAGE, CONTEXT_MENU_PAGE, FLOAT_WIDGET_PAGE};
	enum {AUTOMATIC_PAGE, MANUAL_PAGE};

	explicit twindow_setting(mkwin_controller& controller, const unit& u, const std::vector<std::string>& textdomains);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	bool did_item_pre_change_report(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_item_changed_report(treport& report, ttoggle_button& widget);

	void pre_base(twindow& window);
	void pre_context_menu(twindow& window);
	void pre_float_widget(twindow& window);

	bool save_base(twindow& window);
	bool save_context_menu(twindow& window);
	bool save_float_widget(twindow& window, const int at);

	void fill_automatic_page(twindow& window);
	void fill_manual_page(twindow& window);

	void set_app(twindow& window);
	void set_app_label(twindow& window);

	void set_orientation(twindow& window);
	void set_orientation_label(twindow& window);

	void set_tile_shape(twindow& window);
	void set_tile_shape_label(twindow& window);

	void set_definition(twindow& window);
	void set_definition_label(twindow& window);
	void set_horizontal_layout(twindow& window);
	void set_vertical_layout(twindow& window);
	void set_horizontal_layout_label(twindow& window);
	void set_vertical_layout_label(twindow& window);

	void did_theme_toggled(twidget& widget);
	void automatic_placement_toggled(twidget& widget);
	void save(twindow& window, bool& handled, bool& halt);
	void swap_page(twindow& window, int page, bool swap);

	//
	// context menu page
	//
	void reload_menu_table(tmenu2& menu, twindow& window, int cursel);
	std::string get_menu_item_id(tmenu2& menu, twindow& window, int exclude);
	void reload_submenu_navigate(tmenu2& menu, twindow& window, const tmenu2* cursel);
	tmenu2* current_submenu() const;

	bool menu_pre_toggle_tabbar(twidget* widget, twidget* previous);
	void menu_toggle_tabbar(twidget* widget);

	bool submenu_pre_toggle_tabbar(twidget* widget, twidget* previous);
	void submenu_toggle_tabbar(twidget* widget);

	void append_menu_item(twindow& window);
	void modify_menu_item(twindow& window);
	void erase_menu_item(twindow& window);

	void map_menu_item_to(twindow& window, tmenu2& menu, int row);
	void item_selected(twindow& window, tlistbox& list);

	void refresh_parent_desc(twindow& window, tmenu2& menu);

	//
	// float_widget page
	//
	bool float_widget_pre_toggle_tabbar(twidget* widget, twidget* previous);
	void patch_toggle_tabbar(ttoggle_button& widget);
	std::string form_tab_label(treport& navigate, int at) const;

	void switch_float_widget(twindow& window);

	void append_float_widget(twindow& window);
	void erase_float_widget(twindow& window);
	void rename_float_widget(twindow& window);
	void change_float_widget_type(twindow& window);
	void edit_float_widget(twindow& window);
	void reload_float_widget(treport& navigate, twindow& window);
	void reload_float_widget_label(treport& navigate) const;

private:
	mkwin_controller& controller_;
	const unit& u_;

	const std::vector<std::string>& textdomains_;
	tstack* layout_panel_;

	tstack* bar_panel_;
	treport* bar_;
	int current_page_;
	std::vector<tmenu2>& menus_;

	treport* menu_navigate_;
	treport* submenu_navigate_;
	treport* patch_bar_;
	int float_widget_at_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
