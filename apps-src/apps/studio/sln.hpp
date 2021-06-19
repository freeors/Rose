#ifndef STUDIO_SLN_HPP_INCLUDED
#define STUDIO_SLN_HPP_INCLUDED

#include "config_cache.hpp"
#include "config.hpp"
#include "version.hpp"

#include <set>

namespace apps_sln {

std::map<std::string, std::string> apps_in(const std::string& src2_path);
std::set<std::string> include_directories_in(const std::string& app);

bool add_project(const std::string& app, const std::string& guid_str);
bool remove_project(const std::string& app);

bool can_new_dialog(const std::string& app);
bool new_dialog(const std::string& app, const std::vector<std::string>& files);

bool add_aplt(const std::string& lua_bundleid, const std::vector<std::string>& files);
bool remove_aplt(const std::string& lua_bundleid);
bool new_dialog_aplt(const std::string& lua_bundleid, const std::vector<std::string>& files);
}

#endif