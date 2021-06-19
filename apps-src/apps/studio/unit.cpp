/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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

#include "unit.hpp"
#include "gettext.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "game_config.hpp"

using namespace std::placeholders;
#include <numeric>


int unit::preview_widget_best_width = 0;
int unit::preview_widget_best_height = 0;
const std::string unit::widget_prefix = "widget/";

std::string unit::form_widget_png(const std::string& type, const std::string& definition)
{
	return widget_prefix + type + ".png";
}

unit::unit(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent, int number)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, widget_(widget)
	, type_(WIDGET)
	, parent_(tparent(parent, number))
{
}

unit::unit(mkwin_controller& controller, mkwin_display& disp, unit_map& units, int type, unit* parent, int number)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, widget_()
	, type_(type)
	, parent_(tparent(parent, number))
{
	if (type == WINDOW && !parent) {
		cell_.id = gui2::untitled;
	}
}

unit::unit(const unit& that)
	: base_unit(that)
	, controller_(that.controller_)
	, disp_(that.disp_)
	, units_(that.units_)
	, type_(that.type_)
	, parent_(that.parent_)
	, widget_(that.widget_)
	, cell_(that.cell_)
{
	// caller require call set_parent to set self-parent.

	int number = 0;
	for (std::vector<tchild>::const_iterator it = that.children_.begin(); it != that.children_.end(); ++ it, number ++) {
		const tchild& child = *it;
		children_.push_back(tchild());
		tchild& child2 = children_.back();
		child2.window = new unit(*child.window);
		child2.window->parent_ = tparent(this, number);

		for (std::vector<unit*>::const_iterator it2 = child.rows.begin(); it2 != child.rows.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = tparent(this, number);

			child2.rows.push_back(u);
		}
		for (std::vector<unit*>::const_iterator it2 = child.cols.begin(); it2 != child.cols.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = tparent(this, number);
			child2.cols.push_back(u);
		}
		for (std::vector<unit*>::const_iterator it2 = child.units.begin(); it2 != child.units.end(); ++ it2) {
			const unit& that2 = **it2;
			unit* u = new unit(that2);
			u->parent_ = tparent(this, number);
			child2.units.push_back(u);
		}
	}
}

unit::~unit()
{
	for (std::vector<tchild>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		it->clear(true);
	}
	if (this == controller_.copied_unit()) {
		controller_.set_copied_unit(NULL);
	}
}

bool unit::is_tpl() const
{
	return type_ == WIDGET && gui2::is_tpl_widget(widget_.first);
}

void unit::redraw_widget(int xsrc, int ysrc, int width, int height) const
{
	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image(), image::UNSCALED, xsrc, ysrc, width / 2, height / 2);
	if (is_tpl()) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
				loc_, "misc/tpl_widget_flag.png", image::UNSCALED, xsrc, ysrc, width / 4, height / 4);
	}

	if (!controller_.preview() && widget_.second.get() && widget_.second->id == "default") {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
			loc_, widget_prefix + "default.png", image::SCALED_TO_ZOOM, xsrc, ysrc, 0, 0);
	}

	if (true) {
		unsigned h_flag = cell_.widget.cell.flags_ & gui2::tgrid::HORIZONTAL_MASK;
		image::tblit& blit = disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, gui2::horizontal_align.find(h_flag)->second.icon, image::UNSCALED, xsrc + width / 2, ysrc, 0, 0);
		blit.width = width / 4;
		blit.height = height / 4;

		unsigned v_flag = cell_.widget.cell.flags_ & gui2::tgrid::VERTICAL_MASK;
		image::tblit& blit2 = disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, gui2::vertical_align.find(v_flag)->second.icon, image::UNSCALED, xsrc + width * 3 / 4, ysrc, 0, 0);
		blit2.width = width / 4;
		blit2.height = height / 4;
	}

	surface text_surf;
	if (!cell_.id.empty()) {
		std::string id2 = cell_.id;
		surface text_surf = font::get_rendered_text(id2, 0, 10 * gui2::twidget::hdpi_scale, font::BLACK_COLOR);
		if (text_surf->w > width) {
			SDL_Rect r = create_rect(0, 0, width, text_surf->h);
			text_surf = cut_surface(text_surf, r); 
		}
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, text_surf, xsrc, ysrc + height / 2, 0, 0);
	}
	if (!cell_.widget.label.empty()) {
		text_surf = font::get_rendered_text(cell_.widget.label, 0, 10 * gui2::twidget::hdpi_scale, font::BLUE_COLOR);
		if (text_surf->w > width) {
			SDL_Rect r = create_rect(0, 0, width, text_surf->h);
			text_surf = cut_surface(text_surf, r); 
		}
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, text_surf, xsrc, ysrc + height - text_surf->h, 0, 0);
	}

	if (is_spacer() && (!cell_.widget.width.empty() || !cell_.widget.height.empty())) {
		if (!cell_.widget.width.empty()) {
			text_surf = font::get_rendered_text(cell_.widget.width, 0, 10 * gui2::twidget::hdpi_scale, font::BAD_COLOR);
		} else {
			text_surf = font::get_rendered_text("--", 0, 10 * gui2::twidget::hdpi_scale, font::BAD_COLOR);
		}
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, text_surf, xsrc, ysrc + height - 2 * text_surf->h, 0, 0);

		if (!cell_.widget.height.empty()) {
			text_surf = font::get_rendered_text(cell_.widget.height, 0, 10 * gui2::twidget::hdpi_scale, font::BAD_COLOR);
		} else {
			text_surf = font::get_rendered_text("--", 0, 10 * gui2::twidget::hdpi_scale, font::BAD_COLOR);
		}
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
			loc_, text_surf, xsrc, ysrc + height - text_surf->h, 0, 0);
	}
}

