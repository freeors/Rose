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
#define GETTEXT_DOMAIN "rose-lib"

#include "base_instance.hpp"
#include "gettext.hpp"
#include "builder.hpp"
#include "language.hpp"
#include "preferences_display.hpp"
#include "xwml.hpp"
#include "cursor.hpp"
#include "map.hpp"
#include "version.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/track.hpp"
#include "ble.hpp"
#include "theme.hpp"
#include "aplt_net.hpp"

#ifdef WEBRTC_ANDROID
#include "modules/utility/include/jvm_android.h"
#include <base/android/jni_android.h>
#include <base/android/base_jni_onload.h>
#endif

// chromium
#include <base/command_line.h>
#include <base/logging.h>

// live555
#include <groupsock/include/GroupsockHelper.hh>

#include <iostream>
#include <clocale>
using namespace std::placeholders;

#include <opencv2/core.hpp>
#include "qr_code.hpp"

base_instance* instance = nullptr;

void VALIDATE_IN_MAIN_THREAD()
{
	VALIDATE(instance == nullptr || instance->thread_checker_.CalledOnValidThread(), null_str);
}

void VALIDATE_NOT_MAIN_THREAD()
{
	DCHECK(!instance->thread_checker_.CalledOnValidThread());
}

void VALIDATE_CALLED_ON_THIS_THREAD(unsigned long tid)
{
	VALIDATE(tid == SDL_ThreadID(), null_str);
}

void base_instance::regenerate_heros(hero_map& heros, bool allow_empty)
{
	const std::string hero_data_path = game_config::path + "/xwml/" + "hero.dat";
	heros.map_from_file(hero_data_path);
	if (!heros.map_from_file(hero_data_path)) {
		if (allow_empty) {
			// allow no hero.dat
			heros.realloc_hero_map(HEROS_MAX_HEROS);
		} else {
			std::stringstream err;
			err << _("Can not find valid hero.dat in <data>/xwml");
			throw game::error(err.str());
		}
	}
	hero_map::map_size_from_dat = heros.size();
	hero player_hero((uint16_t)hero_map::map_size_from_dat);
	if (!preferences::get_hero(player_hero, heros.size())) {
		// write [hero] to preferences
		preferences::set_hero(heros, player_hero);
	}

	group.reset();
	heros.add(player_hero);
	hero& leader = heros[(uint16_t)(heros.size() - 1)];
	group.set_leader(leader);
	leader.set_uid(preferences::uid());
	group.set_noble(preferences::noble());
	group.set_coin(preferences::coin());
	group.set_score(preferences::score());
	group.interior_from_str(preferences::interior());
	group.signin_from_str(preferences::signin());
	group.reload_heros_from_string(heros, preferences::member(), preferences::exile());
	group.associate_from_str(preferences::associate());
	// group.set_layout(preferences::layout()); ?????
	group.set_map(preferences::map());

	group.set_city(heros[hero::number_local_player_city]);
	group.city().set_name(preferences::city());

	other_group.clear();
}

extern void preprocess_res_explorer();
static void handle_app_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_app_event(type);
}

static void handle_window_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_window_event(type);
}

static void handle_background(SDL_bool screen_on, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_background(screen_on? true: false);
}

bool is_all_ascii(const std::string& str)
{
	// str maybe ansi, unicode, utf-8.
	const char* c_str = str.c_str();
	int size = str.size();
	VALIDATE(size, null_str);

	for (int at = 0; at < size; at ++) {
		if (c_str[at] & 0x80) {
			return false;
		}
	}
	return true;
}

void path_must_all_ascii(const std::string& path)
{
	// path is utf-8 format.
	VALIDATE(!path.empty(), null_str);

	bool ret = is_all_ascii(path);
	if (!ret) {
		std::string path2 = utils::normalize_path(path);
		utils::string_map symbols;
		symbols["path"] = path2;
		const std::string err = vgettext2("$path contains illegal characters. Please use a pure english path.", symbols);
		VALIDATE(false, err); 
	}
}

static void opencv_verbose()
{
	bool support_avx2 = cv::checkHardwareSupport(CV_CPU_AVX2);
    bool support_neon = cv::checkHardwareSupport(CV_CPU_NEON);
	SDL_Log("opencv, avx2: %s, neon: %s", support_avx2? "true": "false", support_neon? "true": "false");
    cv::Mat mat1(cv::Size(4, 1), CV_32FC1), mat2(cv::Size(4, 1), CV_32FC1), mat3;
    mat1.at<float>(0, 0) = 1;  mat2.at<float>(0, 0) = 1;
    mat1.at<float>(0, 1) = 1;  mat2.at<float>(0, 1) = -1;
    mat1.at<float>(0, 2) = -1; mat2.at<float>(0, 2) = 1;
    mat1.at<float>(0, 3) = -1; mat2.at<float>(0, 3) = -1;
    cv::phase(mat1, mat2, mat3, true);

	const bool extra = false;
	if (extra) {
/*
		std::vector<std::string> images;
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-1.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-2.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-2-small.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1495-1512x2016.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1495.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1496-1512x2016.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1496.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1497.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1498.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1499.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1500.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1517.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1518.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/1.png");
		images.push_back(game_config::path + "/app-kdesktop/images/2.png");

		for (std::vector<std::string>::const_iterator it = images.begin(); it != images.end(); ++ it) {
			const std::string& image = *it;
			surface surf = image::get_image(image);
			VALIDATE(surf.get() != nullptr, null_str);
			tsurface_2_mat_lock mat_lock(surf);
			cv::Mat rgb;
			cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);
			std::string result = find_qr(rgb);
			SDL_Log("find_qr(%s), result: %s", image.c_str(), result.empty()? "null": result.c_str());
		}
*/
		surface surf = image::get_image("misc/save.png");
		VALIDATE(surf.get() != nullptr, null_str);
		SDL_Point src_size{3024, 4032};
		surf = scale_surface(surf, src_size.x, src_size.y);
		VALIDATE(surf->w == src_size.x && surf->h == src_size.y, null_str);

		std::vector<SDL_Point> scales;
		scales.push_back(SDL_Point{surf->w / 2, surf->h / 2});
		scales.push_back(SDL_Point{surf->w / 3, surf->h / 3});
		scales.push_back(SDL_Point{surf->w / 4, surf->h / 4});
		scales.push_back(SDL_Point{1511, 1311});
		scales.push_back(SDL_Point{2345, 1789});
		scales.push_back(SDL_Point{4123, 3123});
		scales.push_back(SDL_Point{5123, 4123});
		for (std::vector<SDL_Point>::const_iterator it = scales.begin(); it != scales.end(); ++ it) {
			const SDL_Point& size = *it;
			uint32_t start_ticks = SDL_GetTicks();
			surface scaled = scale_surface(surf, size.x, size.y);
			uint32_t stop_ticks = SDL_GetTicks();
			VALIDATE(scaled->w == size.x && scaled->h == size.y, null_str);
			SDL_Log("opencv_verbose, scale_surface from (%ix%i) == > (%ix%i), used: %u ms",
				surf->w, surf->h, scaled->w, scaled->h, stop_ticks - start_ticks);
		}
	}
}

