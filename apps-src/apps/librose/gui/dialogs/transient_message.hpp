/* $Id: transient_message.hpp 48961 2011-03-20 18:26:19Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_TRANSIENT_MESSAGE_HPP_INCLUDED
#define GUI_DIALOGS_TRANSIENT_MESSAGE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

/** Shows a transient message. */
class ttransient_message
	: public tdialog
{
public:

	ttransient_message(const std::string& title
			, const bool title_use_markup
			, const std::string& message
			, const bool message_use_markup
			, const std::string& image);

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

private:
	const std::string title_;
	const std::string message_;
	const std::string image_;
};

/**
 * Shows a transient message to the user.
 *
 * This shows a dialog with a short message which can be dismissed with a
 * single click.
 *
 * @note The message _should_ be small enough to fit on the window, the text
 * can contain newlines and will wrap when needed.
 *
 * @param video               The video which contains the surface to draw
 *                            upon.
 * @param title               The title of the dialog.
 * @param message             The message to show in the dialog.
 * @param image               An image to show in the dialog.
 * @param message_use_markup  Use markup for the message?
 * @param title_use_markup    Use markup for the title?
 */
void show_transient_message(const std::string& title
		, const std::string& message
		, const std::string& image = std::string()
		, const bool message_use_markup = false
		, const bool title_use_markup = false);

} // namespace gui2

#endif