void unit::app_draw_unit(const int xsrc, const int ysrc)
{
	SDL_Rect dst_rect;
	int zoom = disp_.hex_width();
	surface surf = create_neutral_surface(zoom, zoom);

	const unit::tchild& child = parent_.u? parent_.u->child(parent_.number): controller_.top();
	Uint32 color = child.window->cell().window.color;

	if (this == controller_.copied_unit()) {
		SDL_Color col = font::BAD_COLOR;
		if (redraw_counter_ % 30 < 15) {
			col = font::GOOD_COLOR;
		}
		color = SDL_MapRGB(&get_neutral_pixel_format(), col.r, col.g, col.b);
	}

	if (type_ == WIDGET) {
		if (disp_.selected_widget() != disp_.spacer && is_spacer()) {
			int xmouse, ymouse;
			SDL_GetMouseState(&xmouse, &ymouse);
			SDL_Rect rcscreen {xsrc, ysrc, zoom, zoom};
			if (point_in_rect(xmouse, ymouse, rcscreen)) {
				if ((redraw_counter_ / 10) & 0x1) {
					fill_surface(surf, 0x20008000);
				}
			}
		}

		{
			surface_lock locker(surf);
			draw_line(surf, color, 0, 0, zoom - 1, 0);
			draw_line(surf, color, 0, 0, 0, zoom - 1);
			if (!units_.find_unit(map_location(loc_.x + 1, loc_.y))) {
				draw_line(surf, color, zoom - 1, 0, zoom - 1, zoom - 1);
			}
			if (!units_.find_unit(map_location(loc_.x, loc_.y + 1))) {
				draw_line(surf, color, 0, zoom - 1, zoom - 1, zoom - 1);
			}
		}
		redraw_widget(xsrc, ysrc, zoom, zoom);

		if (has_child()) {
			blit_integer_surface(children_.size(), surf, 0, 0);
		}

	} else if (type_ == WINDOW) {
		{
			surface_lock locker(surf);
			draw_line(surf, color, 0, 0, zoom - 1, 0);
			draw_line(surf, color, 0, 0, 0, zoom - 1);
		}
		
		if (!parent_.u) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
				loc_, image(), image::UNSCALED, xsrc, ysrc, zoom / 2, zoom / 2);
		} else {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
				loc_, parent_.u->image(), image::UNSCALED, xsrc, ysrc, zoom / 2, zoom / 2);
			if (parent_.u->is_stack()) {
				blit_integer_surface(parent_.number + 1, surf, 0, 0);
			} else if (parent_.u->is_listbox()) {
				surface text_surf = font::get_rendered_text(parent_.number? "Body": "Header", 0, 12 * gui2::twidget::hdpi_scale, font::BAD_COLOR);
				dst_rect = create_rect(0, 0, 0, 0);
				sdl_blit(text_surf, NULL, surf, &dst_rect);
			} else if (parent_.u->is_panel()) {
				if (!parent_.u->cell().id.empty()) {
					surface text_surf = font::get_rendered_text(parent_.u->cell().id, 0, 10 * gui2::twidget::hdpi_scale, font::BLACK_COLOR);
					dst_rect = create_rect(0, 0, 0, 0);
					sdl_blit(text_surf, NULL, surf, &dst_rect);
				}
			}
		}


		if (!cell_.id.empty()) {
			surface text_surf = font::get_rendered_text(cell_.id, 0, 10 * gui2::twidget::hdpi_scale, font::BLACK_COLOR);
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT,
				loc_, text_surf, xsrc, ysrc + zoom / 2, 0, 0);
		}

	} else if (type_ == COLUMN) {
		{
			surface_lock locker(surf);
			draw_line(surf, color, 0, 0, zoom - 1, 0);
			draw_line(surf, color, 0, 0, 0, zoom - 1);
			if (!units_.find_unit(map_location(loc_.x + 1, loc_.y))) {
				draw_line(surf, color, zoom - 1, 0, zoom - 1, zoom - 1);
			}
		}

		blit_integer_surface(loc_.x - child.window->get_location().x, surf, 0, 0);
		blit_integer_surface(cell_.column.grow_factor, surf, 0, 12);

	} else if (type_ == ROW) {
		{
			surface_lock locker(surf);
			draw_line(surf, color, 0, 0, zoom - 1, 0);
			draw_line(surf, color, 0, 0, 0, zoom - 1);
			if (!units_.find_unit(map_location(loc_.x, loc_.y + 1))) {
				draw_line(surf, color, 0, zoom - 1, zoom - 1, zoom - 1);
			}
		}

		blit_integer_surface(loc_.y - child.window->get_location().y, surf, 0, 0);
		blit_integer_surface(cell_.row.grow_factor, surf, 0, 12);

	}
	if (surf) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf, xsrc, ysrc, 0, 0);
	}

	int ellipse_floating = 0;
	if (loc_ == controller_.selected_hex()) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
			"misc/ellipse_top.png", image::SCALED_TO_ZOOM, xsrc, ysrc - ellipse_floating, 0, 0);
	
		disp_.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			"misc/ellipse_bottom.png", image::SCALED_TO_ZOOM, xsrc, ysrc - ellipse_floating, 0, 0);
	}
}

std::string unit::image() const
{
	if (type_ == COLUMN) {
		return widget_prefix + "column.png";

	} else if (type_ == ROW) {
		return widget_prefix + "row.png";

	} else if (type_ == WINDOW) {
		return widget_prefix + "window.png";

	} else if (is_tpl()) {
		return form_widget_png(widget_.first, null_str);

	} else if (widget_.first == "grid") {
		return widget_prefix + "grid.png";

	} else if (widget_.second.get()) {
		return form_widget_png(widget_.first, widget_.second->id);

	} else {
		return widget_prefix + "bg.png";
	}
}

void unit::set_cell(const gui2::tcell_setting& cell) 
{ 
	cell_ = cell;
	if (is_grid()) {
		children_[0].window->cell().id = cell_.id;
	}
}

void unit::set_child(int number, const tchild& child)
{
	// don't delete units that in children_[number].
	// unit's pointer is share new and old.
	children_[number] = child;
}

int unit::children_layers() const
{
	int max = 0;
	if (!children_.empty()) {
		const tchild& child = children_[0];
		int val;
		for (std::vector<unit*>::const_iterator it2 = child.units.begin(); it2 != child.units.end(); ++ it2) {
			const unit& u = **it2;
			val = u.children_layers();
			if (val > max) {
				max = val;
			}
		}
		return 1 + max;
	}
	return 0;
}

bool unit::is_main_map() const 
{ 
	return type_ == WIDGET && cell_.id == "_main_map"; 
}

const unit* unit::parent_at_top() const
{
	const unit* result = this;
	while (result->parent_.u) {
		result = result->parent_.u;
	}
	return result;
}

void unit::insert_child(int w, int h)
{
	unit_map::tconsistent_lock lock(units_);

	tchild child;

	child.window = new unit(controller_, disp_, units_, WINDOW, this, children_.size());
	// child.window->set_location(map_location(0, 0));
	for (int x = 1; x < w; x ++) {
		child.cols.push_back(new unit(controller_, disp_, units_, COLUMN, this, children_.size()));
		// child.cols.back()->set_location(map_location(x, 0));
	}
	for (int y = 1; y < h; y ++) {
		int pitch = y * w;
		for (int x = 0; x < w; x ++) {
			if (x) {
				child.units.push_back(new unit(controller_, disp_, units_, disp_.spacer, this, children_.size()));
				// child.cols.back()->set_location(map_location(x, y));
			} else {
				child.rows.push_back(new unit(controller_, disp_, units_, ROW, this, children_.size()));
				// child.rows.back()->set_location(map_location(0, y));
			}
		}
	}
	children_.push_back(child);
}

void unit::erase_child(int index)
{
	std::vector<tchild>::iterator it = children_.begin();
	std::advance(it, index);
	it->erase(units_);
	children_.erase(it);
	for (int n = index; n < (int)children_.size(); n ++) {
		const unit::tchild& child = children_[n];
		child.window->set_parent_number(n);
		for (std::vector<unit*>::const_iterator it = child.rows.begin(); it != child.rows.end(); ++ it) {
			unit* u = *it;
			u->set_parent_number(n);
		}
		for (std::vector<unit*>::const_iterator it = child.cols.begin(); it != child.cols.end(); ++ it) {
			unit* u = *it;
			u->set_parent_number(n);
		}
		for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
			unit* u = *it;
			u->set_parent_number(n);
		}
	}
}

