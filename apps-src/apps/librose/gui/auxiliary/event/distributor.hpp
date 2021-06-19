/* $Id: distributor.hpp 54413 2012-06-15 18:30:47Z mordante $ */
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

#ifndef GUI_WIDGETS_AUXILIARY_EVENT_DISTRIBUTOR_HPP_INCLUDED
#define GUI_WIDGETS_AUXILIARY_EVENT_DISTRIBUTOR_HPP_INCLUDED

/**
 * @file
 * Contains the event distributor.
 *
 * The event distributor exists of several classes which are combined in one
 * templated tdistributor class. The classes are closly tight together.
 *
 * All classes have direct access to eachothers members since they should act
 * as one. (Since the buttons are a templated subclass it's not possible to use
 * private subclasses.)
 *
 * The tmouse_motion class handles the mouse motion and holds the owner of us
 * since all classes virtually inherit us.
 *
 * The tmouse_button classes are templated classes per mouse button, the
 * template parameters are used to make the difference between the mouse
 * buttons. Althought it's easily possible to add more mouse buttons in the
 * code several places only expect a left, middle and right button.
 *
 * tdistributor is the main class to be used in the user code. This class
 * contains the handling of the keyboard as well.
 */

#include "gui/auxiliary/event/dispatcher.hpp"
#include "gui/widgets/event_executor.hpp"
#include "gui/widgets/timer.hpp"
#include "util.hpp"

namespace gui2{

class twidget;
class tcontrol;

namespace event {

/** The event handler class for the widget library. */
class tdistributor
{
	friend class thandler;

	/***** ***** ***** ***** tmouse_button ***** ***** ***** ***** *****/
	class tmouse_button
	{
		friend class thandler;
		friend class tdistributor;

	public:
		tmouse_button(const tevent _button_down, const tevent _button_up, const tevent _button_click, const tevent _button_double_click,
			tdistributor& distributor, const int button, twidget& owner, const tdispatcher::tposition queue_position);
		~tmouse_button();

	public:
		void signal_handler_sdl_button_down(
			  bool& handled
			, const tpoint& coordinate);

		void signal_handler_sdl_button_up(
			  bool& handled
			, const tpoint& coordinate);

	private:
		void mouse_button_click(twidget& widget);

	private:
		tdistributor& distributor_;

		/** The widget the last click was on, used for double clicking. */
		twidget* last_clicked_widget_;

		/** used for debug messages. */
		const int button_;

		tpoint down_coordinate_;

		/** The time of the last click used for double clicking. */
		Uint32 last_click_stamp_;

		int times_;

		const tevent button_down;
		const tevent button_up;
		const tevent button_click;
		const tevent button_double_click;
	};

	/***** ***** ***** ***** tdistributor ***** ***** ***** ***** *****/
public:
	tdistributor(twidget& owner, const tdispatcher::tposition queue_position);
	~tdistributor();

	tmouse_button& mouse_left() { return mouse_left_; }
	tmouse_button& mouse_middle() { return mouse_middle_; }
	tmouse_button& mouse_right() { return mouse_right_; }

	void new_window_connected();

	/**
	 * Captures the keyboard input.
	 *
	 * @param widget              The widget which should capture the keyboard.
	 *                            Sending NULL releases the capturing.
	 */
	void keyboard_capture(twidget* widget);
	twidget* keyboard_capture_widget() const { return keyboard_focus_; }

	/**
	 * Captures the mouse input.
	 *
	 * When capturing the widget that has the mouse focus_ does the capturing.
	 *
	 * @param capture             Set or release the capturing.
	 */
	void capture_mouse(twidget* widget);

	twidget* mouse_captured_widget() const;

	void set_out_of_chain_widget(twidget* widget);

	/**
	 * Initializes the state of the button.
	 *
	 * @param is_down             The initial state of the button, if true down
	 *                            else initialized as up.
	 */
	twidget* mouse_click_widget() const { return mouse_click_; }
	void clear_mouse_click();

private:
	/**
	 * Starts the hover timer.
	 *
	 * @param widget                 The widget that wants the tooltip.
	 * @param coordinate             The anchor coordinate.
	 */
	void start_hover_timer(twidget* widget, const tpoint& coordinate);

	/** Stops the current hover timer. */
	void stop_hover_timer();

	/**
	 * Called when the mouse enters a widget.
	 *
	 * @param mouse_over          The widget that should receive the event.
	 */
	void mouse_enter(twidget* mouse_over, const tpoint& coordinate);

	/** Called when the mouse leaves the current widget. */
	void mouse_leave(const tpoint& coordinate, const tpoint& coordinate2);

	/**
	 * Called when the mouse moves over a widget.
	 *
	 * @param mouse_over          The widget that should receive the event.
	 * @param coordinate          The current screen coordinate of the mouse.
	 */
	void mouse_motion(twidget* mouse_over, const tpoint& coordinate, const tpoint& coordinate2);

	/** Called when the mouse wants the widget to show its tooltip. */
	void show_tooltip();

	bool signal_handler_sdl_mouse_motion_entered_;
	void signal_handler_sdl_mouse_motion(const tpoint& coordinate, const tpoint& coordinate2);

	void signal_handler_sdl_wheel(
			  const event::tevent event
			, bool& handled
			, const tpoint& coordinate
			, const tpoint& coordinate2);

	void signal_handler_sdl_longpress(const tpoint& coordinate);

	/**
	 * Set of functions that handle certain events and sends them to the proper
	 * widget. These functions are called by the SDL event handling functions.
	 */

	void signal_handler_sdl_key_down(const SDL_Keycode key
			, const SDL_Keymod modifier
			, const Uint16 unicode);

	void signal_handler_sdl_text_input(const char* text);

	void signal_handler_notify_removal(tdispatcher& widget, const tevent event);

	void post_mouse_up();

	bool is_out_of_chain_widget(const twidget& from) const;
private:
	/** The widget that owns us. */
	twidget& owner_;

	bool hover_pending_;			   /**< Is there a hover event pending? */
	unsigned hover_id_;                /**< Id of the pending hover event. */
	SDL_Rect hover_box_;               /**< The area the mouse can move in,
										*   moving outside invalidates the
										*   pending hover event.
										*/

	bool had_hover_;                   /**< A widget only gets one hover event
	                                    *   per enter cycle.
										*/

	twidget* out_of_chain_widget_;

	/** The widget that holds the keyboard focus_. */
	twidget* keyboard_focus_;

	tmouse_button mouse_left_;
	tmouse_button mouse_middle_;
	tmouse_button mouse_right_;

	/**
	 * 1)The widget that currently has the mouse focus. 
	 * 2)if mouse_captured_ is true, the widget that mouse event is captured to. When mouse_captured_ is true, mouse_focus_ must not be nullptr. 
	*/
	twidget* mouse_focus_;

	/**
	 * If the mouse isn't captured we need to verify the up is on the same
	 * widget as the down so we send a proper click, also needed to send the
	 * up to the right widget.
	*/
	twidget* mouse_click_;

	/** Did the current widget capture the focus_? */
	bool mouse_captured_;

	/** The timer for the hover event. */
	ttimer hover_timer_;

	/** The widget which should get the hover event. */
	twidget* hover_widget_;

	/** The anchor point of the hover event. */
	tpoint hover_position_;

	/**
	 * Has the hover been shown for the widget?
	 *
	 * A widget won't get a second hover event after the tooltip has been
	 * triggered. Only after (shortly) entering another widget it will be shown
	 * again for this widget.
	 */
	bool hover_shown_;

	int enter_leave_;
};

} // namespace event

} // namespace gui2

#endif
