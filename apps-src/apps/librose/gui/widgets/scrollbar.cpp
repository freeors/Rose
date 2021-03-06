/* $Id: scrollbar.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/scrollbar.hpp"

#include "gui/widgets/window.hpp" // Needed for invalidate_layout()

using namespace std::placeholders;
#include <boost/foreach.hpp>

namespace gui2 {

tscrollbar_::tscrollbar_(const bool unrestricted_drag)
	: tcontrol(COUNT)
	, unrestricted_drag_(unrestricted_drag)
	, state_(ENABLED)
	, item_count_(0)
	, item_position_(0)
	, visible_items_(1)
	, pixels_per_step_(0.0)
	, mouse_(0, 0)
	, positioner_offset_(0)
	, positioner_length_(0)
{
	connect_signal<event::MOUSE_ENTER>(std::bind(
				&tscrollbar_::signal_handler_mouse_enter, this, _2, _3, _4));
	connect_signal<event::MOUSE_MOTION>(std::bind(
				  &tscrollbar_::signal_handler_mouse_motion
				, this
				, _2
				, _3
				, _4
				, _5));
	connect_signal<event::MOUSE_LEAVE>(std::bind(
				&tscrollbar_::signal_handler_mouse_leave, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_DOWN>(std::bind(
				&tscrollbar_::signal_handler_left_button_down, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_UP>(std::bind(
				&tscrollbar_::signal_handler_left_button_up, this, _2, _3));
}

void tscrollbar_::scroll(const tscroll scroll)
{
	switch(scroll) {
		case BEGIN :
			set_item_position(0);
			break;

		case ITEM_BACKWARDS :
			if(item_position_) {
				set_item_position(item_position_ - 1);
			}
			break;

		case HALF_JUMP_BACKWARDS :
			set_item_position(item_position_ > (visible_items_ / 3) ?
				item_position_ - (visible_items_ / 3) : 0);
			break;

		case JUMP_BACKWARDS :
			set_item_position(item_position_ > visible_items_ ?
				item_position_ - visible_items_  : 0);
			break;

		case END :
			set_item_position(item_count_ - 1);
			break;

		case ITEM_FORWARD :
			set_item_position(item_position_ + 1);
			break;

		case HALF_JUMP_FORWARD :
			set_item_position(item_position_ +  (visible_items_ / 3));
			break;

		case JUMP_FORWARD :
			set_item_position(item_position_ +  visible_items_ );
			break;

		default :
			VALIDATE(false, null_str);
		}
}

void tscrollbar_::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tcontrol::place(origin, size);

	recalculate();
}

void tscrollbar_::set_item_position(const int item_position, const bool force_restrict)
{
	// Set the value always execute since we update a part of the state.
	if (!unrestricted_drag_ || force_restrict) {
		if (!unrestricted_drag_) {
			VALIDATE(item_position >= 0, null_str);
		}
		item_position_ = item_position > item_count_ - visible_items_? item_count_ - visible_items_: item_position;
		if (all_items_visible()) {
			item_position_ = 0;
		}

	} else {
		item_position_ = item_position;
	}

	// Determine the pixel offset of the item position.
	positioner_offset_ = item_position_ >= 0? item_position_ * pixels_per_step_: 0;

	update_canvas();

	if (!unrestricted_drag_ || force_restrict) {
		VALIDATE(visible_items_ > item_count_ || item_position_ <= item_count_ - visible_items_, null_str);
	}
}

void tscrollbar_::update_canvas() 
{
	VALIDATE(positioner_offset_ >= 0 && positioner_length_ >= 0, null_str);

	int positioner_length = positioner_length_;
	if (unrestricted_drag_ && item_position_ < 0) {
		// when item_position_ > 0, because of offset lengthened, it will result fake "shorten".
		// current positioner_length equal to visible_items_.
		positioner_length = positioner_length_ * (visible_items_ + item_position_) / visible_items_;
	}

	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("positioner_offset", variant(positioner_offset_ / twidget::hdpi_scale));
		tmp.set_variable("positioner_length", variant(positioner_length / twidget::hdpi_scale));
	}
	set_dirty();
}

void tscrollbar_::set_state(const tstate state)
{
	if (state != state_) {
		state_ = state;
		set_dirty();
	}
}

void tscrollbar_::recalculate()
{
	// We can be called before the size has been set up in that case we can't do
	// the proper recalcultion so stop before we die with an assert.
	if(!get_length()) {
		return;
	}

	// Get the available size for the slider to move.
	const int available_length =
		get_length() - offset_before() - offset_after();

	VALIDATE(available_length > 0, null_str);

	// All visible.
	if(item_count_ <= visible_items_) {
		positioner_offset_ = offset_before();
		VALIDATE(positioner_offset_ >= 0, null_str);
		positioner_length_ = available_length;
		recalculate_positioner();
		item_position_ = 0;
		update_canvas();
		return;
	}

	/**
	 * @todo In the MP lobby it can happen that a listbox has first zero items,
	 * then gets filled and since there are no visible items the second assert
	 * after this block will be triggered. Use this ugly hack to avoid that
	 * case. (This hack also added the gui/widgets/window.hpp include.)
	 */
	if(!visible_items_) {
		twindow* window = get_window();
		VALIDATE(window, null_str);
		window->invalidate_layout(nullptr);
		// Can't recalculate size, force a window layout phase.;
		return;
	}

	VALIDATE(visible_items_, null_str);

	const unsigned steps = item_count_ - visible_items_;

	positioner_length_ = available_length * visible_items_ / item_count_;
	recalculate_positioner();

	// Make sure we can also show the last step, so add one more step.
	pixels_per_step_ =
		(available_length - positioner_length_)
		/ static_cast<float>(steps + 1);

	set_item_position(item_position_);
}

