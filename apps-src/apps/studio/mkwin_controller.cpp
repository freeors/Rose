/* $Id: editor_controller.cpp 47755 2010-11-29 12:57:31Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "studio-lib"

#include "mkwin_controller.hpp"
#include "mkwin_display.hpp"

#include "gettext.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "hotkeys.hpp"
#include "config_cache.hpp"
#include "preferences_display.hpp"
#include "xwml.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/system.hpp"
#include "gui/dialogs/row_setting.hpp"
#include "gui/dialogs/column_setting.hpp"
#include "gui/dialogs/window_setting.hpp"
#include "gui/dialogs/control_setting.hpp"
#include "gui/dialogs/linked_group.hpp"
#include "gui/dialogs/grid_setting.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/dialogs/new_tpl_widget.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/listbox.hpp"
#include "serialization/parser.hpp"

#include <boost/foreach.hpp>
using namespace std::placeholders;
#include <boost/foreach.hpp>
#include <numeric>

#include "gui/auxiliary/formula.hpp"

const std::string main_map_widget_id = "_main_map";
const std::string mini_map_widget_id = "_mini_map";

void square_fill_frame(t_translation::t_map& tiles, size_t start, const t_translation::t_terrain& terrain, bool front, bool back)
{
	if (front) {
		// first column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[start][n] = terrain;
		}
		// first line(border)
		for (size_t n = start + 1; n < tiles.size() - start; n ++) {
			tiles[n][start] = terrain;
		}
	}

	if (back) {
		// last column(border)
		for (size_t n = start; n < tiles[0].size() - start; n ++) {
			tiles[tiles.size() - start - 1][n] = terrain;
		}
		// last line(border)
		for (size_t n = start; n < tiles.size() - start - 1; n ++) {
			tiles[n][tiles[0].size() - start - 1] = terrain;
		}
	}
}

std::string generate_map_data(int width, int height, bool colorful)
{
	VALIDATE(width > 0 && height > 0, "map must not be empty!");

	const t_translation::t_terrain normal = t_translation::read_terrain_code("Gg");
	const t_translation::t_terrain border = t_translation::read_terrain_code("Gs");
	const t_translation::t_terrain control = t_translation::read_terrain_code("Gd");
	const t_translation::t_terrain forbidden = t_translation::read_terrain_code("Gll");

	t_translation::t_map tiles(width + 2, t_translation::t_list(height + 2, normal));
	if (colorful) {
		square_fill_frame(tiles, 0, border, true, true);
		square_fill_frame(tiles, 1, control, true, false);

		const size_t border_size = 1;
		tiles[border_size][border_size] = forbidden;
	}

	// tiles[border_size][tiles[0].size() - border_size - 1] = forbidden;

	// tiles[tiles.size() - border_size - 1][border_size] = forbidden;
	// tiles[tiles.size() - border_size - 1][tiles[0].size() - border_size - 1] = forbidden;

	std::string str = tmap::default_map_header + t_translation::write_game_map(t_translation::t_map(tiles));

	return str;
}

std::string generate_anchor_rect(int x, int y, int w, int h)
{
	std::stringstream ss;
	ss << x << "," << y << "," << x + w << "," << y + h;
	return ss.str();
}

tmenu2::~tmenu2()
{
	for (std::vector<titem>::iterator it = items.begin(); it != items.end(); ++ it) {
		titem& item = *it;
		if (item.submenu) {
			delete item.submenu;
			item.submenu = NULL;
		}
	}
}

tmenu2* tmenu2::top_menu()
{
	tmenu2* result = this;
	while (result->parent) {
		result = result->parent;
	}
	return result;
}

bool tmenu2::id_existed(const std::string& id) const
{
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (item.id == id) {
			return true;
		}
		if (item.submenu) {
			if (item.submenu->id_existed(id)) {
				return true;
			}
		}
	}
	return false;
}

void tmenu2::submenus(std::vector<tmenu2*>& result) const
{
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (item.submenu) {
			result.push_back(item.submenu);
			item.submenu->submenus(result);
		}
	}
}

void tmenu2::generate(config& cfg) const
{
	if (items.empty()) {
		return;
	}

	std::vector<tmenu2*> submenus;
	std::stringstream ss;
	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;
		if (!ss.str().empty()) {
			ss << ", ";
		}
		ss << item.id;
		if (item.submenu) {
			ss << "_m";
			submenus.push_back(item.submenu);
		}
		if (item.hide || item.param) {
			ss << "|";
			if (item.hide) {
				ss << "h";
			}
			if (item.param) {
				ss << "p";
			}
		}
	}
	if (!report.empty()) {
		cfg["report"] = report;
	}
	cfg[id] = ss.str();

	for (std::vector<tmenu2*>::const_iterator it = submenus.begin(); it != submenus.end(); ++ it) {
		const tmenu2& menu = **it;
		menu.generate(cfg);
	}
}

const int mkwin_controller::default_child_width = 5;
const int mkwin_controller::default_child_height = 4;
const int mkwin_controller::default_dialog_width = 48;
const int mkwin_controller::default_dialog_height = 24;

std::string mkwin_controller::common_float_widgets_cfg()
{
	return game_config::path + "/data/gui/default/macros/common_float_widgets.cfg";
}

std::string mkwin_controller::gui_tpl_widget_path(const std::string& app)
{
	VALIDATE(!app.empty(), null_str);
	if (app != "rose") {
		return game_config::path + "/" + game_config::generate_app_dir(app) + "/gui/tpl_widget";
	} else {
		return game_config::path + "/data/gui/default/tpl_widget";
	}
}

void tfloat_widget::from(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const config& cfg, const std::string& type, const std::string& definition, const std::string& id)
{
	gui2::tfloat_widget_builder::from(cfg);
	
	u.reset(new unit(controller, disp, units, std::make_pair(type, disp.find_widget(type, definition, id)), nullptr, -1));
	u->from_cfg(cfg);
}

void tfloat_widget::generate(const std::set<std::string>& reserve_float_widget_ids, config& cfg) const
{
	if (reserve_float_widget_ids.find(u->cell().id) == reserve_float_widget_ids.end()) {
		gui2::tfloat_widget_builder::generate(cfg);
	}
	u->to_cfg(cfg);
}

mkwin_controller::mkwin_controller(const config& app_cfg, CVideo& video, const std::map<std::string, std::string>& app_tdomains)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, gui_(NULL)
	, map_(null_str)
	, units_(*this, map_, true)
	, copied_unit_(NULL)
	, file_()
	, top_()
	, preview_(false)
	, collect_linked_group_(false)
	, theme_widget_start_(0)
	, top_rect_cfg_(NULL)
	, context_menus_(1, tmenu2(null_str, "main", NULL))
	, app_tdomains_(app_tdomains)
	, last_unpack_tpl_widget_(std::make_pair(map_location(0, 0), null_str))
{
	unit::preview_widget_best_width = 48 * gui2::twidget::hdpi_scale;
	unit::preview_widget_best_height = 48 * gui2::twidget::hdpi_scale;

	reserve_float_widget_ids_.insert(gui2::settings::horizontal_scrollbar_id);
	reserve_float_widget_ids_.insert(gui2::settings::vertical_scrollbar_id);
	reserve_float_widget_ids_.insert(gui2::settings::tooltip_id);
	reserve_float_widget_ids_.insert(gui2::settings::drag_id);
	reserve_float_widget_ids_.insert(gui2::settings::magnifier_id);
	reserve_float_widget_ids_.insert(gui2::settings::track_id);
	reserve_float_widget_ids_.insert(gui2::settings::button_id);
	reserve_float_widget_ids_.insert(gui2::settings::edit_select_all_id);
	reserve_float_widget_ids_.insert(gui2::settings::edit_select_id);
	reserve_float_widget_ids_.insert(gui2::settings::edit_copy_id);
	reserve_float_widget_ids_.insert(gui2::settings::edit_paste_id);

	gui2::init_layout_mode();

	textdomains_.push_back("rose-lib");
	textdomains_.push_back(game_config::app + "-lib");

	VALIDATE(default_dialog_width >= default_child_width, null_str);
	VALIDATE(default_dialog_height >= default_child_height, null_str);

	int original_width = default_child_width;
	int original_height = default_child_height;
	map_ = tmap(generate_map_data(original_width, original_height, preview_));

	units_.create_coor_map(map_.w(), map_.h());
}

mkwin_controller::~mkwin_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = NULL;
	}
}

void mkwin_controller::app_create_display(int initial_zoom)
{
	gui_ = new mkwin_display(*this, units_, video_, map_, initial_zoom);
}

void mkwin_controller::app_post_initialize()
{
	fill_spacer(NULL, -1);
	units_.save_map_to(top_, false);

	set_status();
	gui_->show_context_menu();
}

events::mouse_handler_base& mkwin_controller::get_mouse_handler_base()
{
	return *this;
}

void mkwin_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;
	unit* u = units_.find_unit(selected_hex_, true);

	switch (command) {
		case tmkwin_scene::HOTKEY_RCLICK:
			do_right_click();
			break;

		case tmkwin_scene::HOTKEY_INSERT_TOP:
			insert_row(u, true);
			break;
		case tmkwin_scene::HOTKEY_INSERT_BOTTOM:
			insert_row(u, false);
			break;
		case tmkwin_scene::HOTKEY_ERASE_ROW:
			erase_row(u);
			break;

		case tmkwin_scene::HOTKEY_INSERT_LEFT:
			insert_column(u, true);
			break;
		case tmkwin_scene::HOTKEY_INSERT_RIGHT:
			insert_column(u, false);
			break;
		case tmkwin_scene::HOTKEY_ERASE_COLUMN:
			erase_column(u);
			break;

		case tmkwin_scene::HOTKEY_SETTING:
			widget_setting(false);
			break;
		case tmkwin_scene::HOTKEY_LINKED_GROUP:
			linked_group_setting(u);
			break;
		case tmkwin_scene::HOTKEY_SPECIAL_SETTING:
			widget_setting(true);
			break;

		case HOTKEY_COPY:
			copy_widget(u, false);
			break;
		case HOTKEY_CUT:
			copy_widget(u, true);
			break;
		case HOTKEY_PASTE:
			paste_widget();
			break;
		case tmkwin_scene::HOTKEY_ERASE:
			erase_widget();
			break;

		case tmkwin_scene::HOTKEY_INSERT_CHILD:
			u->insert_child(default_child_width, default_child_height);
			if (!preview()) {
				layout_dirty();
			}
			gui_->show_context_menu();
			break;
		case tmkwin_scene::HOTKEY_ERASE_CHILD:
			u->parent().u->erase_child(u->parent().number);
			if (!preview()) {
				layout_dirty();
			}
			gui_->show_context_menu();
			break;
		
		case tmkwin_scene::HOTKEY_PACK:
			pack_widget_tpl(u);
			break;

		case tmkwin_scene::HOTKEY_UNPACK:
			unpack_widget_tpl(u);
			break;

		case tmkwin_scene::HOTKEY_BUILD:
			{
				gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
				scene->do_build();
			}
			break;

		case tmkwin_scene::HOTKEY_PREVIEW:
			if (!preview_) {
				if (window_is_valid(true)) {
					gui2::show_message("", _("Pass"));
				}
				break;
/*
				if (!window_is_valid(true)) {
					break;
				}
				theme_goto_preview();
*/
			} else {
				theme_goto_widget();
			}

			break;
		case HOTKEY_SYSTEM:
			system();
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