void unit::insert_listbox_child(int w, int h)
{
	// header
	insert_child(w, h);

	// body must is toggle_panel
	unit_map::tconsistent_lock lock(units_);

	int body_w = 2;
	tchild child;
	child.window = new unit(controller_, disp_, units_, WINDOW, this, children_.size());
	for (int x = 1; x < body_w; x ++) {
		child.cols.push_back(new unit(controller_, disp_, units_, COLUMN, this, children_.size()));
	}
	for (int y = 1; y < body_w; y ++) {
		int pitch = y * body_w;
		for (int x = 0; x < body_w; x ++) {
			if (x) {
				child.units.push_back(new unit(controller_, disp_, units_, disp_.toggle_panel, this, children_.size()));
				child.units.back()->insert_child(mkwin_controller::default_child_width, mkwin_controller::default_child_height);
			} else {
				child.rows.push_back(new unit(controller_, disp_, units_, ROW, this, children_.size()));
			}
		}
	}
	VALIDATE(child.cols.size() * child.rows.size() == child.units.size(), "count of unit mistake!");
	children_.push_back(child);
}

void unit::insert_treeview_child()
{
	unit_map::tconsistent_lock lock(units_);

	int cols = 3, lines = 2;
	tchild child;
	child.window = new unit(controller_, disp_, units_, WINDOW, this, children_.size());
	for (int x = 1; x < cols; x ++) {
		child.cols.push_back(new unit(controller_, disp_, units_, COLUMN, this, children_.size()));
	}
	for (int y = 1; y < lines; y ++) {
		for (int x = 0; x < cols; x ++) {
			if (x == 1) {
				child.units.push_back(new unit(controller_, disp_, units_, disp_.toggle_button, this, children_.size()));
				child.units.back()->cell().id = "__icon";
			} else if (x) {
				child.units.push_back(new unit(controller_, disp_, units_, disp_.spacer, this, children_.size()));
			} else if (x == 0) {
				child.rows.push_back(new unit(controller_, disp_, units_, ROW, this, children_.size()));
			}
		}
	}

	VALIDATE(child.cols.size() * child.rows.size() == child.units.size(), "count of unit mistake!");
	children_.push_back(child);
}

std::string unit::child_tag(int index) const
{
	std::stringstream ss;
	if (is_grid()) {
		return "GRID";
	} else if (is_stack()) {
		ss << "S#" << index;
		return ss.str();
	} else if (is_listbox()) {
		if (!index) {
			return "Header";
		} else if (index == 1) {
			return "Body";
		} else if (index == 2) {
			return "Footer";
		}
	} else if (is_panel() || is_scroll_panel()) {
		return "Panel";
	} else if (is_tree()) {
		return "Node";
	}
	return null_str;
}

void unit::to_cfg(config& cfg) const
{
	if (type_ == WIDGET) {
		to_cfg_widget(cfg);
	} else if (type_ == WINDOW) {
		to_cfg_window(cfg);
	} else if (type_ == ROW) {
		to_cfg_row(cfg);
	} else if (type_ == COLUMN) {
		to_cfg_column(cfg);
	}
}

std::string unit::formual_fill_str(const std::string& core)
{
	return std::string("(") + core + ")";
}

std::string unit::formual_extract_str(const std::string& str)
{
	size_t s = str.size();
	if (s < 2 || str.at(0) != '(' || str.at(s -1) != ')') {
		return str;
	}
	return str.substr(1, s - 2);
}

void unit::generate_main_map_border(config& cfg) const
{
	if (cell_.window.tile_shape == game_config::tile_hex) {
		cfg["border_size"] = 0.5;
		cfg["background_image"] = "terrain-hexagonal/off-map/background.png";

	} else {
		cfg["border_size"] = 0;
		cfg["view_rectangle_color"] = "blue";

		cfg["background_image"] = "terrain-square/off-map/background.png";
	}
	cfg["tile_image"] = "off-map/alpha.png";

	cfg["corner_image_top_left"] = "terrain-hexagonal/off-map/fade_corner_top_left_editor.png";
	cfg["corner_image_bottom_left"] = "terrain-hexagonal/off-map/fade_corner_bottom_left_editor.png";

	// odd means the corner is on a tile with an odd x value,
	// the tile is the ingame tile not the odd in C++
	cfg["corner_image_top_right_odd"] = "terrain-hexagonal/off-map/fade_corner_top_right_odd_editor.png";
	cfg["corner_image_top_right_even"] = "terrain-hexagonal/off-map/fade_corner_top_right_even_editor.png";

	cfg["corner_image_bottom_right_odd"] = "terrain-hexagonal/off-map/fade_corner_bottom_right_odd_editor.png";
	cfg["corner_image_bottom_right_even"] = "terrain-hexagonal/off-map/fade_corner_bottom_right_even_editor.png";

	cfg["border_image_left"] = "terrain-hexagonal/off-map/fade_border_left_editor.png";
	cfg["border_image_right"] = "terrain-hexagonal/off-map/fade_border_right_editor.png";

	cfg["border_image_top_odd"] = "terrain-hexagonal/off-map/fade_border_top_odd_editor.png";
	cfg["border_image_top_even"] = "terrain-hexagonal/off-map/fade_border_top_even_editor.png";

	cfg["border_image_bottom_odd"] = "terrain-hexagonal/off-map/fade_border_bottom_odd_editor.png";
	cfg["border_image_bottom_even"] = "terrain-hexagonal/off-map/fade_border_bottom_even_editor.png";
}

void unit::to_cfg_window(config& cfg) const
{
	cfg["id"] = cell_.id;
	cfg["app"] = cell_.window.app;

	config& res_cfg = cfg.add_child("resolution");

	if (cell_.window.orientation != gui2::twidget::auto_orientation) {
		res_cfg["orientation"] = gui2::orientations.find(cell_.window.orientation)->second.id;
	}
	if (cell_.window.scene) {
		res_cfg["scene"] = true;

		config& theme_cfg = res_cfg.add_child("scene");
		VALIDATE(!cell_.window.tile_shape.empty(), null_str);
		theme_cfg["tile_shape"] = cell_.window.tile_shape;

		config& border_cfg = theme_cfg.add_child("main_map_border");
		generate_main_map_border(border_cfg);
	}

	// whether scene or dialog, twindow_setting must make sure below field right.
	res_cfg["definition"] = cell_.window.definition;
	if (cell_.window.click_dismiss) {
		res_cfg["click_dismiss"] = true;
	}
	if (cell_.window.automatic_placement) {
		if (cell_.window.horizontal_placement != gui2::tgrid::HORIZONTAL_ALIGN_CENTER) {
			res_cfg["horizontal_placement"] = gui2::horizontal_align.find(cell_.window.horizontal_placement)->second.id;
		}
		if (cell_.window.vertical_placement != gui2::tgrid::VERTICAL_ALIGN_CENTER) {
			res_cfg["vertical_placement"] = gui2::vertical_align.find(cell_.window.vertical_placement)->second.id;
		}
	} else {
		res_cfg["automatic_placement"] = false;
		if (!cell_.window.x.empty()) {
			res_cfg["x"] = formual_fill_str(cell_.window.x);
		}
		if (!cell_.window.y.empty()) {
			res_cfg["y"] = formual_fill_str(cell_.window.y);
		}
		if (!cell_.window.width.empty()) {
			res_cfg["width"] = formual_fill_str(cell_.window.width);
		}
		if (!cell_.window.height.empty()) {
			res_cfg["height"] = formual_fill_str(cell_.window.height);
		}
	}

	controller_.generate_linked_groups(res_cfg);
	controller_.generate_context_menus(res_cfg);
	controller_.generate_float_widgets(res_cfg);
}

