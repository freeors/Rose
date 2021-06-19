/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#ifndef GUI_DIALOGS_RLOGIN_HPP_INCLUDED
#define GUI_DIALOGS_RLOGIN_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/text_box2.hpp"

namespace gui2 {

class tbutton;
class tlabel;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class trlogin : public tdialog
{
public:
	struct tslot
	{
	public:
		tslot()
			: rlogin(nullptr)
		{}

		virtual void rlogin_did_text_box_changed(const std::string& username, const std::string& password) {}

		virtual bool rlogin_can_login(const std::string& username, const std::string& password) const
		{
			if ((int)username.size() < rlogin->min_username_chars_) {
				return false;
			}

			if ((int)password.size() < rlogin->min_password_chars_) {
				return false;
			}
			return true; 
		}

		virtual void rlogin_click_login(const std::string& username, const std::string& password);

	public:
		trlogin* rlogin;
		std::string rlogin_username;
		std::string rlogin_password;
	};

	trlogin(tslot& slot, const std::string& title, const std::string& login);

	tlabel& title_widget() { return *title_widget_; }
	tbutton& login_widget() { return *login_widget_; }

	ttext_box& username_widget() { return *username_->text_box(); }
	ttext_box& password_widget() { return *password_->text_box(); }

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void click_password();
	void did_text_box_changed(ttext_box& widget);
	void click_login(twindow& window);

	bool can_login_active() const;

private:
	tslot& slot_;

	int min_username_chars_;
	int max_username_chars_;
	int min_password_chars_;
	int max_password_chars_;
	std::string title_;
	std::string login_label_;

	tlabel* title_widget_;
	tbutton* login_widget_;
	ttext_box2* username_;
	ttext_box2* password_;
};

} // namespace gui2

#endif