void mkwin_controller::insert_row(unit* u, bool top)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int row = u->get_location().y - child.window->get_location().y - 1;
	if (!top) {
		row ++;
	}
	int w = (int)child.cols.size();
	std::vector<unit*> v;
	for (int x = 0; x < w; x ++) {
		v.push_back(new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
	}
	std::vector<unit*>::iterator it = child.units.begin();
	if (row) {
		std::advance(it, row * w);
	}
	child.units.insert(it, v.begin(), v.end());

	it = child.rows.begin();
	if (row) {
		std::advance(it, row);
	}
	child.rows.insert(it, new unit(*this, *gui_, units_, unit::ROW, parent.u, parent.number));

	layout_dirty();
	gui_->show_context_menu();
}

void mkwin_controller::erase_row(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int row = u->get_location().y - child.window->get_location().y - 1;
	int w = (int)child.cols.size();
	std::vector<unit*>::iterator it;
	for (int x = 0; x < w; x ++) {
		unit* u = child.units[row * w];
		units_.erase(u->get_location());

		it = child.units.begin();
		std::advance(it, row * w);
		child.units.erase(it);
	}
	units_.erase(u->get_location());

	it = child.rows.begin();
	if (row) {
		std::advance(it, row);
	}
	child.rows.erase(it);

	layout_dirty();

	if (row >= (int)child.rows.size()) {
		selected_hex_.y = child.window->get_location().y + child.rows.size();
	}
	gui_->show_context_menu();
}

void mkwin_controller::insert_column(unit* u, bool left)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int col = u->get_location().x - child.window->get_location().x - 1;
	if (!left) {
		col ++;
	}
	int w = (int)child.cols.size();
	int h = (int)child.rows.size();
	
	std::vector<unit*>::iterator it;
	for (int y = 0; y < h; y ++) {
		unit* u = new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number);

		it = child.units.begin();
		std::advance(it, y * w + col + y);
		child.units.insert(it, u);
	}
	
	it = child.cols.begin();
	if (col) {
		std::advance(it, col);
	}
	child.cols.insert(it, new unit(*this, *gui_, units_, unit::COLUMN, parent.u, parent.number));

	layout_dirty();
	gui_->show_context_menu();
}

void mkwin_controller::erase_column(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	int col = u->get_location().x - child.window->get_location().x - 1;
	int w = (int)child.cols.size();
	int h = (int)child.rows.size();
	std::vector<unit*>::iterator it;
	for (int y = 0; y < h; y ++) {
		unit* u = child.units[y * w + col - y];
		units_.erase(u->get_location());

		it = child.units.begin();
		std::advance(it, y * w + col - y);
		child.units.erase(it);
	}
	units_.erase(u->get_location());

	it = child.cols.begin();
	if (col) {
		std::advance(it, col);
	}
	child.cols.erase(it);

	layout_dirty();
	if (col >= (int)child.cols.size()) {
		selected_hex_.x = child.window->get_location().x + child.cols.size();
	}
	gui_->show_context_menu();
}

void mkwin_controller::fill_object_list()
{
	gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
	scene->fill_object_list(units_);
}

void mkwin_controller::generate_theme_preview(const unit::tchild& src, unit::tchild& dest, const int xdim, const int ydim)
{
	dest.clear(false);

	const int width = 1 + ceil(1.0 * (xdim * gui2::twidget::hdpi_scale) / image::tile_size);
	const int height = 1 + ceil(1.0 * (ydim  * gui2::twidget::hdpi_scale) / image::tile_size);
	
	const int zoom = gui_->zoom();
	const double zoom_factor = gui_->get_zoom_factor();
	// const double zoom_factor = 1;
	for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			SDL_Rect rect = create_rect(x * zoom, y * zoom, zoom, zoom);
			if (!x && !y) {
				dest.window = new unit2(*this, *gui_, units_, unit::WINDOW, nullptr, rect);
			} else if (x && !y) {
				dest.cols.push_back(new unit2(*this, *gui_, units_, unit::COLUMN, nullptr, rect));
			} else if (!x && y) {
				dest.rows.push_back(new unit2(*this, *gui_, units_, unit::ROW, nullptr, rect));
			}
		}
	}

	const int x_offset = zoom, y_offset = zoom;
	game_logic::map_formula_callable variables;

	variables.add("screen_width", variant(xdim));
	variables.add("screen_height", variant(ydim));
	variables.add("svga", variant(xdim >= 800 && ydim >= 600));
	variables.add("vga", variant(xdim >= 640 && ydim >= 480));

	tpoint origin(x_offset, y_offset);
	const tpoint size(xdim * zoom_factor * gui2::twidget::hdpi_scale, ydim * zoom_factor * gui2::twidget::hdpi_scale);
	src.place(*this, *gui_, units_, variables, origin, size, dest.units);
}

void mkwin_controller::theme_goto_widget()
{
	gui_->set_grid(false);

	preview_ = false;
	top_ = top2_;

	units_.set_consistent(true);
	layout_dirty(true);

	{
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("preview"));
		widget->set_label("misc/into.png");
		widget->set_tooltip(_("Preivew"));
	}

	selected_hex_ = map_location();

	fill_object_list();

	gui_->show_context_menu();
}

void mkwin_controller::reconstruct_preview_section()
{
	VALIDATE(preview_, null_str);
	VALIDATE(top2_.window && !top2_.cols.empty() && !top2_.rows.empty() && !top2_.units.empty(), null_str);

	units_.zero_map();
	generate_theme_preview(top2_, top_, 640, 480);

	units_.set_consistent(false);
	reload_map(top_.cols.size() + 1, top_.rows.size() + 1);
	units_.restore_map_from(top_, 0, 0, true);

	// normal, scene not use top_.units
	top_.units.clear();

	fill_object_list();
}

void mkwin_controller::theme_goto_preview()
{
	unit* u = units_.find_unit(map_location(0, 0))->parent().u;
	selected_hex_ = map_location();

	gui_->set_grid(true);

	gui_->click_widget(gui_->spacer.first, gui_->spacer.second->id);
	preview_ = true;
	top2_ = top_;

	reconstruct_preview_section();

	{
		gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("preview"));
		widget->set_label("misc/into.png");
		widget->set_tooltip(_("Back to design"));
	}

	if (selected_hex_.valid()) {
		gui_->scroll_to_xy(u->get_rect().x, u->get_rect().y, display::ONSCREEN);
	}

	gui_->show_context_menu();
}

void mkwin_controller::widget_setting(bool special)
{
	VALIDATE(!special, "now no special settings.");

	if (!selected_hex_.valid()) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);

	gui2::tsetting_dialog *dlg = NULL;
	if (special) {

	} else if (u->type() == unit::WIDGET) {
		const unit* tmp = u->parent().u;
		while (tmp) {
			if (tmp->is_listbox()) {
				break;
			}
			tmp = tmp->parent().u;
		}
		const std::vector<gui2::tlinked_group>& linked_groups = tmp? tmp->cell().widget.listbox.linked_groups: linked_groups_;
		dlg = new gui2::tcontrol_setting(*this, *u, textdomains_, linked_groups, false);

	} else if (u->type() == unit::ROW) {
		dlg = new gui2::trow_setting(*u);

	} else if (u->type() == unit::COLUMN) {
		dlg = new gui2::tcolumn_setting(*u);

	} else if (u->type() == unit::WINDOW) {
		if (!u->parent().u) {
			dlg = new gui2::twindow_setting(*this, *u, textdomains_);
		} else {
			const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u->parent().u->widget();
			dlg = new gui2::tgrid_setting(*this, units_, *u, widget.first);
		}
	}

	dlg->show();
	int res = dlg->get_retval();
	if (res != gui2::twindow::OK) {
		delete dlg;
		return;
	}
	u->set_cell(dlg->cell());
	delete dlg;
}

void mkwin_controller::linked_group_setting(unit* u)
{
	std::vector<gui2::tlinked_group>& linked_groups = u->is_listbox()? u->cell().widget.listbox.linked_groups: linked_groups_;

	gui2::tlinked_group2 dlg(*u, linked_groups);
	dlg.show();
	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}

	linked_groups = dlg.linked_groups();
}

void mkwin_controller::copy_widget(unit* u, bool cut)
{
	cut_ = cut;
	set_copied_unit(u);

	gui_->show_context_menu();
}

