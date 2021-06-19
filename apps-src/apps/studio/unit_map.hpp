/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef STUDIO_UNIT_MAP_HPP_INCLUDED
#define STUDIO_UNIT_MAP_HPP_INCLUDED

#include "unit2.hpp"
#include "base_map.hpp"

class mkwin_controller;

class unit_map: public base_map
{
public:
	unit_map(mkwin_controller& controller, const tmap& gmap, bool consistent);
	~unit_map();

	unit* find_unit(const map_location& loc) const { return dynamic_cast<unit*>(find_base_unit(loc)); }
	unit* find_unit(const map_location& loc, bool overlay) const { return dynamic_cast<unit*>(find_base_unit(loc, overlay)); }
	unit* find_unit(int i) { return dynamic_cast<unit*>(map_[i]); }

	void create_coor_map(int w, int h);
	void clear();
	void add(const map_location& loc, const base_unit* base_u);
	bool erase(const map_location& loc, bool overlay = true);

	void zero_map();
	void save_map_to(unit::tchild& child, bool clear);
	std::vector<unit*> form_units() const;

	void restore_map_from(const unit::tchild& child, const int xstart, const int ystart, bool create);
	tpoint restore_map_from(const std::vector<unit::tchild>& children, bool create);
	void recalculate_size(const unit::tchild& child);
	void layout(const unit::tchild& child);

	bool line_is_spacer(bool row, int index) const;

private:
	mkwin_controller& controller_;
};

#endif
