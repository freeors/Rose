/* $Id: horizontal_scrollbar.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/horizontal_scrollbar.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/auxiliary/widget_definition/horizontal_scrollbar.hpp"
#include "gui/auxiliary/window_builder/horizontal_scrollbar.hpp"

#include "font.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_WIDGET(horizontal_scrollbar)

tpoint thorizontal_scrollbar::mini_get_best_text_size() const
{
	const std::string horizontal_id = settings::horizontal_scrollbar_id;

	if (id_ == horizontal_id) {
		return tpoint(0, SCROLLBAR_BEST_SIZE);
	}
	return tpoint(0, 0);
}

int thorizontal_scrollbar::minimum_positioner_length() const
{
	boost::intrusive_ptr
		<const thorizontal_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const thorizontal_scrollbar_definition::tresolution>(config());

	assert(conf);
	return conf->minimum_positioner_length;
}

int thorizontal_scrollbar::maximum_positioner_length() const
{
	boost::intrusive_ptr
		<const thorizontal_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const thorizontal_scrollbar_definition::tresolution>(config());

	assert(conf);
	return conf->maximum_positioner_length;
}

int thorizontal_scrollbar::offset_before() const
{
	boost::intrusive_ptr
		<const thorizontal_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const thorizontal_scrollbar_definition::tresolution>(config());

	assert(conf);
	return conf->left_offset;
}

int thorizontal_scrollbar::offset_after() const
{
	boost::intrusive_ptr
		<const thorizontal_scrollbar_definition::tresolution> conf =
		boost::dynamic_pointer_cast
		<const thorizontal_scrollbar_definition::tresolution>(config());
	assert(conf);

	return conf->right_offset;
}

bool thorizontal_scrollbar::on_positioner(const tpoint& coordinate) const
{
	// Note we assume the positioner is over the entire height of the widget.
	return coordinate.x >= static_cast<int>(get_positioner_offset())
		&& coordinate.x < static_cast<int>(get_positioner_offset()
				+ get_positioner_length())
		&& coordinate.y > 0
		&& coordinate.y < static_cast<int>(get_height());
}

int thorizontal_scrollbar::on_bar(const tpoint& coordinate) const
{
	// Not on the widget, leave.
	if (coordinate.x > get_width() || coordinate.y > get_height()) {
		return 0;
	}

	// we also assume the bar is over the entire width of the widget.
	if (coordinate.x < get_positioner_offset()) {
		return -1;
	} else if (coordinate.x > get_positioner_offset() + get_positioner_length()) {

		return 1;
	} else {
		return 0;
	}
}

const std::string& thorizontal_scrollbar::get_control_type() const
{
	static const std::string type = "horizontal_scrollbar";
	return type;
}

} // namespace gui2

