/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_APLT_HPP_INCLUDED
#define LIBROSE_APLT_HPP_INCLUDED

#include "scripts/lua_kernel_base.hpp" // for lua_kernel_base
#include "scripts/gui/widgets/vwidget.hpp"
#include "version.hpp"

enum {zipt_applet = 1, zipt_apk = 2};

#pragma pack(1)
struct trsp_header {
	uint32_t fourcc;
	uint32_t version;
	uint32_t build_date;
	uint32_t rose_version;
	char bundleid[24];
	uint32_t manufactor;
	uint32_t apk_size;
};
#pragma pack()

#define RSP_HEADER_BYTES	sizeof(trsp_header)

#define APPLET_ICON		"icon_192.png"

namespace applet {
std::string get_settings_cfg(const std::string& aplt_path, bool alert_valid, version_info* out_version);

enum {type_distribution, type_development, type_studio, type_count};
extern std::map<int, std::string> srctypes;

struct tapplet
{
	tapplet()
		: type(nposm)
		, ts(0)
	{
		memset(&rspheader, 0, RSP_HEADER_BYTES);
	}

	void set(int _type, const std::string& _title, const std::string& _subtitle, const std::string& _username, int64_t _ts, const std::string& _path, const std::string& _icon)
	{
		type = _type;
		title = _title;
		subtitle = _subtitle;
		username = _username;
		ts = _ts;
		path = _path;
		icon = _icon;
	}

	bool valid() const { return type != nposm && !bundleid.empty() && version.is_rose_recommended() && !path.empty(); }

	void write_distribution_cfg() const;

	int type;
	std::string bundleid;
	version_info version;
	std::string title;
	std::string subtitle;
	std::string username;
	int64_t ts;
	std::string path;
	std::string icon;
	trsp_header rspheader;
};

void initial(std::vector<applet::tapplet>& applets);

class texecutor
{
public:
	texecutor(rose_lua_kernel& lua, const applet::tapplet& aplt);

	void run();

private:
	rose_lua_kernel& lua_;
	const applet::tapplet& aplt_;
	std::string lua_bundleid_;
	lua_State* L;
	const std::string launcher_id_;
};

}

#endif