base_instance::base_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv, int sample_rate, size_t sound_buffer_size)
	: sdl_thread_(&ss)
	, icon_()
	, chromium_env_(base::test::TaskEnvironment::TimeSource::SYSTEM_TIME, base::test::TaskEnvironment::MainThreadType::IO)
	, video_()
	, heros_(game_config::path)
	, lua_(nullptr)
	, font_manager_() 
	, prefs_manager_()
	, image_manager_()
	, music_thinker_()
	, paths_manager_()
	, anim2_manager_(video_)
	, cursor_manager_(NULL)
	, loadscreen_manager_(NULL)
	, gui2_event_manager_(NULL)
	, app_cfg_()
	, old_defines_map_()
	, cache_(config_cache::instance())
	, foreground_(true) // normally app cannnot receive first DIDFOREGROUND.
	, terminating_(false)
	, minimized_(false)
	, silent_background_(true)
	, send_helper_(nullptr)
	// , background_persist_xmit_audio_(NULL)
	, background_callback_id_(INVALID_UINT32_ID)
	, rtc_client_(nullptr)
	, ble_(nullptr)
	, check_ip_threshold_(10000)
	, next_check_ip_ticks_(0)
	, disable_check_ip_(false)
	, ip_require_invalid_(false)
	, server_check_port_(8081)
	, invalidate_layout_(false)
{
	// VALIDATE_CALLED_ON_THIS_THREAD require it.
	VALIDATE(sizeof(unsigned long) == sizeof(SDL_threadID), null_str);

	// base::CommandLine::InitUsingArgvForTesting(argc, argv);
	base::CommandLine::Init(argc, argv);

	void* v1 = NULL;
	void* v2 = NULL;
	SDL_GetPlatformVariable(&v1, &v2, NULL);
#ifdef WEBRTC_ANDROID
#ifndef _KOS
	webrtc::JVM::Initialize(reinterpret_cast<JavaVM*>(v1), reinterpret_cast<jobject>(v2));
	base::android::InitVM(reinterpret_cast<JavaVM*>(v1));
	base::android::OnJNIOnLoadInit();
#endif
#endif
	logging::LoggingSettings settings;
	settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG; // [chromium]default to file on windows.
	logging::InitLogging(settings);

	// rtc::ThreadManager::Instance()->SetCurrentThread(&sdl_thread_);

	VALIDATE(game_config::path.empty(), null_str);
#ifdef _WIN32
	std::string exe_dir = get_exe_dir();
	if (!exe_dir.empty() && file_exists(exe_dir + "/data/_main.cfg")) {
		game_config::path = exe_dir;
		path_must_all_ascii(game_config::path);
	}

	// windows is debug os. allow pass command line.
	for (int arg_ = 1; arg_ != argc; ++ arg_) {
		const std::string option(argv[arg_]);
		if (option.empty()) {
			continue;
		}

		if (option == "--res-dir") {
			const std::string val = argv[++ arg_];

			// Overriding data directory
			if (val.c_str()[1] == ':') {
				game_config::path = val;
			} else {
				game_config::path = get_cwd() + '/' + val;
			}
			path_must_all_ascii(game_config::path);

			if (!is_directory(game_config::path)) {
				std::stringstream err;
				err << "Use --res-dir set " << utils::normalize_path(game_config::path) << " to game_config::path, but it isn't valid directory.";
				VALIDATE(false, err.str());
			}
		}
	}

#elif defined(__APPLE__)
	// ios, macosx
	game_config::path = get_cwd();

#elif defined(ANDROID)
#ifndef _KOS	
	// on android, use asset.
	const std::string internal_storage_path = SDL_AndroidGetInternalStoragePath();
	SDL_Log("internal_storage_path: %s", internal_storage_path.c_str());
	game_config::path = "res";
#else
	game_config::path = SDL_AndroidGetInternalStoragePath();
#endif
#else
	// linux, etc
	game_config::path = get_cwd();
#endif

	game_config::path = utils::normalize_path(game_config::path);
/*
	std::string path1 = utils::normalize_path(game_config::path + "//.");
	std::string path2 = utils::normalize_path(game_config::path + "//.g");
	std::string path3 = utils::normalize_path(game_config::path + "//.g/h./..");
	std::string path4 = utils::normalize_path(game_config::path + "//.g/.");
	std::string path5 = utils::normalize_path(game_config::path + "//.g/./");
	std::string path6 = utils::normalize_path(game_config::path + "//.g/.h");
*/
	VALIDATE(game_config::path.find('\\') == std::string::npos, null_str);
	VALIDATE(file_exists(game_config::path + "/data/_main.cfg"), "game_config::path(" + game_config::path + ") must be valid!");

	SDL_Log("Data directory: %s, sizeof(long): %u", game_config::path.c_str(), sizeof(long));
	game_config::app_path = game_config::path + "/" + game_config::app_dir;

	preprocess_res_explorer();

	sound::set_play_params(sample_rate, sound_buffer_size);

	bool no_music = false;
	bool no_sound = false;

	// if allocate static, iOS may be not align 4! 
	// it necessary to ensure align great equal than 4.
	game_config::savegame_cache = (unsigned char*)malloc(game_config::savegame_cache_size);

	font_manager_.update_font_path();
	heros_.set_path(game_config::path);

#if defined(_WIN32) && defined(_DEBUG)
	// sound::init_sound make no memory leak output.
	// By this time, I doesn't find what result it, to easy, don't call sound::init_sound. 
	no_sound = true;
#endif

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {

		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	} else if (no_music) { // else disable the music in nomusic mode
		preferences::set_music(false);
	}

	game_config::reserve_players.insert("");
	game_config::reserve_players.insert("kingdom");
	game_config::reserve_players.insert("Player");
	game_config::reserve_players.insert(_("Player"));


	//
	// initialize player group
	//
	upgrade::fill_require();
	regenerate_heros(heros_, true);

	memset(&sdl_apphandlers_, 0, sizeof(sdl_apphandlers_));
	sdl_apphandlers_.app_event_handler = ::handle_app_event;
	sdl_apphandlers_.app_event_param = this;
	sdl_apphandlers_.window_event_handler = ::handle_window_event;
	sdl_apphandlers_.window_event_param = this;
	sdl_apphandlers_.background_handler = ::handle_background;
	sdl_apphandlers_.background_param = this;
	SDL_SetAppHandlers(&sdl_apphandlers_);

	opencv_verbose();
}

