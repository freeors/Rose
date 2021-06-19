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

#ifndef GUI_DIALOGS_MENU_HPP_INCLUDED
#define GUI_DIALOGS_MENU_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class ttoggle_panel;
class tlistbox;
class tbutton;
class ttext_box;

class tmenu: public tdialog
{
public:
	struct titem {
		static const int submeum_val = secondary_nposm;
		titem(const std::string& str, int val, const std::string& icon = "")
			: str(str)
			, val(val)
			, separator(false)
			, icon(icon)
		{
			VALIDATE(val != submeum_val, str);
		}
		titem(const std::string& str, const std::vector<titem>& submenu, const std::string& icon = "")
			: str(str)
			, val(submeum_val)
			, submenu(submenu)
			, separator(false)
			, icon(icon)
		{
			VALIDATE(!submenu.empty(), str);
		}

		void validate_unique_val(std::set<int>& vals) const;

		std::string str;
		int val;
		std::vector<titem> submenu;
		bool separator;
		std::string icon;
	};
	explicit tmenu(const std::vector<titem>& items, int val, const tmenu* parent = nullptr);
	
	int selected_val() const;

	const std::vector<titem>& items() const { return items_; }
	void request_close(int retval, int selected_val);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void validate_unique_val() const;

	void list_insert_row(const titem& item, tlistbox& list);
	void did_item_changed(tlistbox& list, ttoggle_panel& widget);
	void did_item_focus_changed(tlistbox& list, twidget& panel, const bool enter);
	bool did_leave_dismiss(const int x, const int y);
	bool did_click_dismiss_except(const int x, const int y);
	void did_key_changed(ttext_box& widget);
	const ttoggle_panel& focused_panel() const;
	void click_find(tbutton& widget);
	void show_tip(int items, int list_rows);

	enum {MSG_POPUP_MENU = POST_MSG_MIN_APP};
	void app_OnMessage(rtc::Message* msg) override;

private:
	const int show_find_min_items_;
	const int initial_val_;
	bool show_icon_;
	bool show_submenu_icon_;
	bool can_find_;
	bool motion_result_popup_;
	int cursel_;
	std::vector<titem> items_;
	const tmenu* parent_;
	const bool list_selectable_;
	bool mouse_motioned_;
	ttoggle_panel* focused_panel_;
	int selected_val_;
	bool request_invoke_popup_menu_;
	std::string last_key_;

	class tfind_result_lock
	{
	public:
		tfind_result_lock(tmenu& menu)
			: menu_(menu)
		{
			VALIDATE(!menu_.find_result_, null_str);
			menu_.find_result_ = true;
		}
		~tfind_result_lock()
		{
			menu_.find_result_ = false;
		}

	private:
		tmenu& menu_;
	};
	bool find_result_;
};


}


#endif /* ! GUI_DIALOGS_MENU_HPP_INCLUDED */