bool mkwin_controller::paste_widget2(const unit& from, const map_location to_loc)
{
	bool require_layout_dirty = false;

	unit* to = units_.find_unit(to_loc);
	unit::tparent parent = to->parent();
	units_.erase(to_loc);
	unit* clone = new unit(from);
	// from and to maybe in different page.
	clone->set_parent(parent.u, parent.number);	
	units_.insert(to_loc, clone);

	to = units_.find_unit(to_loc);
	replace_child_unit(to);
	if (to->has_child()) {
		require_layout_dirty = true;
	}
	return require_layout_dirty;
}

void mkwin_controller::paste_widget()
{
	if (!selected_hex_.valid() || !copied_unit_) {
		return;
	}

	unit* u = units_.find_unit(selected_hex_);
	bool require_layout_dirty = false;
	std::vector<const unit*> require_cut_widgets;

	if (copied_unit_->type() == unit::WIDGET) {
		require_layout_dirty = paste_widget2(*copied_unit_, selected_hex_);
		require_cut_widgets.push_back(copied_unit_);

	} else if (copied_unit_->type() == unit::ROW) {
		unit::tparent from_parent = copied_unit_->parent();
		unit::tparent to_parent = u->parent();

		const unit::tchild& from_child = from_parent.u? from_parent.u->child(from_parent.number): top_;
		const unit::tchild& to_child = to_parent.u? to_parent.u->child(to_parent.number): top_;

		int from_row = copied_unit_->get_location().y - from_child.window->get_location().y - 1;
		int to_row = u->get_location().y - to_child.window->get_location().y - 1;
		int w = (int)from_child.cols.size();
		for (int x = 0; x < w; x ++) {
			const unit* from = from_child.units[from_row * w + x];
			const unit* to = to_child.units[to_row * w + x];
			require_layout_dirty |= paste_widget2(*from, to->get_location());
			require_cut_widgets.push_back(from);
		}

	} else if (copied_unit_->type() == unit::COLUMN) {
		unit::tparent from_parent = copied_unit_->parent();
		unit::tparent to_parent = u->parent();

		const unit::tchild& from_child = from_parent.u? from_parent.u->child(from_parent.number): top_;
		const unit::tchild& to_child = to_parent.u? to_parent.u->child(to_parent.number): top_;

		int from_col = copied_unit_->get_location().x - from_child.window->get_location().x - 1;
		int to_col = u->get_location().x - to_child.window->get_location().x - 1;
		int from_w = (int)from_child.cols.size();
		int to_w = (int)to_child.cols.size();
		int h = (int)from_child.rows.size();
		for (int y = 0; y < h; y ++) {
			const unit* from = from_child.units[y * from_w + from_col];
			const unit* to = to_child.units[y * to_w + to_col];
			require_layout_dirty |= paste_widget2(*from, to->get_location());
			require_cut_widgets.push_back(from);
		}

	}
	if (cut_) {
		for (std::vector<const unit*>::const_iterator it = require_cut_widgets.begin(); it != require_cut_widgets.end(); ++ it) {
			const unit* erasing = *it;
			const map_location loc = erasing->get_location();
			unit::tparent parent = erasing->parent();
			units_.erase(loc);
			VALIDATE(!copied_unit_ || copied_unit_->type() != unit::WIDGET, null_str);

			units_.insert(loc, new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
			unit* u = units_.find_unit(loc);
			replace_child_unit(u);
		}
	}
	if (require_layout_dirty) {
		layout_dirty();
	}

	gui_->show_context_menu();
}

void mkwin_controller::erase_widget()
{
	if (!selected_hex_.valid()) {
		return;
	}

	unit* erasing = units_.find_unit(selected_hex_);
	unit::tparent parent = erasing->parent();

	if (erasing->type() == unit::WINDOW && parent.u && parent.u->is_grid()) {
		selected_hex_ = parent.u->get_location();
		erase_widget();
		return;
	}

	bool has_child = erasing->has_child();
	if (has_child) {
		bool all_spacer = true;
		const std::vector<unit::tchild>& children = erasing->children();
		for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			if (!it->is_all_spacer()) {
				all_spacer = false;
				break;
			}
		}
		if (!all_spacer) {
			utils::string_map symbols;
			std::stringstream ss;
			
			symbols["id"] = ht::generate_format(erasing->cell().id.empty()? "---": erasing->cell().id, color_to_uint32(font::BAD_COLOR));
			ss << vgettext2("$id holds grid that not all spacer. Do you want to delete?", symbols);
			int res = gui2::show_message2("", ss.str(), gui2::tmessage::yes_no_buttons);
			if (res != gui2::twindow::OK) {
				return;
			}
		}
	}
	units_.erase(selected_hex_);
	if (preview()) {
		selected_hex_ = map_location();

	} else {
		units_.insert(selected_hex_, new unit(*this, *gui_, units_, gui_->spacer, parent.u, parent.number));
		unit* u = units_.find_unit(selected_hex_);
		replace_child_unit(u);

		if (has_child) {
			layout_dirty();
		}
	}

	fill_object_list();
	gui_->show_context_menu();
}

void mkwin_controller::layout_dirty(bool force_change_map)
{
	const int default_expand_size = 4;
	
	VALIDATE(!preview(), "Must not be in preview section!");

	bool require_change = false;
	int w = map_.w(), h = map_.h();
	
	units_.recalculate_size(top_);
	int new_w = top_.window->cell().window.cover_width;
	int new_h = top_.window->cell().window.cover_height;

	if (force_change_map || w < new_w) {
		require_change = true;
		w = new_w + default_expand_size;
	} else if (w > default_dialog_width && w > new_w + default_expand_size) {
		require_change = true;
		w = std::max(new_w + default_expand_size, default_dialog_width);
	}

	if (force_change_map || h < new_h) {
		require_change = true;
		h = new_h + default_expand_size;
	} else if (h > default_dialog_height && h > new_h + default_expand_size) {
		require_change = true;
		h = std::max(new_h + default_expand_size, default_dialog_height);
	}

	if (require_change) {
		reload_map(w, h);
	}

	units_.layout(top_);
	if (require_change) {
		gui_->recalculate_minimap();
	} else {
		gui_->redraw_minimap();
	}
}

void mkwin_controller::set_status()
{
	gui2::tcontrol* widget = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("status"));
	int w = widget->get_width();
	int h = widget->get_height();
	std::vector<gui2::tformula_blit> result;
	// result.push_back(gui2::tformula_blit("misc/background45.png", null_str, null_str, "(width)", "(height)"));
	if (copied_unit_) {
		SDL_Rect dst_clip = empty_rect;
		std::string png = cut_? "buttons/cut.png": "buttons/copy.png";
		// surface fg = scale_surface(image::get_image(png), w / 2, h / 2);
		// sdl_blit(fg, NULL, result, &dst_clip);
		result.push_back(gui2::tformula_blit(png, null_str, null_str, "(width / 2)", "(height / 2)"));

		const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = copied_unit_->widget();
		png = copied_unit_->image();
		// fg = scale_surface(image::get_image(png), w / 2, h / 2);
		surface fg = image::get_image(png);
		if (fg) {
			dst_clip.x = w / 2;
			dst_clip.y = 0;
			// sdl_blit(fg, NULL, result, &dst_clip);
			result.push_back(gui2::tformula_blit(png, str_cast(dst_clip.x), str_cast(dst_clip.y), "(width / 2)", "(height / 2)"));
		}
	}
	gui_->widget_set_blits("status", result);
	gui_->redraw_minimap();
}

void mkwin_controller::set_copied_unit(unit* u)
{
	copied_unit_ = u;
	if (gui_) {
		set_status();
	}
}

void mkwin_controller::fill_spacer(unit* parent, int number)
{
	int width = map_.w();
	int height = map_.h();

	int zoom = gui_->hex_width();
	for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			map_location loc = map_location(x, y);
			if (!units_.find_unit(loc, true)) {
				if (x && y) {
					if (!preview()) {
						units_.insert(loc, new unit(*this, *gui_, units_, gui_->spacer, parent, number));
					}
				} else {
					int type = unit::NONE;
					if (!x && !y) {
						type = unit::WINDOW;
					} else if (x) {
						type = unit::COLUMN;
					} else if (y) {
						type = unit::ROW;
					}
					if (!preview()) {
						units_.insert(loc, new unit(*this, *gui_, units_, type, parent, number));
					} else {
						SDL_Rect rect = create_rect(x * zoom, y * zoom, zoom, zoom);
						units_.insert2(*gui_, new unit2(*this, *gui_, units_, type, parent, rect));
					}
				}
			}
		}
	}
}

class tclick_dismiss
{
public:
	tclick_dismiss(gui2::tcell_setting& cell)
		: cell_(cell)
	{
		original_ = cell.window.click_dismiss;
		cell_.window.click_dismiss = true;
	}
	~tclick_dismiss()
	{
		cell_.window.click_dismiss = original_;
	}

private:
	gui2::tcell_setting& cell_;
	bool original_;
};

unit* mkwin_controller::get_window() const
{
	unit* window = units_.find_unit(map_location(0, 0));
	return window;
}

bool mkwin_controller::did_no_multiline_widget(const unit* u, bool show_error)
{
	if (u->cell().widget.mtwusc) {
		return false;
	}
	return true;
}

bool mkwin_controller::no_multiline_widget(const std::vector<unit*>& top_units)
{
	if (!enumerate_child(std::bind(&mkwin_controller::did_no_multiline_widget, this, _1, _2), top_units, false)) {
		return false;
	}
	return true;
}