base_instance::~base_instance()
{
	SDL_Log("base_instance::~base_instance()---");
	// enter deconstruct, instance require nullptr.
	VALIDATE(instance == nullptr, null_str);

	if (icon_) {
		icon_.get()->refcount --;
	}
	if (game_config::savegame_cache) {
		free(game_config::savegame_cache);
		game_config::savegame_cache = NULL;
	}
	terrain_builder::release_heap();
	sound::close_sound();

	clear_anims();

	game_config::path.clear();

	base::CommandLine::Reset();
	SDL_Log("---base_instance::~base_instance() X");
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
void base_instance::init_locale() 
{
#ifdef _WIN32
    std::setlocale(LC_ALL, "English");
#else
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain("rose-lib", intl_dir.c_str());
	bind_textdomain_codeset ("rose-lib", "UTF-8");
	

	const std::string path = game_config::app_path + "/translations"; 
	bindtextdomain(def_textdomain, path.c_str());
	bind_textdomain_codeset(def_textdomain, "UTF-8");
	textdomain(def_textdomain);

	app_init_locale(intl_dir);
}

bool base_instance::init_language()
{
	if (!::load_language_list()) {
		return false;
	}

	if (!::set_language(get_locale())) {
		return false;
	}

	// hotkey::load_descriptions();

	return true;
}

uint32_t base_instance::get_callback_id() const
{
	uint32_t id = background_callback_id_ + 1;
	while (true) {
		if (id == INVALID_UINT32_ID) {
			// turn aournd.
			id ++;
			continue;
		}
		if (background_callbacks_.count(id)) {
			id ++;
			continue;
		}
		break;
	}
	return id;
}

uint32_t base_instance::background_connect(const std::function<bool (uint32_t ticks, bool screen_on)>& callback)
{
	SDL_Log("base_instance::background_connect------, callbacks_: %u", background_callbacks_.size());
	if (!foreground_) {
		if (background_callbacks_.empty()) {
			// VALIDATE(!background_persist_xmit_audio_.get(), null_str);
			// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
        // VALIDATE(background_persist_xmit_audio_.get(), null_str);
	}

	background_callback_id_ = get_callback_id();
	background_callbacks_.insert(std::make_pair(background_callback_id_, callback));

	SDL_Log("------base_instance::background_connect, callbacks_: %u", background_callbacks_.size());
	return background_callback_id_;
}

void base_instance::background_disconnect_th(const uint32_t id)
{
	if (!foreground_) {
		// VALIDATE(background_persist_xmit_audio_.get(), null_str);
		if (background_callbacks_.size() == 1) {
			// background_persist_xmit_audio_.reset(NULL);
		}
	}
}

void base_instance::background_disconnect(const uint32_t id)
{
	SDL_Log("base_instance::background_disconnect------, callbacks_: %u", background_callbacks_.size());

	background_disconnect_th(id);

	VALIDATE(background_callbacks_.count(id), null_str);
	std::map<uint32_t, std::function<bool (uint32_t, bool)> >::iterator it = background_callbacks_.find(id);
	background_callbacks_.erase(it);

	SDL_Log("------base_instance::background_disconnect, callbacks_: %u", background_callbacks_.size());
}

void base_instance::handle_background(bool screen_on)
{
	uint32_t ticks = SDL_GetTicks();
	for (std::map<uint32_t, std::function<bool (uint32_t ticks, bool screen_on)> >::iterator it = background_callbacks_.begin(); it != background_callbacks_.end(); ) {
		// must not app call background_disconnect! once enable, this for will result confuse!
		bool erase = it->second(ticks, screen_on);
		if (erase) {
			background_disconnect_th(it->first);
			background_callbacks_.erase(it ++);
		} else {
			++ it;
		}
	}
}


void base_instance::did_enter_background()
{
	app_didenterbackground();

	foreground_ = false;
		
	// app require insert callback in app_didenterbackground.
	// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
	if (!background_callbacks_.empty()) {
		// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
	}
	if (ble_) {
		ble_->handle_app_event(true);
	}
}

void base_instance::did_enter_foreground()
{
	// force clear all backgroud callback. app requrie clear callback_id in app_didenterforeground.
	if (!background_callbacks_.empty()) {
		// VALIDATE(background_persist_xmit_audio_.get() != NULL, null_str);
		// background_persist_xmit_audio_.reset(NULL);
		background_callbacks_.clear();
	} else {
		// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
	}
	app_didenterforeground();
	
	foreground_ = true;
	invalidate_layout_ = true;
	if (ble_ != nullptr) {
		ble_->handle_app_event(false);
	}
}

void base_instance::handle_app_event(Uint32 type)
{
	// Notice! it isn't in main thread, except SDL_APP_LOWMEMORY.
	if (type == SDL_APP_TERMINATING || type == SDL_QUIT) {
		SDL_Log("handle_app_event, %s", type == SDL_APP_TERMINATING? "SDL_APP_TERMINATING": "SDL_QUIT");
		if (!terminating_) {
			SDL_Log("handle_app_event, handle terminating task");
			SDL_EnableStatusBar(SDL_TRUE);
			app_terminating();

			terminating_ = true;

			if (game_config::os == os_ios) {
				// now iOS is default!
				// if call CVideo::quit when SDL_APP_WILLENTERBACKGROUND, it will success. Of couse, no help.
				throw CVideo::quit();
			}
		}

	} else if (type == SDL_APP_WILLENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERBACKGROUND");
		app_willenterbackground();
		// FIX SDL BUG! normally DIDENTERBACKGROUND should be called after WILLENTERBACKGROUND.
		// but on iOS, because SDL event queue, SDL-DIDENTERBACKGROUND is called, but app-DIDENTERBACKGROUND not!
		// app-DIDENTERBACKGROUND is call when WILLENTERFOREGROUND.

	} else if (type == SDL_APP_DIDENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERBACKGROUND");
		did_enter_background();
/*
		app_didenterbackground();

		foreground_ = false;
		
		// app require insert callback in app_didenterbackground.
		// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		if (!background_callbacks_.empty()) {
			// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
		if (ble_) {
			ble_->handle_app_event(true);
		}
*/
	} else if (type == SDL_APP_WILLENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERFOREGROUND");
		app_willenterforeground();

	} else if (type == SDL_APP_DIDENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERFOREGROUND");
		did_enter_foreground();
/*
		// force clear all backgroud callback. app requrie clear callback_id in app_didenterforeground.
		if (!background_callbacks_.empty()) {
			// VALIDATE(background_persist_xmit_audio_.get() != NULL, null_str);
            // background_persist_xmit_audio_.reset(NULL);
			background_callbacks_.clear();
		} else {
			// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		}
		app_didenterforeground();
	
		foreground_ = true;
		invalidate_layout_ = true;
		if (ble_ != nullptr) {
			ble_->handle_app_event(false);
		}
*/

	} else if (type == SDL_APP_LOWMEMORY) {
		SDL_Log("handle_app_event, SDL_APP_LOWMEMORY");
		app_lowmemory();

	} else if (type == SDL_CLIPBOARDUPDATE) {
		SDL_Log("handle_app_event, SDL_CLIPBOARDUPDATE");
		std::string val;
		if (SDL_HasClipboardText()) {
			char* text = SDL_GetClipboardText();
			val = text;
			SDL_free(text);
		}
		// if "copy" non-text content, it will enter it also.
		// If it's empty, it means copying non-text content, like files.
		instance->app_handle_clipboard_paste(val);
	}
	video_.set_force_render_present();
}

void base_instance::handle_window_event(Uint32 type)
{
	if (type == SDL_WINDOWEVENT_MINIMIZED) {
		minimized_ = true;
		SDL_Log("handle_window_event, MINIMIZED");
		if (game_config::os == os_windows) {
			// foreground_ = false;
			did_enter_background();
		}

	} else if (type == SDL_WINDOWEVENT_MAXIMIZED || type == SDL_WINDOWEVENT_RESTORED) {
		minimized_ = false;
		SDL_Log("handle_window_event, %s", type == SDL_WINDOWEVENT_MAXIMIZED? "MAXIMIZED": "RESTORED");
		if (game_config::os == os_windows && type == SDL_WINDOWEVENT_RESTORED) {
			// invalidate_layout_ = true;
			// foreground_ = true;
			did_enter_foreground();
		}

	}
	video_.set_force_render_present();
}

void base_instance::handle_resize_screen(const int sdl_width, const int sdl_height)
{
	// CVideo::setMode will result handle_resize_screen. 
	// in that case, (width,height) equals to (gui2::settings::screen_width, gui2::settings::screen_width).
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(game_config::os == os_windows, null_str);

	int actual_w = 0;
	int actual_h = 0;
	SDL_GetWindowSize(get_sdl_window(), &actual_w, & actual_h);
	if (sdl_width != actual_w || sdl_height != actual_h) {
		// SDL_WINDOWEVENT_SIZE_CHANGED maybe delay received,
		SDL_Log("base_instance::handle_resize_screen X, sdl size(%ix%i) doesn't equal to actual size(%ix%i), think delay result, do nothing", sdl_width, sdl_height, actual_w, actual_h);
		return;
	}

	int width = posix_align_floor(sdl_width, gui2::twidget::hdpi_scale);
	int height = posix_align_floor(sdl_height, gui2::twidget::hdpi_scale);
	const bool size_not_equal = width != sdl_width || height != sdl_height;

	tpoint min_size = gui2::twidget::orientation_swap_size(game_config::min_allowed_width(), game_config::min_allowed_height());
	bool enlarge_size = false;
	if (width < min_size.x) {
		enlarge_size = true;
		width = min_size.x;
	}
	if (height < min_size.y) {
		enlarge_size = true;
		height = min_size.y;
	}
	// if (enlarge_size || size_not_equal) {
	if (enlarge_size) {
		// if SDL_WINDOW_MAXIMIZED is set, SDL_SetWindowSize cannot change size.
		// if cannnot change size, will result handle_resize_screen dead loop. so don't consider size_not_equal(true).
		video_.sdl_set_window_size(width, height);

	}
	if (width == gui2::settings::screen_width && height == gui2::settings::screen_height) {
		if (enlarge_size) {
			// if enlarge, SDL_SetWindowSize will result "black edge" in enlarged area.
			// relayout window to erase "black edge".
			if (gui2::ttrack::global_did_drawing || events::progress_running) {
				gui2::invalidate_layout_all();
			} else {
				gui2::absolute_draw_all(true);
			}
		}
		return;
	}

	keyboard::set_visible(false);
	preferences::set_resolution(video_, width, height, true);

	gui2::settings::screen_width = width;
	gui2::settings::screen_height = height;

	std::vector<gui2::twindow*> windows = gui2::connectd_window();
	for (std::vector<gui2::twindow*>::const_reverse_iterator rit = windows.rbegin(); rit != windows.rend(); ++ rit) {
		gui2::twindow* window = *rit;
		window->dialog().resize_screen();
	}

	if (gui2::ttrack::global_did_drawing || events::progress_running) {
		gui2::invalidate_layout_all();

	} else {
		gui2::absolute_draw_all(true);
	}
}

surface icon2;
bool base_instance::init_video()
{
	applet::initial(applets_);
	{
		cache_.clear_defines();
		config cfg;
		cache_.get_config(game_config::path + "/data/settings.cfg", cfg);
		const config& rose_settings_cfg = cfg.child("settings");
		VALIDATE(!rose_settings_cfg.empty(), null_str);
		game_config::load_config(&rose_settings_cfg);

		cache_.clear_defines();
		cache_.get_config(game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/settings.cfg", cfg);
		const config& app_settings_cfg = cfg.child("settings");
		app_load_settings_config(app_settings_cfg? app_settings_cfg: config());
	}

	// when startup, rose must enable statusbar first. app require disable later by itself.
	SDL_EnableStatusBar(SDL_TRUE);

	uint32_t _startup_servers = 0;
	int default_font_size = nposm;
	VALIDATE(game_config::longpress_time == nposm, null_str);
	bool fullscreen = false;
	tpre_setmode_settings pre_settings(gui2::twidget::current_landscape, silent_background_, fullscreen, _startup_servers, default_font_size,
		game_config::longpress_time, game_config::min_width, game_config::min_height, server_check_port_);
	app_pre_setmode(pre_settings);
	VALIDATE(game_config::longpress_time == nposm || game_config::longpress_time > 0, null_str);
	VALIDATE(game_config::min_width >= MIN_WIN_WIDTH || game_config::min_height >= MIN_WIN_HEIGHT, null_str);
	font::update_7size(default_font_size);

	if (_startup_servers & server_httpd) {
		register_server(server_httpd, nullptr);
	}
	if (_startup_servers & server_rtspd) {
		register_server(server_rtspd, nullptr);
	}

	// So far not initialize font, don't call function related to the font. 
	// for example get_rendered_text/get_rendered_text_size.
	if (fullscreen) {
		set_fullscreen(fullscreen);
	}
	if (!set_orientation(nposm, true)) {
		return false;
	}
	
	{
		const std::string default_bg_image = "assets/LaunchImage-1242x2208.png";

		std::string bg_image = game_config::preferences_dir + "/images/misc/oem-logo.png";
		// render startup image. 
		// normally, app use this png only once, and it' size is large. to save memory, don't add to cache.
		surface surf = image::locator(bg_image).load_from_disk();
		if (!surf) {
			surf = image::locator(default_bg_image).load_from_disk();
			if (!surf) {
				return false;
			}
		}
		surf = makesure_neutral_surface(surf);
		const uint32_t fill_color = surf_calculate_most_color(surf);

		tpoint ratio_size = calculate_adaption_ratio_size(video_.getx(), video_.gety(), surf->w, surf->h);
		surface bg = create_neutral_surface(video_.getx(), video_.gety());
		if (ratio_size.x != video_.getx() || ratio_size.y != video_.gety()) {
			fill_surface(bg, fill_color);
		}
		surf = scale_surface(surf, ratio_size.x, ratio_size.y);
		SDL_Rect dstrect {(video_.getx() - ratio_size.x) / 2, (video_.gety() - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
		sdl_blit(surf, nullptr, bg, &dstrect);
		render_surface(get_renderer(), bg, nullptr, nullptr);
		video_.flip();
	}

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::version.str(true);
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());

#ifdef _WIN32
	icon2 = image::get_image("game_icon.png");
	if (icon2) {
		SDL_SetWindowIcon(video_.getWindow(), icon2);
	}
#endif

	return true;
}

#define BASENAME_DATA		"data.bin"

bool base_instance::load_data_bin()
{
	VALIDATE(!game_config::app.empty(), _("Must set game_config::app"));
	VALIDATE(!game_config::app_channel.empty(), _("Must set game_config::app_channel"));
	VALIDATE(core_cfg_.empty(), null_str);

	cache_.clear_defines();

	loadscreen::global_loadscreen_manager loadscreen_manager(video_);
	cursor::setter cur(cursor::WAIT);

	try {		
		wml_config_from_file(game_config::path + "/xwml/" + BASENAME_DATA, app_cfg_);
	
		// [terrain_type]
		const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
		BOOST_FOREACH (const config &t, terrains) {
			tmap::terrain_types.add_child("terrain_type", t);
		}
		app_cfg_.clear_children("terrain_type");

		// animation
		anim2::fill_anims(app_cfg_.child("units"));

		app_load_data_bin();

		// save this to core_cfg_
		core_cfg_ = app_cfg_;
		paths_manager_.set_paths(core_cfg_);

	} catch (game::error& e) {
		// ERR_CONFIG << "Error loading game configuration files";
		gui2::show_error_message(_("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		return false;
	}
	return true;
}

void base_instance::reload_data_bin(const config& data_cfg)
{
	// place execute to main thread.
	sdl_thread_.Invoke<void>(RTC_FROM_HERE, rtc::Bind(&base_instance::set_data_bin_cfg, this, data_cfg));
}

void base_instance::set_data_bin_cfg(const config& cfg)
{
	app_cfg_ = cfg;
	// [terrain_type]
	const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
	BOOST_FOREACH (const config &t, terrains) {
		tmap::terrain_types.add_child("terrain_type", t);
	}
	app_cfg_.clear_children("terrain_type");

	// save this to core_cfg_
	core_cfg_ = app_cfg_;

	// conitnue to [rose_config].
	const config& core_cfg = instance->core_cfg();
	const config& game_cfg = core_cfg.child("rose_config");
	game_config::load_config(game_cfg? &game_cfg : NULL);
}

bool base_instance::change_language()
{
	std::vector<std::string> items;

	const std::vector<language_def>& languages = get_languages();
	const language_def& current_language = get_language();
	
	int initial_sel = 0;
	BOOST_FOREACH (const language_def& lang, languages) {
		items.push_back(lang.language);
		if (lang == current_language) {
			initial_sel = items.size() - 1;
		}
	}

	int cursel = nposm;
	if (game_config::svga) {
		gui2::tcombo_box dlg(items, initial_sel, _("Language"), true);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK || !dlg.dirty()) {
			return false;
		}

		cursel = dlg.cursel();

	} else {
		gui2::tcombo_box2 dlg(_("Language"), items, initial_sel);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK || !dlg.dirty()) {
			return false;
		}

		cursel = dlg.cursel();
	}

	::set_language(languages[cursel]);
	preferences::set_language(languages[cursel].localename);

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::version.str(true);
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());
	return true;
}

int base_instance::show_preferences_dialog()
{
	if (game_config::mobile) {
		return gui2::twindow::OK;
	}

	std::vector<std::string> items;

	std::vector<int> values;
	int fullwindowed = preferences::fullscreen()? preferences::MAKE_WINDOWED: preferences::MAKE_FULLSCREEN;
	std::string str = preferences::fullscreen()? _("Exit fullscreen") : _("Enter fullscreen");
	items.push_back(str);
	values.push_back(fullwindowed);
	if (!preferences::fullscreen()) {
		items.push_back(_("Change Resolution"));
		values.push_back(preferences::CHANGE_RESOLUTION);
	}
	items.push_back(_("Close"));
	values.push_back(gui2::twindow::OK);

	gui2::tcombo_box dlg(items, nposm);
	dlg.show();
	if (dlg.cursel() == nposm) {
		return gui2::twindow::OK;
	}

	return values[dlg.cursel()];
}

void base_instance::pre_create_renderer()
{
	app_pre_create_renderer();
}

void base_instance::post_create_renderer()
{
	app_post_create_renderer();
}

void base_instance::fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg)
{
	if (area) {
		anims_.insert(std::make_pair(at, new animation(cfg)));
	} else {
		if (tpl) {
			utype_anim_tpls_.insert(std::make_pair(id, cfg));
		} else {
			anims_.insert(std::make_pair(at, new animation(cfg)));
		}
	}
}

void base_instance::clear_anims()
{
	utype_anim_tpls_.clear();

	for (std::map<int, animation*>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
		animation* anim = it->second;
		delete anim;
	}
	anims_.clear();
}

const animation* base_instance::anim(int at) const
{
	std::map<int, animation*>::const_iterator i = anims_.find(at);
	if (i != anims_.end()) {
		return i->second;
	}
	return NULL;
}

void base_instance::prefix_create(const std::string& app, const std::string& channel)
{
	std::stringstream err;
	// if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		err << "Couldn't initialize SDL: " << SDL_GetError();
		throw twml_exception(err.str());
	}

	srand((unsigned int)time(NULL));
	game_config::init(app, channel);
}

void base_instance::initialize()
{
	// why not call app_setup_user_data_dir from setup_user_data_dir?
	// --instance is null when setup_user_data_dir.
	app_setup_user_data_dir();

	// Followed operation maybe poupup messagebox, it require locale language message. so init internation first.
	init_locale();

	bool res = init_language();
	if (!res) {
		throw twml_exception("could not initialize the language");
	}

	// In order to reduce 'black screen' time, so as soon as possible to call init_video.
	res = init_video();
	if (!res) {
		throw twml_exception("could not initialize display");
	}

	lobby = create_lobby();
	if (!lobby) {
		throw twml_exception("could not create lobby");
	}

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if (!res) {
		throw twml_exception("could not initialize fonts");
	}

	cursor_manager_ = new cursor::manager;
	cursor::set(cursor::WAIT);

	// in order to 
	loadscreen_manager_ = new loadscreen::global_loadscreen_manager(video_);
	loadscreen::start_stage("titlescreen");

	res = gui2::init();
	if (!res) {
		throw twml_exception("could not initialize gui2-subsystem");
	}
	gui2_event_manager_ = new gui2::event::tmanager;

	// load core config require some time, so execute after gui2::init.
	res = load_data_bin();
	if (!res) {
		throw twml_exception("could not initialize game config");
	}

	// if you modified chinesepinyin.raw, want to generate chinesepinyin.code, enalbe it.
	// chinese::chinesepinyin_raw_2_code();

	chinese::load_pinyin_code();

	app_load_pb();

	lua_  = new rose_lua_kernel;

	std::pair<std::string, std::string> desire_theme = utils::split_app_prefix_id(preferences::theme());
	theme_switch_to(desire_theme.first, desire_theme.second);
}

void base_instance::uninitialize()
{
	SDL_Log("base_instance::uninitialize()---");
	VALIDATE(instance != nullptr, null_str);

	std::set<int> flags;
	for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
		flags.insert(it->first);
	}
	for (std::set<int>::const_reverse_iterator it = flags.rbegin(); it != flags.rend(); ++ it) {
		unregister_server(*it);
	}
	VALIDATE(servers_.empty(), null_str);

	// 1. info all invoker, I will uninitialize.
	send_helper_.clear_msg();

	preferences::write_preferences();

	// in reverse order compaire to initialize 
	if (gui2_event_manager_) {
		delete gui2_event_manager_;
		gui2_event_manager_ = NULL;
	}

	if (loadscreen_manager_) {
		delete loadscreen_manager_;
		loadscreen_manager_ = NULL;
	}

	if (cursor_manager_) {
		delete cursor_manager_;
		cursor_manager_ = NULL;
	}

	if (lobby) {
		delete lobby;
		lobby = nullptr;
	}

	if (lua_ != nullptr) {
		delete lua_;
		lua_ = nullptr;
	}

	gui2::release();
	SDL_Log("---base_instance::uninitialize() X");
}