void unit::to_cfg_row(config& cfg) const
{
	if (cell_.row.grow_factor) {
		cfg["grow_factor"] = cell_.row.grow_factor;
	}
}

void unit::to_cfg_column(config& cfg) const
{
	if (cell_.column.grow_factor) {
		cfg["grow_factor"] = cell_.column.grow_factor;
	}
}

void unit::to_cfg_widget(config& cfg) const
{
	std::stringstream ss;

	if (cell_.widget.cell.border_size_ && cell_.widget.cell.flags_ & gui2::tgrid::BORDER_ALL) {
		ss.str("");
		if ((cell_.widget.cell.flags_ & gui2::tgrid::BORDER_ALL) == gui2::tgrid::BORDER_ALL) {
			ss << "all";
		} else {
			bool first = true;
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_LEFT) {
				first = false;
				ss << "left";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_RIGHT) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "right";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_TOP) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "top";
			}
			if (cell_.widget.cell.flags_ & gui2::tgrid::BORDER_BOTTOM) {
				if (!first) {
					ss << ", ";
				}
				first = false;
				ss << "bottom";
			}
		}

		cfg["border"] = ss.str();
		cfg["border_size"] = (int)cell_.widget.cell.border_size_;
	}

	unsigned h_flag = cell_.widget.cell.flags_ & gui2::tgrid::HORIZONTAL_MASK;
	if (h_flag != gui2::tgrid::HORIZONTAL_ALIGN_CENTER) {
		cfg["horizontal_alignment"] = gui2::horizontal_align.find(h_flag)->second.id;
	}
	unsigned v_flag = cell_.widget.cell.flags_ & gui2::tgrid::VERTICAL_MASK;
	if (v_flag != gui2::tgrid::VERTICAL_ALIGN_CENTER) {
		cfg["vertical_alignment"] = gui2::vertical_align.find(v_flag)->second.id;
	}

	config& sub = cfg.add_child(widget_.first);
	if (!cell_.id.empty()) {
		sub["id"] = cell_.id;
	}
	if (has_size()) {
		if (!cell_.widget.width.empty()) {
			sub["width"] = formual_fill_str(cell_.widget.width);
		}
		if (!cell_.widget.height.empty()) {
			sub["height"] = formual_fill_str(cell_.widget.height);
		}
		if (!cell_.widget.min_text_width.empty()) {
			sub["min_text_width"] = formual_fill_str(cell_.widget.min_text_width);
		}
	}

	if (gui2::gui.tpl_widgets.find(widget_.first) != gui2::gui.tpl_widgets.end()) {
		to_cfg_widget_tpl(sub);
		return;
	}

	if (!cell_.widget.linked_group.empty()) {
		sub["linked_group"] = cell_.widget.linked_group;
		controller_.collect_linked_group_if_necessary(cell_.widget.linked_group);
	}
	if (widget_.second.get()) {
		sub["definition"] = utils::generate_app_prefix_id(widget_.second->app, widget_.second->id);
		if (has_mtwusc()) {
			if (cell_.widget.mtwusc) {
				sub["mtwusc"] = true;
			}
		}

		if (widget_.second->label_is_text) {
			if (cell_.widget.text_font_size != font_cfg_reference_size) {
				sub["text_font_size"] = cell_.widget.text_font_size;
			}
			if (cell_.widget.text_color_tpl) {
				sub["text_color_tpl"] = cell_.widget.text_color_tpl;
			}
		}
		if (has_size_is_max()) {
			if (cell_.widget.width_is_max) {
				sub["width_is_max"] = true;
			}
			if (cell_.widget.height_is_max) {
				sub["height_is_max"] = true;
			}
		}
		if (has_atom_markup()) {
			if (cell_.widget.atom_markup) {
				sub["atom_markup"] = true;
			}
		}
		if (has_margin()) {
			if (cell_.widget.margin.left != nposm) {
				sub["left_margin"] = cell_.widget.margin.left;
			}
			if (cell_.widget.margin.right != nposm) {
				sub["right_margin"] = cell_.widget.margin.right;
			}
			if (cell_.widget.margin.top != nposm) {
				sub["top_margin"] = cell_.widget.margin.top;
			}
			if (cell_.widget.margin.bottom != nposm) {
				sub["bottom_margin"] = cell_.widget.margin.bottom;
			}
		}
	}

	if (is_scroll()) {
		if (is_report() && cell_.widget.vertical_mode != gui2::tscroll_container::auto_visible) {
			sub["horizontal_scrollbar_mode"] = gui2::horizontal_mode.find(gui2::tscroll_container::always_invisible)->second.id;

		} else if (cell_.widget.horizontal_mode != gui2::tscroll_container::auto_visible) {
			sub["horizontal_scrollbar_mode"] = gui2::horizontal_mode.find(cell_.widget.horizontal_mode)->second.id;
		}

		if (cell_.widget.vertical_mode != gui2::tscroll_container::auto_visible) {
			sub["vertical_scrollbar_mode"] = gui2::vertical_mode.find(cell_.widget.vertical_mode)->second.id;
		}
	}

	if (!cell_.widget.label.empty()) {
		if (cell_.widget.label_textdomain.empty()) {
			sub["label"] = cell_.widget.label;
		} else {
			sub["label"] = t_string(cell_.widget.label, cell_.widget.label_textdomain);
		}
	}

	if (!cell_.widget.tooltip.empty()) {
		if (cell_.widget.tooltip_textdomain.empty()) {
			sub["tooltip"] = cell_.widget.tooltip;
		} else {
			sub["tooltip"] = t_string(cell_.widget.tooltip, cell_.widget.tooltip_textdomain);
		}
	}

	if (is_grid()) {
		generate_grid(sub);

	} else if (is_stack()) {
		generate_stack(sub);

	} else if (is_listbox()) {
		generate_listbox(sub);

	} else if (is_panel()) {
		generate_toggle_panel(sub);

	} else if (is_scroll_panel()) {
		generate_scroll_panel(sub);

	} else if (is_tree()) {
		generate_tree_view(sub);

	} else if (is_report()) {
		generate_report(sub);

	} else if (is_slider()) {
		generate_slider(sub);

	} else if (is_text_box()) {
		generate_text_box(sub);

	} else if (is_label()) {
		generate_label(sub);

	}
}

void unit::to_cfg_widget_tpl(config& subcfg) const
{
	// [column]
	//		{GUI__CHAT_WIDGET}
	// [/column]
	std::string tpl_id = widget_.first;
	const gui2::ttpl_widget& tpl = gui2::get_tpl_widget(tpl_id);

	controller_.insert_used_widget_tpl(tpl);
}

void unit::generate_grid(config& cfg) const
{
	// [grid]
	//		[..cfg..]
	// [/grid]
	const tchild& child = children_[0];
	child.generate(cfg);
}

void unit::generate_stack(config& cfg) const
{
	// [stack]
	//		[..cfg..]
	// [/stack]

	for (std::vector<tchild>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		const tchild& child = *it;
		if (child.is_all_spacer()) {
			continue;
		}
		if (cell_.widget.stack.mode != gui2::tstack::pip) {
			cfg["mode"] = gui2::implementation::form_stack_mode_str(cell_.widget.stack.mode);
		}
		config& layer_cfg = cfg.add_child("layer");
		child.generate(layer_cfg);
	}
}

