/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#ifndef GUI_RLDIALOGS_DIALOG_HPP_INCLUDED
#define GUI_RLDIALOGS_DIALOG_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class rose_lua_kernel;

namespace gui2 {

// rldialog: Rose Lua dialog
class trldialog: public tdialog
{
public:
	trldialog(rose_lua_kernel& lua, const std::string& aplt, const std::string& id);
	virtual ~trldialog();

	std::string get_next_dlg() const { return next_dlg_; }

	struct tmsg_data_cfg: public rtc::MessageData {
		explicit tmsg_data_cfg(const config& cfg)
			: cfg(cfg)
		{}

		const config cfg;
	};

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;
	void post_show() override;

	void app_timer_handler(uint32_t now) override;
	void app_OnMessage(rtc::Message* msg) override;

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string aplt_;
	const std::string id_;
	const std::string joined_id_;

	bool has_timer_handler_;
	std::string next_dlg_;
};

class vdialog
{
public:
	static const char metatableKey[];

	explicit vdialog(trldialog& dlg)
		: dlg_(dlg)
	{}

	trldialog& dialog() { return dlg_; }

private:
	trldialog& dlg_;
};

} // namespace gui2

#endif