void base_instance::theme_switch_to(const std::string& app, const std::string& id)
{
	if (!theme::current_id.empty() && theme::current_id == utils::generate_app_prefix_id(app, id)) {
		return;
	}

	const config* default_theme_cfg = nullptr;
	const config* theme_cfg = nullptr;
	BOOST_FOREACH(const config& cfg, core_cfg_.child_range("theme")) {
		if (cfg["app"].str() == app && cfg["id"].str() == id) {
			theme_cfg = &cfg;
			break;
		} else if (cfg["app"].str() == "rose" && cfg["id"].str() == "default") {
			default_theme_cfg = &cfg;
		}
	}
	VALIDATE(theme_cfg != nullptr || default_theme_cfg != nullptr, null_str);
	theme::switch_to(theme_cfg? *theme_cfg: *default_theme_cfg);

	preferences::set_theme(theme::current_id);
}

void base_instance::will_longblock()
{
	// app require can block >= game_config::min_longblock_threshold_s second.
	// if cannot, modify game_config::min_longblock_threshold_s to your capability.
	VALIDATE_IN_MAIN_THREAD();
	app_will_longblock();
}

void base_instance::handle_http_request(const net::HttpServerRequestInfo& info, net::tresponse_data& resp)
{
	app_handle_http_request(info, resp);
}

