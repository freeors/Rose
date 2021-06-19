/* $Id: message.hpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#ifndef GUI_DIALOGS_MESSAGE_HPP_INCLUDED
#define GUI_DIALOGS_MESSAGE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"

namespace gui2 {

class tbutton;

/**
 * Main class to show messages to the user.
 *
 * It can be used to show a message or ask a result from the user. For the
 * most common usage cases there are helper functions defined.
 */
class tmessage : public tdialog
{
	friend struct tmessage_implementation;
public:
	tmessage(const std::string& title, const std::string& message,
			const bool auto_close)
		: title_(title)
		, message_(message)
		, auto_close_(auto_close)
		, buttons_(count)
	{
		set_timer_interval(400);
	}

	enum tbutton_id {
		cancel = 0
		, ok
		, count
	};

	/**
	 * Selects the style of the buttons to be shown.
	 *
	 * These values are not directly implemented in this class but are used
	 * by our helper functions.
	 */
	enum tbutton_style
	{
		  auto_close        /**< Enables auto close. */
		, ok_button         /**< Shows an ok button. */
		, close_button      /**< Shows a close button. */
		, ok_cancel_buttons /**< Shows an ok and cancel button. */
		, cancel_button     /**< Shows a cancel button. */
		, yes_no_buttons    /**< Shows a yes and no button. */
	};

	void set_button_caption(const tbutton_id button,
			const std::string& caption);

	void set_button_visible(const tbutton_id button,
			const twidget::tvisible visible);

	void set_button_retval(const tbutton_id button,
			const int retval);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_title(const std::string& title) {  title_ = title; }

	void set_message(const std::string& message) {  message_ = message; }

	void set_auto_close(const bool auto_close) { auto_close_ = auto_close; }

protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

protected:
	/** The title for the dialog. */
	std::string title_;

	/** The message to show to the user. */
	std::string message_;

	/**
	 * Does the window need to use click_dismiss when the dialog doesn't need a
	 * scrollbar.
	 */
	bool auto_close_;

	struct tbutton_status
	{
		tbutton_status();

		tbutton* button;
		std::string caption;
		twidget::tvisible visible;
		int retval;
	};

	/** Holds a pointer to the buttons. */
	std::vector<tbutton_status> buttons_;
};

class tsimple_message : public tmessage
{
public:
	tsimple_message(const std::string& title, const std::string& message, const bool auto_close, const bool countdown_close)
		: tmessage(title, message, auto_close)
		, countdown_close_(countdown_close)
		, will_close_threshold_(10 * 1000)
		, will_close_ticks_(0)
	{}

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	void pre_show() override;
	void app_timer_handler(uint32_t now) override;

private:
	const bool countdown_close_;
	const int will_close_threshold_;
	uint32_t will_close_ticks_;
};

class tportrait_message : public tmessage
{
public:
	tportrait_message(const std::string& title, const std::string& message,
			const std::string& portrait, const std::string& incident,
			const bool auto_close)
		: tmessage(title, message, auto_close)
		, portrait_(portrait)
		, incident_(incident)
	{}

private:
	/**
	 * The image which is shown in the dialog.
	 *
	 * This image can be an icon or portrait or any other image.
	 */
	std::string portrait_;
	std::string incident_;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;
};

void show_message_onlycountdownclose(const std::string& title, const std::string& message);

void show_message(const std::string& title, const std::string& message, bool countdown_close = false);

/**
 * Shows a message to the user.
 *
 * @note this function is rather untested, and the API might change in the
 * near future.
 *
 * @param video               The video which contains the surface to draw
 *                            upon.
 * @param title               The title of the dialog.
 * @param message             The message to show in the dialog.
 * @param button_style        The style of the button(s) shown.
 * @param message_use_markup  Use markup for the message?
 * @param title_use_markup    Use markup for the title?
 *
 * @returns                   The retval of the dialog shown.
 */
int show_message2(const std::string& title,
	const std::string& message, const tmessage::tbutton_style button_style,
	const std::string& portrait = "", const std::string& incident = "");

/**
 * Shows an error message to the user.
 *
 * @param video               The video which contains the surface to draw
 *                            upon.
 * @param message             The message to show in the dialog.
 * @param message_use_markup  Use markup for the message?
 */
void show_error_message(const std::string& message);

} // namespace gui2

#endif

