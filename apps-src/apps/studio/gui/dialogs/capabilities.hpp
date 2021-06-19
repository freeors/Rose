/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_CAPABILITIES_HPP_INCLUDED
#define GUI_DIALOGS_CAPABILITIES_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "editor.hpp"

namespace gui2 {

class ttext_box;
class tbutton;

class tcapabilities: public tdialog
{
public:
	// enum {none, add_app};
	explicit tcapabilities(const std::vector<std::unique_ptr<tapp_copier> >& app_copiers, int app_at);

	tapp_capabilities get_capabilities() const { return current_capabilities_; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	void did_changed_app(ttext_box& widget);
	void did_changed_bundle_id(ttext_box& widget);
	void did_changed_ble(twidget& widget);
	void did_changed_wifi(twidget& widget);
	void did_changed_bootup(twidget& widget);
	void save(twindow& window, bool& handled, bool& halt);

	bool is_valid_app(const std::string& app) const;
	bool is_valid_bundle_id(const tdomain& bundle_id) const;

private:
	const std::vector<std::unique_ptr<tapp_copier> >& app_copiers_;
	const int app_at_;
	tapp_capabilities current_capabilities_;
	std::map<std::string, std::string> apps_in_apps_sln_;

	ttext_box* app_text_;
	ttext_box* bundle_id_text_;
	tbutton* ok_;
};

}

#endif /* ! GUI_DIALOGS_CAPABILITIES_HPP_INCLUDED */