void base_instance::register_server(int flag, tserver_* _server)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(flag != 0 && servers_.count(flag) == 0, std::string("flag: ") + str_cast(flag));

	tserver_* server = _server;
	if (_server == nullptr) {
		if (flag == server_httpd) {
			server = &httpd_mgr_;
		} else if (flag == server_rtspd) {
			server = &live555d_mgr_;
		}
	}
	VALIDATE(server != nullptr, null_str);
	servers_.insert(std::make_pair(flag, server));

	VALIDATE(!server->started(), null_str);

	// all start during check_ip, immediate start.
	next_check_ip_ticks_ = SDL_GetTicks();
}

void base_instance::unregister_server(int flag)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(flag != 0, null_str);
	std::map<int, tserver_*>::iterator it = servers_.find(flag);
	VALIDATE(it != servers_.end(), null_str);

	tserver_* server = it->second;
	servers_.erase(it);

	if (server->started()) {
		tdisable_check_ip_lock lock(*this);
		server->stop();
	}
}

bool base_instance::start_servers_internal(uint32_t ipaddr)
{
	VALIDATE(!servers_.empty(), null_str);
	VALIDATE(ipaddr == ReceivingInterfaceAddr && ipaddr != INADDR_ANY, null_str);

	// start httpd before live555d, because live55d require httpd.
	for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
		tserver_* server = it->second;
		if (!server->started()) {
			server->start(ipaddr);
			if (!server->started()) {
				server->stop();
			}
		}
	}
	return true;
}

