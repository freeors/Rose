/* $Id: helper.hpp 54585 2012-07-05 18:33:52Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_HELPER_HPP_INCLUDED
#define GUI_WIDGETS_HELPER_HPP_INCLUDED

#include "global.hpp"

#include "sdl_utils.hpp"

#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>

#include <string>

struct surface;
class t_string;

namespace game_logic {
class map_formula_callable;
} // namespace game_logic

namespace gui2 {

/**
 * Initializes the gui subsystems.
 *
 * This function needs to be called before other parts of the gui engine are
 * used.
 */
bool init();

void release();

/**
 * Creates a rectangle.
 *
 * @param origin                  The top left corner.
 * @param size                    The width (x) and height (y).
 *
 * @returns                       SDL_Rect with the proper rectangle.
 */
SDL_Rect create_rect(const tpoint& origin, const tpoint& size);


/**
 * Converts a font style string to a font style.
 *
 * @param style                   A font style string see
 *                                /wiki/GUIVariable for
 *                                more info.
 *
 * @returns                       The font style.
 */
unsigned decode_font_style(const std::string& style);

/**
 * Returns a default error message if a mandatory widget is omitted.
 *
 * @param id                      The id of the omitted widget.
 * @returns                       The error message.
 */
t_string missing_widget(const std::string& id);

/**
 * Gets a formula object with the screen size.
 *
 * @param variable                A formula object in which the screen_width,
 *                                screen_height, gamemap_width and
 *                                gamemap_height variable will set to the
 *                                current values of these in settings. It
 *                                modifies the object send.
 */
void get_screen_size_variables(game_logic::map_formula_callable& variable);

/**
 * Gets a formula object with the screen size.
 *
 * @returns                       Formula object with the screen_width,
 *                                screen_height, gamemap_width and
 *                                gamemap_height variable set to the current
 *                                values of these in settings.
 */
game_logic::map_formula_callable get_screen_size_variables();

/** Returns the current mouse position. */
tpoint get_mouse_position();

/**
 * Helper for function wrappers.
 *
 * For boost bind the a function sometimes needs to return a value althought
 * the function called doesn't return one. This wrapper function can return a
 * fixed result for a certain functor.
 *
 * @tparam R                      The return type.
 * @tparam F                      The type of the functor.
 *
 * @param result                  The result value.
 * @param function                The functor to call.
 *
 * @returns                       result.
 */
template<class R, class F>
R function_wrapper(const R result, const F& function)
{
	function();
	return result;
}

} // namespace gui2

#endif
