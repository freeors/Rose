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

#include "unit_map.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "game_config.hpp"

unit_map::unit_map(mkwin_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
	, controller_(controller)
{
}

unit_map::~unit_map()
{
	// destruct cannot call virtual function. but clear is.
	clear();
}

void unit_map::create_coor_map(int w, int h)
{
	size_t orignal_map_vsize = map_vsize_;
	base_unit** orignal_map = NULL;
	if (orignal_map_vsize) {
		VALIDATE(w * h >= (int)orignal_map_vsize, "desire map size must not less than orignal nodes!"); 
		orignal_map = (base_unit**)malloc(map_vsize_ * sizeof(base_unit*));
		memcpy(orignal_map, map_, map_vsize_ * sizeof(base_unit*));
	}
	base_map::clear();
	base_map::create_coor_map(w, h);
	if (orignal_map_vsize) {
		map_vsize_ = orignal_map_vsize;
		memcpy(map_, orignal_map, map_vsize_ * sizeof(base_unit*));

		for (int i = 0; i < map_vsize_; i ++) {
			base_unit* n = map_[i];
			const map_location& loc = n->get_location();
			int pitch = loc.y * w;
			if (n->base()) {
				coor_map_[pitch + loc.x].base = n;
			} else {
				coor_map_[pitch + loc.x].overlay = n;
			}
		}
	}
}

void unit_map::clear()
{
	for (int i = 1; i < map_vsize_; i ++) {
		unit* u = dynamic_cast<unit*>(map_[i]);
		if (u->type() == unit::WINDOW) {
			map_vsize_ = i;
			break;
		}
	}
	base_map::clear();
}

void unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const unit* u = dynamic_cast<const unit*>(base_u);
	insert(loc, new unit(*u));
}

bool unit_map::erase(const map_location& loc, bool overlay)
{
	VALIDATE(overlay, null_str);

	unit* u = find_unit(loc, overlay);
	if (!u) {
		return false;
	}
	if (u->consistent()) {
		std::vector<unit::tchild>& children = u->children();
		for (std::vector<unit::tchild>::iterator it = children.begin(); it != children.end();) {
			unit::tchild& child = *it;
			child.erase(*this);
			it = children.erase(it);
		}
	}
	return erase2(u, true);
}

void unit_map::save_map_to(unit::tchild& child, bool clear)
{
	child.clear(false);

	child.window = dynamic_cast<unit*>(map_[0]);
	if (consistent_) {
		for (int x = 1; x < w_; x ++) {
			child.cols.push_back(dynamic_cast<unit*>(map_[x]));
		}
		for (int y = 1; y < h_; y ++) {
			int pitch = y * w_;
			for (int x = 0; x < w_; x ++) {
				if (x) {
					child.units.push_back(dynamic_cast<unit*>(map_[pitch + x]));
				} else {
					child.rows.push_back(dynamic_cast<unit*>(map_[pitch + x]));
				}
			}
		}
	} else {
		for (int i = 1; i < map_vsize_; i ++) {
			unit* u = dynamic_cast<unit*>(map_[i]);
			if (u->type() == unit::ROW) {
				child.rows.push_back(dynamic_cast<unit*>(map_[i]));
			} else if (u->type() == unit::COLUMN) {
				child.cols.push_back(dynamic_cast<unit*>(map_[i]));
			} else {
				child.units.push_back(dynamic_cast<unit*>(map_[i]));
			}
		}
	}

	if (clear) {
		// clear, except unit.
		if (coor_map_) {
			free(coor_map_);
			coor_map_ = NULL;
		}
		free(map_);
		map_ = NULL;
		map_vsize_ = 0;
	}
}

std::vector<unit*> unit_map::form_units() const
{
	std::vector<unit*> result;

	if (consistent_) {
		for (int y = 1; y < h_; y ++) {
			int pitch = y * w_;
			for (int x = 0; x < w_; x ++) {
				if (x) {
					result.push_back(dynamic_cast<unit*>(map_[pitch + x]));
				}
			}
		}
	} else {
		for (int i = 1; i < map_vsize_; i ++) {
			unit* u = dynamic_cast<unit*>(map_[i]);
			if (u->type() == unit::WIDGET) {
				result.push_back(dynamic_cast<unit*>(map_[i]));
			}
		}
	}

	return result;
}