void base_instance::stop_servers_internal()
{
	VALIDATE(!servers_.empty(), null_str);

	tdisable_check_ip_lock lock(*this);
	for (std::map<int, tserver_*>::const_reverse_iterator it = servers_.rbegin(); it != servers_.rend(); ++ it) {
		tserver_* server = it->second;
		if (server->started()) {
			server->stop();
		}
	}
	ReceivingInterfaceAddr = INADDR_ANY;
}

bool base_instance::server_registered(int flag) const
{
	VALIDATE(flag != 0, null_str);
	return servers_.count(flag) != 0;
}

bool base_instance::servers_ready() const
{
	return ReceivingInterfaceAddr != INADDR_ANY;
}

void base_instance::set_sdl_reboothandler(SDL_RebootHandler handler, void* param)
{
	sdl_apphandlers_.reboot_handler = handler;
	sdl_apphandlers_.reboot_param = param;
	SDL_SetAppHandlers(&sdl_apphandlers_);
}

bool base_instance::invalidate_layout(bool clear)
{
	bool result = invalidate_layout_;
	if (result && clear) {
		invalidate_layout_ = false;
	}
	return result;
}

tpoint base_instance::get_landscape_size() const
{
	tpoint result(0, 0);
	if (game_config::os == os_ios || game_config::os == os_android) {
		// width/height from preferences.cfg must be landscape.
		SDL_Rect rc = video_.bound();

		SDL_Log("init_video, video_.bound(): (%i, %i, %i, %i), landscape: %s", rc.x, rc.y, rc.w, rc.h, gui2::twidget::current_landscape? "true": "false");

		if (rc.w < rc.h) {
			int tmp = rc.w;
			rc.w = rc.h;
			rc.h = tmp;
		}

		if (game_config::os == os_ios) {
			rc.w *= gui2::twidget::hdpi_scale;
			rc.h *= gui2::twidget::hdpi_scale;
		}

		if (!gui2::twidget::should_conside_orientation(rc.w, rc.h)) {
			// for iOS/Android, it is get screen's width/height first.
			// rule: if should_conside_orientation() is false, current_landscape always must be true.
			// gui2::twidget::current_landscape = true;
		}
		// tpoint normal_size = gui2::twidget::orientation_swap_size(rc.w, rc.h);
		result.x = rc.w;
		result.y = rc.h;

	} else {
		result = preferences::landscape_size(video_);
	}

	return result;
}

