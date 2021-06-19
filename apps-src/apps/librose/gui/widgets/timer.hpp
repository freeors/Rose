/* $Id: timer.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

/**
 * @file
 * Contains the gui2 timer routines.
 *
 * This code avoids the following problems with the sdl timers:
 * - the callback must be a C function with a fixed signature.
 * - the callback needs to push an event in the event queue, between the
 *   pushing and execution of that event the timer can't be stopped. (Makes
 *   sense since the timer has expired, but not what the user wants.)
 *
 * With these functions it's possible to remove the event between pushing in
 * the queue and the actual execution. Since the callback is a std::function
 * object it's possible to make the callback as fancy as wanted.
 */

#ifndef GUI_WIDGETS_TIMER_HPP_INCLUDED
#define GUI_WIDGETS_TIMER_HPP_INCLUDED

#include <SDL_types.h>
#include <SDL_timer.h>
#include <map>
#include <functional>

#define INVALID_TIMER_ID		0
namespace gui2 {

class twindow;

class ttimer
{
	friend twindow;

public:
	ttimer()
		: id(INVALID_TIMER_ID)
		, executing_(false)
		, next_execute_ticks(0)
		, window_(nullptr)
		, execute_now(false)
	{}
	~ttimer()
	{
		reset();
	}

	/**
	 * Adds a new timer.
	 *
	 * @param interval                The timer interval in ms.
	 * @param callback                The function to call when the timer expires,
	 *                                the id send as parameter is the id of the
	 *                                timer.
	 *
	 */
	void reset(const Uint32 interval, twindow& window, const std::function<void(size_t id)>& callback);
	void reset();
	bool valid() const { return id != INVALID_TIMER_ID; }

private:
	void execute();

public:
	class texecutor
	{
	public:
		texecutor(ttimer& timer, size_t id);
		~texecutor();

	private:
		ttimer& timer_;	
	};

	static size_t sid;
	size_t id;
	bool executing_;

	uint32_t next_execute_ticks;
	uint32_t interval_; // ms
	std::function<void(size_t id)> callback_;
	const twindow* window_;
	bool execute_now;
};


} //namespace gui2

#endif

