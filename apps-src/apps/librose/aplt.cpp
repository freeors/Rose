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

/**
 * @file
 * Provides a Lua interpreter, to be embedded in WML.
 *
 * @note Naming conventions:
 *   - intf_ functions are exported in the rose domain,
 *   - impl_ functions are hidden inside metatables,
 *   - cfun_ functions are closures,
 *   - luaW_ functions are helpers in Lua style.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "aplt.hpp"

#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"
#include "scripts/gui/dialogs/rldialog.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/dialogs/message.hpp"
#include "serialization/string_utils.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "config_cache.hpp"
#include "xwml.hpp"
#include "wml_exception.hpp"

namespace applet {

std::string get_settings_cfg(const std::string& aplt_path, bool alert_valid, version_info* out_version)
{
	enum {invalid_bundleid, invalid_version};

	const std::string file_path = aplt_path + "/settings.cfg";

	config cfg;
	config_cache& cache = config_cache::instance();
	cache.get_config(file_path, cfg);
	cache.clear_defines();

	std::string bundleid;
	int errcode = nposm;
	if (!cfg.empty()) {
		 bundleid = cfg["bundle_id"].str();
		if (is_bundleid(bundleid)) {
			version_info version(cfg["version"].str());
			if (version.is_rose_recommended()) {
				if (out_version != nullptr) {
					*out_version = version;
				}
			} else {
				bundleid.clear();
				errcode = invalid_version;
			}
		} else {
			bundleid.clear();
			errcode = invalid_bundleid;
		}
	} else {
		errcode = invalid_bundleid;
	}
	if (!bundleid.empty()) {
		VALIDATE(errcode == nposm, null_str);
	} else {
		VALIDATE(errcode != nposm, null_str);
	}
	if (alert_valid && errcode != nposm) {
		utils::string_map symbols;
		symbols["file"] = file_path;
		symbols["field"] = errcode == invalid_bundleid? "bundle_id": "version";
		gui2::show_message(null_str, vgettext2("No valid $field in $file", symbols));
	}
	return bundleid;
}

#define APLT_DISTRIBUTION_CFG	"distribution.cfg"

config get_distribution_cfg(const std::string& aplt_path)
{
	const std::string file_path = aplt_path + "/" + APLT_DISTRIBUTION_CFG;

	config cfg;
	config_cache& cache = config_cache::instance();
	cache.get_config(file_path, cfg);
	cache.clear_defines();

	return cfg;
}

void tapplet::write_distribution_cfg() const
{
	VALIDATE(valid(), null_str);
	VALIDATE(type == type_distribution || type == type_development, null_str);
	std::stringstream ss;

	ss << "distribution = " << (type == type_distribution? config::attribute_value::s_yes: config::attribute_value::s_no) << "\n";
	ss << "title = \"" << title << "\"\n";
	ss << "subtitle = \"" << subtitle << "\"\n";
	ss << "username = \"" << username << "\"\n";
	ss << "ts = " << ts << "\n";

	write_file(path + "/" + APLT_DISTRIBUTION_CFG, ss.str().c_str(), ss.str().size());
}

std::map<int, std::string> srctypes;

void initial(std::vector<applet::tapplet>& applets)
{
	VALIDATE(applets.empty(), null_str);

	srctypes.insert(std::make_pair(type_distribution, "distribution"));
	srctypes.insert(std::make_pair(type_development, "development"));
	srctypes.insert(std::make_pair(type_studio, "studio"));
	VALIDATE(srctypes.size() == type_count, null_str);

	std::set<std::string> aplts;
	std::set<std::string> partial = aplts_in_preferences_dir();
	for (std::set<std::string>::const_iterator it = partial.begin(); it != partial.end(); ++ it) {
		const std::string& lua_bundleid = *it;
		const std::string aplt_path = game_config::preferences_dir + "/" + lua_bundleid;
		aplts.insert(aplt_path);
	}
	partial = aplts_in_res();
	for (std::set<std::string>::const_iterator it = partial.begin(); it != partial.end(); ++ it) {
		const std::string& lua_bundleid = *it;
		const std::string aplt_path = game_config::path + "/" + lua_bundleid;
		aplts.insert(aplt_path);
	}

	for (std::set<std::string>::const_iterator it = aplts.begin(); it != aplts.end(); ++ it) {
		const std::string& aplt_path = *it;
		const std::string lua_bundleid = extract_file(aplt_path);
		bool in_preferences = aplt_path.find(game_config::path) != 0;

		applets.push_back(tapplet());
		tapplet& aplt = applets.back();
		aplt.bundleid = get_settings_cfg(aplt_path, false, &aplt.version);
		if (utils::replace_all(aplt.bundleid, ".", "_") == lua_bundleid) {
			std::string title, subtitle, username;
			int64_t ts = 0;
			int type = type_studio;
			if (in_preferences) {
				config dist = get_distribution_cfg(aplt_path);
				title = dist["title"].str();
				subtitle = dist["subtitle"].str();
				username = dist["username"].str();
				type = dist["distribution"].to_bool()? type_distribution:type_development;
				ts = dist["ts"].to_long_long();
			} else {
				title = aplt.bundleid;
			}
			aplt.set(type, title, subtitle, username, ts, aplt_path, aplt_path + "/" + APPLET_ICON);
		} else {
			applets.pop_back();
		}
	}
}

texecutor::texecutor(rose_lua_kernel& lua, const applet::tapplet& aplt)
	: lua_(lua)
	, aplt_(aplt)
	, lua_bundleid_(utils::replace_all(aplt.bundleid, ".", "_"))
	, L(lua.get_state())
{
}

void texecutor::run()
{
	config aplt_cfg;
	const std::string aplt_path = aplt_.path;
	const std::string textdomain = lua_bundleid_ + "-lib";

	{
		// bindtextdomain must be before load gui/*cfg
		const std::string path = aplt_path + "/translations";

		bindtextdomain(textdomain.c_str(), path.c_str());
		bind_textdomain_codeset(textdomain.c_str(), "UTF-8");
	}
	wml_config_from_file(aplt_path + "/xwml/" + BASENAME_APLT, aplt_cfg);
	
	gui2::insert_window_builder_from_cfg(aplt_cfg);

	config& sub_cfg = aplt_cfg.add_child("binary_path");
	sub_cfg["path"] = binary_paths_manager::full_path_indicator + aplt_path;
	const binary_paths_manager bin_paths_manager(aplt_cfg);

	// 
	create_directory_if_missing(get_aplt_user_data_dir(lua_bundleid_));

	lua_.register_applet(lua_bundleid_, aplt_.version, aplt_cfg);

	std::string next_dlg;
	std::map<int, std::string> windows;
	{
		tstack_size_lock lock(L, 0);

		luaW_getglobal(L, lua_bundleid_.c_str(), "startup");
		lua_.protected_call(0, 2);

		const int MIN_APP_RETVAL = 1;
		vconfig vcfg = luaW_checkvconfig(L, -2);
		for (const config::attribute& val: vcfg.get_config().attribute_range()) {
			const std::string id = val.first;
			int n = val.second.to_int();
			VALIDATE(n >= MIN_APP_RETVAL && windows.count(n) == 0, null_str);
			windows.insert(std::make_pair(n, id));
		}
		int launcher = lua_tointeger(L, -1);
		lua_pop(L, 2);

		VALIDATE(windows.count(launcher) != 0, null_str);
		next_dlg = windows.find(launcher)->second;

	}

	while (!next_dlg.empty()) {
		gui2::trldialog dlg(lua_, lua_bundleid_, next_dlg);
		dlg.show();
		lua_.post_show(lua_bundleid_, next_dlg);

		int retval = dlg.get_retval();
		if (windows.count(retval) != 0) {
			next_dlg = windows.find(retval)->second;
		} else {
			next_dlg.clear();
		}
	}
	lua_.unregister_applet(lua_bundleid_);
	gui2::remove_window_builder_baseon_app(lua_bundleid_);
	unbindtextdomain(textdomain.c_str());

	// Enforce a complete garbage collection
	lua_gc(L, LUA_GCCOLLECT);
}

}