void unit::generate_listbox(config& cfg) const
{
	// [listbox]
	//		[..cfg..]
	// [/listbox]

	for (std::vector<gui2::tlinked_group>::const_iterator it = cell_.widget.listbox.linked_groups.begin(); it != cell_.widget.listbox.linked_groups.end(); ++ it) {
		const gui2::tlinked_group& linked = *it;
		linked.generate(cfg);
	}

	if (!children_[0].is_all_spacer()) {
		children_[0].generate(cfg.add_child("header"));
	}
	children_[1].generate(cfg.add_child("list_definition"));

	// now not support footer
/*
	if (!children_[2].is_all_spacer()) {
		children_[2].generate(cfg.add_child("footer"));
	}
*/
}

void unit::generate_toggle_panel(config& cfg) const
{
	// [toggle_panel]
	//		[..cfg..]
	// [/toggle_panel]

	config& grid_cfg = cfg.add_child("grid");
	const tchild& child = children_[0];
	child.generate(grid_cfg);
}

void unit::generate_scroll_panel(config& cfg) const
{
	// [scroll_panel]
	//		[..cfg..]
	// [/scroll_panel]

	config& grid_cfg = cfg.add_child("grid");
	const tchild& child = children_[0];
	child.generate(grid_cfg);
}

void unit::generate_tree_view(config& cfg) const
{
	// [tree_view]
	//		[..cfg..]
	// [/tree_view]
	cfg["indention_step_size"] = cell_.widget.tree_view.indention_step_size;
	if (cell_.widget.tree_view.foldable) {
		cfg["foldable"] = cell_.widget.tree_view.foldable;
	}
	config& node_cfg = cfg.add_child("node");
	node_cfg["id"] = cell_.widget.tree_view.node_id;

	config& node_definition_cfg = node_cfg.add_child("node_definition");
	node_definition_cfg["definition"] = cell_.widget.tree_view.node_definition;

	config& node_grid = node_definition_cfg.add_child("grid");
	const tchild& child = children_[0];
	child.generate(node_grid);
}

void unit::generate_report(config& cfg) const
{
	// [report]
	//		[..cfg..]
	// [/report]
	if (cell_.widget.report.multi_line) {
		cfg["multi_line"] = true;
		if (cell_.widget.report.fixed_cols) {
			VALIDATE(cell_.widget.report.fixed_cols > 0, null_str);
			cfg["fixed_cols"] = cell_.widget.report.fixed_cols;
		}
	} else if (cell_.widget.report.segment_switch) {
		cfg["segment_switch"] = true;
	}
	if (cell_.widget.report.toggle) {
		cfg["toggle"] = true;
	}
	if (cell_.widget.report.multi_select) {
		VALIDATE(cell_.widget.report.toggle, null_str);
		cfg["multi_select"] = true;
	}
	// VALIDATE(!cell_.widget.report.definition.empty(), null_str);
	cfg["unit_definition"] = cell_.widget.report.definition;
	if (!cell_.widget.report.unit_width.empty()) {
		cfg["unit_width"] = formual_fill_str(cell_.widget.report.unit_width);
	}
	if (!cell_.widget.report.unit_height.empty()) {
		cfg["unit_height"] = formual_fill_str(cell_.widget.report.unit_height);
	}
	if (cell_.widget.report.gap) {
		cfg["gap"] = cell_.widget.report.gap;
	}
}

void unit::generate_slider(config& cfg) const
{
	// [slider]
	//		[..cfg..]
	// [/slider]
	if (cell_.widget.slider.minimum_value) {
		cfg["minimum_value"] = cell_.widget.slider.minimum_value;
	}
	if (cell_.widget.slider.maximum_value) {
		cfg["maximum_value"] = cell_.widget.slider.maximum_value;
	}
	if (cell_.widget.slider.step_size) {
		cfg["step_size"] = cell_.widget.slider.step_size;
	}
}

void unit::generate_text_box(config& cfg) const
{
	// [text_box]
	//		[..cfg..]
	// [/text_box]
	if (cell_.widget.text_box.desensitize) {
		cfg["desensitize"] = cell_.widget.text_box.desensitize;
	}
}

void unit::generate_label(config& cfg) const
{
	// [label]
	//		[..cfg..]
	// [/label]
	if (cell_.widget.label_widget.state != gui2::tlabel::ENABLED) {
		cfg["state"] = cell_.widget.label_widget.state;
	}
}

void unit::tchild::generate(config& cfg) const
{
	config unit_cfg;
	config* row_cfg = NULL;
	int current_y = -1;
	if (!window->cell().id.empty() && window->cell().id != gui2::untitled) {
		cfg["id"] = window->cell().id;
	}
	if (window->cell().grid.grow_factor) {
		cfg["grow_factor"] = window->cell().grid.grow_factor;
	}
	for (int y = 0; y < (int)rows.size(); y ++) {
		int pitch = y * (int)cols.size();
		for (int x = 0; x < (int)cols.size(); x ++) {
			unit* u = units[pitch + x];
			if (u->get_location().y != current_y) {
				row_cfg = &cfg.add_child("row");
				current_y = u->get_location().y;

				unit* row = rows[y];
				row->to_cfg(*row_cfg);
			}
			unit_cfg.clear();
			u->to_cfg(unit_cfg);
			if (y == 0) {
				unit* column = cols[x];
				column->to_cfg(unit_cfg);
			}
			row_cfg->add_child("column", unit_cfg);
		}
		// fixed. one grid must exist one [row].
		if (!cfg.child_count("row")) {
			cfg.add_child("row");
		}
	}
}

void unit::from_cfg(const config& cfg)
{
	if (type_ == WIDGET) {
		from_widget(cfg, false);
	} else if (type_ == WINDOW) {
		from_window(cfg);
	} else if (type_ == ROW) {
		from_row(cfg);
	} else if (type_ == COLUMN) {
		from_column(cfg);
	}
}

void unit::from_window(const config& cfg)
{
	// [window] <= cfg
	// [/window]

	set_location(map_location(0, 0));
	cell_.id = cfg["id"].str();
	cell_.window.app = cfg["app"].str();
	
	const config& res_cfg = cfg.child("resolution");

	cell_.window.definition = res_cfg["definition"].str();
	cell_.window.orientation = gui2::implementation::get_orientation(res_cfg["orientation"]);
	cell_.window.scene = res_cfg["scene"].to_bool();
	if (cell_.window.scene) {
		const config& theme_cfg = res_cfg.child("scene");

		// field: tile shape
		cell_.window.tile_shape = theme_cfg["tile_shape"].str();
		std::set<std::string> shapes;
		const config& core_config = controller_.app_cfg();
		BOOST_FOREACH (const config &c, core_config.child_range("tb")) {
			const std::string& id = c["id"].str();
			shapes.insert(id);
		}
		if (shapes.find(cell_.window.tile_shape) == shapes.end()) {
			cell_.window.tile_shape = game_config::tile_square;
		}

		cell_.window.click_dismiss = false;
		cell_.window.automatic_placement = false;

	} else {
		cell_.window.click_dismiss = res_cfg["click_dismiss"].to_bool();
		cell_.window.automatic_placement = res_cfg["automatic_placement"].to_bool(true);
	}

	if (cell_.window.automatic_placement) {
		cell_.window.horizontal_placement = gui2::implementation::get_h_align(res_cfg["horizontal_placement"]);
		cell_.window.vertical_placement = gui2::implementation::get_v_align(res_cfg["vertical_placement"]);
	} else {
		cell_.window.x = formual_extract_str(res_cfg["x"].str());
		cell_.window.y = formual_extract_str(res_cfg["y"].str());
		cell_.window.width = formual_extract_str(res_cfg["width"].str());
		cell_.window.height = formual_extract_str(res_cfg["height"].str());
	}
}