void tscrollbar_::recalculate_positioner()
{
	const int minimum = minimum_positioner_length();
	const int maximum = maximum_positioner_length();

	if(minimum == maximum) {
		positioner_length_ = maximum;
	} else if(maximum != 0 && positioner_length_ > maximum) {
		positioner_length_ = maximum;
	} else if(positioner_length_ < minimum) {
		positioner_length_ = minimum;
	}
}

void tscrollbar_::move_positioner(const int distance)
{
	VALIDATE(item_position_ >= 0, null_str);

	if (distance < 0 && -distance > positioner_offset_) {
		positioner_offset_ = 0;
	} else {
		positioner_offset_ += distance;
	}

	const int length = get_length() - offset_before() - offset_after() - positioner_length_;

	if (positioner_offset_ > length) {
		positioner_offset_ = length;
	}

	int position = positioner_offset_ / pixels_per_step_;

	// Note due to floating point rounding the position might be outside the
	// available positions so set it back.
	if (position > item_count_ - visible_items_) {
		position = item_count_ - visible_items_;
	}

	if(position != item_position_) {
		item_position_ = position;

		child_callback_positioner_moved();

		fire2(event::NOTIFY_MODIFIED, *this);
		if (did_modified_) {
			did_modified_(*this);
		}

//		positioner_moved_notifier_.notify();
	}
	update_canvas();
}

void tscrollbar_::load_config_extra()
{
	// These values won't change so set them here.
	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("offset_before", variant(offset_before() / twidget::hdpi_scale));
		tmp.set_variable("offset_after", variant(offset_after() / twidget::hdpi_scale));
	}
}

void tscrollbar_::signal_handler_mouse_enter(
		const event::tevent event, bool& handled, bool& halt)
{
	// Send the motion under our event id to make debugging easier.
	signal_handler_mouse_motion(event, handled, halt, get_mouse_position());
}

void tscrollbar_::signal_handler_mouse_motion(
		  const event::tevent event
		, bool& handled
		, bool& halt
		, const tpoint& coordinate)
{
	if (!get_active()) {
		handled = true;
		return;
	}
	tpoint mouse = coordinate;
	mouse.x -= get_x();
	mouse.y -= get_y();

	switch(state_) {
		case ENABLED :
			if(on_positioner(mouse)) {
				set_state(FOCUSSED);
			}

			break;

		case PRESSED : {
				const int distance = get_length_difference(mouse_, mouse);
				mouse_ = mouse;
				move_positioner(distance);
			}
			break;

		case FOCUSSED :
			if(!on_positioner(mouse)) {
				set_state(ENABLED);
			}
			break;

		case DISABLED :
			// Shouldn't be possible, but seems to happen in the lobby
			// if a resize layout happens during dragging.
			halt = true;
			break;

		default :
			VALIDATE(false, null_str);
	}
	// handled = true;
}

void tscrollbar_::signal_handler_mouse_leave(
		const event::tevent event, bool& handled)
{
	if (!get_active()) {
		handled = true;
		return;
	}
	if (state_ == PRESSED || state_ == FOCUSSED) {
		set_state(ENABLED);
	}
	handled = true;
}


void tscrollbar_::signal_handler_left_button_down(
		const event::tevent event, bool& handled)
{
	tpoint mouse = get_mouse_position();
	mouse.x -= get_x();
	mouse.y -= get_y();

	if (on_positioner(mouse)) {
		VALIDATE(get_window(), null_str);
		mouse_ = mouse;
		get_window()->mouse_capture();
		set_state(PRESSED);
	}

	const int bar = on_bar(mouse);

	if (bar == -1) {
		scroll(HALF_JUMP_BACKWARDS);
		fire2(event::NOTIFY_MODIFIED, *this);
		if (did_modified_) {
			did_modified_(*this);
		}
//		positioner_moved_notifier_.notify();
	} else if (bar == 1) {
		scroll(HALF_JUMP_FORWARD);
		fire2(event::NOTIFY_MODIFIED, *this);
		if (did_modified_) {
			did_modified_(*this);
		}
//		positioner_moved_notifier_.notify();
	} else {
		VALIDATE(bar == 0, null_str);
	}

	handled = true;
}

void tscrollbar_::signal_handler_left_button_up(
		const event::tevent event, bool& handled)
{
	tpoint mouse = get_mouse_position();
	mouse.x -= get_x();
	mouse.y -= get_y();

	if(state_ != PRESSED) {
		return;
	}

	if(on_positioner(mouse)) {
		set_state(FOCUSSED);
	} else {
		set_state(ENABLED);
	}

	handled = true;
}

} // namespace gui2