void base_instance::set_fullscreen(bool fullscreen)
{
	VALIDATE(game_config::fullscreen != fullscreen, null_str);
	SDL_SetFullscreen(fullscreen? SDL_TRUE: SDL_FALSE);
	game_config::fullscreen = fullscreen;

	// game_config::statusbar_height = game_config::fullscreen? 0: SDL_GetStatusBarHeight();
}

bool base_instance::set_orientation(int orientation, bool with_mode)
{
	VALIDATE(gui2::connectd_window().empty(), null_str);
	bool landscape = gui2::twidget::current_landscape;
	if (orientation != nposm) {
		VALIDATE(orientation == orientation_portrait || orientation == orientation_landscape, null_str);
		landscape = orientation == orientation_landscape;

		VALIDATE(landscape != gui2::twidget::current_landscape, null_str);
		gui2::twidget::current_landscape = landscape;
	}
	SDL_SetOrientation(landscape? SDL_TRUE: SDL_FALSE);

	bool result = true;
	if (with_mode) {
		result = set_mode();
	}
	return result;
}

bool base_instance::set_mode()
{
	int video_flags = 0;
	if (game_config::os == os_windows) {
		video_flags = preferences::fullscreen()? SDL_WINDOW_FULLSCREEN: 0;

	} else if (game_config::os == os_ios) {
		// on ios, use SDL_WINDOW_FULLSCREEN, and must not set SDL_WINDOW_BORDERLESS.
		video_flags = game_config::fullscreen? SDL_WINDOW_FULLSCREEN: 0;

	} else if (game_config::os == os_android) {
		// base_instance::set_fullscreen is responsible for managing the full screen, where must not set SDL_WINDOW_FULLSCREEN flag.
	}

	const tpoint landscape_size = get_landscape_size();
	const tpoint resolution = gui2::twidget::orientation_swap_size(landscape_size.x, landscape_size.y);

	SDL_Log("set_mode, setting mode to %ix%ix32, landscape: %s", 
		resolution.x, resolution.y, gui2::twidget::current_landscape? "true": "false");
	if (!video_.setMode(resolution.x, resolution.y, video_flags)) {
		SDL_Log("set_mode, required video mode, %i x %i x32 is not supported", resolution.x, resolution.y);
		return false;
	}
	SDL_Log("set_mode, using mode %ix%ix32", video_.getx(), video_.gety());
	return true;
}

