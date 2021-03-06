/* $Id: button.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_BUTTON_HPP_INCLUDED
#define GUI_WIDGETS_BUTTON_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

/**
 * Simple push button.
 */
class tbutton
	: public tcontrol
{
public:
	tbutton();

	/**
	 * Possible sort mode of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tsort { NONE, ASCEND, DESCEND, TOGGLE };

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void load_config_extra() override;

	void set_active(const bool active) override;

	/** Inherited from tcontrol. */
	bool get_active() const override { return state_ != DISABLED; }

	/** Inherited from tcontrol. */
	unsigned get_state() const override { return state_; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_retval(const int retval) { retval_ = retval; }

	void set_sort(tsort sort);
	int get_sort() const;

	void disable_change_keyboard_focus() { change_keyboard_focus_ = false; }

private:
	std::string mini_default_border() const override { return "button"; }
	bool captured_mouse_can_to_scroll_container() const override { return true; }

private:
	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, PRESSED, FOCUSSED, COUNT };

	void set_state(const tstate state);
	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/**
	 * Current sort mode of the widget.
	 *
	 * The sort mode of the widget determines what to sort and how the widget
	 * reacts to certain 'events'.
	 */
	tsort sort_;

	bool change_keyboard_focus_;

	/**
	 * The return value of the button.
	 *
	 * If this value is not 0 and the button is clicked it sets the retval of
	 * the window and the window closes itself.
	 */
	int retval_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const override;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_mouse_enter(const event::tevent event, bool& handled);

	void signal_handler_mouse_leave(const event::tevent event, bool& handled);

	void signal_handler_left_button_down(
			const event::tevent event, bool& handled);

	void signal_handler_left_button_up(
			const event::tevent event, bool& handled);

	void signal_handler_left_button_click(
			const event::tevent event, bool& handled);
};

tbutton* create_button(const config& cfg);
tbutton* create_button(const std::string& id, const std::string& definition, const std::string& label);
tbutton* create_blits_button(const std::string& id);
} // namespace gui2

#endif