bool mkwin_controller::did_valid_id(const unit* u, bool show_error)
{
	VALIDATE(!preview_, null_str);

	std::string err;
	std::stringstream ss;
	utils::string_map symbols;

	const std::string id = u->cell().id;
	symbols["id"] = ht::generate_format(id.empty()? "--": id, color_to_uint32(font::BAD_COLOR));
	symbols["widget"] = ht::generate_format(u->widget_tag(), color_to_uint32(font::BAD_COLOR));

	const unsigned h_align = u->cell().widget.cell.flags_ & gui2::tgrid::HORIZONTAL_MASK;
	const unsigned v_align = u->cell().widget.cell.flags_ & gui2::tgrid::VERTICAL_MASK;

	if (top_.window->cell().window.scene && id == main_map_widget_id) {
		if (!u->is_spacer() || u->widget().second->id != "default") {
			err = vgettext2("widget($widget) is _main_map, it must be default style's spacer.", symbols);
		}

	} else if (top_.window->cell().window.scene && id == mini_map_widget_id) {
		if (!u->is_image() || u->widget().second->id != "blits") {
			err = vgettext2("widget($widget) is _mini_map, it must be blits style's image.", symbols);
		}

	} else if (u->is_label()) {
		if (v_align == gui2::tgrid::VERTICAL_ALIGN_EDGE) {
			// it will  always trigger invalidate_layout when label_ changed. see tcontrol::set_label.
			err = vgettext2("widget($widget) is label, it's vertical align must not set edge.", symbols);
		} else if (u->cell().widget.width_is_max && !u->cell().widget.height.empty()) {
			err = vgettext2("widget($widget) is label, and set width_is_max, best_height must be empty.", symbols);
		}

	} else if (u->is_stack()) {
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			const unit::tchild& child = *it;
			if (child.is_all_spacer()) {
				err = vgettext2("widget($widget) is stack, there is all spacer layer.", symbols);
				break;
			}
		}

	} else if (u->is_scroll_panel()) {
		if (u->cell().widget.height == "0") {
			// check grow_factor
			const unit::tparent& parent = u->parent();
			const unit::tchild& child = parent.u? parent.u->children()[parent.number]: top_;
			const tpoint locate = child.locate_u(*u);
			if (child.rows.size() > 1 && child.rows[locate.y]->cell().row.grow_factor == 0) {
				err = vgettext2("widget($widget) is scroll_panel and best height is 0, there is tow or more rows in one column, it's row's grow_factor must be set >= 1.", symbols);
			}
		}

		if (err.empty() && !u->cell().widget.width.empty()) {
			VALIDATE(u->children().size() == 1, null_str);
			if (!no_multiline_widget(u->child(0).units)) {
				err = vgettext2("widget($widget) is scroll_panel, and has multiline widget, it's width must be empty.", symbols);
			}
		}

	} else if (u->is_listbox()) {
		const std::vector<unit::tchild>& children = u->children();
		if (children[1].units.size() > 1) {
			// has left_drag_grid, it's parent must not has scroll_panel.
			const unit* temp = u->parent().u;
			while (temp) {
				if (temp->is_scroll_panel()) {
					err = vgettext2("widget($widget) is listbox and has left drag_grid, must not in scroll panel. Maybe implete by LongPress.", symbols);
					break;
				}
				temp = temp->parent().u;
			}
		}

	} else if (u->is_tree()) {
		const std::vector<unit::tchild>& children = u->children();
		VALIDATE(children.size() == 1, null_str); // now only one node type.
		const unit::tchild& child = children[0];
		if (u->cell().widget.tree_view.foldable) {
			if (child.rows.size() != 1) {
				err = vgettext2("widget($widget) is tree view, only one row in 1th grid.", symbols);
			} else if (!child.units[0]->is_toggle_button() || child.units[0]->widget().second->id != "tree_node") {
				err = vgettext2("widget($widget) is foldable tree view, first widget must be tree_node's style toggle_button.", symbols);
			} else if (child.cols[0]->cell().column.grow_factor) {
				err = vgettext2("widget($widget) is foldable tree view, first widget's grow_factor must be 0.", symbols);
			}
		} else {
			if (child.units[0]->is_toggle_button() && child.units[0]->widget().second->id == "tree_node") {
				err = vgettext2("widget($widget) is unable fold tree view, first widget must not be tree_node's style toggle_button.", symbols);
			}
		}

	} else if (u->is_tpl()) {
		if (u->cell().id.empty()) {
			err = vgettext2("widget($widget) is template widget, it's id must not empty.", symbols);
		}
	}

	 if (err.empty() && u->cell().widget.mtwusc) {
		// must not set linked_group
		if (!u->has_mtwusc()) {
			err = vgettext2("widget($widget) set multiline, it's label_is_text must be yes.", symbols);

		} else if (!u->cell().widget.linked_group.empty()) {
			err = vgettext2("widget($widget) set multiline, must not set linked_group.", symbols);

		} else {
			{
				// check parent.
				const unit* temp = u->parent().u;
				while (temp) {
					if (temp->is_tree()) {
						err = vgettext2("widget($widget) set multiline, must not in tree's node.", symbols);
						break;
					}
					temp = temp->parent().u;
				}
			}

			if (err.empty()) {
				// check grow_factor
				const unit::tparent& parent = u->parent();
				const unit::tchild& child = parent.u? parent.u->children()[parent.number]: top_;
				const tpoint locate = child.locate_u(*u);
				if (child.cols.size() > 1 && child.cols[locate.x ]->cell().column.grow_factor == 0) {
					err = vgettext2("widget($widget) set multiline, there is tow or more cols in one row, it's col's grow_factor must be set >= 1.", symbols);
				}
			}

			if (err.empty()) {
				// horizontal align must use edge.
				if (h_align == gui2::tgrid::HORIZONTAL_ALIGN_EDGE) {
					err = vgettext2("widget($widget) set multiline, it's horizontal align must not set edge.", symbols);
				}

				// vertical align must not use edge.
				if (err.empty()) {
					if (v_align == gui2::tgrid::VERTICAL_ALIGN_EDGE) {
						err = vgettext2("widget($widget) set multiline, it's vertical align must not set edge.", symbols);
					}
				}
			}

			if (err.empty()) {
				if (!u->cell().widget.width.empty() || !u->cell().widget.height.empty()) {
					err = vgettext2("widget($widget) set multiline, both best_width and best_height must be empty.", symbols);
				}
			}
		}

	}

	if (err.empty() && u->is_scroll()) {
		if (u->cell().widget.width == "0") {
			if (h_align != gui2::tgrid::HORIZONTAL_ALIGN_EDGE) {
				err = vgettext2("widget($widget) is scorllable widget, best width is 0, must set horizontal align to edge.", symbols);
			}
		}
		if (err.empty() && u->cell().widget.height == "0") {
			if (v_align != gui2::tgrid::VERTICAL_ALIGN_EDGE) {
				err = vgettext2("widget($widget) is scorllable widget, best height is 0, must set vertical align to edge.", symbols);
			}
		}
	}

	if (show_error && !err.empty()) {
		gui2::show_message("", err);
		select_unit(*u);
	}
	return err.empty();
}

