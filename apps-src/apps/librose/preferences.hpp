/* $Id: preferences.hpp 47642 2010-11-21 13:58:27Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef PREFERENCES_HPP_INCLUDED
#define PREFERENCES_HPP_INCLUDED

class config;
class display;
class hero;
class hero_map;
class CVideo;

#include "rose_config.hpp"
#include "terrain_translation.hpp"
#include "util.hpp"

#include <utility>

namespace preferences {

	extern std::string public_account;
	extern std::string public_password;
	extern uint32_t last_write_ticks;

	struct base_manager
	{
		base_manager();
		~base_manager();
	};

	void write_preferences();

	void set_str(const std::string& key, const std::string &value);
	void set_bool(const std::string& key, bool value);
	void set_int(const std::string& key, int value);
	void clear(const std::string& key);
	void set_child(const std::string& key, const config& val);
	const config& get_child(const std::string &key);
	std::string get_str(const std::string& key);
	bool get_bool(const std::string& key, bool def);
	int get_int(const std::string& key, int def);
	uint32_t get_uint32(const std::string& key, uint32_t def);
	void erase(const std::string& key);

	config* get_prefs();
	const config& get_critical_prefs();

	std::string version();
	void set_version(const std::string& str);

	std::string title();
	void set_title(const std::string& str);

	bool get_hero(hero& h, int default_image);
	void set_hero(const hero_map& heros, hero& h);

	bool fullscreen();
	void _set_fullscreen(bool ison);

	bool scroll_to_action();
	void _set_scroll_to_action(bool ison);

	tpoint landscape_size(const CVideo& video);
	void _set_resolution(const std::pair<int,int>& res);

	void set_float_button_xy(const std::string& prefix, int x, int y);
	tpoint float_button_xy(const std::string& prefix);

	void set_openid_type(int value);
	int openid_type();

	void set_noble(int value);
	int noble();

	int uid();
	void set_uid(int value);

	std::string city();
	void set_city(const std::string& str);

	std::string interior();
	void set_interior(const std::string& str);

	std::string signin();
	void set_signin(const std::string& str);

	std::string member();
	void set_member(const std::string& str);

	std::string exile();
	void set_exile(const std::string& str);

	std::string associate();
	void set_associate(const std::string& str);

	std::string layout();
	void set_layout(const std::string& str);

	std::string map();
	void set_map(const std::string& str);

	void set_coin(int value);
	int coin();

	void set_score(int value);
	int score();

	std::string login();

	void set_vip_expire(time_t value);
	time_t vip_expire();

	void set_developer(bool ison);
	bool developer();

	std::string nick();
	void set_nick(const std::string& nick);

	bool chat();
	void set_chat(bool val);

	std::string chat_person();
	void set_chat_person(const std::string& person);

	std::string chat_channel();
	void set_chat_channel(const std::string& chanel);

	bool startup_login();
	void set_startup_login(bool val);

	std::string password();
	void set_password(const std::string& password);

	bool turbo();
	void _set_turbo(bool ison);

	double turbo_speed();
	void save_turbo_speed(const double speed);

	bool default_move();
	void _set_default_move(const bool ison);

	std::string language();
	void set_language(const std::string& s);

	// Don't rename it to sound() because of a gcc-3.3 branch bug,
	// which will cause it to conflict with the sound namespace.
	bool sound_on();
	bool set_sound(bool ison);

	int sound_volume();
	void set_sound_volume(int vol);

	int bell_volume();
	void set_bell_volume(int vol);

	int UI_volume();
	void set_UI_volume(int vol);

	bool music_on();
	bool set_music(bool ison);

	int music_volume();
	void set_music_volume(int vol);

	bool turn_bell();
	bool set_turn_bell(bool ison);

	bool UI_sound_on();
	bool set_UI_sound(bool ison);

	bool message_bell();

	// Proxies for preferences_dialog
	void load_hotkeys();
	void save_hotkeys();

	void add_alias(const std::string& alias, const std::string& command);
	const config &get_alias();

	bool use_color_cursors();
	void _set_color_cursors(bool value);

	int scroll_speed();
	void set_scroll_speed(const int scroll);

	bool middle_click_scrolls();
	bool mouse_scroll_enabled();
	void enable_mouse_scroll(bool value);

	/**
	 * Gets the threshold for when to scroll.
	 *
	 * This scrolling happens when the mouse is in the application and near
	 * the border.
	 */
	int mouse_scroll_threshold();

	bool grid();
	void _set_grid(bool ison);

	int zoom();
	void _set_zoom(int value);

	std::string theme();
	void set_theme(const std::string& theme);

	void set_use_rose_keyboard(bool value);
	bool use_rose_keyboard();

} // end namespace preferences

#endif
