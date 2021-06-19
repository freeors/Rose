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

#ifndef STUDIO_MKWIN_DISPLAY_HPP_INCLUDED
#define STUDIO_MKWIN_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "gui/auxiliary/widget_definition.hpp"

class mkwin_controller;
class unit_map;
class base_unit;
class unit;

namespace gui2 {
class treport;
class ttoggle_button;
}

class mkwin_display : public display
{
public:
	static gui2::tcontrol_definition_ptr find_widget(const std::string& type, const std::string& definition, const std::string& id);

	mkwin_display(mkwin_controller& controller, unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~mkwin_display();

	void pre_change_resolution(std::map<const std::string, bool>& actives);
	void post_change_resolution(const std::map<const std::string, bool>& actives);

	bool in_theme() const { return true; }
	void click_widget(const std::string& type, const std::string& definition);

	const std::pair<std::string, gui2::tcontrol_definition_ptr>& selected_widget() const { return selected_widget_; }

	void resume_mouseover_hex_overlay() { set_mouseover_hex_overlay(using_mouseover_hex_overlay_); }

	gui2::ttoggle_button* scroll_header_widget(int index) const;

	std::pair<std::string, gui2::tcontrol_definition_ptr> spacer;
	std::pair<std::string, gui2::tcontrol_definition_ptr> image;
	std::pair<std::string, gui2::tcontrol_definition_ptr> toggle_button;
	std::pair<std::string, gui2::tcontrol_definition_ptr> toggle_panel;

protected:
	/**
	* The editor uses different rules for terrain highlighting (e.g. selections)
	*/
	// image::TYPE get_image_type(const map_location& loc);

	void draw_hex(const map_location& loc) override;

	// const SDL_Rect& get_clip_rect();
	void draw_sidebar() override;
	void app_post_set_zoom(int last_zoom);
	
private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;
	void app_draw_minimap_units(surface& screen) override;

	void reload_widget_palette();
	void reload_scroll_header();
	void scroll_top(gui2::treport& widget);
	void scroll_bottom(gui2::treport& widget);

private:
	mkwin_controller& controller_;
	unit_map& units_;
	gui2::treport* widget_palette_;
	gui2::treport* scroll_header_;

	std::string current_widget_type_;
	std::pair<std::string, gui2::tcontrol_definition_ptr> selected_widget_;
	surface using_mouseover_hex_overlay_;
};

#endif