bool mkwin_controller::enumerate_child2(const std::function<bool (const unit*, bool)>& did, const unit::tchild& child, bool show_error) const
{
	if (!did(child.window, show_error)) {
		return false;
	}

	for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(did, *it2, show_error)) {
				return false;
			}
		}
		if (!did(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::enumerate_child(const std::function<bool (const unit*, bool)>& did, const std::vector<unit*>& top_units, bool show_error) const
{
	for (std::vector<unit*>::const_iterator it = top_units.begin(); it != top_units.end(); ++ it) {
		const unit* u = *it;
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			if (!enumerate_child2(did, *it2, show_error)) {
				return false;
			}
		}
		if (!did(u, show_error)) {
			return false;
		}
	}
	return true;
}

bool mkwin_controller::window_is_valid(bool show_error)
{
	std::stringstream err;
	err << _("Window isn't ready!") << "\n\n";

	const unit* window = get_window();
	if (!window) {
		return false;
	}

	if (window->cell().id.empty() || window->cell().id == gui2::untitled) {
		if (show_error) {
			gui2::show_id_error("id", err.str());
		}
		return false;
	}

	if (window->cell().window.app.empty()) {
		if (show_error) {
			gui2::show_id_error("app", err.str());
		}
		return false;
	}

	std::vector<unit*> top_units = form_top_units();
	{
		if (!enumerate_child(std::bind(&mkwin_controller::did_valid_id, this, _1, _2), top_units, show_error)) {
			return false;
		}
	}
	return true;
}

void mkwin_controller::system()
{
	enum {_LOAD, _SAVE, _SAVE_AS, _PREFERENCES, _QUIT};

	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;

	const config top = generate_window_cfg().child("window");
	const unit* window = get_window();
	bool window_dirty = original_.first != window->cell().window.app || original_.second != top;

	{
		std::string str = _("Save");

		items.push_back(gui2::tsystem::titem(_("Load")));
		rets.push_back(_LOAD);

		items.push_back(gui2::tsystem::titem(str, window_is_valid(false) && window_dirty && !file_.empty()));
		rets.push_back(_SAVE);

		items.push_back(gui2::tsystem::titem(_("Save As"), window_is_valid(false)));
		rets.push_back(_SAVE_AS);

		items.push_back(gui2::tsystem::titem(_("Preferences")));
		rets.push_back(_PREFERENCES);

		items.push_back(gui2::tsystem::titem(_("Quit")));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(items);
		try {
			dlg.show();
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show();
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}
	bool require_show_context_menu = false;
	if (rets[retval] == _LOAD) {
		load_window();	

	} else if (rets[retval] == _SAVE) {
		save_window(false);
		require_show_context_menu = true;

	} else if (rets[retval] == _SAVE_AS) {
		save_window(true);
		require_show_context_menu = true;

	} else if (rets[retval] == _PREFERENCES) {
		preferences::show_preferences_dialog(gui_->video());
		// gui_->redraw_everything();

	} else if (rets[retval] == _QUIT) {
		quit_confirm(gui2::twindow::OK, window_dirty);
	}

	if (require_show_context_menu) {
		gui_->show_context_menu();
	}
}

config window_cfg_from_gui_bin()
{
	config cfg;
	wml_config_from_file(game_config::path + "/xwml/" + "gui.bin", cfg);
	const config& gui_cfg = cfg.find_child("gui", "id", "default");
	return gui_cfg.find_child("window", "id", "chat2");
	// return gui_cfg.find_child("window", "id", "recruit");
}

std::vector<std::string> mkwin_controller::generate_textdomains(const std::string& file, bool scene) const
{
	std::string dir = extract_directory(file) + "/";
	static const std::string dir_separators = "\\/";

	std::string::size_type pos = dir.find_last_of(dir_separators);
	if (pos == dir.size() - 1) {
		dir = dir.substr(0, dir.size() - 1);
	}

	std::string name = extract_file(dir);
	if (scene) {
		if (name != "scene") {
			return textdomains_;
		}
	} else if (name != "window") {
		return textdomains_;
	}

	// <res>/app-kdesktop/gui/window
	// below "do {} while" get ride of gui/window.
	int extracts = 2;
	do {
		dir = extract_directory(dir);
		if (dir.size() < 2) { // 0, 1
			return textdomains_;
		}
		extracts --;
	} while (extracts > 0);

	name = extract_file(dir);
	std::string app;
	if (utils::is_short_app_dir(name)) {
		app = game_config::extract_app_from_app_dir(name);
	} else if (is_lua_bundleid(name)) {
		app = name;
	} else {
		return textdomains_;
	}

	std::vector<std::string> res;
	res.push_back("rose-lib");
	if (!app.empty()) {
		res.push_back(app + "-lib");
	}

	return res;
}

void mkwin_controller::load_window()
{
	bool browse_cfg = true;
	std::stringstream err;

	config_cache& cache = config_cache::instance();
	config window_cfg;
	if (browse_cfg) {
		std::string initial = game_config::preferences_dir + "/editor/maps";
		if (!file_.empty()) {
			if (SDL_IsDirectory(extract_directory(file_).c_str())) {
				initial = extract_directory(file_);
			}
		}
		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, true, initial, _("Choose a Window to Open"));
		if (preview_) {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/scene", _("gui/scene"), "misc/dir_res.png");
		} else {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir_res.png");
		}

		if (true) {
			gui2::tbrowse dlg(param, false);
			dlg.show();
			int res = dlg.get_retval();
			if (res != gui2::twindow::OK) {
				return;
			}
		} else {
			int ii = 0;
			param.result = game_config::path + "/data/gui/default/window/browse.cfg";
		}

		std::string file = param.result;

		config cfg;

		// get_partial_windows_cfg(file, cfg);
		{
			config_cache_transaction main_transaction;
			cache.get_config(common_float_widgets_cfg(), cfg);
			cache.get_config(gui_tpl_widget_path("rose"), cfg);
			cfg.clear();

			cache.get_config(file, cfg);
		}

		const config& window_cfg1 = cfg.child("window");
		if (!window_cfg1) {
			err << _("Invalid dialog cfg file!");
			gui2::show_message("", err.str());
			return;
		}
		window_cfg = window_cfg1;
		
		file_ = file;

		refresh_title();

		textdomains_ = generate_textdomains(file_, preview_);

	} else {
		window_cfg = window_cfg_from_gui_bin();
	}

	const config& res_cfg = window_cfg.child("resolution");
	if (!res_cfg) {
		return;
	}

	tused_tpl_widget_lock tpl_lock(*this);
	{
		const config& top_grid = res_cfg.child("grid");
		if (!top_grid) {
			return;
		}

		top_.erase(units_);
		
		top_.from(*this, *gui_, units_, NULL, -1, top_grid);
		top_.window->from_cfg(window_cfg);
		form_linked_groups(res_cfg);
		form_context_menus(res_cfg);
		form_float_widgets(res_cfg);

		layout_dirty();
	}

	if (!used_tpl_widget_.empty()) {
		// erase [linked_group] belong to widget template.
		std::set<std::string> erase_ids;
		{
			config_cache_transaction main_transaction;

			config cfg;
			cache.get_config(common_float_widgets_cfg(), cfg);
			cache.get_config(gui_tpl_widget_path("rose"), cfg);

			preproc_map& map = cache.make_copy_map();

			for (std::set<const gui2::ttpl_widget*>::const_iterator it = used_tpl_widget_.begin(); it != used_tpl_widget_.end(); ++ it ) {
				const gui2::ttpl_widget& tpl = **it;
				if (tpl.linked_group.empty()) {
					continue;
				}
				preproc_map::const_iterator it2 = map.find(tpl.linked_group);
				if (it2 != map.end()) {
					const preproc_define& define = it2->second;
					::read(cfg, define.value);
					BOOST_FOREACH (const config& linked, cfg.child_range("linked_group")) {
						erase_ids.insert(linked["id"]);
					}
				}
			}
		}
		
		for (std::vector<gui2::tlinked_group>::iterator it = linked_groups_.begin(); it != linked_groups_.end(); ) {
			const std::string& id = it->id;
			if (erase_ids.find(id) != erase_ids.end()) {
				it = linked_groups_.erase(it);
			} else {
				++ it;
			}
		}
	}

	fill_object_list();
	selected_hex_ = map_location();

	original_.first = top_.window->cell().window.app;
	// window_cfg is format from *.cfg directly that parsed macro.
	// here requrie format that is tpl_widget.
	original_.second = generate_window_cfg().child("window");
}

void mkwin_controller::save_window(bool as)
{
	VALIDATE(as || !file_.empty(), "When save, file_ must not be empty!");

	const unit* u = get_window();
	std::string fname;
	if (!as) {
		fname = file_;
	} else {
		fname = game_config::preferences_dir + "/editor/maps/";

		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, false, fname, _("Choose a Window to Save"), _("Save"));
		if (preview_) {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/themes", _("data/themes"), "misc/dir_res.png");
		} else {
			param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir_res.png");
		}

		gui2::tbrowse dlg(param, false);
		dlg.show();
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		fname = param.result;
	}

	const std::string textdomain = u->cell().window.app + "-lib";
	std::stringstream ss;

	ss << "#textdomain " << textdomain << "\n";
	ss << "\n";

	VALIDATE(!preview_, null_str);

	tused_tpl_widget_lock tpl_lock(*this);
	config top = generate_window_cfg();
	if (!as) {
		original_.second = top.child("window");
	}

	::write(ss, top, 0, textdomain);

	std::string data = ss.str();
	{
		std::string key = "[resolution]";
		size_t pos = data.find(key);
		pos += key.size();
		const char* c_str = data.c_str();
		while (!utils::notspace(c_str[pos])) { pos ++; }

		std::stringstream dst_ss;
		dst_ss << "{GUI__COMMON_FLOAT_WIDGETS}\n\t\t";

		const config& res_cfg = top.child("window").child("resolution");
		for (std::set<const gui2::ttpl_widget*>::const_iterator it = used_tpl_widget_.begin(); it != used_tpl_widget_.end(); ++ it ) {
			const gui2::ttpl_widget& tpl = **it;
			if (!tpl.linked_group.empty()) {
				dst_ss << "{" << tpl.linked_group << "}\n\t\t";
			}
		}

		data.replace(pos, 0, dst_ss.str());
	}
	
	write_file(fname, data.c_str(), data.size());

	if (!as) {
		original_.first = u->cell().window.app;
	}
}

void mkwin_controller::quit_confirm(int mode, bool dirty)
{
	std::string message = _("Do you really want to quit?");
	if (dirty) {
		utils::string_map symbols;
		symbols["window"] = preview_? _("Theme"): _("Dialog");
		std::string str = vgettext2("Changes in the $window since the last save will be lost.", symbols);
		message += "\n\n" + ht::generate_format(str, color_to_uint32(font::BAD_COLOR));
	}
	const int res = gui2::show_message2(_("Quit"), message, gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::CANCEL) {
		do_quit_ = true;
		quit_mode_ = mode;
	}
}

bool mkwin_controller::app_mouse_motion(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return true;
	}

	const map_location& mouseover_hex = gui_->mouseover_hex();
	if (cursor::get() != cursor::WAIT) {
		if (!mouseover_hex.x || !mouseover_hex.y || !map_.on_board(mouseover_hex)) {
			// cursor::set(cursor::INTERIOR);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else {
			// no selected unit or we can't move it
			cursor::set(cursor::NORMAL);
			gui_->resume_mouseover_hex_overlay();
		}
	}

	return true;
}

void mkwin_controller::select_unit(const unit& u)
{
	gui_->scroll_to_tile(*u.get_draw_locations().begin(), display::ONSCREEN, true, true);

	selected_hex_ = u.get_location();
	gui_->show_context_menu();
}

std::string calculate_app_from_gui_cfg_file(const std::string& file)
{
	std::string app;
	if (!file.empty()) {
		const std::string prefix = game_config::path + "/";
		if (file.find(prefix) == 0) {
			app = game_config::extract_app_from_app_dir(file.substr(prefix.size()));
		}
	}
	if (app.empty()) {
		app = "rose";
	}
	return app;
}

