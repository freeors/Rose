/* $Id: preferences.cpp 47642 2010-11-21 13:58:27Z mordante $ */
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

/**
 *  @file
 *  Get and set user-preferences.
 */

#include "global.hpp"

#define GETTEXT_DOMAIN "rose-lib"

#include "config.hpp"
#include "filesystem.hpp"
#include "gui/widgets/settings.hpp"
#include "hotkeys.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "display.hpp"
#include "gettext.hpp"
#include "hero.hpp"
#include "lobby.hpp"
#include "base_instance.hpp"

namespace {

bool color_cursors = false;

config prefs;
config critical_prefs;

}

namespace preferences {

std::string public_account = "";
std::string public_password = "";
uint32_t last_write_ticks = 0;

base_manager::base_manager()
{
	std::string stream;
	{
		tfile file(get_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = file.read_2_data();
		SDL_Log("base_manager::base_manager--- prefs_file: %s, fsize: %i", get_prefs_file().c_str(), fsize);
		if (fsize > 0) {
			stream.assign(file.data, fsize);
			try {
				read(prefs, stream);
				const config& hero_cfg = prefs.child("hero");
				if (!hero_cfg) {
					// exist preferences, but isn't valid
					SDL_Log("base_manager::base_manager, exist preferences, but isn't valid, because hero_cfg is <nil>");
					prefs.clear();
				} else {
					SDL_Log("base_manager::base_manager, use preferences, fsize: %i", fsize);
				}
			} catch (twml_exception& ) {
				prefs.clear();
			}
		}
	}

	if (prefs.empty()) {
		tfile bak(get_prefs_back_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = bak.read_2_data();
		SDL_Log("base_manager::base_manager, preferences is valid, try preferences.bak, fsize: %i", fsize);
		if (fsize > 0) {
			stream.assign(bak.data, fsize);
			try {
				read(prefs, stream);
				const config& hero_cfg = prefs.child("hero");
				if (!hero_cfg) {
					// exist preferences.bak, but isn't valid
					SDL_Log("base_manager::base_manager, exist preferences.bak, but isn't valid, because hero_cfg is <nil>");
					prefs.clear();
				} else {
					SDL_Log("base_manager::base_manager, use preferences.bak, fsize: %u", stream.size());
					// preferences doesn't exist, but preferences.bak is valid. use preferences.bak replace preferences.
					single_open_copy(get_prefs_back_file(), get_prefs_file());
				}
			} catch (twml_exception& ) {
				prefs.clear();
			}
		}
	}
	SDL_Log("base_manager::base_manager, %s find valid preferences or preferences.bak", prefs.empty()? "cannot": "can");

	{
		// critical preferences
		tfile file(get_critical_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = file.read_2_data();
		SDL_Log("base_manager::base_manager, prefs_file: %s, fsize: %i", get_critical_prefs_file().c_str(), fsize);
		if (fsize > 0) {
			std::string stream(file.data, fsize);
			try {
				read(critical_prefs, stream);
			} catch (twml_exception& ) {
				critical_prefs.clear();
			}
		}
	}

	if (member().empty()) {
		std::stringstream strstr;
		std::map<int, int> member;
		member.insert(std::make_pair(172, 5));
		member.insert(std::make_pair(176, 6));
		member.insert(std::make_pair(244, 7));
		member.insert(std::make_pair(279, 8));

		member.insert(std::make_pair(103, 8));
		member.insert(std::make_pair(106, 9));
		member.insert(std::make_pair(120, 9));
		member.insert(std::make_pair(381, 9));
		for (std::map<int ,int>::const_iterator it = member.begin(); it != member.end(); ++ it) {
			if (it != member.begin()) {
				strstr << ", ";
			}
			strstr << ((it->second << 16) | it->first);
		}
		set_member(strstr.str());
	}
}

base_manager::~base_manager()
{
	// Set the 'hidden' preferences.
	prefs["scroll_threshold"] = mouse_scroll_threshold();

	write_preferences();
}

void write_preferences()
{
	VALIDATE_IN_MAIN_THREAD();

	std::stringstream out;

	// write critical prefs
	if (instance) {
		::config cfg = instance->app_critical_prefs();
		config diff_cfg;
		if (!cfg.empty()) {
			cfg.get_diff(critical_prefs, diff_cfg);
		}
		if (!diff_cfg.empty()) {
			try {
				write(out, cfg);
				critical_prefs = cfg;
			} catch (io_exception&) {
				// error writing to preferences file '$get_prefs_file()'
			}
			tfile file(get_critical_prefs_file(), GENERIC_WRITE, CREATE_ALWAYS);
			VALIDATE(file.valid(), null_str);
			posix_fwrite(file.fp, out.str().c_str(), out.str().size());
		}
	}
	out.str("");
	
	// wirte full prefs
	try {
		write(out, prefs);

	} catch (io_exception&) {
		// error writing to preferences file '$get_prefs_file()'
		return;
	}

	std::string data;
	{
		tfile file(get_prefs_file(), GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0) {
			data.assign(file.data, fsize);
		}
	}
	if (!data.empty()) {
		if (data.size() == out.str().size()) {
			if (memcmp(data.c_str(), out.str().c_str(), data.size()) == 0) {
				return;
			}
		}
		tfile file(get_prefs_back_file(), GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, data.c_str(), data.size());
	}

	{
		tfile file(get_prefs_file(), GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		posix_fwrite(file.fp, out.str().c_str(), out.str().size());
	}
	last_write_ticks = SDL_GetTicks();
}

void set_bool(const std::string &key, bool value)
{
	VALIDATE_IN_MAIN_THREAD();
	prefs[key] = value;
}

void set_int(const std::string &key, int value)
{
	VALIDATE_IN_MAIN_THREAD();
	prefs[key] = value;
}

void set_str(const std::string &key, const std::string &value)
{
	VALIDATE_IN_MAIN_THREAD();
	prefs[key] = value;
}

void clear(const std::string& key)
{
	VALIDATE_IN_MAIN_THREAD();
	prefs.recursive_clear_value(key);
}

void set_child(const std::string& key, const config& val) 
{
	VALIDATE_IN_MAIN_THREAD();
	prefs.clear_children(key);
	prefs.add_child(key, val);
}

const config& get_child(const std::string& key)
{
	return prefs.child(key);
}

void erase(const std::string& key) 
{
	VALIDATE_IN_MAIN_THREAD();
	prefs.remove_attribute(key);
}

std::string get_str(const std::string& key) 
{
	if (!prefs.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return null_str;
	}
	return prefs[key];
}

bool get_bool(const std::string &key, bool def)
{
	if (!prefs.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return prefs[key].to_bool(def);
}

int get_int(const std::string &key, int def)
{
	if (!prefs.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return prefs[key].to_int(def);
}

uint32_t get_uint32(const std::string &key, uint32_t def)
{
	if (!prefs.has_attribute(key)) {
		// if key doesn't exist in prefs, get will create a "empty" key. make avoid it.
		return def;
	}
	return prefs[key].to_unsigned(def);
}

config* get_prefs()
{
	config* pointer = &prefs;
	return pointer;
}

const config& get_critical_prefs()
{
	return critical_prefs;
}

std::string version()
{
	return preferences::get_str("version");
}

void set_version(const std::string& str)
{
	if (str != version()) {
		set_str("version", str);
		write_preferences();
	}
}

std::string title()
{
	std::string ret = preferences::get_str("title");
	if (ret.empty()) {
		ret = instance->app_defaut_title();
	}
	return ret;
}

void set_title(const std::string& str)
{
	if (str != title()) {
		set_str("title", str);
		write_preferences();
	}
}

bool get_hero(hero& h, int default_image)
{
	const config& cfg = prefs.child("hero");
	if (cfg) {
		h.set_name(cfg["name"].str());
		h.set_surname(cfg["surname"].str());
		h.set_biography(cfg["biography"].str());

		h.gender_ = cfg["gender"].to_int(hero_gender_male);
		h.image_ = cfg["image"].to_int(default_image);

		h.leadership_ = ftofxp9(cfg["leadership"].to_int(1));
		h.force_ = ftofxp9(cfg["force"].to_int(1));
		h.intellect_ = ftofxp9(cfg["intellect"].to_int(1));
		h.spirit_ = ftofxp9(cfg["spirit"].to_int(1));
		h.charm_ = ftofxp9(cfg["charm"].to_int(1));

		std::stringstream str;
		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			str.str("");
			str << "arms" << i;
			h.arms_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			str.str("");
			str << "skill" << i;
			h.skill_[i] = ftofxp12(cfg[str.str()].to_int(0));
		}

		h.side_feature_ = cfg["side_feature"].to_int(HEROS_NO_FEATURE);
		if (h.side_feature_ < 0 || h.side_feature_ > HEROS_MAX_FEATURE) {
			h.side_feature_ = HEROS_NO_FEATURE;
		}
		h.feature_ = cfg["feature"].to_int(HEROS_NO_FEATURE);
		if (h.feature_ < 0 || h.feature_ > HEROS_MAX_FEATURE) {
			h.feature_ = HEROS_NO_FEATURE;
		}
		h.tactic_ = cfg["tactic"].to_int(HEROS_NO_TACTIC);
		h.utype_ = cfg["utype"].to_int(HEROS_NO_UTYPE);
		h.character_ = cfg["character"].to_int(HEROS_NO_CHARACTER);

		h.base_catalog_ = cfg["base_catalog"].to_int();
		h.float_catalog_ = ftofxp8(h.base_catalog_);

		return true;
	} else {
		// h.set_name(_("Press left button to create hero"));
		h.set_name(public_account);
		h.set_surname("Mr.");

		h.gender_ = hero_gender_male;
		h.image_ = default_image;

		h.leadership_ = ftofxp9(1);
		h.force_ = ftofxp9(1);
		h.intellect_ = ftofxp9(1);
		h.spirit_ = ftofxp9(1);
		h.charm_ = ftofxp9(1);

		for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
			h.arms_[i] = 0;
		}
		for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
			h.skill_[i] = 0;
		}

		return false;
	}
}

void set_hero(const hero_map& heros, hero& h)
{
	VALIDATE_IN_MAIN_THREAD();
	config* ptr = NULL;
	if (config& cfg = prefs.child("hero")) {
		ptr = &cfg;
	} else {
		ptr = &prefs.add_child("hero");
	}
	config& cfg = *ptr;
	std::stringstream str;

	cfg["name"] = h.name();
	cfg["surname"] = h.surname();
	cfg["biography"] = h.biography2(heros);

	cfg["leadership"] = fxptoi9(h.leadership_);
	cfg["force"] = fxptoi9(h.force_);
	cfg["intellect"] = fxptoi9(h.intellect_);
	cfg["spirit"] = fxptoi9(h.spirit_);
	cfg["charm"] = fxptoi9(h.charm_);

	cfg["gender"] = h.gender_;
	cfg["image"] = h.image_;

	for (int i = 0; i < HEROS_MAX_ARMS; i ++) {
		str.str("");
		str << "arms" << i;
		cfg[str.str()] = fxptoi12(h.arms_[i]);
	}
	for (int i = 0; i < HEROS_MAX_SKILL; i ++) {
		str.str("");
		str << "skill" << i;
		cfg[str.str()] = fxptoi12(h.skill_[i]);
	}

	cfg["base_catalog"] = h.base_catalog_;

	cfg["feature"] = h.feature_;
	cfg["side_feature"] = h.side_feature_;

	cfg["tactic"] = h.tactic_;
	cfg["utype"] = h.utype_;
	cfg["character"] = h.character_;

	write_preferences();
}

bool fullscreen()
{
	return get_bool("fullscreen", false);
}

void _set_fullscreen(bool ison)
{
	set_bool("fullscreen", ison);
}

bool scroll_to_action()
{
	return get_bool("scroll_to_action", false);
}

void _set_scroll_to_action(bool ison)
{
	set_bool("scroll_to_action", ison);
}

tpoint landscape_size(const CVideo& video)
{
	int width, height;
	if (!fullscreen()) {
		const std::string postfix = "windowsize";
		std::string x = prefs['x' + postfix], y = prefs['y' + postfix];
		if (!x.empty() && !y.empty()) {
			width = std::max(atoi(x.c_str()) * gui2::twidget::hdpi_scale, game_config::min_allowed_width());
			height = std::max(atoi(y.c_str()) * gui2::twidget::hdpi_scale, game_config::min_allowed_height());
			// width/height from preferences.cfg must be landscape.
			if (width < height) {
				int tmp = width;
				width = height;
				height = tmp;
			}

		} else {
			width = game_config::min_allowed_width();
			height = game_config::min_allowed_height();
		}
	} else {
		SDL_Rect bound = video.bound();
		width = bound.w;
		height = bound.h;
	}

	return tpoint(width, height);
}

int noble()
{
	int noble =  prefs["noble"].to_int();
	// if (noble < 0 || noble > unit_types.max_noble_level()) {
	// 	noble = 0;
	// }
	return noble;
}

void set_noble(int value)
{
	// if (value < 0 || value > unit_types.max_noble_level()) {
	// 	value = 0;
	// }
	set_int("noble", value);
}

void set_float_button_xy(const std::string& prefix, int x, int y)
{
	tpoint pt = float_button_xy(prefix);
	bool dirty = false;
	if (x != pt.x) {
		prefs[prefix + "x"] = x;
		dirty = true;
	}
	if (y != pt.y) {
		prefs[prefix + "y"] = y;
		dirty = true;
	}
	if (dirty) {
		write_preferences();
	}
}

tpoint float_button_xy(const std::string& prefix)
{
	return tpoint(prefs[prefix + "x"].to_int(), prefs[prefix + "y"].to_int());
}

int openid_type()
{
	return prefs["openid_type"].to_int();
}

void set_openid_type(int value)
{
	prefs["openid_type"] = value;
}

int uid()
{
	return prefs["uid"].to_int();
}

void set_uid(int value)
{
	prefs["uid"] = value;
}

std::string city()
{
	return prefs["city"].str();
}

void set_city(const std::string& str)
{
	prefs["city"] = str;
}

std::string interior()
{
	return prefs["interior"].str();
}

void set_interior(const std::string& str)
{
	prefs["interior"] = str;
}

std::string signin()
{
	return prefs["signin"].str();
}

void set_signin(const std::string& str)
{
	prefs["signin"] = str;
}

std::string member()
{
	return prefs["member"].str();
}

void set_member(const std::string& str)
{
	prefs["member"] = str;
}

std::string exile()
{
	return prefs["exile"].str();
}

void set_exile(const std::string& str)
{
	prefs["exile"] = str;
}

std::string associate()
{
	return prefs["associate"].str();
}

void set_associate(const std::string& str)
{
	prefs["associate"] = str;
}

std::string layout()
{
	return prefs["layout"].str();
}

void set_layout(const std::string& str)
{
	prefs["layout"] = str;
}

std::string map()
{
	return prefs["map"].str();
}

void set_map(const std::string& str)
{
	prefs["map"] = str;
}

void set_coin(int value)
{
	prefs["coin"] = value;
}

int coin()
{
	return prefs["coin"].to_int();
}

void set_score(int value)
{
	prefs["score"] = value;
}

int score()
{
	return prefs["score"].to_int();
}

std::string login()
{
	const config& cfg = get_child("hero");

	if (cfg && !cfg["name"].empty()) {
		return cfg["name"].str();
	}
	return public_account;
}

void set_vip_expire(time_t value)
{
	std::stringstream strstr;
	strstr << login() << ", " << (long)value;
	prefs["vip_expire"] = strstr.str();
}

time_t vip_expire()
{
	std::vector<std::string> vstr = utils::split(prefs["vip_expire"].str());
	if (vstr.size() != 2) {
		return 0;
	}
	if (vstr[0] != login()) {
		return 0;
	}
	return lexical_cast_default<long>(vstr[1]);
}

void set_developer(bool ison)
{
	prefs["developer"] = ison;
}

bool developer()
{
	return prefs["developer"].to_bool();
}

std::string nick()
{
	return prefs["nick"].str();
}

void set_nick(const std::string& nick)
{
	prefs["nick"] = nick;
}

bool chat()
{
	return prefs["chat"].to_bool(true);
}

void set_chat(bool val)
{
	prefs["chat"] = val;
}

std::string chat_person()
{
	return prefs["chat_person"].str();
}

void set_chat_person(const std::string& person)
{
	prefs["chat_person"] = person;
}

std::string chat_channel()
{
	return prefs["chat_channel"].str();
}

void set_chat_channel(const std::string& channel)
{
	prefs["chat_channel"] = channel;
}

bool startup_login()
{
	return prefs["startup_login"].to_bool(true);
}

void set_startup_login(bool val)
{
	prefs["startup_login"] = val;
}

std::string encode_pw(const std::string& str)
{
	std::stringstream ss;
	ss << "pw_";
	if (utils::is_utf8str(str.c_str(), str.size())) {
		ss << str;
	}
	return ss.str();
}

std::string decode_pw(const std::string& str)
{
	if (!utils::is_utf8str(str.c_str(), str.size())) {
		return null_str;
	}

	int pos = str.find("pw_");
	if (pos != std::string::npos) {
		return str.substr(3);
	} else {
		return str;
	}
}

std::string password()
{
	if (login() == public_account) {
		return public_password;
	}
	return decode_pw(preferences::get_str("password"));
}

void set_password(const std::string& password)
{
	preferences::set_str("password", encode_pw(password));
}

bool turbo()
{
	return get_bool("turbo", false);
}

void _set_turbo(bool ison)
{
	prefs["turbo"] = ison;
}

double turbo_speed()
{
	return prefs["turbo_speed"].to_double(2.0);
}

void save_turbo_speed(const double speed)
{
	prefs["turbo_speed"] = speed;
}

bool default_move()
{
	return  get_bool("default_move", true);
}

void _set_default_move(const bool ison)
{
	prefs["default_move"] = ison;
}

std::string language()
{
	std::string str = prefs["locale"];
	if (str.empty()) {
		str = "zh_CN";
	}
	return str;
}

void set_language(const std::string& s)
{
	preferences::set_str("locale", s);
}

bool grid()
{
	return get_bool("grid", false);
}

void _set_grid(bool ison)
{
	preferences::set_bool("grid", ison);
}

bool use_rose_keyboard()
{
	return get_bool("use_rose_keyboard", true);
}

void set_use_rose_keyboard(bool value)
{
	if (use_rose_keyboard() != value) {
		preferences::set_bool("use_rose_keyboard", value);
		preferences::write_preferences();
	}
}

int zoom()
{
	return display::adjust_zoom(prefs["zoom"].to_int(game_config::svga? display::ZOOM_72: display::ZOOM_48));
}

void _set_zoom(int value)
{
	preferences::set_int("zoom", display::adjust_zoom(value));
}

int music_volume()
{
	return prefs["music_volume"].to_int(100);
}

void set_music_volume(int vol)
{
	if(music_volume() == vol) {
		return;
	}

	prefs["music_volume"] = vol;
	sound::set_music_volume(music_volume());
}

int sound_volume()
{
	return prefs["sound_volume"].to_int(100);
}

void set_sound_volume(int vol)
{
	if(sound_volume() == vol) {
		return;
	}

	prefs["sound_volume"] = vol;
	sound::set_sound_volume(sound_volume());
}

int bell_volume()
{
	return prefs["bell_volume"].to_int(100);
}

void set_bell_volume(int vol)
{
	if(bell_volume() == vol) {
		return;
	}

	prefs["bell_volume"] = vol;
	sound::set_bell_volume(bell_volume());
}

int UI_volume()
{
	return prefs["UI_volume"].to_int(100);
}

void set_UI_volume(int vol)
{
	if(UI_volume() == vol) {
		return;
	}

	prefs["UI_volume"] = vol;
	sound::set_UI_volume(UI_volume());
}

bool turn_bell()
{
	return get_bool("turn_bell", true);
}

bool set_turn_bell(bool ison)
{
	if(!turn_bell() && ison) {
		preferences::set_bool("turn_bell", true);
		if(!music_on() && !sound_on() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("turn_bell", false);
				return false;
			}
		}
	} else if(turn_bell() && !ison) {
		preferences::set_bool("turn_bell", false);
		sound::stop_bell();
		if(!music_on() && !sound_on() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool UI_sound_on()
{
#ifdef _WIN32
	// fix bug
	return true;
#else
	return get_bool("UI_sound", true);
#endif
}

bool set_UI_sound(bool ison)
{
	if(!UI_sound_on() && ison) {
		preferences::set_bool("UI_sound", true);
		if(!music_on() && !sound_on() && !turn_bell()) {
			if(!sound::init_sound()) {
				preferences::set_bool("UI_sound", false);
				return false;
			}
		}
	} else if(UI_sound_on() && !ison) {
		preferences::set_bool("UI_sound", false);
		sound::stop_UI_sound();
		if(!music_on() && !sound_on() && !turn_bell())
			sound::close_sound();
	}
	return true;
}

bool message_bell()
{
	return get_bool("message_bell", true);
}

bool sound_on()
{
	return get_bool("sound", true);
}

bool set_sound(bool ison) {
	if(!sound_on() && ison) {
		preferences::set_bool("sound", true);
		if(!music_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("sound", false);
				return false;
			}
		}
	} else if(sound_on() && !ison) {
		preferences::set_bool("sound", false);
		sound::stop_sound();
		if(!music_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
	}
	return true;
}

bool music_on()
{
	return get_bool("music", true);
}

bool set_music(bool ison) {
	if(!music_on() && ison) {
		preferences::set_bool("music", true);
		if(!sound_on() && !turn_bell() && !UI_sound_on()) {
			if(!sound::init_sound()) {
				preferences::set_bool("music", false);
				return false;
			}
		}
		else
			sound::play_music();
	} else if(music_on() && !ison) {
		preferences::set_bool("music", false);
		if(!sound_on() && !turn_bell() && !UI_sound_on())
			sound::close_sound();
		else
			sound::stop_music();
	}
	return true;
}

namespace {
	double scroll = 0.2;
}

int scroll_speed()
{
	const int value = lexical_cast_in_range<int>(get_str("scroll"), 50, 1, 100);
	scroll = value/100.0;

	return value;
}

void set_scroll_speed(const int new_speed)
{
	prefs["scroll"] = new_speed;
	scroll = new_speed / 100.0;
}

bool middle_click_scrolls()
{
	return get_bool("middle_click_scrolls", true);
}

bool mouse_scroll_enabled()
{
	return get_bool("mouse_scrolling", true);
}

void enable_mouse_scroll(bool value)
{
	set_bool("mouse_scrolling", value);
}

int mouse_scroll_threshold()
{
	return prefs["scroll_threshold"].to_int(10);
}

bool use_color_cursors()
{
	return color_cursors;
}

void _set_color_cursors(bool value)
{
	preferences::set_bool("color_cursors", value);
	color_cursors = value;
}

void load_hotkeys() {
	hotkey::load_hotkeys(prefs);
}
void save_hotkeys() {
	hotkey::save_hotkeys(prefs);
}

void add_alias(const std::string &alias, const std::string &command)
{
	config &alias_list = prefs.child_or_add("alias");
	alias_list[alias] = command;
}


const config &get_alias()
{
	return get_child("alias");
}

std::string theme()
{
	return preferences::get_str("theme");
}

void set_theme(const std::string& theme)
{
	preferences::set_str("theme", theme);
}

} // end namespace preferences