void unit::from_row(const config& cfg)
{
	cell_.row.grow_factor = cfg["grow_factor"].to_int();
}

void unit::from_column(const config& cfg)
{
	cell_.column.grow_factor = cfg["grow_factor"].to_int();
}

void unit::from_widget(const config& cfg, bool unpack)
{
	if (units_.consistent()) {
		cell_.widget.cell.flags_ = gui2::implementation::read_flags(cfg);
		cell_.widget.cell.border_size_ = cfg["border_size"].to_int();
	}

	const config& sub_cfg = units_.consistent()? cfg.child(widget_.first): cfg;
	cell_.id = sub_cfg["id"].str();

	if (has_size()) {
		cell_.widget.width = formual_extract_str(sub_cfg["width"].str());
		cell_.widget.height = formual_extract_str(sub_cfg["height"].str());
		cell_.widget.min_text_width = formual_extract_str(sub_cfg["min_text_width"].str());
	}
	if (!unpack && gui2::gui.tpl_widgets.find(widget_.first) != gui2::gui.tpl_widgets.end()) {
		if (from_cfg_widget_tpl()) {
			return;
		}
	}

	cell_.widget.linked_group = sub_cfg["linked_group"].str();
	if (has_mtwusc()) {
		cell_.widget.mtwusc = sub_cfg["mtwusc"].to_bool();
	}
	if (widget_.second && widget_.second->label_is_text) {
		cell_.widget.text_font_size = sub_cfg["text_font_size"].to_int();
		if (!cell_.widget.text_font_size) {
			cell_.widget.text_font_size = font_cfg_reference_size;
		}
		cell_.widget.text_color_tpl = sub_cfg["text_color_tpl"].to_int();
	}
	if (has_size_is_max()) {
		cell_.widget.width_is_max = sub_cfg["width_is_max"].to_bool();
		cell_.widget.height_is_max = sub_cfg["height_is_max"].to_bool();
	}
	if (has_atom_markup()) {
		cell_.widget.atom_markup = sub_cfg["atom_markup"].to_bool();
	}
	if (has_margin()) {
		cell_.widget.margin.left = sub_cfg["left_margin"].to_int(nposm);
		cell_.widget.margin.right = sub_cfg["right_margin"].to_int(nposm);
		cell_.widget.margin.top = sub_cfg["top_margin"].to_int(nposm);
		cell_.widget.margin.bottom = sub_cfg["bottom_margin"].to_int(nposm);
	}
	split_t_string(sub_cfg["label"].t_str(), cell_.widget.label_textdomain, cell_.widget.label);
	split_t_string(sub_cfg["tooltip"].t_str(), cell_.widget.tooltip_textdomain, cell_.widget.tooltip);

	if (is_scroll()) {
		cell_.widget.vertical_mode = gui2::implementation::get_scrollbar_mode(sub_cfg["vertical_scrollbar_mode"]);
		cell_.widget.horizontal_mode = gui2::implementation::get_scrollbar_mode(sub_cfg["horizontal_scrollbar_mode"]);
	}
	if (is_grid()) {
		from_grid(sub_cfg);

	} else if (is_stack()) {
		from_stack(sub_cfg);

	} else if (is_listbox()) {
		from_listbox(sub_cfg);

	} else if (is_panel()) {
		from_toggle_panel(sub_cfg);

	} else if (is_scroll_panel()) {
		from_scroll_panel(sub_cfg);

	} else if (is_tree()) {
		from_tree_view(sub_cfg);

	} else if (is_report()) {
		from_report(sub_cfg);

	} else if (is_slider()) {
		from_slider(sub_cfg);

	} else if (is_text_box()) {
		from_text_box(sub_cfg);

	} else if (is_label()) {
		from_label(sub_cfg);

	}
}

void unit::tchild::from(mkwin_controller& controller, mkwin_display& disp, unit_map& units2, unit* parent, int number, const config& cfg)
{
	if (parent || !controller.preview()) {
		unit_map::tconsistent_lock lock(units2);

		window = new unit(controller, disp, units2, unit::WINDOW, parent, number);
		if (!cfg["id"].empty()) {
			window->cell().id = cfg["id"].str();
		}
		if (cfg["grow_factor"].to_int()) {
			window->cell().grid.grow_factor = cfg["grow_factor"].to_int();
		}
		// generate require x, y. for example, according it to next line.
		window->set_location(map_location(0, 0));

		bool first_row = true;
		std::string type, definition;
		BOOST_FOREACH (const config& row, cfg.child_range("row")) {
			rows.push_back(new unit(controller, disp, units2, unit::ROW, parent, number));
			rows.back()->set_location(map_location(0, rows.size()));
			rows.back()->from_cfg(row);
			
			BOOST_FOREACH (const config& col, row.child_range("column")) {
				if (first_row) {
					cols.push_back(new unit(controller, disp, units2, unit::COLUMN, parent, number));
					cols.back()->set_location(map_location(cols.size(), 0));
					cols.back()->from_cfg(col);
				}
				int colno = 1;
				BOOST_FOREACH (const config::any_child& c, col.all_children_range()) {
					type = c.key;
					definition = c.cfg["definition"].str();
					gui2::tcontrol_definition_ptr ptr;
					if (type != "grid" && gui2::gui.tpl_widgets.find(type) == gui2::gui.tpl_widgets.end()) {
						ptr = mkwin_display::find_widget(type, definition, c.cfg["id"].str());
					}
					units.push_back(new unit(controller, disp, units2, std::make_pair(type, ptr), parent, number));
					units.back()->set_location(map_location(colno ++, rows.size()));
					units.back()->from_cfg(col);
				}
			}
			first_row = false;
		}
	} else {
		VALIDATE(false, null_str);
	}
}

