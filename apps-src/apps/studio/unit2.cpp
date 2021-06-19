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

#include "unit2.hpp"
#include "gettext.hpp"
#include "mkwin_display.hpp"
#include "mkwin_controller.hpp"
#include "gui/dialogs/mkwin_scene.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "game_config.hpp"

using namespace std::placeholders;

unit2::unit2(mkwin_controller& controller, mkwin_display& disp, unit_map& units, const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget, unit* parent, const SDL_Rect& rect)
	: unit(controller, disp, units, widget, parent, -1)
{
	rect_ = rect;
}

unit2::unit2(mkwin_controller& controller, mkwin_display& disp, unit_map& units, int type, unit* parent, const SDL_Rect& rect)
	: unit(controller, disp, units, type, parent, -1)
{
	set_rect(rect);
}

unit2::unit2(const unit2& that)
	: unit(that)
{
}

void unit2::app_draw_unit(const int xsrc, const int ysrc)
{
	const int zoom = disp_.zoom();

	int map_index_xoffset = 0;
	int map_index_yoffset = 12;
	if (type_ == WIDGET) {
		redraw_widget(xsrc, ysrc, preview_widget_best_width, preview_widget_best_height);
		map_index_xoffset = 2 * gui2::twidget::hdpi_scale;
		map_index_yoffset = preview_widget_best_height / 2;

	} else if (type_ == WINDOW) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, 
			loc_, image(), image::UNSCALED, xsrc, ysrc, zoom / 2, zoom / 2);

		if (!cell_.id.empty()) {
			surface text_surf = font::get_rendered_text(cell_.id, 0, 10 * gui2::twidget::hdpi_scale, font::BLACK_COLOR);
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, text_surf, xsrc, ysrc + rect_.w / 2, 0, 0);
		}
		
	} else if (type_ == COLUMN) {
		int x = loc_.x * zoom;
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, generate_integer_surface(x), xsrc, ysrc, 0, 0);

	} else if (type_ == ROW) {
		int y = loc_.y * zoom;
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, generate_integer_surface(y), xsrc, ysrc, 0, 0);
	}

	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, generate_integer_surface(map_index_), xsrc + map_index_xoffset, ysrc + map_index_yoffset, 0, 0);

	if (type_ == WINDOW || type_ == WIDGET) {
		//
		// draw rectangle
		//
		Uint32 border_color = 0xff00ff00;
		if (loc_ == controller_.selected_hex()) {
			border_color = 0xff0000ff;
		} else if (this == controller_.mouseover_unit()) {
			border_color = 0xffff0000;
		}

		// draw the border
		for (unsigned i = 0; i < 2; i ++) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_FRAME, 
				xsrc + i, ysrc + i, rect_.w - (i * 2), rect_.h - (i * 2), border_color);
		}
	}
}

bool unit2::sort_compare(const base_unit& that_base) const
{
	const unit2* that = dynamic_cast<const unit2*>(&that_base);
	if (type_ != WIDGET) {
		if (that->type() == WIDGET) {
			return true;
		}
		return unit::sort_compare(that_base);
	}
	if (that->type() != WIDGET) {
		return false;
	}
	if (rect_.x != that->rect_.x || rect_.y != that->rect_.y) {
		return rect_.y < that->rect_.y || (rect_.y == that->rect_.y && rect_.x < that->rect_.x);
	}
	return unit::sort_compare(that_base);
}