void mkwin_controller::unpack_widget_tpl(unit* u)
{
	utils::string_map symbols;
	std::stringstream ss;
	symbols["id"] = ht::generate_format(u->cell().id, color_to_uint32(font::BAD_COLOR));
	ss << vgettext2("Once unpake, can not be back to template. Do you want to unpake $id?", symbols);
	int res = gui2::show_message2("", ss.str(), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	unit::tparent parent = u->parent();
	gui2::tgrid::tchild cell = u->cell().widget.cell;
	
	const std::string original_id = u->cell().id;
	std::string tpl_id = u->widget().first;
	const gui2::ttpl_widget& tpl = gui2::get_tpl_widget(tpl_id);
	
	int orientation = gui2::tnew_tpl_widget::option_landscape;
	const gui2::ttpl_widget& tpl_widget = gui2::get_tpl_widget(tpl_id);
	if (tpl_widget.portrait) {
		std::vector<std::string> items;
		std::vector<int> values;
		items.push_back(_("Landscape"));
		values.push_back(gui2::tnew_tpl_widget::option_landscape);
		items.push_back(_("Portrait"));
		values.push_back(gui2::tnew_tpl_widget::option_portrait);

		gui2::tcombo_box dlg(items, 0, _("Select desire unpack orientation"));
		dlg.show();
		orientation = values[dlg.cursel()];
	}

	config widget_cfg, linked_group_cfg;
	{
		config_cache& cache = config_cache::instance();
		config_cache_transaction main_transaction;

		config cfg;
		cache.get_config(gui_tpl_widget_path("rose"), cfg);

		preproc_map& map = cache.make_copy_map();
		
		std::string widget_macro = "GUI__TPL_WIDGET_" + utils::uppercase(tpl_id);
		if (orientation == gui2::tnew_tpl_widget::option_portrait) {
			std::string widget_portrait_macro = "GUI__TPL_WIDGET_PORTRAIT_" + utils::uppercase(tpl_id);
			widget_macro = widget_portrait_macro;
		}
		preproc_map::const_iterator it = map.find(widget_macro);
		VALIDATE(it != map.end(), null_str);

		const preproc_define& define = it->second;
		::read(widget_cfg, define.value);

		it = map.find(tpl.linked_group);
		if (it != map.end()) {
			const preproc_define& define = it->second;
			::read(linked_group_cfg, define.value);
		}
	}

	units_.erase(selected_hex_);

	BOOST_FOREACH (const config& linked, linked_group_cfg.child_range("linked_group")) {
		linked_groups_.push_back(gui2::tlinked_group(linked));
	}

	BOOST_FOREACH (const config::any_child& c, widget_cfg.all_children_range()) {
		const std::string& type = c.key;
		const std::string& definition = c.cfg["definition"].str();
		gui2::tcontrol_definition_ptr ptr;
		if (type != "grid") {
			ptr = mkwin_display::find_widget(type, definition, c.cfg["id"].str());
		}
		u = new unit(*this, *gui_, units_, std::make_pair(type, ptr), parent.u, parent.number);
		units_.insert(selected_hex_, u);
		u->from_widget(widget_cfg, true);
	}
	u->cell().widget.cell = cell;
	// keep original id.
	u->cell().id = original_id;

	replace_child_unit(u);
	if (u->has_child()) {
		layout_dirty();
	}

	last_unpack_tpl_widget_ = std::make_pair(selected_hex_, tpl.id);

	gui_->show_context_menu();
}

void mkwin_controller::collect_linked_group_if_necessary(const std::string& linked_group)
{
	VALIDATE(!linked_group.empty(), null_str);
	if (collect_linked_group_) {
		collected_linked_group_.insert(linked_group);
	}
}

void mkwin_controller::pack_widget_tpl(unit* u)
{
	const std::string app2 = calculate_app_from_gui_cfg_file(file_);
	
	const std::string original_id = u->cell().id;

	config tpl_cfg;
	tused_tpl_widget_lock tpl_lock(*this);
	tcollect_linked_group_lock collect_lock(*this);
	u->to_cfg(tpl_cfg);
	const bool nested = !used_tpl_widget_.empty();

	gui2::tnew_tpl_widget::tresult result;
	{
		std::string default_id;
		if (selected_hex_ == last_unpack_tpl_widget_.first && !last_unpack_tpl_widget_.second.empty()) {
			default_id = last_unpack_tpl_widget_.second;
		}
		gui2::tnew_tpl_widget dlg(app2, *u, used_tpl_widget_, default_id);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		result = dlg.result();

		// will create/modify template widget, confirm again.
		utils::string_map symbols;
		std::stringstream ss;
		symbols["id"] = ht::generate_format(result.id, color_to_uint32(font::BAD_COLOR));
		symbols["file"] = result.file;
		ss << vgettext2("Do you want to generate template widget($id) at $file?", symbols);
		ss << "\n\n";
		ss << _("It will modify relative *.cfg. After build, exit Studio, then run again.");
		int res = gui2::show_message2("", ss.str(), gui2::tmessage::yes_no_buttons);
		if (res != gui2::twindow::OK) {
			return;
		}
	}
	tfile cfg_file(result.file, GENERIC_WRITE, OPEN_EXISTING);
	const int fsize = cfg_file.read_2_data();
	if (result.option == gui2::tnew_tpl_widget::option_portrait) {
		VALIDATE(fsize > 0, null_str);
	}

	unit::tparent parent = u->parent();
	gui2::tgrid::tchild cell = u->cell().widget.cell;
	
	const std::string app_prefix_id = utils::generate_app_prefix_id(result.app, result.id);
	std::string widget_macro = "GUI__TPL_WIDGET_" + utils::uppercase(app_prefix_id);
	std::string widget_portrait_macro = "GUI__TPL_WIDGET_PORTRAIT_" + utils::uppercase(app_prefix_id);
	std::string linked_group_macro = "GUI__TPL_LINKED_GROUP_" + utils::uppercase(app_prefix_id);

	const std::string textdomain = result.app + "-lib";
	std::stringstream ss;
	ss << "#\n";
	ss << "# NOTE: it is generated by rose studio, don't edit yourself.\n";
	ss << "#\n\n";
	ss << "#textdomain " << textdomain << "\n";
	ss << "\n";

	//
	// landscape section
	//
	if (result.option == gui2::tnew_tpl_widget::option_landscape) {
		ss << "#define " << widget_macro << "\n";
		// remove all attribute out of [stack]. for example border/border_size/horizontal_grow/vertical_grow
		bool one_attribute_removed = true;
		while (one_attribute_removed) {
			one_attribute_removed = false;
			BOOST_FOREACH (const config::attribute &istrmap, tpl_cfg.attribute_range()) {
				one_attribute_removed = true;
				tpl_cfg.remove_attribute(istrmap.first);
			}
		}
		// remove attribute: id in [stack]
		config::all_children_itors children = tpl_cfg.all_children_range();
		size_t nb_children = std::distance(children.first, children.second);
		VALIDATE(nb_children == 1, "Grid cell does not have exactly 1 child.");
		tpl_cfg.child(children.first->key).remove_attribute("id");

		::write(ss, tpl_cfg, 0, textdomain);

		ss << "#enddef\n\n";
	} else {
		// copy landscape section from file
		std::string prefix = "#define " + widget_macro + "\n";
		const char* start = strstr(cfg_file.data, prefix.c_str());
		VALIDATE(start, null_str);
		prefix = "#enddef\n\n";
		const char* end = strstr(start, prefix.c_str());
		std::string cut(start, end - start + prefix.size());
		ss << cut;
	}

	//
	// portrait section
	//
	bool has_portrait = false;
	if (result.option == gui2::tnew_tpl_widget::option_landscape && fsize) {
		std::string prefix = "#define " + widget_portrait_macro + "\n";
		const char* start = strstr(cfg_file.data, prefix.c_str());
		if (start) {
			prefix = "#enddef\n\n";
			const char* end = strstr(start, prefix.c_str());
			std::string cut(start, end - start + prefix.size());
			ss << cut;

			has_portrait = true;
		}
	} else if (result.option == gui2::tnew_tpl_widget::option_portrait) {
		has_portrait = true;
		ss << "#define " << widget_portrait_macro<< "\n";
		// remove all attribute out of [stack]. for example border/border_size/horizontal_grow/vertical_grow
		bool one_attribute_removed = true;
		while (one_attribute_removed) {
			one_attribute_removed = false;
			BOOST_FOREACH (const config::attribute &istrmap, tpl_cfg.attribute_range()) {
				one_attribute_removed = true;
				tpl_cfg.remove_attribute(istrmap.first);
			}
		}
		// remove attribute: id in [stack]
		config::all_children_itors children = tpl_cfg.all_children_range();
		size_t nb_children = std::distance(children.first, children.second);
		VALIDATE(nb_children == 1, "Grid cell does not have exactly 1 child.");
		tpl_cfg.child(children.first->key).remove_attribute("id");

		::write(ss, tpl_cfg, 0, textdomain);

		ss << "#enddef\n\n";
	}

	//
	// linked_group section
	//
	bool has_linked_group = false;
	if (result.option == gui2::tnew_tpl_widget::option_landscape) {
		if (!collected_linked_group_.empty()) {
			tpl_cfg.clear();
			ss << "#define " << linked_group_macro << "\n";

			for (std::set<std::string>::const_iterator it = collected_linked_group_.begin(); it != collected_linked_group_.end(); ++ it) {
				const std::string& id = *it;
				for (std::vector<gui2::tlinked_group>::iterator it2 = linked_groups_.begin(); it2 != linked_groups_.end(); ++ it2) {
					const gui2::tlinked_group& linked_group = *it2;
					if (linked_group.id == id) {
						config& linked_group_cfg = tpl_cfg.add_child("linked_group");
						linked_group_cfg["id"] = linked_group.id;
						if (linked_group.fixed_width) {
							linked_group_cfg["fixed_width"] = true;
						}
						if (linked_group.fixed_height) {
							linked_group_cfg["fixed_height"] = true;
						}
					}
				}
			}
			::write(ss, tpl_cfg, 0, textdomain);

			ss << "#enddef\n\n";
			has_linked_group = true;
		}
	} else {
		// if exist linked-group_section, copy.
		std::string prefix = "#define " + linked_group_macro + "\n";
		const char* start = strstr(cfg_file.data, prefix.c_str());
		if (start) {
			prefix = "#enddef\n\n";
			const char* end = strstr(start, prefix.c_str());
			std::string cut(start, end - start + prefix.size());
			ss << cut;

			has_linked_group = true;
		}
	}

	config top_cfg;
	config& tpl_widget_cfg = top_cfg.add_child("tpl_widget");
	tpl_widget_cfg["id"] = result.id;
	tpl_widget_cfg["app"] = result.app;
	if (has_linked_group) {
		tpl_widget_cfg["linked_group"] = linked_group_macro;
	}
	tpl_widget_cfg.add_child("landscape");
	if (has_portrait) {
		tpl_widget_cfg.add_child("portrait");
	}

	std::stringstream top_cfg_ss;
	::write(top_cfg_ss, top_cfg, 0, textdomain);

	//
	// post handler
	//
	std::string data = top_cfg_ss.str();
	// 1. insert: description=_"rose__chat template"
	// use c/c++ code can not generate below.
	std::string key = "[tpl_widget]";
	size_t pos = data.find(key);
	pos += key.size();
	const char* c_str = data.c_str();
	while (!utils::notspace(c_str[pos])) { pos ++; }

	std::string dst = "description=_\"" + app_prefix_id + " template\"\n\t";
	data.replace(pos, 0, dst);

	// 2. insert: {GUI__TPL_WIDGET_ROSE__CHAT} into [landscape][/landscape]
	key = "[landscape]";
	pos = data.find(key);
	pos += key.size();
	c_str = data.c_str();
	while (!utils::notspace(c_str[pos])) { pos ++; }

	dst = "\t{" + widget_macro + "}\n\t";
	data.replace(pos, 0, dst);

	if (has_portrait) {
		// 3. insert: {GUI__TPL_WIDGET_PORTRAIT_ROSE__CHAT} into [portrait][/portrait]
		key = "[portrait]";
		pos = data.find(key);
		pos += key.size();
		c_str = data.c_str();
		while (!utils::notspace(c_str[pos])) { pos ++; }

		const std::string dst = "\t{" + widget_portrait_macro + "}\n\t";
		data.replace(pos, 0, dst);
	}
	ss << data;

	if (fsize) {
		posix_fseek(cfg_file.fp, 0);
		posix_fwrite(cfg_file.fp, ss.str().c_str(), ss.str().size());
		cfg_file.truncate(ss.str().size());
	} else {
		tfile new_file(result.file, GENERIC_WRITE, CREATE_ALWAYS);
		posix_fwrite(new_file.fp, ss.str().c_str(), ss.str().size());
	}

	//
	// refresh gui
	//
	units_.erase(selected_hex_);

	for (std::vector<gui2::tlinked_group>::iterator it = linked_groups_.begin(); it != linked_groups_.end(); ) {
		const std::string& id = it->id;
		if (collected_linked_group_.find(id) != collected_linked_group_.end()) {
			it = linked_groups_.erase(it);
		} else {
			++ it;
		}
	}

	u = new unit(*this, *gui_, units_, std::make_pair(app_prefix_id, nullptr), parent.u, parent.number);
	units_.insert(selected_hex_, u);
	u->cell().widget.cell = cell; // for example: horizontal_alignment/vertical_alignment
	u->cell().id = original_id;

	replace_child_unit(u);
	if (u->has_child()) {
		layout_dirty();
	}

	last_unpack_tpl_widget_.second.clear();

	gui_->show_context_menu();
}

void mkwin_controller::insert_used_widget_tpl(const gui2::ttpl_widget& tpl)
{
	used_tpl_widget_.insert(&tpl);
}

void mkwin_controller::derive_create(unit* u)
{
	int create_childs = 0;
	if (u->is_grid() || u->is_stack() || u->is_panel() || u->is_scroll_panel()) {
		create_childs = 1;

	} else if (u->is_listbox()) {
		u->insert_listbox_child(default_child_width, default_child_height);
		return;
	} else if (u->is_tree()) {
		u->insert_treeview_child();
		return;
	}
	for (int i = 0; i < create_childs; i ++) {
		u->insert_child(default_child_width, default_child_height);
	}
}

void mkwin_controller::replace_child_unit(unit* u)
{
	const unit::tparent& parent = u->parent();
	unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	const map_location& window_loc = child.window->get_location();
	int pitch = (u->get_location().y - 1 - window_loc.y) * child.cols.size();
	int at = pitch + u->get_location().x - 1 - window_loc.x;
	child.units[at] = u;
}

void mkwin_controller::app_left_mouse_down(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return;
	}

	const map_location& mouseover_hex = gui_->mouseover_hex();
	if (!map_.on_board(mouseover_hex)) {
		return;
	}
	if (preview()) {
		int type = mouseover_unit_? dynamic_cast<unit*>(mouseover_unit_)->type(): unit::NONE;
		if (type == unit::WINDOW || type == unit::WIDGET) {
			selected_hex_ = mouseover_unit_->get_location();
		} else {
			selected_hex_ = map_location::null_location;
		}
	} else {
		selected_hex_ = mouseover_hex;
	}
	
	gui_->show_context_menu();
}

