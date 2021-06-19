/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TEXT_BOX2_HPP_INCLUDED
#define GUI_WIDGETS_TEXT_BOX2_HPP_INCLUDED

#include "gui/widgets/text_box.hpp"

namespace gui2 {

class tpanel;
class tbutton;

class ttext_box2 : public tbase_tpl_widget
{
public:
	explicit ttext_box2(twindow& window, twidget& widget, const std::string& panel_border = "", const std::string& image_label = "", bool desensitize = false, const std::string& button_label = "misc/delete.png", bool clear = true);
	~ttext_box2();

	tpanel* panel() const override { return widget_; }
	tcontrol* image() const override { return image_; }
	ttext_box* text_box() const override { return text_box_; }
	tbutton* button() const override { return button_; }

	void set_did_text_changed(const std::function<void (ttext_box& widget) >& did) override
	{
		did_text_changed_ = did;
	}

private:
	void did_text_changed(ttext_box& widget);
	void clear_text_box();

private:
	tpanel* widget_;
	tcontrol* image_;
	ttext_box* text_box_;
	tbutton* button_;
	bool clear_;

	std::function<void (ttext_box&)> did_text_changed_;
};

} // namespace gui2

#endif

