/* $Id: button.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/auxiliary/window_builder/button.hpp"

#include "config.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/widgets/button.hpp"

namespace gui2 {

namespace implementation {

tbuilder_button::tbuilder_button(const config& cfg)
	: tbuilder_control(cfg)
	, mtwusc(cfg["mtwusc"].to_bool())
	, retval_id_(cfg["return_value_id"])
	, retval_(cfg["return_value"])
{
}

twidget* tbuilder_button::build() const
{
	tbutton* widget = new tbutton();

	init_control(widget);

	if (mtwusc) {
		widget->set_mtwusc();
	}

	widget->set_retval(get_retval(retval_id_, retval_, id));

	// Window builder: found button with id(id) and definition(definition)
	return widget;
}

} // namespace implementation

} // namespace gui2

/*WIKI_MACRO
 * @begin{macro}{button_description}
 *
 *        A button is a control that can be pushed to start an action or close
 *        a dialog.
 * @end{macro}
 */

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_button
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="button"}{min=0}{max=-1}{super="generic/widget_instance"}
 * == Button ==
 *
 * @macro = button_description
 *
 * Instance of a button. When a button has a return value it sets the
 * return value for the window. Normally this closes the window and returns
 * this value to the caller. The return value can either be defined by the
 * user or determined from the id of the button. The return value has a
 * higher precedence as the one defined by the id. (Of course it's weird to
 * give a button an id and then override its return value.)
 *
 * When the button doesn't have a standard id, but you still want to use the
 * return value of that id, use return_value_id instead. This has a higher
 * precedence as return_value.
 *
 * List with the button specific variables:
 * @begin{table}{config}
 *     return_value_id & string & "" &   The return value id. $
 *     return_value & int & 0 &          The return value. $
 *
 * @end{table}
 * @end{tag}{name="button"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