void mkwin_controller::app_right_mouse_down(const int x, const int y)
{
	do_right_click();
}

void mkwin_controller::do_right_click()
{
	selected_hex_ = map_location();
	gui_->show_context_menu();
}

void mkwin_controller::post_change_resolution()
{
	set_status();

	unit* u = units_.find_unit(selected_hex_);
	if (u) {
		if (preview()) {
			gui_->scroll_to_xy(u->get_rect().x, u->get_rect().y, display::ONSCREEN);
		} else {
			gui_->scroll_to_tile(u->get_location(), display::ONSCREEN);
		}
	}

	gui_->show_context_menu();
}

void mkwin_controller::longpress_widget(bool& halt, const tpoint& coordinate, gui2::twindow& window, const std::string& type, const std::string& definition)
{
	VALIDATE(gui_->selected_widget() == gui_->spacer, null_str);

	halt = true;

	const std::string filename = unit::form_widget_png(type, definition);
	surface surf(image::get_image(filename));

	window.set_drag_surface(surf, false);

	gui_->click_widget(type, definition);

	window.start_drag(coordinate, std::bind(&mkwin_controller::did_drag_mouse_motion, this, _1, _2, std::ref(window)),
		std::bind(&mkwin_controller::did_drag_mouse_leave, this, _1, _2, _3));
}

bool mkwin_controller::did_drag_mouse_motion(const int x, const int y, gui2::twindow& window)
{
	VALIDATE(!preview_, null_str);
	VALIDATE(gui_->selected_widget() != gui_->spacer, null_str);
	const unit* u = dynamic_cast<const unit*>(units_.find_base_unit(x, y));

	std::string border = "label-tooltip";
	gui2::tfloat_widget& drag_widget = window.float_drag();
	if (u) {
		if (u->is_spacer()) {
			// border = "float_widget";
		}

	}
	// float_drag.widget->set_border(border);

	return true;
}

void mkwin_controller::did_drag_mouse_leave(const int x, const int y, bool up_result)
{
	VALIDATE(!preview_, null_str);
	VALIDATE(gui_->selected_widget() != gui_->spacer, null_str);

	if (up_result) {
		unit* u = dynamic_cast<unit*>(units_.find_base_unit(x, y));
			
		if (u && u->is_spacer()) {
			const map_location hex = u->get_location();

			unit::tparent parent = u->parent();
			units_.erase(hex);

			units_.insert(hex, new unit(*this, *gui_, units_, gui_->selected_widget(), parent.u, parent.number));
			u = units_.find_unit(hex);
			derive_create(u);

			replace_child_unit(u);
			if (u->has_child()) {
				layout_dirty();
			}

			fill_object_list();

			u = units_.find_unit(hex);
			select_unit(*u);
		}
	}

	gui_->click_widget(gui_->spacer.first, gui_->spacer.second->id);
}

void mkwin_controller::did_scroll_header_item_changed(gui2::ttoggle_button& widget)
{
	long index = (long)widget.cookie();

	gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
	if (index == 0) {
		scene->load_widget_page();
	} else if (index == 1) {
		scene->load_object_page(units_);
	}
}

void mkwin_controller::form_linked_groups(const config& res_cfg)
{
	linked_groups_.clear();

	BOOST_FOREACH (const config& linked, res_cfg.child_range("linked_group")) {
		linked_groups_.push_back(gui2::tlinked_group(linked));
	}
}

void mkwin_controller::generate_linked_groups(config& res_cfg) const
{
	config tmp;
	for (std::vector<gui2::tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it) {
		const gui2::tlinked_group& linked = *it;

		tmp.clear();
		tmp["id"] = linked.id;
		if (linked.fixed_width) {
			tmp["fixed_width"] = true;
		}
		if (linked.fixed_height) {
			tmp["fixed_height"] = true;
		}
		res_cfg.add_child("linked_group", tmp);
	}
}

void mkwin_controller::form_context_menu(tmenu2& menu, const config& cfg, const std::string& menu_id)
{
	const std::string& items = cfg[menu_id].str();
	std::vector<std::string> vstr = utils::split(items);
	std::vector<std::string> vstr2;
	size_t flags;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		vstr2 = utils::split(id, '|');
		flags = 0;
		if (vstr2.size() >= 2) {
			flags = gui2::tcontext_menu::decode_flag_str(vstr[1]);
		}
		menu.items.push_back(tmenu2::titem(vstr2[0], NULL, flags & gui2::tcontext_menu::F_HIDE, flags & gui2::tcontext_menu::F_PARAM));
		size_t pos = id.rfind("_m");
		if (id.size() <= 2 || pos != id.size() - 2) {
			continue;
		}
		const std::string sub = id.substr(0, pos);
		menu.items.back().id = sub;
		menu.items.back().submenu = new tmenu2(null_str, sub, &menu);
		tmenu2& menu2 = *menu.items.back().submenu;
		form_context_menu(menu2, cfg, menu2.id);
	}
}

void mkwin_controller::form_context_menus(const config& res_cfg)
{
	context_menus_.clear();

	std::vector<std::string> vstr, vstr2;
	BOOST_FOREACH (const config &cfg, res_cfg.child_range("context_menu")) {
		const std::string& report = cfg["report"].str();
		const std::string& main = cfg["main"].str();
		if (report.empty() || main.empty()) {
			continue;
		}
		context_menus_.push_back(tmenu2(report, "main", NULL));

		tmenu2& menu = context_menus_.back();
		form_context_menu(menu, cfg, menu.id);
	}
	if (context_menus_.empty()) {
		context_menus_.push_back(tmenu2(null_str, "main", NULL));
	}
}

void mkwin_controller::generate_context_menus(config& res_cfg)
{
	for (std::vector<tmenu2>::const_iterator it = context_menus_.begin(); it != context_menus_.end(); ++ it) {
		const tmenu2& menu = *it;
		if (menu.items.empty()) {
			continue;
		}
		config& cfg = res_cfg.add_child("context_menu");
		menu.generate(cfg);
	}
}

