/* $Id: lobby_player_info.hpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#ifndef GUI_DIALOGS_MKWIN_THEME_HPP_INCLUDED
#define GUI_DIALOGS_MKWIN_THEME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/dialogs/build.hpp"

class mkwin_display;
class mkwin_controller;
class unit_map;

namespace gui2 {

class treport;
class tstack;
class tlistbox;

#define LAYER_CONTEXT_MENU		0
#define LAYER_TASK_STATUS		1

class tmkwin_scene: public tdialog, public tbuild
{
public:
	enum { UNIT_NAME, UNIT_TYPE,
		    UNIT_STATUS,
		    UNIT_HP, UNIT_XP,
		    UNIT_SECOND,
		    UNIT_IMAGE, 
			
			TIME_OF_DAY,
		    TURN, GOLD, VILLAGES, UPKEEP,
		    INCOME, TECH_INCOME, TACTIC, POSITION, STRATUM, MERITORIOUS, SIDE_PLAYING, OBSERVERS,
			REPORT_COUNTDOWN, REPORT_CLOCK, EDITOR_SELECTED_TERRAIN, 
			EDITOR_LEFT_BUTTON_FUNCTION, EDITOR_TOOL_HINT, NUM_REPORTS
	};
	enum { UNIT_REPORTS_BEGIN = UNIT_NAME, UNIT_REPORTS_END = UNIT_IMAGE + 1 };
	enum { STATUS_REPORTS_BEGIN = TIME_OF_DAY, STATUS_REPORTS_END = EDITOR_TOOL_HINT };

	enum {
		HOTKEY_STATUS = HOTKEY_MIN,
		HOTKEY_RCLICK,

		HOTKEY_BUILD, HOTKEY_PREVIEW,
		HOTKEY_SETTING, HOTKEY_SPECIAL_SETTING, HOTKEY_LINKED_GROUP, HOTKEY_ERASE,
		HOTKEY_INSERT_TOP, HOTKEY_INSERT_BOTTOM, HOTKEY_ERASE_ROW,
		HOTKEY_INSERT_LEFT, HOTKEY_INSERT_RIGHT, HOTKEY_ERASE_COLUMN,
		HOTKEY_INSERT_CHILD, HOTKEY_ERASE_CHILD, HOTKEY_UNPACK, HOTKEY_PACK
	};

	enum {WIDGET_PAGE, OBJECT_PAGE};

	tmkwin_scene(mkwin_display& disp, mkwin_controller& controller);
	void report_ptr(treport** unit_ptr, treport** hero_ptr, treport** ctrl_bar_ptr);

	void load_widget_page();
	void load_object_page(const unit_map& units);

	void fill_object_list(const unit_map& units);

	bool require_build();
	void do_build();

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_show() override;
	void object_selected(twindow& window, tlistbox& listobx);

	void app_work_start() override;
	void app_work_done() override;
	void app_handle_desc(const bool started, const int at, const bool ret)  override {}

	void did_first_layouted(twindow& window) override;

private:
	mkwin_controller& controller_;
	tstack* page_panel_;
	tstack* context_menu_panel_;
	int current_widget_page_;
};

} //end namespace gui2

#endif
