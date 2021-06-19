/* $Id: timer.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/widgets/timer.hpp"
#include "gui/widgets/window.hpp"

#include "events.hpp"
#include "wml_exception.hpp"

#include <SDL_timer.h>
#include <map>

namespace gui2 {

/** The active timers. */
static std::map<size_t, ttimer*> timers;

/** The id of the event being executed, 0 if none. */
static size_t executing_id = 0;

/** Did somebody try to remove the timer during its execution? */
static bool executing_id_removed = false;

/**
 * Helper to make removing a timer in a callback safe.
 *
 * Upon creation it sets the executing id and clears the remove request flag.
 *
 * If an remove_timer() is called for the id being executed it requests a
 * remove the timer and exits remove_timer().
 *
 * Upon destruction it tests whether there was a request to remove the id and
 * does so. It also clears the executing id. It leaves the remove request flag
 * since the execution function needs to know whether or not the event was
 * removed.
 */
ttimer::texecutor::texecutor(ttimer& timer, size_t id)
	: timer_(timer)
{
	VALIDATE(!timer_.executing_, null_str);
	timer_.executing_ = true;

	executing_id = id;
	executing_id_removed = false;
}

ttimer::texecutor::~texecutor()
{
	const size_t id = executing_id;
	executing_id = 0;
	if (executing_id_removed) {
		std::map<size_t, ttimer*>::iterator itor = timers.find(id);
		VALIDATE(itor != timers.end(), null_str);
		itor->second->reset();
	}

	timer_.executing_ = false;
}

size_t ttimer::sid = 0;

void ttimer::reset()
{
	if (id == INVALID_TIMER_ID) {
		return;
	}

	std::map<size_t, ttimer*>::iterator itor = timers.find(id);
	if (itor == timers.end()) {
		// Can't remove timer since it no longer exists.
		return;
	}
	VALIDATE(itor->second == this, null_str);

	if (id == executing_id) {
		executing_id_removed = true;
		return;
	}

	id = INVALID_TIMER_ID;
	timers.erase(itor);
}

void ttimer::reset(const Uint32 interval, twindow& window, const std::function<void(size_t id)>& callback)
{
	VALIDATE(interval > 0, null_str);
	reset();

	do {
		++ sid;
	} while (sid == 0 || timers.find(sid) != timers.end());

	id = sid;
	std::pair<std::map<size_t, ttimer*>::iterator, bool> ins = timers.insert(std::make_pair(sid, this));

	interval_ = interval;
	next_execute_ticks = SDL_GetTicks() + interval_;
	callback_ = callback;
	window_ = &window;
}

void ttimer::execute()
{
	VALIDATE(valid(), null_str);
	if (executing_) {
		// avoid reenter.
		return;
	}

	texecutor executor(*this, id);
	callback_(id);
}

void twindow::remove_timer_during_destructor()
{
	while (!timers.empty()) {
		bool has_erase = false;
		for (std::map<size_t, ttimer*>::iterator it = timers.begin(); it != timers.end() && !has_erase; ) {
			ttimer& timer = *it->second;
			if (timer.window_ != this) {
				++ it;
				continue;
			}
			VALIDATE(timer.id != executing_id, "removed timer during destructor must not be in executing.");

			timer.reset();
			has_erase = true;
		}
		if (!has_erase) {
			break;
		}
	}
}

void twindow::pump_timers() const
{
	uint32_t now = SDL_GetTicks();
	size_t original = timers.size();
	for (std::map<size_t, ttimer*>::iterator it = timers.begin(); it != timers.end(); ++ it) {
		ttimer& timer = *it->second;
		const size_t timer_id = timer.id;
		if (timer.window_ != this) {
			continue;
		}
		if (!timer.execute_now && now < timer.next_execute_ticks) {
			continue;
		}
		timer.execute();

		// reset execute_now flag.
		timer.execute_now = false;

		now = SDL_GetTicks();
		if (timers.count(timer_id)) {
			// this timer maybe remove. if not, update next_execute_ticks.
			timer.next_execute_ticks = now + timer.interval_;
		}
		if (timers.size() != original) {
			// if some timer insert or erase, terminate this loop.
			break;
		}
	}
}

} //namespace gui2