void unit::tchild::draw_minimap_architecture(surface& screen, const SDL_Rect& minimap_location, const double xscaling, const double yscaling, int level) const
{
	static std::vector<SDL_Color> candidates;
	if (candidates.empty()) {
		candidates.push_back(font::TITLE_COLOR);
		candidates.push_back(font::BAD_COLOR);
		candidates.push_back(font::GRAY_COLOR);
		candidates.push_back(font::BLACK_COLOR);
		candidates.push_back(font::GOOD_COLOR);
	}
	SDL_Color col = candidates[level % candidates.size()];
	const Uint32 box_color = SDL_MapRGB(screen->format, col.r, col.g, col.b);

	double u_x = window->get_location().x * xscaling;
	double u_y = window->get_location().y * yscaling;
	double u_w = window->cell().window.cover_width * xscaling;
	double u_h = window->cell().window.cover_height * yscaling;

	window->cell().window.color = box_color;
	if (level) {
		draw_rectangle(minimap_location.x + round_double(u_x)
			, minimap_location.y + round_double(u_y)
			, round_double(u_w)
			, round_double(u_h)
			, box_color, screen);
	}

	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		unit* u = *it;
		if (u->cell().id == "_main_map" || u->cell().id == "_mini_map") {
			double u_x = u->get_location().x * xscaling;
			double u_y = u->get_location().y * yscaling;
			double u_w = xscaling;
			double u_h = yscaling;

			SDL_Rect r = create_rect(minimap_location.x + round_double(u_x)
					, minimap_location.y + round_double(u_y)
					, round_double(u_w)
					, round_double(u_h));

			sdl_fill_rect(screen, &r, 0xff0000ff);
		}
		const std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::const_iterator it2 = children.begin(); it2 != children.end(); ++ it2) {
			const unit::tchild& sub = *it2;
			sub.draw_minimap_architecture(screen, minimap_location, xscaling, yscaling, level + 1);
		}
	}
}

void unit::tchild::erase(unit_map& units2)
{
	for (std::vector<unit*>::iterator it = units.begin(); it != units.end(); ) {
		unit* u = *it;
		units2.erase(u->get_location());
		it = units.erase(it);
	}
	for (std::vector<unit*>::iterator it = rows.begin(); it != rows.end(); ) {
		unit* u = *it;
		units2.erase(u->get_location());
		it = rows.erase(it);
	}
	for (std::vector<unit*>::iterator it = cols.begin(); it != cols.end(); ) {
		unit* u = *it;
		units2.erase(u->get_location());
		it = cols.erase(it);
	}
	units2.erase(window->get_location());
	window = NULL;
}

unit* unit::tchild::find_unit(const std::string& id) const
{
	unit* u = NULL;
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		u = *it;
		if (u->cell().id == id) {
			return u;
		}
		const std::vector<tchild>& children = u->children();
		for (std::vector<tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			const tchild& child = *it;
			u = child.find_unit(id);
			if (u) {
				return u;
			}
		}
	}
	return NULL;
}

std::pair<unit*, int> unit::tchild::find_layer(const std::string& id) const
{
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		unit* u = *it;
		
		const std::vector<tchild>& children = u->children();
		for (std::vector<tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
			const tchild& child = *it;
			if (u->is_stack() && child.window->cell().id == id) {
				return std::make_pair(u, std::distance(children.begin(), it));
			}
			std::pair<unit*, int> ret = child.find_layer(id);
			if (ret.first) {
				return ret;
			}
		}
	}
	return std::make_pair((unit*)NULL, nposm);
}

tpoint unit::tchild::locate_u(const unit& hit) const
{
	int at = 0;
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it, at ++) {
		const unit* u = *it;
		
		if (u == &hit) {
			return tpoint(at % cols.size(), at / cols.size());
		}
	}

	// hit must be in first units.
	VALIDATE(false, null_str);
	return tpoint(0, 0);
}

tpoint unit::tchild::calculate_best_size(const game_logic::map_formula_callable& variables) const
{
	const unsigned cols_ = cols.size();
	const unsigned rows_ = rows.size();

	std::vector<unsigned> row_height_;
	std::vector<unsigned> col_width_;

	for (std::vector<unit*>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
		const unit& u = **it;

		const int pitch = row_height_.size() * cols_;
		int max_val = 0;
		for (unsigned n = 0; n < cols_; n ++) {
			const unit& u = *units[pitch + n];
			if (u.has_size() && !u.cell().widget.height.empty()) {
				gui2::tformula<int> height("(" + u.cell().widget.height + ")");
				int val = height(variables);
				max_val = max_val > val? max_val: val;
			} else if (u.is_extensible()) {
				const std::vector<unit::tchild>& children = u.children();
				VALIDATE(children.size() == 1, null_str);
				tpoint val = children[0].calculate_best_size(variables);
				max_val = max_val > val.y? max_val: val.y;
			}
		}
		if (!max_val) {
			max_val = unit::preview_widget_best_height;
		}
		row_height_.push_back(max_val);
	}
	for (std::vector<unit*>::const_iterator it = cols.begin(); it != cols.end(); ++ it) {
		const unit& u = **it;

		int max_val = 0;
		for (unsigned n = 0; n < rows_; n ++) {
			const unit& u = *units[n * cols_ + col_width_.size()];
			if (u.has_size() && !u.cell().widget.width.empty()) {
				gui2::tformula<int> width("(" + u.cell().widget.width + ")");
				int val = width(variables);
				max_val = max_val > val? max_val: val;
			} else if (u.is_extensible()) {
				const std::vector<unit::tchild>& children = u.children();
				VALIDATE(children.size() == 1, null_str);
				tpoint val = children[0].calculate_best_size(variables);
				max_val = max_val > val.x? max_val: val.x;
			}
		}
		if (!max_val) {
			max_val = unit::preview_widget_best_width;
		}
		col_width_.push_back(max_val);
	}

	return tpoint(std::accumulate(col_width_.begin(), col_width_.end(), 0), std::accumulate(row_height_.begin(), row_height_.end(), 0));
}

