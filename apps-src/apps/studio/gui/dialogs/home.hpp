/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_HOME_HPP_INCLUDED
#define GUI_DIALOGS_HOME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "lobby.hpp"
#include "build.hpp"
#include "theme.hpp"
#include "builder.hpp"

class hero_map;
class hero;
class tapp_copier;
class tfile;

namespace gui2 {

class tbutton;
class ttree_node;
class ttoggle_button;
class ttoggle_panel;
class ttrack;
class ttext_box;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class thome : public tdialog, public tlobby::thandler, public tbuild
{
public:
	thome(hero& player_hero);
	~thome();

	/**
	 * Values for the menu-items of the main menu.
	 *
	 * @todo Evaluate the best place for these items.
	 */
	enum tresult {
			WINDOW = 1     /**< Let user select a campaign to play */
			, ANIMATION
			, DESIGN
			, CHANGE_LANGUAGE
			, EDIT_PREFERENCES
			, OPENCV
			, MESSAGE
			, QUIT_GAME
			, NOTHING             /**<
			                       * Default, nothing done, no redraw needed
			                       */
			};

	bool handle(int tag, tsock::ttype type, const config& data) override;
	bool walk_dir2(const std::string& dir, const SDL_dirent2* dirent, ttree_node* root);
	const std::map<std::string, std::string>& get_app_tdomains() const { return tdomains; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	void set_retval(twindow& window, int retval);
	void did_toolbar_item_click(tbutton& widget);
	void did_explorer_icon_folded(ttree_node& node, const bool folded);
	void did_left_double_click_explorer(ttree_node& node);
	void did_wml_folded(ttree_node& node, const bool folded);

	bool compare_sort(const ttree_node& a, const ttree_node& b);
	void fill_items(twindow& window);

	void pre_base(twindow& window);

	void check_build_toggled(twidget& widget);
	void click_refresh(twindow& window);
	void click_build(twindow& window, const bool refresh_listbox);

	void do_build(twindow& window);
	void set_build_active(twindow& window);

	void app_work_start() override;
	void app_work_done() override;
	void app_handle_desc(const bool started, const int at, const bool ret) override;

	void on_change_working_dir(twindow& window, const std::string& dir);
	void build_on_app_changed(const std::string& app, twindow& window, const bool remove);

	void right_click_explorer(ttree_node& node, const tpoint& coordinate);
	void new_app(const tapp_capabilities& capabilities);
	void new_aplt(const std::string& bundleid);
	void do_new_window(tapp_copier& copier, const std::string& id, bool scene, bool unit_files = false);
	void do_new_window_aplt(const std::string& lua_bundleid, const std::string& id);
	void do_remove_dialog(tapp_copier& copier, const std::string& id, bool scene);
	void do_new_theme(tapp_copier& copier, const std::string& id, const theme::tfields& result);
	bool export_files_must_exist(const tapp_copier& copier) const;
	void export_app(tapp_copier& copier);
	bool zip_kos_res(const tapp_copier& copier, bool silent);
	bool generate_aplt_rsp(const std::string& lua_bundleid, const version_info& aplt_version);
	bool copy_android_res(const tapp_copier& copier, bool silent);
	bool export_ios_kit(const tapp_copier& copier);
	void import_apps(const std::string& import_res);

	ttree_node& add_explorer_node(const std::string& dir, ttree_node& parent, const std::string& name, bool isdir);

	void validater_res(const std::set<std::string>& apps);
	void refresh_explorer_tree(twindow& window);

	void generate_gui_app_main_cfg(const std::string& res_path, const std::set<std::string>& apps) const;
	void generate_app_cfg(const std::string& res_path, const std::set<std::string>& apps) const;
	void generate_app_cfg(const std::string& res_path, const std::string& app) const
	{
		std::set<std::string> apps;
		apps.insert(app);
		generate_app_cfg(res_path, apps);
	}
	void generate_capabilities_cfg(const std::string& res_path) const;
	void generate_capabilities_cfg2(const std::string& res_path, const tapp_capabilities& copier) const;
	void reload_mod_configs();

	bool collect_app(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& apps);
	bool collect_app2(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::unique_ptr<tapp_copier> >& app_copiers);
	bool browse_import_res_changed(const std::string& path, const std::string& terminate);

	// wml section
	void wml_2_tv(const std::string& name, const config& cfg, const uint32_t maxdeep);

	// building rule section
	void br_2_tv(const std::string& name, const terrain_builder::building_rule* rules, const int rules_size);
	void br_2_tv_fields(ttree_node& root, const terrain_builder::building_rule& rule);
	void br_release_heap();

	enum {build_normal, build_export, build_import, build_new, build_remove, build_new_dialog, build_ios_kit};
	class build_msg_data: public rtc::MessageData {
	public:
		build_msg_data(int _type, const std::string& _app) 
		{ 
			set(_type, _app); 
		}
		void set(int _type, const std::string& _app) 
		{
			type = _type;
			app = _app;
		}
		void set(int _type, const std::set<std::string>& _apps) 
		{
			type = _type;
			apps = _apps;
		}
		void set(int _type, const std::string& _app, const std::string& val) 
		{
			type = _type;
			app = _app;
			apps.clear();
			apps.insert(val);
		}

		int type;
		std::string app;
		std::set<std::string> apps;
	};

	enum {MSG_BUILD_FINISHED = POST_MSG_MIN_APP};
	void app_OnMessage(rtc::Message* msg) override;

	bool compare_row(const ttoggle_panel& row1, const ttoggle_panel& row2);

	void did_draw_splitter(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn);

	struct tcpp_file {
		tcpp_file(const std::string& name)
			: name(name)
		{}

		std::string name;
		std::set<std::string> ops;
		std::set<std::string> depends;
	};
	bool walk_kernels(const std::string& dir, const SDL_dirent2* dirent, std::vector<tcpp_file>& files);
	bool walk_ops(const std::string& dir, const SDL_dirent2* dirent, std::vector<tcpp_file>& files);

private:
	hero& player_hero_;

	std::string app_bin_id_;
	build_msg_data build_msg_data_;

	const tapp_copier* current_copier_;
	const uint32_t gap_argb_;

	struct tcookie {
		tcookie(std::string dir, std::string name, bool isdir)
			: dir(dir)
			, name(name)
			, isdir(isdir)
		{}

		std::string dir;
		std::string name;
		bool isdir;
	};
	std::map<const ttree_node*, tcookie> cookies_;

	// why use std::vector<std::unique_ptr<tapp_copier> > insteal with std::vector<tapp_copier>.
	// deriver class from ttask will use tapp_copier instance, so tapp_copier must not make another one.
	std::vector<std::unique_ptr<tapp_copier> > app_copiers;
	std::unique_ptr<tnewer> newer;
	std::unique_ptr<taplt_newer> aplt_newer;
	std::unique_ptr<timporter> importer;
	std::unique_ptr<tremover> remover;
	std::unique_ptr<taplt_remover> aplt_remover;
	std::unique_ptr<tnew_dialog> new_dialog;
	std::unique_ptr<taplt_new_dialog> aplt_new_dialog;
	std::unique_ptr<tnew_scene> new_scene;
	std::unique_ptr<tvalidater> validater;
	std::unique_ptr<taplt_validater> aplt_validater;
	std::unique_ptr<tsave_theme> save_theme;
	std::unique_ptr<tremove_theme> remove_theme;
	std::unique_ptr<topencv_header> opencv_header;

	std::pair<std::string, config> current_wml_;
	// building rule section
	terrain_builder::building_rule* br_rules_;
	uint32_t br_rules_size_;

	config generate_cfg;
};

} // namespace gui2

#endif
