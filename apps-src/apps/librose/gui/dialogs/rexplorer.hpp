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

#ifndef GUI_DIALOGS_TEXPLORER_HPP_INCLUDED
#define GUI_DIALOGS_TEXPLORER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "util.hpp"
#include "serialization/string_utils.hpp"
#include <vector>
#include <set>

using namespace std::placeholders;

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;
class treport;
class ttext_box;
class tlabel;
class tstack;
class tgrid;

class trexplorer: public tdialog
{
public:
	struct tfile2 {
		tfile2(const std::string& name, bool dir);

		bool operator<(const tfile2& that) const;

		const std::string name;
		const std::string lower;
		const int method;
		const bool dir;
	};

	enum {PAPER_LAYER, EXPLORER_LAYER};
	enum {LT_CANCEL_LAYER, LT_LABEL_LAYER};
	struct tslot
	{
	public:
		tslot()
			: rexplorer_(nullptr)
			, window_(nullptr)
			, main_stack_(nullptr)
			, explorer_grid_(nullptr)
			, lt_stack_(nullptr)
			, file_list_(nullptr)
		{}

		virtual void rexplorer_pre_show(twindow& window) = 0;
		virtual void rexplorer_post_show() {};
		virtual void rexplorer_first_drawn() {};
		virtual void rexplorer_resize_screen() {};

		virtual void rexplorer_click_edit(tbutton& widget) = 0;
		virtual void rexplorer_timer_handler(uint32_t now) = 0;
		virtual void rexplorer_OnMessage(rtc::Message* msg) {}

		virtual void rexplorer_handler_mouse_motion(const int x, const int y) {};
		virtual void rexplorer_handler_longpress_item(const tpoint& coordinate, ttoggle_panel& widget) {}
		virtual void rexplorer_drag_started(const int x, const int y) {}
		virtual bool rexplorer_did_drag_mouse_motion(const int x, const int y) { return true; }
		virtual void rexplorer_drag_stoped(const int x, const int y, bool up_result) {}

	public:
		trexplorer* rexplorer_;
		twindow* window_;
		tstack* main_stack_;
		tgrid* explorer_grid_;
		tstack* lt_stack_;
		tlistbox* file_list_;
	};

	enum {MSG_OPEN_DIRECTORY = POST_MSG_MIN_APP, POST_MSG_MIN_SLOT};

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

	class tdisable_click_open_dir_lock
	{
	public:
		tdisable_click_open_dir_lock(trexplorer& rexplorer)
			: rexplorer_(rexplorer)
			, original_(rexplorer.click_open_dir_)
		{
			rexplorer_.click_open_dir_ = false;
		}
		~tdisable_click_open_dir_lock()
		{
			rexplorer_.click_open_dir_ = original_;
		}

	private:
		trexplorer& rexplorer_;
		const bool original_;
	};

	static const char path_delim;
	static const std::string path_delim_str;

	explicit trexplorer(tslot& slot, const std::string& initial, const tentry& extra, bool only_extra_entry, bool click_open_dir);

	void set_did_result_changed(const std::function<bool (const std::string& path, const std::string& terminate)>& callback)
	{
		did_result_changed_ = callback;
	}

	const std::string current_dir() const { return current_dir_; }
	twebrtc_send_helper& send_helper() { return send_helper_; }

	std::string row_full_name(ttoggle_panel& widget, int* type_ptr);
	std::string selected_full_name(int* type_ptr);
	std::vector<std::string> selected_full_multinames();
	tfile2 row_2_file2(int row) const;
	SDL_Point analysis_selected() const;
	void update_file_lists();
	void start_drag2(const tpoint& coordinate, int files, bool has_folder);

	surface get_drag_surface(int files, bool has_dir, const std::string& path);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void app_first_drawn() override;
	void app_resize_screen() override;
	void app_handler_mouse_motion(const tpoint& coordinate) override;

	void pre_lt_cancel(tgrid& grid);

	void init_entry(twindow& window);

	void click_edit(tbutton& widget);
	void click_multiselect(tbutton& widget);
	void click_new_folder(tbutton& widget);


	void click_multiselect_internal(tbutton& widget);
	void canel_multiselect();
	enum {edittype_newfolder, edittype_count};
	bool verify_edit_item(const std::string& label, const std::string& def, int type) const;
	void handle_edit_item(int type);

	void did_item_selected(tlistbox& list, ttoggle_panel& widget);
	void did_item_double_click(tlistbox& list, ttoggle_panel& widget);
	void goto_higher();
	void click_navigate(twindow& window, tbutton& widget);

	void did_root_changed(tlistbox& list, ttoggle_panel& widget);

	void reload_navigate();
	void reload_file_table(int cursel);
	void add_row(twindow& window, tlistbox& list, const std::string& name, bool dir);
	void open(twindow& window, const int at);
	std::string get_path(const std::string& file_or_dir) const;

	void set_summary_label();

	void click_directory(ttoggle_panel& row);

	void signal_handler_longpress_item(bool& halt,const tpoint& coordinate, ttoggle_panel& widget);
	bool did_drag_mouse_motion(const int x, const int y);
	void did_drag_mouse_leave(const int x, const int y, bool up_result);

	void app_timer_handler(uint32_t now) override;
	struct tmsg_open_directory: public rtc::MessageData {
		tmsg_open_directory(int row)
			: row(row)
		{}
		int row;
	};
	void app_OnMessage(rtc::Message* msg) override;

private:
	tslot& slot_;
	const bool support_longpress_;
	twebrtc_send_helper send_helper_;
	const std::string initial_;
	tentry extra_;
	const bool only_extra_entry_;

	std::function<bool (const std::string& path, const std::string& terminate)> did_result_changed_;

	treport* navigate_;
	tbutton* goto_higher_;
	tlistbox* file_list_;
	tlabel* summary_;
	tstack* main_stack_;
	tgrid* explorer_grid_;
	tstack* lt_stack_;

	bool click_open_dir_;

	std::vector<tentry> entries_;
	std::string current_dir_;
	std::string chosen_file_;
	std::set<tfile2> files_in_current_dir_;
	std::set<tfile2> dirs_in_current_dir_;
	std::vector<std::string> cookie_paths_;

	std::map<uint32_t, surface> drag_surfs_;
};


}


#endif /* ! GUI_DIALOGS_TEXPLORER_HPP_INCLUDED */