tlobby* base_instance::create_lobby()
{
	return new tlobby(new tlobby::tchat_sock(), new tlobby::thttp_sock(), new tlobby::ttransit_sock());
}

#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/dns/host_resolver.h"
#include <net/socket/client_socket_factory.h>
#include <net/socket/stream_socket.h>
#include "net/socket/tcp_server_socket.h"
#include "net/base/net_errors.h"

bool check_ip_valid(uint32_t ip, uint16_t check_port)
{
	VALIDATE(check_port != 0, null_str);
	// check this ip is valid or not.

	// If in window and use WLAN. require disable WLAN adapter. 
	// if disconect from all ap, but enable WLAN adapter, ListenWithAddressAndPort still return net::OK.
	// In order to improve the accuracy, this method needs to be revised in the future.
/*
	net::IPAddress address((const uint8_t*)&ip, 4);
	net::AddressList addresses_(net::IPEndPoint(address, 6554));

	net::NetLogWithSource net_log2_;
	net::ClientSocketFactory* const socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
	std::unique_ptr<net::StreamSocket> socket = socket_factory_->CreateTransportClientSocket(addresses_, nullptr, net_log2_.net_log(), net_log2_.source());
*/
	uint32_t addrNBO = htonl(ip);
	std::stringstream addr_ss;
	addr_ss << (int)((addrNBO >> 24) & 0xFF) << "." << (int)((addrNBO>>16) & 0xFF);
	addr_ss << "." << (int)((addrNBO>>8)&0xFF) << "." << (int)(addrNBO & 0xFF);

	std::unique_ptr<net::ServerSocket> server_socket(new net::TCPServerSocket(NULL, net::NetLogSource()));
	int ret = server_socket->ListenWithAddressAndPort(addr_ss.str(), check_port, 1);
	if (ret != net::OK) {
		SDL_Log("check_ip, ListenWithAddressAndPort(%s, %i), ret: %i", addr_ss.str().c_str(), check_port, ret);
	}

	return ret == net::OK;
}

void base_instance::make_ip_invalid()
{
	SDL_Log("base_instance::make_ip_invalid, original ip_require_invalid_: %s", ip_require_invalid_? "true": "false");
	ip_require_invalid_ = true;
}

uint32_t base_instance::current_ip() const
{ 
	return ReceivingInterfaceAddr; 
}

void base_instance::check_ip_slice(bool slice)
{
	if (disable_check_ip_ || servers_.empty()) {
		return;
	}
	const uint32_t now = SDL_GetTicks();
	if (now >= next_check_ip_ticks_) {
		SDL_Log("%u [%s]check_ip, now is in %s, check... [%s]", now, foreground_? "foreground": "background", 
			ReceivingInterfaceAddr == INADDR_ANY? "no-ip": "has-ip", slice? "slice": "normal");

		if (ReceivingInterfaceAddr == INADDR_ANY) {
			// now think no valid ip
			ReceivingInterfaceAddr = get_local_ipaddr();
			if (ReceivingInterfaceAddr != INADDR_ANY) {
				SDL_Log("check_ip, thie time get valid ipaddr: 0x%08x", ReceivingInterfaceAddr);
				start_servers_internal(ReceivingInterfaceAddr);
			} else {
				for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
					const tserver_* server = it->second;
					VALIDATE(!server->started(), null_str);
				}
			}
		} else {
			// now think valid ip
			bool ok = false;
			if (!ip_require_invalid_) {
				ok = check_ip_valid(ReceivingInterfaceAddr, server_check_port_);

			} else {
				SDL_Log("check_ip, 0x%08x, ip_require_invalid_ is true, stop servers", ReceivingInterfaceAddr);
				ip_require_invalid_ = false;
			}
			
			if (!ok) {
				stop_servers_internal();
			} else {
				// there is maybe some server require it start.
				start_servers_internal(ReceivingInterfaceAddr);
			}
		}
		next_check_ip_ticks_ = SDL_GetTicks() + check_ip_threshold_;
	}
}

void base_instance::pump(bool slice)
{
	check_ip_slice(slice);
	// if (!slice) {
		sdl_thread_.pump();
		lobby->pump();
	// }
	if (ble_ != nullptr) {
		ble_->pump();
	}
}