void unit_map::restore_map_from(const unit::tchild& child, const int xstart, const int ystart, bool create)
{
	const int serial_gap = 1;
	if (create) {
		VALIDATE(xstart || ystart || !map_vsize_, "map_vsize_ must be 0.");
	}

	unit* window = child.window;
	tpoint max_increase(0, 0);

	if (consistent_) {
		int w = (int)child.cols.size();
		int h = (int)child.rows.size();
		
		if (create) {
			insert(map_location(xstart, ystart), window);
			for (int x = 0; x < w; x ++) {
				insert(map_location(xstart + x + 1, ystart), child.cols[x]);
			}
			for (int y = 0; y < h; y ++) {
				insert(map_location(xstart, ystart + y + 1), child.rows[y]);
			}
		}
		int uindex = 0;
		tpoint child_max(0, 0);
		int increase_xstart = 0;
		int increase_ystart = 1 + h + serial_gap;
		for (int y = 1; y <= h; y ++) {
			for (int x = 1; x <= w; x ++) {
				unit* u = child.units[uindex ++];
				if (create) {
					insert(map_location(xstart + x, ystart + y), u);
				}
				const std::vector<unit::tchild>& children = u->children();
				
				for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
					const unit::tchild& child = *it;
					restore_map_from(child, xstart + increase_xstart, ystart + increase_ystart, create);
					gui2::tcell_setting& cell = child.window->cell();

					increase_xstart += cell.window.cover_width + serial_gap;
					if (cell.window.cover_height + serial_gap > child_max.y) {
						child_max.y = cell.window.cover_height + serial_gap;
					}
				}
			}
		}
		max_increase.x = 1 + w + serial_gap;
		if (increase_xstart > max_increase.x) {
			max_increase.x = increase_xstart;
		}
		max_increase.y = increase_ystart + child_max.y;

		window->cell().window.cover_width = max_increase.x - serial_gap;
		window->cell().window.cover_height = max_increase.y - serial_gap;

	} else {
		display& disp = controller_.get_display();
		insert2(disp, child.window);
		for (std::vector<unit*>::const_iterator it = child.rows.begin(); it != child.rows.end(); ++ it) {
			insert2(disp, *it);
		}
		for (std::vector<unit*>::const_iterator it = child.cols.begin(); it != child.cols.end(); ++ it) {
			insert2(disp, *it);
		}
		for (std::vector<unit*>::const_iterator it = child.units.begin(); it != child.units.end(); ++ it) {
			insert2(disp, *it);
		}
	}
}

tpoint unit_map::restore_map_from(const std::vector<unit::tchild>& children, bool create)
{
	const int serial_gap = 1;

	if (create) {
		VALIDATE(!map_vsize_, "map_vsize_ must be 0.");
	}
	VALIDATE(consistent_, "Must be consistent_ == true!");
	VALIDATE(!children.empty(), "children must not be empty!");

	tpoint max_increase(0, 0);

	int uindex = 0;
	tpoint child_max(0, 0);
	int increase_xstart = 0;
	
	for (std::vector<unit::tchild>::const_iterator it = children.begin(); it != children.end(); ++ it) {
		const unit::tchild& child = *it;
		restore_map_from(child, increase_xstart, 0, create);
		gui2::tcell_setting& cell = child.window->cell();

		increase_xstart += cell.window.cover_width + serial_gap;
		if (cell.window.cover_height + serial_gap > child_max.y) {
			child_max.y = cell.window.cover_height + serial_gap;
		}
	}

	max_increase.x = increase_xstart - serial_gap;
	max_increase.y = child_max.y - serial_gap;

	return max_increase;
}

void unit_map::recalculate_size(const unit::tchild& child)
{
	restore_map_from(child, 0, 0, false);
}

void unit_map::zero_map()
{
	// attention! if map_vsize_ != 0, means has unit in base_map, you maybe call display::invalidate_all to redraw them.
	// don't delete unit during here. only set map_vsize to 0.
	// unit is saved by tmkwin_controller::top_.

	if (map_) {
		memset(map_, 0, map_size_ * sizeof(base_unit*));
		map_vsize_ = 0;
	}
	if (coor_map_) {
		memset(coor_map_, 0, w_ * h_ * sizeof(loc_cookie));
	}
}

void unit_map::layout(const unit::tchild& child)
{
	zero_map();
	VALIDATE(!controller_.preview(), null_str);

	restore_map_from(child, 0, 0, true);
}

bool unit_map::line_is_spacer(bool row, int index) const
{
	if (row) {
		int pitch = index * w_;
		for (int x = 1; x < w_; x ++) {
			const unit* u = dynamic_cast<unit*>(coor_map_[pitch + x].overlay);
			if (!u->is_spacer()) {
				return false;
			}
		}
	} else {
		for (int y = 1; y < h_; y ++) {
			int pitch = y * w_;
			const unit* u = dynamic_cast<unit*>(coor_map_[pitch + index].overlay);
			if (!u->is_spacer()) {
				return false;
			}
		}
	}
	return true;
}

