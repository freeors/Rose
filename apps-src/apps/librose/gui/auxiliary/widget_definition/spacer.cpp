/* $Id: spacer.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/auxiliary/widget_definition/spacer.hpp"

namespace gui2 {

tspacer_definition::tspacer_definition(const config& cfg)
	: tcontrol_definition(cfg)
{
	load_resolutions<tresolution>(cfg);
}

tspacer_definition::tresolution::tresolution(const config& cfg)
	: tresolution_definition_(cfg)
{
/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = 1_spacer
 *
 * == Spacer ==
 *
 * @macro = spacer_description
 *
 * A spacer has no states so nothing to load.
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="spacer_definition"}{min=0}{max=-1}{super="generic/widget_definition"}
 * @end{tag}{name="spacer_definition"}
 * @end{parent}{name="gui/"}
 */
}

} // namespace gui2

