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

#ifndef GUI_DIALOGS_BROWSE_HPP_INCLUDED
#define GUI_DIALOGS_BROWSE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"
#include <vector>
#include <set>

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;
class treport;
class ttext_box;

class tbrowse: public tdialog
{
public:
	enum {METHOD_ALPHA, METHOD_DATE};
	enum {TYPE_FILE = 0x1, TYPE_DIR = 0x2};

	struct tentry {
		tentry(const std::string& path, const std::string& label, const std::string& icon)
			: path(path)
			, label(label)
			, icon(icon)
		{}

		bool valid() const { return !path.empty(); }

		std::string path;
		std::string label;
		std::string icon;
	};

	struct tparam {
		tparam(int flags, bool readonly, const std::string& initial = null_str, 
			const std::string& title = null_str, const std::string& open = null_str)
			: flags(flags)
			, readonly(readonly)
			, initial(initial)
			, title(title)
			, open(open)
			, extra(null_str, null_str, null_str)
		{}

		int flags;
		bool readonly;
		std::string initial;
		std::string title;
		std::string open;
		std::string result;
		tentry extra;
	};

	struct tfile2 {
		tfile2(const std::string& name);

		bool operator<(const tfile2& that) const;

		std::string name;
		std::string lower;
		int method;
	};

	static const char path_delim;
	static const std::string path_delim_str;

	explicit tbrowse(tparam& param, bool only_extra_entry);

	void set_did_result_changed(const std::function<bool (const std::string& path, const std::string& terminate)>& callback)
	{
		did_result_changed_ = callback;
	}

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void init_entry(twindow& window);

	void did_item_selected(tlistbox& list, ttoggle_panel& widget);
	void did_item_double_click(tlistbox& list, ttoggle_panel& widget);
	void goto_higher();
	void click_navigate(twindow& window, tbutton& widget);

	void did_root_changed(tlistbox& list, ttoggle_panel& widget);

	void reload_navigate();
	void reload_file_table(int cursel);
	void add_row(twindow& window, tlistbox& list, const std::string& name, bool dir);
	void open(twindow& window, const int at);
	void update_file_lists();
	std::string get_path(const std::string& file_or_dir) const;

	void text_changed_callback(ttext_box& widget);
	void set_filename(const std::string& label);
	std::string calculate_result() const;

	void click_directory(ttoggle_panel& row);
	void signal_handler_longpress_item(bool& halt, const tpoint& coordinate, ttoggle_panel& row);
	enum {MSG_POPUP_DIRECTORY = POST_MSG_MIN_APP, MSG_OPEN_DIRECTORY};
	void app_OnMessage(rtc::Message* msg) override;

private:
	tparam& param_;
	const bool only_extra_entry_;

	std::function<bool (const std::string& path, const std::string& terminate)> did_result_changed_;

	treport* navigate_;
	tbutton* goto_higher_;
	ttext_box* filename_;
	tbutton* ok_;

	class tauto_select_lock
	{
	public:
		tauto_select_lock(tbrowse& browse)
			: browse_(browse)
		{
			VALIDATE(!browse_.auto_select_, null_str);
			browse_.auto_select_ = true;
		}
		~tauto_select_lock()
		{
			browse_.auto_select_ = false;
		}

	private:
		tbrowse& browse_;
	};
	bool auto_select_;

	std::vector<tentry> entries_;
	std::string current_dir_;
	std::string chosen_file_;
	std::set<tfile2> files_in_current_dir_;
	std::set<tfile2> dirs_in_current_dir_;
	std::vector<std::string> cookie_paths_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
