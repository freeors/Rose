/* $Id: control.hpp 54322 2012-05-28 08:21:28Z mordante $ */
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

#ifndef GUI_AUXILIARY_WINDOW_BUILDER_CONTROL_HPP_INCLUDED
#define GUI_AUXILIARY_WINDOW_BUILDER_CONTROL_HPP_INCLUDED

#include "gui/auxiliary/window_builder.hpp"

namespace gui2 {

class tcontrol;

namespace implementation {

struct tbuilder_control
	: public tbuilder_widget
{
public:

	tbuilder_control(const config& cfg);

	using tbuilder_widget::build;

	/** @deprecated The control can initalise itself. */
	void init_control(tcontrol* control) const;

	/** Parameters for the control. */
	std::string definition;
	t_string label;
	t_string tooltip;
	std::string width;
	std::string height;
	std::string min_text_width;
	std::string min_text_height;
	bool width_is_max;
	bool height_is_max;
	int text_font_size;
	int text_color_tpl;
};

} // namespace implementation

} // namespace gui2

#endif

