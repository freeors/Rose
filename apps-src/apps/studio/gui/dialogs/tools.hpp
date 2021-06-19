/* $Id: mp_login.hpp 48879 2011-03-13 07:49:40Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_TOOLS_HPP_INCLUDED
#define GUI_DIALOGS_TOOLS_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include <map>
#include "thread.hpp"


namespace gui2 {

class tgrid;
class ttrack;
class ttoggle_button;

class ttools : public tdialog, public tworker
{
public:
	enum {IMAGE_LAYER, CNFTL_LAYER};
	enum tstyle {style_transition, style_min = style_transition, style_stuff, style_nine, style_max = style_nine};
	static std::map<tstyle, std::string> styles;

	ttools();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void did_draw_showcase(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void refresh_according_to_style(tgrid& image_layer);
	std::string text_box_str(twindow& window, const std::string& id, const std::string& name, int min, int max, bool allow_empty);
	void style(twindow& window);
	void execute(twindow& window);

	bool did_navigation_pre_change(ttoggle_button& widget);
	void did_navigation_changed(ttoggle_button& widget);

	void pre_image(twindow& window);

	void pre_cnftl(twindow& window);
	void generate_cnftl(twindow& window);
	void browse_cnftl(twindow& window);
	void did_cnftl_changed();

	void DoWork() override;
	void OnWorkStart() override;
	void OnWorkDone() override;

	struct tfont
	{
		tfont(const std::string& name, bool ascii, bool chinese)
			: name(name)
			, ascii(ascii)
			, chinese(chinese)
		{}

		std::string name;
		bool ascii;
		bool chinese;
		std::map<int, wchar_t> absent_chars;
	};
	void handle_font(const tfont& font, const bool started, const int at, const bool ret);

private:
	std::string design_dir;
	std::string input_png;
	tstyle style_;

	std::string cnftl_path_;
	std::vector<std::unique_ptr<tfont> > fonts_;
	bool cnftl_generating_;
};

} // namespace gui2

#endif

