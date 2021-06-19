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

#ifndef GUI_DIALOGS_EDIT_BOX_HPP_INCLUDED
#define GUI_DIALOGS_EDIT_BOX_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "serialization/string_utils.hpp"
#include "gui/widgets/text_box2.hpp"

namespace gui2 {

class ttext_box;
class ttext_box2;
class tbutton;

class tedit_box : public tdialog
{
public:
	static tedit_box* instance;

	tedit_box(tedit_box_param& param);
	virtual ~tedit_box();
	void text_changed_callback(ttext_box& widget);

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void click_reset();

	void signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode);

	void app_did_keyboard_hidden(int reason) override;
	void app_timer_handler(uint32_t now) override;

private:
	tedit_box_param& param_;
	const std::string cancel_label_;
	tbutton* ok_;
	tbutton* cancel_;
	ttext_box2* txt_;
	uint32_t will_close_ticks_;
	bool sync_with_keyboard_;
};

} // namespace gui2

#endif