void unit::tchild::place(mkwin_controller& controller, mkwin_display& disp, unit_map& units2, const game_logic::map_formula_callable& variables, const tpoint& origin2, const tpoint& size, std::vector<unit*>& to_units) const
{
	map_location window_loc = window->get_location();
	const unsigned cols_ = cols.size();
	const unsigned rows_ = rows.size();
	SDL_Rect rect;

	std::vector<unsigned> row_height_;
	std::vector<unsigned> col_width_;
	std::vector<unsigned> row_grow_factor_;
	std::vector<unsigned> col_grow_factor_;

	for (std::vector<unit*>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
		const unit& u = **it;
		row_grow_factor_.push_back(u.cell().row.grow_factor);

		const int pitch = row_height_.size() * cols_;
		int max_val = 0;
		for (unsigned n = 0; n < cols_; n ++) {
			const unit& u = *units[pitch + n];
			if (u.has_size() && !u.cell().widget.height.empty()) {
				gui2::tformula<int> height("(" + u.cell().widget.height + ")");
				int val = height(variables);
				max_val = max_val > val? max_val: val;
			} else if (u.is_extensible()) {
				const std::vector<unit::tchild>& children = u.children();
				VALIDATE(children.size() == 1, null_str);
				tpoint val = children[0].calculate_best_size(variables);
				max_val = max_val > val.y? max_val: val.y;
			}
		}
		if (!max_val) {
			max_val = unit::preview_widget_best_height;
		}
		row_height_.push_back(max_val);
	}
	for (std::vector<unit*>::const_iterator it = cols.begin(); it != cols.end(); ++ it) {
		const unit& u = **it;
		col_grow_factor_.push_back(u.cell().column.grow_factor);

		int max_val = 0;
		for (unsigned n = 0; n < rows_; n ++) {
			const unit& u = *units[n * cols_ + col_width_.size()];
			if (u.has_size() && !u.cell().widget.width.empty()) {
				gui2::tformula<int> width("(" + u.cell().widget.width + ")");
				int val = width(variables);
				max_val = max_val > val? max_val: val;
			} else if (u.is_extensible()) {
				const std::vector<unit::tchild>& children = u.children();
				VALIDATE(children.size() == 1, null_str);
				tpoint val = children[0].calculate_best_size(variables);
				max_val = max_val > val.x? max_val: val.x;
			}
		}
		if (!max_val) {
			max_val = unit::preview_widget_best_width;
		}
		col_width_.push_back(max_val);
	}
	const tpoint best_size(std::accumulate(col_width_.begin(), col_width_.end(), 0), std::accumulate(row_height_.begin(), row_height_.end(), 0));
	VALIDATE(size.x >= best_size.x && size.y >= best_size.y, null_str);

		{
			// expand it.
			if (size.x > best_size.x) {
				const unsigned w = size.x - best_size.x;
				unsigned w_size = std::accumulate(
						col_grow_factor_.begin(), col_grow_factor_.end(), 0);

				if (w_size == 0) {
					// If all sizes are 0 reset them to 1
					BOOST_FOREACH(unsigned& val, col_grow_factor_) {
						val = 1;
					}
					w_size = cols_;
				}
				// We might have a bit 'extra' if the division doesn't fix exactly
				// but we ignore that part for now.
				const unsigned w_normal = w / w_size;
				for(unsigned i = 0; i < cols_; ++i) {
					col_width_[i] += w_normal * col_grow_factor_[i];
				}
			}
		}
		if (size.y > best_size.y) {
			const unsigned h = size.y - best_size.y;
			unsigned h_size = std::accumulate(
					row_grow_factor_.begin(), row_grow_factor_.end(), 0);

			if (h_size == 0) {
				// If all sizes are 0 reset them to 1
				BOOST_FOREACH(unsigned& val, row_grow_factor_) {
					val = 1;
				}
				h_size = rows_;
			}
			// We might have a bit 'extra' if the division doesn't fix exactly
			// but we ignore that part for now.
			const unsigned h_normal = h / h_size;
			for(unsigned i = 0; i < rows_; ++i) {
				row_height_[i] += h_normal * row_grow_factor_[i];
			}
		}

	tpoint origin = origin2;
	unsigned xinc = 0, yinc = 0;
	for (std::vector<unit*>::const_iterator it = units.begin(); it != units.end(); ++ it) {
		const unit& u = **it;
		const map_location& loc = u.get_location();
	
		
		rect.x = origin.x;
		rect.y = origin.y;
		rect.w = col_width_[xinc];
		rect.h = row_height_[yinc];

		if (!u.is_extensible()) {
			unit2* u2 = new unit2(controller, disp, units2, u.widget(), nullptr, rect);
			to_units.push_back(u2);
		} else {
			const std::vector<unit::tchild>& children = u.children();
			VALIDATE(children.size() == 1, null_str);
			children[0].place(controller, disp, units2, variables, origin, tpoint(rect.w, rect.h), to_units);
		}

		xinc ++;
		if (xinc != cols_) {
			origin.x += rect.w;
		} else {
			xinc = 0;
			yinc ++;
			origin.x = origin2.x;
			origin.y += rect.h; 
		}
	}
}

bool unit::from_cfg_widget_tpl()
{
	std::string tpl_id = widget_.first;
	const gui2::ttpl_widget& tpl = gui2::get_tpl_widget(tpl_id);

	controller_.insert_used_widget_tpl(tpl);
	return true;
}

void unit::from_grid(const config& cfg)
{
	children_.push_back(tchild());
	unit::tchild& child = children_.back();
	child.from(controller_, disp_, units_, this, children_.size() - 1, cfg);
}

void unit::from_stack(const config& cfg)
{
	cell_.widget.stack.mode = gui2::implementation::get_stack_mode(cfg["mode"].str());
	const config& s = cfg.has_child("stack")? cfg.child("stack"): cfg;
	BOOST_FOREACH (const config& layer, s.child_range("layer")) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, units_, this, children_.size() - 1, layer);
	}
}

void unit::from_listbox(const config& cfg)
{
	BOOST_FOREACH (const config& linked, cfg.child_range("linked_group")) {
		cell_.widget.listbox.linked_groups.push_back(gui2::tlinked_group(linked));
	}
	const config& header = cfg.child("header");
	if (header) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, units_, this, children_.size() - 1, header);
	} else {
		insert_child(mkwin_controller::default_child_width, mkwin_controller::default_child_height);
	}

	children_.push_back(tchild());
	children_.back().from(controller_, disp_, units_, this, children_.size() - 1, cfg.child("list_definition"));

	// now not support footer
/*
	const config& footer = cfg.child("footer");
	if (footer) {
		children_.push_back(tchild());
		children_.back().from(controller_, disp_, units_, this, children_.size() - 1, footer);
	} else {
		insert_child(mkwin_controller::default_child_width, mkwin_controller::default_child_height);
	}
*/
}

void unit::from_toggle_panel(const config& cfg)
{
	children_.push_back(tchild());
	children_.back().from(controller_, disp_, units_, this, children_.size() - 1, cfg.child("grid"));
}

void unit::from_scroll_panel(const config& cfg)
{
	children_.push_back(tchild());
	children_.back().from(controller_, disp_, units_, this, children_.size() - 1, cfg.child("grid"));
}

void unit::from_tree_view(const config& cfg)
{
	cell_.widget.tree_view.foldable = cfg["foldable"].to_bool();
	cell_.widget.tree_view.indention_step_size = cfg["indention_step_size"].to_int();
	const config& node_cfg = cfg.child("node");
	if (!node_cfg) {
		return;
	}
	cell_.widget.tree_view.node_id = node_cfg["id"].str();
	const config& node_definition_cfg = node_cfg.child("node_definition");
	if (!node_definition_cfg) {
		return;
	}
	cell_.widget.tree_view.node_definition = node_definition_cfg["definition"].str();

	children_.push_back(tchild());
	children_.back().from(controller_, disp_, units_, this, children_.size() - 1, node_definition_cfg.child("grid"));
}

void unit::from_report(const config& cfg)
{
	cell_.widget.report.multi_line = cfg["multi_line"].to_bool();
	cell_.widget.report.toggle = cfg["toggle"].to_bool();
	cell_.widget.report.definition = cfg["unit_definition"].str();
	cell_.widget.report.unit_width = formual_extract_str(cfg["unit_width"].str());
	cell_.widget.report.unit_height = formual_extract_str(cfg["unit_height"].str());
	cell_.widget.report.gap = cfg["gap"].to_int();
	cell_.widget.report.multi_select = cell_.widget.report.toggle? cfg["multi_select"].to_bool(): false;
	cell_.widget.report.segment_switch = cfg["segment_switch"].to_bool();
	cell_.widget.report.fixed_cols = cfg["fixed_cols"].to_int();
}

void unit::from_slider(const config& cfg)
{
	cell_.widget.slider.minimum_value = cfg["minimum_value"].to_int();
	cell_.widget.slider.maximum_value = cfg["maximum_value"].to_int();
	cell_.widget.slider.step_size = cfg["step_size"].to_int();
}

void unit::from_text_box(const config& cfg)
{
	cell_.widget.text_box.desensitize = cfg["desensitize"].to_bool();
}

void unit::from_label(const config& cfg)
{
	cell_.widget.label_widget.state = cfg["state"].to_int(gui2::tlabel::ENABLED);
}

std::string unit::widget_tag() const
{
	std::stringstream ss;
	ss << "#" << get_map_index();

	return ss.str();
}