void mkwin_controller::form_float_widgets(const config& res_cfg)
{
	float_widgets_.clear();

	std::vector<std::string> vstr, vstr2;
	BOOST_FOREACH (const config &cfg, res_cfg.child_range("float_widget")) {
		std::string type, definition, id;
		BOOST_FOREACH (const config::any_child &c, cfg.all_children_range()) {
			type = c.key;
			definition = c.cfg["definition"].str();
			id = c.cfg["id"].str();
			break;
		}
		if (reserve_float_widget_ids_.find(id) != reserve_float_widget_ids_.end()) {
			continue;
		}

		float_widgets_.push_back(tfloat_widget());
		float_widgets_.back().from(*this, *gui_, units_, cfg, type, definition, id);
	}
}

void mkwin_controller::generate_float_widgets(config& res_cfg)
{
	for (std::vector<tfloat_widget>::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++ it) {
		const tfloat_widget& item = *it;

		config& cfg = res_cfg.add_child("float_widget");
		item.generate(reserve_float_widget_ids_, cfg);
	}
}

void mkwin_controller::insert_float_widget(const std::string& id)
{
	VALIDATE(!id.empty(), null_str);

	float_widgets_.push_back(tfloat_widget());
	tfloat_widget& tmp = float_widgets_.back();

	const std::string type = "image";
	const std::string definition = "default";
	tmp.u.reset(new unit(*this, *gui_, units_, std::make_pair(type, gui_->find_widget(type, definition, id)), nullptr, -1));
	tmp.u->cell().id = id;
}

config mkwin_controller::generate_window_cfg() const
{
	config top, tmp;
	config& window_cfg = top.add_child("window");
	units_.find_unit(map_location(0, 0))->to_cfg(window_cfg);
	config& res_cfg = window_cfg.child("resolution");

	config& top_grid = res_cfg.add_child("grid");

	int rows = (int)top_.rows.size();
	int cols = (int)top_.cols.size();

	config unit_cfg;
	config* row_cfg = NULL;
	int current_y = -1;
	for (int y = 1; y < rows + 1; y ++) {
		for (int x = 1; x < cols + 1; x ++) {
			unit* u = units_.find_unit(map_location(x, y));
			if (u->get_location().y != current_y) {
				row_cfg = &top_grid.add_child("row");
				current_y = u->get_location().y;

				unit* row = units_.find_unit(map_location(0, y));
				row->to_cfg(*row_cfg);
			}
			unit_cfg.clear();
			u->to_cfg(unit_cfg);
			if (y == 1) {
				unit* column = units_.find_unit(map_location(x, 0));
				column->to_cfg(unit_cfg);
			}
			row_cfg->add_child("column", unit_cfg);
		}
	}

	return top;
}

std::vector<unit*> mkwin_controller::form_top_units() const
{
	std::vector<unit*> result;

	if (preview_) {
		VALIDATE(top_.units.empty(), "units in top_ must be empty!");
		result = units_.form_units();

	} else {
		result = top_.units;
	}

	return result;
}

void mkwin_controller::reload_map(int w, int h)
{
	map_ = tmap(generate_map_data(w, h, preview()));
	gui_->reload_map();
	units_.zero_map();
	units_.create_coor_map(map_.w(), map_.h());
}

bool same_parent(const unit* a, const unit* b)
{
	const unit* a_parent = a;
	const unit* b_parent = b;

	while (a_parent->parent().u) {
		a_parent = a_parent->parent().u;
		if (a_parent == b) {
			return true;
		}
	}
	while (b_parent->parent().u) {
		b_parent = b_parent->parent().u;
		if (b_parent == a) {
			return true;
		}
	}
	return false;
}

bool mkwin_controller::can_copy(const unit* u, bool cut) const
{
	if (preview_) {
		return false;
	}
	if (!u) {
		return false;
	}
	if (u == copied_unit_) {
		if (cut_) {
			return !cut;
		} else {
			return cut;
		}
	}
	if (u->type() == unit::WINDOW) {
		return false;
	}
	if (u->is_main_map()) {
		return false;
	}
	return true;
}

bool mkwin_controller::can_paste(const unit* u) const
{
	if (preview_) {
		return false;
	}
	if (!u || !copied_unit_ || u == copied_unit_) {
		return false;
	}
	if (u->type() != copied_unit_->type()) {
		return false;
	}

	unit::tparent this_parent = u->parent();
	unit::tparent that_parent = copied_unit_->parent();

	if (u->type() == unit::ROW) {
		int this_cols = this_parent.u? this_parent.u->child(this_parent.number).cols.size(): top_.cols.size(); 
		int that_cols = that_parent.u? that_parent.u->child(that_parent.number).cols.size(): top_.cols.size();
		if (this_cols != that_cols) {
			return false;
		}
		
	} else if (u->type() == unit::COLUMN) {
		int this_rows = this_parent.u? this_parent.u->child(this_parent.number).rows.size(): top_.rows.size(); 
		int that_rows = that_parent.u? that_parent.u->child(that_parent.number).rows.size(): top_.rows.size();
		if (this_rows != that_rows) {
			return false;
		}

	} else if (!u->is_spacer()) {
		return false;
	}

	return !same_parent(u, copied_unit_);
}

bool mkwin_controller::can_adjust_row(const unit* u) const
{
	const unit::tparent& parent = u->parent();
	if (parent.u) {
		// return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::can_adjust_column(const unit* u) const
{
	const unit::tparent& parent = u->parent();
	if (parent.u) {
		return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::can_erase(const unit* u) const
{
	if (preview_) {
		return false;
	}

	if (!u) {
		return false;
	}

	const unit::tparent& parent = u->parent();

	if (u->type() == unit::WINDOW) {
		return parent.u && parent.u->is_grid();
	}
	if (u->type() != unit::WIDGET) {
		return false;
	}
	if (u->is_spacer() || u->is_main_map()) {
		return false;
	}
	if (parent.u) {
		return !parent.u->is_listbox() || parent.number != 1;
	}
	return true;
}

bool mkwin_controller::app_in_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);

	switch(command) {
	// idle section
	case tmkwin_scene::HOTKEY_BUILD:
	{
		gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
		return !selected_hex_.valid() && scene->require_build();
	}

	case tmkwin_scene::HOTKEY_PREVIEW:
	case HOTKEY_ZOOM_IN:
	case HOTKEY_ZOOM_OUT:
		return preview_ || !selected_hex_.valid();
	case HOTKEY_SYSTEM:
		return !selected_hex_.valid() && !preview_;

	case tmkwin_scene::HOTKEY_SETTING: // setting
		return !preview_ && u && (u->type() != unit::WINDOW || (!u->parent().u || u->parent().u->is_stack()));
	case tmkwin_scene::HOTKEY_SPECIAL_SETTING:
		return false;

	case HOTKEY_COPY:
		return can_copy(u, false);
	case HOTKEY_CUT:
		return can_copy(u, true);
	case HOTKEY_PASTE:
		return can_paste(u);

	// unit
	case tmkwin_scene::HOTKEY_ERASE: // erase
		return can_erase(u);

	// window
	case tmkwin_scene::HOTKEY_LINKED_GROUP:
		return !preview_ && u && ((u->type() == unit::WINDOW && !u->parent().u) || u->is_listbox());

	// row
	case tmkwin_scene::HOTKEY_INSERT_TOP:
	case tmkwin_scene::HOTKEY_INSERT_BOTTOM:
	case tmkwin_scene::HOTKEY_ERASE_ROW:
		return !preview() && u && u->type() == unit::ROW && can_adjust_row(u);

	// column
	case tmkwin_scene::HOTKEY_INSERT_LEFT:
	case tmkwin_scene::HOTKEY_INSERT_RIGHT:
	case tmkwin_scene::HOTKEY_ERASE_COLUMN:
		return !preview() && u && u->type() == unit::COLUMN && can_adjust_column(u);

	case tmkwin_scene::HOTKEY_INSERT_CHILD: // add a page
		return u && u->is_stack();
	case tmkwin_scene::HOTKEY_ERASE_CHILD: // erase this page
		return u && u->type() == unit::WINDOW && u->parent().u && u->parent().u->is_stack() && u->parent().u->children().size() >= 2;

	case tmkwin_scene::HOTKEY_PACK:
		return !preview_ && u && u->type() == unit::WIDGET && !u->is_tpl() && !u->is_spacer() && !u->is_grid();

	case tmkwin_scene::HOTKEY_UNPACK:
		return !preview_ && u && u->is_tpl();

	default:
		return false;
	}

	return false;
}

bool mkwin_controller::actived_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const unit* u = units_.find_unit(selected_hex_, true);
	if (!u) {
		return true;
	}
	const unit::tparent& parent = u->parent();
	const unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	switch(command) {
	// row
	case tmkwin_scene::HOTKEY_ERASE_ROW:
		return child.rows.size() >= 2;

	// column
	case tmkwin_scene::HOTKEY_ERASE_COLUMN:
		return child.cols.size() >= 2;

	}
	return true;
}

bool mkwin_controller::verify_new_float_widget(const std::string& label, const int at)
{
	const int min_id_len = 1;
	const int max_id_len = 32;

	const std::string id = utils::lowercase(label);
	if (!utils::isvalid_id(id, false, min_id_len, max_id_len)) {
		return false;
	}
	int at2 = 0;
	for (std::vector<tfloat_widget>::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++ it, at2 ++) {
		if (at != at2 && it->u->cell().id == id) {
			return false;
		}
	}
	return true;
}

void mkwin_controller::refresh_title()
{
	std::stringstream ss;
	gui2::tcontrol* label = dynamic_cast<gui2::tcontrol*>(gui_->get_theme_object("title"));
	if (preview_) {
		ss << _("Theme");
	} else {
		ss << _("Dialog");
	}
	ss << " (";

	std::string file = "---";
	if (!file_.empty()) {
		file = extract_file(file_);
	}
	ss << ht::generate_format(file, color_to_uint32(font::BLUE_COLOR));
	ss << ")";

	label->set_label(ss.str());
}

