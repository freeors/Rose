/* $Id: cursor.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef CURSOR_HPP_INCLUDED
#define CURSOR_HPP_INCLUDED

#include "SDL.h"

namespace cursor
{

struct manager
{
	manager();
	~manager();
};

enum CURSOR_TYPE { NORMAL, WAIT, MOVE, ATTACK, TACTIC, BUILD, ENTER, ILLEGAL, PLACE, INTERIOR, TECHNOLOGY_TREE, HYPERLINK,
	MOVE_DRAG, ATTACK_DRAG, TACTIC_DRAG, BUILD_DRAG, ENTER_DRAG, ILLEGAL_DRAG, PLACE_DRAG, INTERIOR_DRAG, TECHNOLOGY_TREE_DRAG, NO_CURSOR, NUM_CURSORS };

/**
 * Use the default parameter to reset cursors.
 * e.g. after a change in color cursor preferences
 */
void set(CURSOR_TYPE type = NUM_CURSORS);
void set_dragging(bool drag);
CURSOR_TYPE get();

void draw();
void undraw();

void set_focus(bool focus);

struct setter
{
	setter(CURSOR_TYPE type);
	~setter();

private:
	CURSOR_TYPE old_;
};

} // end namespace cursor

#endif
