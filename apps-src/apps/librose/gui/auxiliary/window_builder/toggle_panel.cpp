/* $Id: toggle_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/auxiliary/window_builder/toggle_panel.hpp"

#include "config.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "wml_exception.hpp"

namespace gui2 {

namespace implementation {

tbuilder_toggle_panel::tbuilder_toggle_panel(const config& cfg)
	: tbuilder_control(cfg)
	, left_margin(cfg["left_margin"].to_int(nposm))
	, right_margin(cfg["right_margin"].to_int(nposm))
	, top_margin(cfg["top_margin"].to_int(nposm))
	, bottom_margin(cfg["bottom_margin"].to_int(nposm))
	, grid(NULL)
	, retval_id_(cfg["return_value_id"])
	, retval_(cfg["return_value"])
{
	if (left_margin != nposm) {
		left_margin = cfg_2_os_size(left_margin);
	}
	if (right_margin != nposm) {
		right_margin = cfg_2_os_size(right_margin);
	}
	if (top_margin != nposm) {
		top_margin = cfg_2_os_size(top_margin);
	}
	if (bottom_margin != nposm) {
		bottom_margin = cfg_2_os_size(bottom_margin);
	}

	const config &c = cfg.child("grid");

	VALIDATE(c, _("No grid defined."));

	grid = new tbuilder_grid(c);
}

twidget* tbuilder_toggle_panel::build() const
{
	ttoggle_panel* widget = new ttoggle_panel();

	init_control(widget);

	widget->set_retval(get_retval(retval_id_, retval_, id));

	// Window builder: placed toggle panel 'id' with definition 'definition'.

	widget->init_grid(grid);
	widget->set_margin(left_margin, right_margin, top_margin, bottom_margin);

	return widget;
}

void tbuilder_toggle_panel::build2(ttoggle_panel& widget) const
{
	// ttoggle_panel* widget = new ttoggle_panel();

	init_control(&widget);

	// Window builder: placed toggle panel 'id' with definition 'definition'.

	widget.init_grid(grid);
}

} // namespace implementation

} // namespace gui2

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_toggle_panel
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="toggle_panel"}{min=0}{max=-1}{super="generic/widget_instance"}
 * == Toggle panel ==
 *
 * A toggle panel is an item which can hold other items. The difference between
 * a grid and a panel is that it's possible to define how a panel looks. A grid
 * in an invisible container to just hold the items. The toggle panel is a
 * combination of the panel and a toggle button, it allows a toggle button with
 * its own grid.
 *
 * @begin{table}{config}
 *     grid & grid & &                 Defines the grid with the widgets to
 *                                     place on the panel. $
 *     return_value_id & string & "" & The return value id, see
 *                                     [[GUIToolkitWML#Button]] for more
 *                                     information. $
 *     return_value & int & 0 &        The return value, see
 *                                     [[GUIToolkitWML#Button]] for more
 *                                     information. $
 * @end{table}
 * @allow{link}{name="gui/window/resolution/grid"}
 * @end{tag}{name="toggle_panel"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

