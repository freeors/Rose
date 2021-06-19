/* $Id: video.hpp 47615 2010-11-21 13:57:02Z mordante $ */
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
#ifndef VIDEO_HPP_INCLUDED
#define VIDEO_HPP_INCLUDED

#include "events.hpp"
#include "exceptions.hpp"
#include "lua_jailbreak_exception.hpp"

#include <boost/utility.hpp>

struct surface;
struct texture;

texture& get_screen_texture();
texture& get_white_texture();
int frameTexture_width();
int frameTexture_height();
SDL_Rect frameTexture_rect();
SDL_Window* get_sdl_window();
const SDL_PixelFormat& get_screen_format();
SDL_Point SDL_GetNotchSizeRose();

class CVideo : private boost::noncopyable 
{
public:
	CVideo();
	~CVideo();

	void create_frameTexture(int w, int h, bool reset_zoom);
	bool setMode(int x, int y, int flags);

	//functions to get the dimensions of the current video-mode
	int getx() const;
	int gety() const;
	SDL_Rect bound() const;

	void sdl_set_window_size(int width, int height);
	void flip();

	texture& getTexture();
	SDL_Window* getWindow();
	const SDL_PixelFormat& getformat() const;

	bool isFullScreen() const;
	void set_force_render_present();

	struct error : public game::error
	{
		error() : game::error("Video initialization failed") {}
	};

	class quit
		: public lua_jailbreak_exception
	{
	public:

		quit()
			: lua_jailbreak_exception()
		{
		}

	private:

		IMPLEMENT_LUA_JAILBREAK_EXCEPTION(quit)
	};

	//function to stop the screen being redrawn. Anything that happens while
	//the update is locked will be hidden from the user's view.
	//note that this function is re-entrant, meaning that if lock_updates(true)
	//is called twice, lock_updates(false) must be called twice to unlock
	//updates.
	void lock_updates(bool value);
	bool update_locked() const;

private:
	void initSDL();

private:
	int updatesLocked_;
	uint32_t force_render_present_;
};

//an object which will lock the display for the duration of its lifetime.
struct update_locker
{
	update_locker(CVideo& v, bool lock=true) : video(v), unlock(lock) {
		if(lock) {
			video.lock_updates(true);
		}
	}

	~update_locker() {
		unlock_update();
	}

	void unlock_update() {
		if(unlock) {
			video.lock_updates(false);
			unlock = false;
		}
	}

private:
	CVideo& video;
	bool unlock;
};

#endif
