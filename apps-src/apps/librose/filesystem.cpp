/* $Id: filesystem.cpp 47496 2010-11-07 15:53:11Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


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
 * File-IO
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"

// Include files for opendir(3), readdir(3), etc.
// These files may vary from platform to platform,
// since these functions are NOT ANSI-conforming functions.
// They may have to be altered to port to new platforms

#ifdef _WIN32
#include <shlobj.h>	// CSIDL_PROGRAM_FILES
#include <direct.h>
#include <cctype>
#else /* !_WIN32 */
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#ifndef ANDROID
#include <sys/param.h> // statfs 
#include <sys/mount.h> // statfs
#else
#include <sys/vfs.h> // statfs 
#endif
#endif /* !_WIN32 */

// for getenv
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <set>
// #include <boost/algorithm/string.hpp>

// for strerror
#include <cstring>

#include "config.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "loadscreen.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "saes.hpp"
#include "version.hpp"
#include "wml_exception.hpp"
#include "theme.hpp"
#include "config_cache.hpp"
#include "libyuv/convert_argb.h"
#include "libyuv/video_common.h"

#include <openssl/sha.h>
#include <openssl/mem.h>

#include <boost/foreach.hpp>
using namespace std::placeholders;

namespace {
	// const mode_t AccessMode = 00770;

	// These are the filenames that get special processing
	const std::string maincfg_filename = "_main.cfg";
	const std::string finalcfg_filename = "_final.cfg";
	const std::string initialcfg_filename = "_initial.cfg";

}

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBase.h>
#endif

static std::set<std::string> predef_dirs;
static std::map<int, std::set<std::string> > predef_files;

bool ends_with(const std::string& str, const std::string& suffix)
{
	return str.size() >= suffix.size() && std::equal(suffix.begin(),suffix.end(),str.end()-suffix.size());
}

void get_files_in_dir(const std::string& directory,
					std::vector<std::string>* files,
					std::vector<std::string>* dirs,
					bool entire_file_path,
					bool skip_media_dir,
					bool reorder_wml,
					file_tree_checksum* checksum,
                    bool subdir)
{
	// If we have a path to find directories in,
	// then convert relative pathnames to be rooted
	// on the wesnoth path
	if (!SDL_IsFromRootPath(directory.c_str())) {
		std::string dir = game_config::path + "/" + directory;
		if (is_directory(dir)) {
			get_files_in_dir(dir, files, dirs, entire_file_path, skip_media_dir, reorder_wml, checksum);
		}
		return;
	}

	if (reorder_wml) {
		std::string maincfg;
		if (directory.empty() || directory[directory.size()-1] == '/') {
			maincfg = directory + maincfg_filename;
		} else {
			maincfg = (directory + "/") + maincfg_filename;
		}

		if (file_exists(maincfg)) {
			if (files != NULL) {
				if (entire_file_path) {
					files->push_back(maincfg);
				} else {
					files->push_back(maincfg_filename);
				}
			}
			return;
		}
	}

	SDL_DIR* dir = SDL_OpenDir(directory.c_str());
	if (dir == NULL) {
		return;
	}

	SDL_dirent2* entry;
	while ((entry = SDL_ReadDir(dir)) != NULL) {
		if (entry->name[0] == '.') {
			continue;
		}

		// generic Unix
		const std::string basename = entry->name;

		std::string fullname;
		if (directory.empty() || directory[directory.size()-1] == '/') {
			fullname = directory + basename;
		} else {
			fullname = directory + "/" + basename;
		}

		if (SDL_DIRENT_DIR(entry->mode)) {
			if (skip_media_dir && (basename == "images" || basename == "sounds" || basename == "music")) {
				continue;
			}

			if (reorder_wml && file_exists(fullname + "/" + maincfg_filename)) {
				if (files != NULL) {
					if (entire_file_path) {
						files->push_back(fullname + "/" + maincfg_filename);
					} else {
						files->push_back(basename + "/" + maincfg_filename);
					}
				} else {
					// Show what I consider strange
					// <fullname>/<maincfg_filename> not used now but skip the directory.
				}
			} else if (dirs != NULL) {
				if (entire_file_path) {
					dirs->push_back(fullname);
				} else {
					dirs->push_back(basename);
				}
			}
		} else {
			if (files != NULL) {
				if (entire_file_path) {
					files->push_back(fullname);
				} else {
					files->push_back(basename);
				}
			}
			if (checksum != NULL) {
				if (entry->mtime > checksum->modified) {
					checksum->modified = entry->mtime;
				}
				checksum->sum_size += entry->size;
				checksum->nfiles++;
			}
		}
	}

	SDL_CloseDir(dir);

	if (files != NULL) {
		std::sort(files->begin(),files->end());
	}

	if (dirs != NULL) {
		std::sort(dirs->begin(),dirs->end());
	}

	if (files != NULL && reorder_wml) {
		// move finalcfg_filename, if present, to the end of the vector
		for (unsigned int i = 0; i < files->size(); i++) {
			if (ends_with((*files)[i], "/" + finalcfg_filename)) {
				files->push_back((*files)[i]);
				files->erase(files->begin()+i);
				break;
			}
		}
		// move initialcfg_filename, if present, to the beginning of the vector
		int foundit = -1;
		for (unsigned int i = 0; i < files->size(); i++)
			if (ends_with((*files)[i], "/" + initialcfg_filename)) {
				foundit = i;
				break;
			}
		if (foundit > 0) {
			std::string initialcfg = (*files)[foundit];
			for (unsigned int i = foundit; i > 0; i--)
				(*files)[i] = (*files)[i-1];
			(*files)[0] = initialcfg;
		}
	}
}

std::string get_prefs_file()
{
	return get_user_data_dir() + "/preferences";
}

std::string get_prefs_back_file()
{
	return get_user_data_dir() + "/preferences.bak";
}

std::string get_critical_prefs_file()
{
	return get_user_data_dir() + "/critical_preferences";
}

std::string create_if_not(const std::string& dir_utf8) 
{
	std::string dir_ansi = dir_utf8;
#ifdef _WIN32
	conv_ansi_utf8(dir_ansi, false);
#endif
	if (get_dir(dir_ansi).empty()) {
		return "";
	}
	return dir_utf8;
}

std::string get_saves_dir()
{
	const std::string dir_utf8 = get_user_data_dir() + "/saves";
	return create_if_not(dir_utf8);
}

std::string get_addon_campaigns_dir()
{
	const std::string dir_utf8 = get_user_data_dir() + "/data/add-ons";
	return create_if_not(dir_utf8);
}

std::string get_intl_dir()
{
	return game_config::path + "/translations";
}

std::string get_screenshot_dir()
{
	const std::string dir_utf8 = get_user_data_dir() + "/screenshots";
	return create_if_not(dir_utf8);
}

std::string get_next_filename(const std::string& name, const std::string& extension)
{
	std::string next_filename;
	int counter = 0;

	do {
		std::stringstream filename;

		filename << name;
		filename.width(3);
		filename.fill('0');
		filename.setf(std::ios_base::right);
		filename << counter << extension;
		counter++;
		next_filename = filename.str();
	} while(file_exists(next_filename) && counter < 1000);
	return next_filename;
}


std::string get_dir(const std::string& dir_path)
{
	if (is_directory(dir_path)) {
		return dir_path;
	}
	if (SDL_MakeDirectory(dir_path.c_str())) {
		return dir_path;
	}
	return null_str;
}

std::string get_cwd()
{
	char buf[1024];
	const char* const res = getcwd(buf,sizeof(buf));
	if(res != NULL) {
		std::string str(res);

#ifdef _WIN32
		conv_ansi_utf8(str, true);
		std::replace(str.begin(), str.end(),'\\','/');
#endif

		return str;
	} else {
		return "";
	}
}

std::string get_exe_dir()
{
#ifndef _WIN32
	char buf[1024];
	size_t path_size = readlink("/proc/self/exe", buf, 1024);
	if(path_size == static_cast<size_t>(-1))
		return std::string();
	buf[path_size] = 0;
	return std::string(dirname(buf));
#else
	const std::string path = get_cwd();
	return path;
#endif
}

bool create_directory_if_missing(const std::string& dirname)
{
	if (dirname.empty()) {
		return false;
	}

	if (is_directory(dirname)) {
		return true;
	} else if (file_exists(dirname)) {
		return false;
	}

	return SDL_MakeDirectory(dirname.c_str());
}

bool create_directory_if_missing_recursive(const std::string& dirname)
{
	if (is_directory(dirname) == false && dirname.empty() == false)
	{
		std::string tmp_dirname = dirname;
		// remove trailing slashes or backslashes
		while ((tmp_dirname[tmp_dirname.size()-1] == '/' ||
			  tmp_dirname[tmp_dirname.size()-1] == '\\') &&
			  tmp_dirname.size()>0)
		{
			tmp_dirname.erase(tmp_dirname.size()-1);
		}

		// create the first non-existing directory
		size_t pos = tmp_dirname.rfind("/");

		// we get the most right directory and *skip* it
		// we are creating it when we get back here
		if (tmp_dirname.rfind('\\') != std::string::npos &&
			tmp_dirname.rfind('\\') > pos )
			pos = tmp_dirname.rfind('\\');

		if (pos != std::string::npos)
			create_directory_if_missing_recursive(tmp_dirname.substr(0,pos));

		return create_directory_if_missing(tmp_dirname);
	}
	return create_directory_if_missing(dirname);
}

void preprocess_res_explorer()
{
	Uint32 start = SDL_GetTicks();

	// copy font directory from res/fonts to user_data_dir/fonts
	std::vector<std::string> v;
	v.push_back("Andagii.ttf");
	v.push_back("DejaVuSans.ttf");
	// v.push_back("wqy-zenhei.ttc");
	v.push_back("PingFang-SC-Regular.ttc");
	create_directory_if_missing(game_config::preferences_dir + "/fonts");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = game_config::path + "/fonts/" + *it;
		const std::string dst = game_config::preferences_dir + "/fonts/" + *it;
		if (!file_exists(dst)) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			if (src_file.valid() && dst_file.valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file.fp, src_file.data, fsize);
			}
		}
	}
/*
	v.clear();
	v.push_back("wechat_qrcode/detect.caffemodel");
	v.push_back("wechat_qrcode/detect.prototxt");
	v.push_back("wechat_qrcode/sr.caffemodel");
	v.push_back("wechat_qrcode/sr.prototxt");
	create_directory_if_missing(game_config::preferences_dir + "/tflites/wechat_qrcode");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = game_config::path + "/data/core/tflites/" + *it;
		const std::string dst = game_config::preferences_dir + "/tflites/" + *it;
		if (!file_exists(dst)) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			if (src_file.valid() && dst_file.valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file.fp, src_file.data, fsize);
			}
		}
	}
*/
	SDL_Log("preprocess res explorer, used %u", SDL_GetTicks() - start);
}

static std::string user_data_dir, user_config_dir, cache_dir;

static void setup_user_data_dir()
{
	const std::string& dir_path = user_data_dir;

	const bool res = create_directory_if_missing(dir_path);
	// probe read permissions (if we could make the directory)
	if (!res || !is_directory(dir_path)) {
		// could not open or create preferences directory at $dir_path;
		return;
	}

	// Create user data and add-on directories
	create_directory_if_missing(dir_path + "/data");
	create_directory_if_missing(dir_path + "/images");
	create_directory_if_missing(dir_path + "/images/misc");
	create_directory_if_missing(dir_path + "/saves");
	create_directory_if_missing(dir_path + "/tflites");
}

void set_preferences_dir(std::string path)
{
#ifdef _WIN32
	WCHAR wc[MAX_PATH];
	char my_documents_path[MAX_PATH];
	HRESULT hr = SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, wc);
	VALIDATE(SUCCEEDED(hr), "os must support SHGetFolderPath.");

	// unicode to utf8
	WideCharToMultiByte(CP_UTF8, 0, wc, -1, my_documents_path, MAX_PATH, NULL, NULL);
	const char last_char = my_documents_path[strlen(my_documents_path) - 1];
	VALIDATE(last_char != '\\' || last_char != '/', null_str);
	game_config::preferences_dir = std::string(my_documents_path) + "/RoseApp/" + path;

#elif defined(ANDROID)
	// SDL_GetPrefPath use SDL_AndroidGetInternalStoragePath(). format: /data/data/com.leagor.sleep/files
	// SDL_AndroidGetExternalStoragePath(), format: /storage/emulated/0/Android/data/com.leagor.sleep/files
	game_config::preferences_dir = SDL_AndroidGetExternalStoragePath();
	if (!path.empty()) {
		game_config::preferences_dir += "/" + path;
	}

#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
    game_config::preferences_dir = getenv("HOME");
#else
    game_config::preferences_dir = get_cwd() + "/..";
#endif
    if (!path.empty()) {
        game_config::preferences_dir += "/" + path;
    }
#else

	std::string path2 = ".wesnoth" + game_config::version.str(false);

#ifdef _X11
	const char *home_str = getenv("HOME");

	if (path.empty()) {
		char const *xdg_data = getenv("XDG_DATA_HOME");
		if (!xdg_data || xdg_data[0] == '\0') {
			if (!home_str) {
				path = path2;
				goto other;
			}
			user_data_dir = home_str;
			user_data_dir += "/.local/share";
		} else user_data_dir = xdg_data;
		user_data_dir += "/wesnoth/";
		user_data_dir += game_config::version.str(false);
		create_directory_if_missing_recursive(user_data_dir);
		game_config::preferences_dir = user_data_dir;
	} else {
		other:
		std::string home = home_str ? home_str : ".";

		if (path[0] == '/')
			game_config::preferences_dir = path;
		else
			game_config::preferences_dir = home + "/" + path;
	}
#else
	if (path.empty()) path = path2;

	const char* home_str = getenv("HOME");
	std::string home = home_str ? home_str : ".";

	if (path[0] == '/')
		game_config::preferences_dir = path;
	else
		game_config::preferences_dir = home + std::string("/") + path;
#endif

#endif /*_WIN32*/

	game_config::preferences_dir = utils::normalize_path(game_config::preferences_dir);
	user_data_dir = game_config::preferences_dir;
	SDL_Log("set_preferences, user_data_dir: %s", user_data_dir.c_str());

	setup_user_data_dir();
}

const std::string& get_user_data_dir()
{
	VALIDATE(!user_data_dir.empty(), null_str);
	return user_data_dir;
}

const std::string &get_user_config_dir()
{
	if (user_config_dir.empty())
	{
#if defined(_X11)
		char const *xdg_config = getenv("XDG_CONFIG_HOME");
		if (!xdg_config || xdg_config[0] == '\0') {
			xdg_config = getenv("HOME");
			if (!xdg_config) {
				user_config_dir = get_user_data_dir();
				return user_config_dir;
			}
			user_config_dir = xdg_config;
			user_config_dir += "/.config";
		} else user_config_dir = xdg_config;
		user_config_dir += "/wesnoth";
		create_directory_if_missing_recursive(user_config_dir);
#else
		user_config_dir = get_user_data_dir();
#endif
	}
	return user_config_dir;
}

std::string get_aplt_user_data_dir(const std::string& lua_bundleid)
{
	const std::string documents = "documents";
	return game_config::preferences_dir + "/" + utils::generate_app_prefix_id(lua_bundleid, documents);
}

std::string get_aplt_prefs_file(const std::string& lua_bundleid)
{
	return get_aplt_user_data_dir(lua_bundleid) + "/preferences";
}

std::string read_file(const std::string &fname)
{
	tfile lock(fname, GENERIC_READ, OPEN_EXISTING);
	int32_t fsize = lock.read_2_data();
	if (!fsize) {
		return null_str;
	}
	std::string ret(lock.data, fsize);
	return ret;
}

void write_file(const std::string& fname, const char* data, int len)
{
	tfile file(fname, GENERIC_WRITE, CREATE_ALWAYS);
	if (!file.valid()) {
		return;
	}
	posix_fwrite(file.fp, data, len);
}

std::string read_map(const std::string& name)
{
	std::string res;
	std::string map_location = get_wml_location("maps/" + name);
	if(!map_location.empty()) {
		res = read_file(map_location);
	}

	if (res.empty()) {
		res = read_file(get_user_data_dir() + "/editor/maps/" + name);
	}

	return res;
}

bool is_directory(const std::string& dir)
{
	if (dir.empty()) {
		return false;
	}

	return SDL_IsDirectory(dir.c_str());
}

bool file_exists(const std::string& name)
{
	if (name.empty()) {
		return false;
	}

	return SDL_IsFile(name.c_str());
}

time_t file_create_time(const std::string& fname)
{
	SDL_dirent dirent;
	if (!SDL_GetStat(fname.c_str(), &dirent)) {
		return 0;
	}
	return dirent.mtime;
}

int64_t file_size(const std::string& file)
{
	VALIDATE(!file.empty(), null_str);

	int64_t fsize = 0;
	SDL_RWops* __h = SDL_RWFromFile(file.c_str(), "rb");
	if (__h) {
		posix_fseek(__h, 0);
		fsize = posix_fsize(__h);
		posix_fclose(__h);
    }
	return fsize;
}

/**
 * Returns true if the file ends with '.gz'.
 *
 * @param filename                The name to test.
 */
bool is_gzip_file(const std::string& filename)
{
	return (filename.length() > 3
		&& filename.substr(filename.length() - 3) == ".gz");
}

file_tree_checksum* preprocessor_checksum = nullptr;

file_tree_checksum::file_tree_checksum()
	: nfiles(0), sum_size(0), modified(0)
{}

file_tree_checksum::file_tree_checksum(const config& cfg) :
	nfiles	(lexical_cast_default<size_t>(cfg["nfiles"])),
	sum_size(lexical_cast_default<size_t>(cfg["size"])),
	modified(lexical_cast_default<time_t>(cfg["modified"]))
{
}

void file_tree_checksum::write(config& cfg) const
{
	cfg["nfiles"] = lexical_cast<std::string>(nfiles);
	cfg["size"] = lexical_cast<std::string>(sum_size);
	cfg["modified"] = lexical_cast<std::string>(modified);
}

bool file_tree_checksum::operator==(const file_tree_checksum &rhs) const
{
	return nfiles == rhs.nfiles && sum_size == rhs.sum_size &&
		modified == rhs.modified;
}

static void get_file_tree_checksum_internal(const std::string& path, file_tree_checksum& res, bool skip_media_dir)
{

	std::vector<std::string> dirs;
	get_files_in_dir(path, NULL, &dirs, true, skip_media_dir, false, &res);
	loadscreen::increment_progress();

	for (std::vector<std::string>::const_iterator j = dirs.begin(); j != dirs.end(); ++j) {
		get_file_tree_checksum_internal(*j, res, skip_media_dir);
	}
}

const file_tree_checksum& data_tree_checksum(bool reset, bool skip_media_dir)
{
	static file_tree_checksum checksum;
	if (reset) {
		checksum.reset();
	}
	if (checksum.nfiles == 0) {
		get_file_tree_checksum_internal("data/", checksum, skip_media_dir);
		get_file_tree_checksum_internal(get_user_data_dir() + "/data/", checksum, skip_media_dir);
	}

	return checksum;
}

void data_tree_checksum(const std::vector<std::string>& paths, file_tree_checksum& checksum, bool skip_media_dir)
{
	checksum.reset();
	for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		get_file_tree_checksum_internal(*it, checksum, skip_media_dir);
	}
}

struct tpreprocessor_checksum_lock
{
	tpreprocessor_checksum_lock(file_tree_checksum& checksum)
	{
		VALIDATE(!preprocessor_checksum, null_str);
		preprocessor_checksum = &checksum;
	}
	~tpreprocessor_checksum_lock()
	{
		preprocessor_checksum = nullptr;
	}
};

void data_tree_checksum(const std::vector<std::string>& defines, const std::vector<std::string>& paths, file_tree_checksum& checksum)
{
	tpreprocessor_checksum_lock lock(checksum);
	checksum.reset();

	config_cache_transaction transaction;
	config_cache& cache = config_cache::instance();
	cache.clear_defines();
	for (std::vector<std::string>::const_iterator it = defines.begin(); it != defines.end(); ++ it) {
		const std::string& macro = *it;
		cache.add_define(macro);
	}

	config fake_cfg;
	for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		const std::string& path = *it;
#ifdef ANDROID
		// now, android cannot suprt *.cfg in assets.
#else
		cache.get_config(path, fake_cfg);
#endif
	}
}

std::string extract_file(const std::string& file)
{
	static const std::string dir_separators = game_config::os == os_windows? "\\/:": "/";
	std::string::size_type pos = file.find_last_of(dir_separators);

	if (pos == std::string::npos) {
		return file;
	}
	if (pos >= file.size() - 1) {
		return "";
	}

	return file.substr(pos + 1);
}

std::string extract_directory(const std::string& file)
{
	static const std::string dir_separators = game_config::os == os_windows? "\\/:": "/";
	std::string::size_type pos = file.find_last_of(dir_separators);

	if (pos == std::string::npos) {
		return "";
	}

	return file.substr(0, pos);
}

std::string file_main_name(const std::string& file)
{
	std::string::size_type pos = file.find_last_of(".");

	if (pos == std::string::npos) {
		return file;
	}

	return file.substr(0, pos);
}

std::string file_ext_name(const std::string& file)
{
	std::string::size_type pos = file.find_last_of(".");

	if (pos == std::string::npos) {
		return "";
	}
	if (pos >= file.size() - 1) {
		return "";
	}

	return file.substr(pos + 1);
}

namespace {

#define PRIORITIEST_BINARY_PATHS	1
std::vector<std::string> binary_paths;

typedef std::map<std::string,std::vector<std::string> > paths_map;
paths_map binary_paths_cache;

}

static void init_binary_paths()
{
	if (binary_paths.empty()) {
		binary_paths.push_back(""); // userdata directory
		// here is self-add paths
		binary_paths.push_back(game_config::app_dir + "/");
		binary_paths.push_back("data/core/");
	}
}

binary_paths_manager::binary_paths_manager() : paths_()
{}

binary_paths_manager::binary_paths_manager(const config& cfg) : paths_()
{
	set_paths(cfg);
}

binary_paths_manager::~binary_paths_manager()
{
	cleanup();
}

void binary_paths_manager::set_paths(const config& cfg)
{
	cleanup();
	init_binary_paths();

	int inserted = 0;
	BOOST_FOREACH (const config &bp, cfg.child_range("binary_path")) {
		std::string path = bp["path"].str();
		if (path.find("..") != std::string::npos) {
			// Invalid binary path $path
			continue;
		}
		if (!path.empty() && path[path.size()-1] != '/') {
			path += "/";
		}
		std::vector<std::string>::iterator it = std::find(binary_paths.begin(), binary_paths.end(), path);
		if (it == binary_paths.end()) {
			it = binary_paths.begin();
			std::advance(it, PRIORITIEST_BINARY_PATHS + inserted);
			binary_paths.insert(it, path);

			VALIDATE(std::find(paths_.begin(), paths_.end(), path) == paths_.end(), null_str);

			paths_.push_back(path);
			inserted ++;
		}
	}
}

void binary_paths_manager::cleanup()
{
	binary_paths_cache.clear();

	for (std::vector<std::string>::const_iterator i = paths_.begin(); i != paths_.end(); ++i) {
		std::vector<std::string>::iterator it2 = std::find(binary_paths.begin(), binary_paths.end(), *i);
		binary_paths.erase(it2);
	}
	paths_.clear();
}

void clear_binary_paths_cache()
{
	binary_paths_cache.clear();
}

const std::vector<std::string>& get_binary_paths(const std::string& type)
{
	const paths_map::const_iterator itor = binary_paths_cache.find(type);
	if(itor != binary_paths_cache.end()) {
		return itor->second;
	}

	if (type.find("..") != std::string::npos) {
		// Not an assertion, as language.cpp is passing user data as type.
		// Invalid WML type $type for binary paths
		static std::vector<std::string> dummy;
		return dummy;
	}

	std::vector<std::string>& res = binary_paths_cache[type];

	init_binary_paths();

	BOOST_FOREACH (const std::string &path, binary_paths)
	{
		if (path.empty()) {
			res.push_back(get_user_data_dir() + "/" + type + "/");

		} else if (path[0] == binary_paths_manager::full_path_indicator) {
			res.push_back(path.substr(1) + type + "/");

		} else {
			res.push_back(game_config::path + "/" + path + type + "/");
		}
	}
	return res;
}

std::string get_binary_file_location(const std::string& type, const std::string& filename)
{
	if (filename.empty()) {
		return null_str;
	}

	if (filename.find("..") != std::string::npos) {
		return null_str;
	}

	const size_t theme_end_chars_size = theme::path_end_chars.size();

	const std::vector<std::string>& binary_paths = get_binary_paths(type);
	for (std::vector<std::string>::const_iterator it = binary_paths.begin(); it != binary_paths.end(); ++ it) {
		const std::string& path = *it;
		const std::string file = path + filename;
		
		if (theme_end_chars_size && !path.compare(path.size() - theme_end_chars_size, theme_end_chars_size, theme::path_end_chars)) {
			// if maybe require theme special, priority get it.
			std::string dir = extract_directory(file);
			std::string file2 = dir + "/" + theme::instance.id + (file.c_str() + dir.size());
			if (file_exists(file2)) {
				return file2;
			}	
		}

		if (file_exists(file)) {
			return file;
		}
	}

	return std::string();
}

std::string get_binary_dir_location(const std::string &type, const std::string &filename)
{
	if (filename.empty()) {
		return std::string();
	}

	if (filename.find("..") != std::string::npos) {
		// Illegal path $filename ' (\"..\" not allowed).
		return std::string();
	}

	BOOST_FOREACH (const std::string &path, get_binary_paths(type))
	{
		const std::string file = path + filename;
		if (is_directory(file)) {
			return file;
		}
	}

	return std::string();
}

std::string get_wml_location(const std::string &filename, const std::string &current_dir)
{
	std::string result;

	if (filename.empty()) {
		return result;
	}

	if (filename.find("..") != std::string::npos) {
		return result;
	}

	const char first = filename[0];
	if (first == '~')  {
		// If the filename starts with '~', look in the user data directory.
		// result = get_user_data_dir() + "/data/" + filename.substr(1);
		result = get_user_data_dir() + "/" + filename.substr(1);

	} else if (filename.size() >= 2 && first == '.' && filename[1] == '/') {
		// !!!FIX, I want get rid of this branch in future.
		// If the filename begins with a "./", look in the same directory
		// as the file currrently being preprocessed.
		result = current_dir + filename.substr(2);

	} else if (first == '^') {
		// If the filename starts with '^', look in the topest directory.
		result = game_config::path + "/" + filename.substr(1);

	} else if (!game_config::path.empty()) {
		result = game_config::path + "/data/" + filename;
	}

	if (result.empty() || (!file_exists(result) && !is_directory(result))) {
		result.clear();
	}

	return result;
}

std::string get_short_wml_path(const std::string &filename)
{
	std::string match = get_user_data_dir() + "/data/";
	if (filename.find(match) == 0) {
		return "~" + filename.substr(match.size());
	}
	match = game_config::path + "/data/";
	if (filename.find(match) == 0) {
		return filename.substr(match.size());
	}
	return filename;
}

static bool collect_app(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& apps)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		const std::string app = game_config::extract_app_from_app_dir(dirent->name);
		if (!app.empty()) {
			apps.insert(app);
		}
	}

	return true;
}

std::set<std::string> apps_in_res()
{
	std::set<std::string> ret;
	::walk_dir(game_config::path, false, std::bind(
				&collect_app
				, _1, _2, std::ref(ret)));
	return ret;
}

static bool collect_aplt(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& aplts)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (isdir) {
		if (is_lua_bundleid(dirent->name)) {
			aplts.insert(dirent->name);
		}
	}

	return true;
}

std::set<std::string> aplts_in_dir(const std::string& dir)
{
	std::set<std::string> ret;
	::walk_dir(dir, false, std::bind(
				&collect_aplt
				, _1, _2, std::ref(ret)));
	return ret;
}

std::vector<std::string> get_disks()
{
	std::vector<std::string> ret;
#ifdef _WIN32
	int DiskCount = 0;
	DWORD DiskInfo = GetLogicalDrives();
	while (DiskInfo) {
		if (DiskInfo & 1) {
			DiskCount ++;
		}
		DiskInfo = DiskInfo >> 1;
	}

	int DSLength = GetLogicalDriveStrings(0, NULL);
	wchar_t* DStr = new wchar_t[DSLength];
	GetLogicalDriveStrings(DSLength, (LPTSTR)DStr);

	char utf8_str[64];

	wchar_t* lpDriveStr = DStr;
	ULARGE_INTEGER i64FreeBytesToCaller, i64TotalBytes, i64FreeBytes;

	for (int i = 0; i < DiskCount; ++ i, lpDriveStr += 4) {
		int DType = GetDriveType(lpDriveStr);
		if (DType != DRIVE_FIXED && DType != DRIVE_REMOVABLE) {
			continue;
			
		}

		BOOL fResult = GetDiskFreeSpaceEx(lpDriveStr, (PULARGE_INTEGER)&i64FreeBytesToCaller,(PULARGE_INTEGER)&i64TotalBytes, (PULARGE_INTEGER)&i64FreeBytes);
		if (!fResult || i64TotalBytes.QuadPart < 4I64 * 1024 * 1024) {
			continue;
		}

		// unicode to utf8
		WideCharToMultiByte(CP_UTF8, 0, lpDriveStr, -1, utf8_str, MAX_PATH, NULL, NULL);
		ret.push_back(utils::normalize_path(utf8_str));
	}
#else
	ret.push_back("/");
#endif

	return ret;
}

static bool collect_file(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& files)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		files.insert(dirent->name);
	}

	return true;
}

std::set<std::string> files_in_directory(const std::string& path)
{
	VALIDATE(!path.empty(), null_str);

	std::set<std::string> ret;
	::walk_dir(path, false, std::bind(
				&collect_file
				, _1, _2, std::ref(ret)));
	return ret;
}

char path_sep(bool standard)
{
#ifdef _WIN32
	return standard? '\\': '/';
#else
	return standard? '/': '\\';
#endif
}

std::string get_hdpi_name(const std::string& name, int hdpi_scale)
{
	size_t pos = name.rfind('.');
	if (pos == std::string::npos) {
		return name;
	}
	char flag[4] = {'@', static_cast<char>(hdpi_scale + 0x30), 'x', '\0'};

	std::string ret = name;
	ret.insert(pos, flag);
	return ret;
}

bool walk_dir(const std::string& rootdir, bool subfolders, const twalk_dir_function& fn)
{
	bool ret = true;
	std::stringstream ss;
	SDL_DIR* dir = SDL_OpenDir(rootdir.c_str());
	if (!dir) {
		return false;
	}
	SDL_dirent2* dirent;
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
				if (fn) {
					// shallow->deep: first call fn, then walk it.
					if (!fn(rootdir, dirent)) {
						ret = false;
						break;
					}
				}
				ss.str("");
				ss << rootdir << "/" << dirent->name;
				if (subfolders) {
					walk_dir(ss.str(), true, fn);
				}
			}
		} else {
			// file
			if (fn) {
				if (!fn(rootdir, dirent)) {
					ret = false;
					break;
				}
			}
		}
	}
	SDL_CloseDir(dir);

	return ret;
}

bool did_walk_white_extname(const std::string& dir, const SDL_dirent2* dirent, std::map<std::string, std::string>& files, const std::string& root, const std::vector<std::string>& white_extname)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		const std::string name = utils::lowercase(dirent->name);

		bool hit = false;
		for (std::vector<std::string>::const_iterator it = white_extname.begin(); it != white_extname.end(); ++ it) {
			const std::string& extname = *it;
			size_t pos = name.rfind(extname);
			if (pos != std::string::npos && pos + extname.size() == name.size()) {
				hit = true;
				break;
			}
		}
		if (hit) {
			// files example. root = c:/movie/test
			// [0]804s/0804001.jpeg   --> c:/movie/test/804s/0804001.jpEg
			// [1]804s/jz0804002.jpg  --> c:/movie/test/804s/Jz0804002.jpg
			// [2]806s/jz0804002.jpg  --> c:/movie/test/806s/JZ0804002.jpg
			// [3]0804001.jpeg  --> c:/movie/test/0804001.Jpeg
			// [4]jz0804002.jpg  --> c:/movie/test/JZ0804002.jpg
			// rules:
			// 1) characters in key must lowercase. and in value keep original.
			// 2) key is value cut by root.

			VALIDATE(dir.find(root) == 0, null_str);
			std::string key = dir.substr(root.size());
			if (key.empty()) {
				key = name;
			} else {
				key = key.substr(1) + "/" + name;
			}
			files.insert(std::make_pair(utils::lowercase(key), dir + "/" + dirent->name));
		}
	}
	return true;
}

bool copy_root_files(const std::string& src, const std::string& dst, std::set<std::string>* files, const std::function<bool(const std::string& src)>& filter)
{
	if (files) {
		files->clear();
	}

	if (src.empty() || dst.empty()) {
		return false;
	}

	SDL_bool ret = SDL_TRUE;
	std::stringstream src_ss, dst_ss;
	SDL_DIR* dir = SDL_OpenDir(src.c_str());
	if (!dir) {
		return false;
	}
	SDL_dirent2* dirent;
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (!SDL_DIRENT_DIR(dirent->mode)) {
			src_ss.str("");
			dst_ss.str("");
			src_ss << src << '/' << dirent->name;
			dst_ss << dst << '/' << dirent->name;
			if (filter && !filter(src_ss.str())) {
				continue;
			}
			ret = SDL_CopyFiles(src_ss.str().c_str(), dst_ss.str().c_str());

			if (!ret) {
				break;
			}
			if (files) {
				files->insert(dirent->name);
			}
		}
	}
	SDL_CloseDir(dir);

	return ret? true: false;
}

class tcompare_dir_param
{
public:
	tcompare_dir_param(const std::string& dir1, const std::string& dir2, int& recursion_count)
		: current_path1_(dir1)
		, current_path2_(dir2)
		, recursion_count_(recursion_count)
	{
		const char c = current_path1_.at(current_path1_.size() - 1);
		if (c == '\\' || c == '/') {
			current_path1_.erase(current_path1_.size() - 1);
		}

		if (!dir2.empty()) {
			const char c = current_path2_.at(current_path2_.size() - 1);
			if (c == '\\' || c == '/') {
				current_path2_.erase(current_path2_.size() - 1);
			}
		}
	}

	bool cb_compare_dir_explorer(const std::string& dir, const SDL_dirent2* dirent);

private:
	std::string current_path1_;
	std::string current_path2_;
	int& recursion_count_;
};

bool tcompare_dir_param::cb_compare_dir_explorer(const std::string& dir, const SDL_dirent2* dirent)
{
	bool compair = !current_path2_.empty();
	recursion_count_ ++;
	std::string path2 = current_path2_;
	if (compair) {
		path2.append("/");
		path2.append(dirent->name);
	}
	
	// compair
	if (SDL_DIRENT_DIR(dirent->mode)) {
		if (compair) {
			if (!is_directory(path2.c_str())) {
				return false;
			}
		}

		tcompare_dir_param cdp2(current_path1_ + "/" + dirent->name, path2, recursion_count_);
		if (!walk_dir(cdp2.current_path1_, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp2, _1, _2))) {
			return false;
		}

	} else if (compair) {
		if (!file_exists(path2)) {
			return false;
		}

	}
	return true;
}

bool compare_directory(const std::string& dir1, const std::string& dir2)
{
	int recursion1_count = 0;
	tcompare_dir_param cdp1(dir1, dir2, recursion1_count);
	bool fok = walk_dir(dir1, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp1, _1, _2));
	if (!fok) {
		return false;
	}

	int recursion2_count = 0;
	tcompare_dir_param cdp2(dir2, "", recursion2_count);
	fok = walk_dir(dir2, false, std::bind(&tcompare_dir_param::cb_compare_dir_explorer, &cdp2, _1, _2));

	if (recursion1_count != recursion2_count) {
		return false;
	}
	return true;
}

void path_summary(tpath_summary& summary, const char* name, bool first)
{
	SDL_DIR* dir;
	SDL_dirent2* dirent;
	char* full_name = NULL;
	int deep = 0;

	if (first) {
		memset(&summary, 0, sizeof(summary));
	}

	if (!name || !name[0]) {
		return;
	}
	if (SDL_IsFile(name)) {
		summary.files ++;
		summary.bytes += file_size(name);
		return;
	}

	dir = SDL_OpenDir(name);
	if (!dir) {
		return;
	}
	
	while ((dirent = SDL_ReadDir(dir))) {
		if (!full_name) {
			full_name = (char*)SDL_malloc(1024);
		}
		sprintf(full_name, "%s/%s", name, dirent->name);
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
				path_summary(summary, full_name, false);
			}
		} else {
			// file
			summary.files ++;
			summary.bytes += dirent->size;
		}
	}
	SDL_CloseDir(dir);
	if (full_name) {
		SDL_free(full_name);
	}

	if (!first) {
		summary.dirs ++;
	}
}

scoped_istream& scoped_istream::operator=(std::istream *s)
{
	delete stream;
	stream = s;
	return *this;
}

scoped_istream::~scoped_istream()
{
	delete stream;
}

scoped_ostream& scoped_ostream::operator=(std::ostream *s)
{
	delete stream;
	stream = s;
	return *this;
}

scoped_ostream::~scoped_ostream()
{
	delete stream;
}

#ifdef _WIN32
/**
 * conv_ansi_utf8()
 *   - Convert a string between ANSI encoding (for Windows filename) and UTF-8
 *  string &name
 *     - filename to be converted
 *  bool a2u
 *     - if true, convert the string from ANSI to UTF-8.
 *     - if false, reverse. (convert it from UTF-8 to ANSI)
 */
void conv_ansi_utf8(std::string &name, bool a2u) 
{
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
								   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return;
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return;
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
							wc, wlen) == 0) {
		delete [] wc;
		return;
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
								   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete [] wc;
		return;
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete [] wc;
		return;
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
						ac, alen, NULL, NULL);
	delete [] wc;
	if (ac == NULL) {
		return;
	}
	name = ac;
	delete [] ac;

	return;
}

std::string conv_ansi_utf8_2(const std::string &name, bool a2u) 
{
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
								   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return "";
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return "";
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
							wc, wlen) == 0) {
		delete [] wc;
		return "";
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
								   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete [] wc;
		return "";
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete [] wc;
		return "";
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
						ac, alen, NULL, NULL);
	delete [] wc;
	if (ac == NULL) {
		return "";
	}
	std::string result = ac;
	delete [] ac;

	return result;
}

const char* utf8_2_ansi(const std::string& str)
{
	static std::string ret;
	ret = conv_ansi_utf8_2(str, false);
	return ret.c_str();
}

const char* ansi_2_utf8(const std::string& str)
{
	static std::string ret;
	ret = conv_ansi_utf8_2(str, true);
	return ret.c_str();
}

#else

void conv_ansi_utf8(std::string &name, bool a2u)
{
}

std::string conv_ansi_utf8_2(const std::string &name, bool a2u)
{
	return name;
}

const char* utf8_2_ansi(const std::string& str)
{
	static std::string ret = str;
	return ret.c_str();
}

const char* ansi_2_utf8(const std::string& str)
{
	static const std::string ret = str;
	return ret.c_str();
}

#endif

std::string posix_strftime(const std::string& format, const struct tm& timeptr)
{
	char ret[128] = {'\0'};

#ifdef _WIN32
	// on windows, should use wcsftime, not strftime.
	/*
	cbMultiByte [in]
	  Size, in bytes, of the string indicated by the lpMultiByteStr parameter. 
	  Alternatively, this parameter can be set to -1 if the string is null-terminated. 
	  Note that, if cbMultiByte is 0, the function fails.

	  If this parameter is -1, the function processes the entire input string, including the terminating null character. 
	  Therefore, the resulting Unicode string has a terminating null character, and the length returned by the function includes this character.

	  If this parameter is set to a positive integer, the function processes exactly the specified number of bytes. 
	  If the provided size does not include a terminating null character, the resulting Unicode string is not null-terminated, 
	  and the returned length does not include this character.
	*/
	int len = MultiByteToWideChar(CP_UTF8, 0, format.c_str(), -1, nullptr, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, format.c_str(), -1, &wstr[0], len);

	wchar_t strDest[64];
	/*
	maxsize
	  Size of the strDest buffer, measured in characters (char or wchart_t).
	*/
	if (wcsftime(strDest, 64, wstr.c_str(), &timeptr) > 0) {
		WideCharToMultiByte(CP_UTF8, 0, strDest, -1, ret, sizeof(ret), NULL, NULL);
	}
#else
	strftime(ret, sizeof(ret), format.c_str(), &timeptr);
#endif

	return ret;
}

std::string format_time_ymd(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%b %d %y"), *tm_l);
	}

	return null_str;
}

std::string format_time_ymd2(time_t t, const char separator, bool align)
{
	tm* timeptr = localtime(&t);
	if (timeptr == NULL) {
		return null_str;
	}

	char buf[64];
	if (align) {
		sprintf(buf, "%04d%c%02d%c%02d", 1900 + timeptr->tm_year, separator, timeptr->tm_mon + 1, separator, timeptr->tm_mday);
	} else {
		sprintf(buf, "%d%c%d%c%d", 1900 + timeptr->tm_year, separator, timeptr->tm_mon + 1, separator, timeptr->tm_mday);
	}
	return buf;
}

std::string format_time_hms(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%H:%M:%S"), *tm_l);
	}

	return null_str;
}

std::string format_time_hm(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%H:%M"), *tm_l);
	}

	return null_str;
}

std::string format_time_date(time_t t)
{
	time_t curtime = time(NULL);
	const struct tm* timeptr = localtime(&curtime);
	if (timeptr == NULL) {
		return "";
	}

	const struct tm current_time = *timeptr;

	timeptr = localtime(&t);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm save_time = *timeptr;

	const char* format_string = _("%b %d %y");

	if (current_time.tm_year == save_time.tm_year) {
		const int days_apart = current_time.tm_yday - save_time.tm_yday;

		if (days_apart == 0) {
			// save is from today
			format_string = _("%H:%M:%S");
		} else if (days_apart > 0 && days_apart <= current_time.tm_wday) {
			// save is from this week. On chinese, %A cannot display. use %m%d instead.
			format_string = _("%A, %H:%M");
		} else {
			// save is from current year
			// format_string = _("%b %d");
			format_string = _("%A, %H:%M");
		}
	} else {
		// save is from a different year
		format_string = _("%b %d %y");
	}

	return posix_strftime(format_string, save_time);
}

std::string format_time_local(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%a %b %d %H:%M %Y"), *tm_l);
	}

	return null_str;
}

std::string format_time_ymdhms(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime("%Y-%m-%d %H:%M:%S", *tm_l);
	}

	return null_str;
}

std::string format_time_ymdhms2(time_t t)
{
	// yyyymmddhhmmss => 20181206154225
	const struct tm* timeptr = localtime(&t);
	char buf[64];
	sprintf(buf, "%04d%02d%02d%02d%02d%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
	return buf;
}

std::string format_time_ymdhms3(time_t t)
{
	// yyyy-mm-dd hh:mm:ss => 2018-12-06 15:42:25
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	char buf[64];
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
	return buf;
}

std::string format_time_dms(time_t t)
{
	// dd hh:mm => 06 15:42
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	
	utils::string_map symbols;
	symbols["mday"] = str_cast(timeptr->tm_mday);
	symbols["hour"] = str_cast(timeptr->tm_hour);
	symbols["min"] = str_cast(timeptr->tm_min);

	return vgettext2("number $mday hour $hour min $min", symbols);
}

std::string format_elapse_hms(time_t elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (day) {
		utils::string_map symbols;
		symbols["d"] = str_cast(day);
		ss << vgettext2("$d days", symbols);
	}
	if (hour) {
		ss << hour << _("time^h");
	}
	if (align || min) {
		if (align) {
			ss << std::setfill('0') << std::setw(2);
		}
		ss << min << _("time^m");
	}
	if (align || (sec != 0 || (day == 0 && hour == 0 && min == 0))) {
		if (align) {
			ss << std::setfill('0') << std::setw(2);
		}
		ss << sec << _("time^s");
	}
	
	return ss.str();
}

std::string format_elapse_hms2(time_t elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream strstr;
	if (day) {
		strstr << day << "-";
	}
	strstr << std::setfill('0') << std::setw(2) << hour << ":";
	strstr << std::setfill('0') << std::setw(2) << min << ":";
	strstr << std::setfill('0') << std::setw(2) << sec;
	
	return strstr.str();
}

std::string format_elapse_hm(time_t elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (align) {
		ss << std::setfill('0') << std::setw(2);
	}
	ss << hour << _("time^h");
	ss << std::setfill('0') << std::setw(2) << min << _("time^m");
	
	return ss.str();
}

std::string format_elapse_hm2(time_t elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (align) {
		ss << std::setfill('0') << std::setw(2);
	}
	ss << hour << ":";
	ss << std::setfill('0') << std::setw(2) << min;
	
	return ss.str();
}

std::string format_elapse_ms2(time_t elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream strstr;
	if (day) {
		utils::string_map symbols;
		symbols["d"] = str_cast(day);
		strstr << vgettext2("$d days", symbols) << " ";
	}
	if (hour) {
		strstr << std::setfill('0') << std::setw(2) << hour << ":";
	}
	strstr << std::setfill('0') << std::setw(2) << min << ":";
	strstr << std::setfill('0') << std::setw(2) << sec;
	
	return strstr.str();
}

std::string format_elapse_smsec(time_t elapse_ms, bool show_unit)
{
	if (elapse_ms < 0) {
		elapse_ms = 0;
	}
	int sec = elapse_ms / 1000;
	int msec = elapse_ms % 1000;

	std::stringstream ss;
	ss << sec << ".";
	ss << std::setfill('0') << std::setw(3) << msec;
	if (show_unit) {
		ss << _("time^s");
	}
	
	return ss.str();
}

std::string format_i64size2(int64_t size)
{
	std::stringstream ss;

	const int K = 1 << 10;
	const int M = 1 << 20;
	const int64_t G = 1 << 30;

	int integer, decimal;
	std::string unit;
	if (size >= 10 * G) {
		// 4,98 GB
		size = size / G + ((size % G)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "GB";

	} else if (size >= 10 * M) {
		// 4,98 MB
		size = size / M + ((size % M)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "MB";

	} else if (size >= 10 * K) {
		// 4,98 KB
		size = size / K + ((size % K)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "KB";

	} else {
		// 4,98 B
		integer = size / 1000;
		decimal = size % 1000;
		unit = "B";
	}

	if (integer) {
		ss << integer << "," << std::setfill('0') << std::setw(3);
	}
	ss << decimal << " " << unit;
	return ss.str();
}

std::string format_i64size(int64_t size)
{
	std::stringstream ss;

	const int K = 1 << 10;
	const int M = 1 << 20;
	const int64_t G = 1 << 30;

	int integer, decimal;
	bool less_1k = false;
	std::string unit;
	if (size >= G) {
		// 49.08 GB
		size = (int64_t)(1.0 * size / G * 1000);
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "GB";

	} else if (size >= M) {
		// 49.08 MB
		size = size * 1000 / M;
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "MB";

	} else if (size >= K) {
		// 49.08 KB
		size = size * 1000 / K;
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "KB";

	} else {
		// 498 B
		integer = size / 1000;
		decimal = size % 1000;
		unit = "B";
		less_1k = true;
	}

	if (integer) {
		if (!less_1k) {
			ss << integer << "." << std::setfill('0') << std::setw(2);
		} else {
			ss << integer << "," << std::setfill('0') << std::setw(3);
		}
	}
	ss << decimal << " " << unit;
	return ss.str();
}

int days_in_month(int year, int month)
{
	VALIDATE(year > 0 && month >= 1 && month <= 12, null_str);
	if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
		return 31;
	} else if (month == 4 || month == 6 || month == 9 || month == 11) {
		return 30;
	} else if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
		return 29;
	}
	return 28;
}

int64_t datetime_str_2_ts(const std::string& datetime)
{
	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));
	bool align = false;

	if (align) {
		const std::string example = "2018-04-20 19:54:12";
		if (datetime.size() != example.size()) {
			return 0;
		}
		sscanf(datetime.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday, &tm1.tm_hour, &tm1.tm_min, &tm1.tm_sec);        

	} else {
		std::vector<std::string> vstr = utils::split(datetime, ' ');
		if (vstr.size() != 2) {
			return 0;
		}
		std::vector<std::string> vstr2 = utils::split(vstr[0], '-');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_year = utils::to_int(vstr2[0]);
		tm1.tm_mon = utils::to_int(vstr2[1]);
		tm1.tm_mday = utils::to_int(vstr2[2]);
		if (tm1.tm_year <= 1900 || tm1.tm_mon <= 0 || tm1.tm_mon > 12 || tm1.tm_mday <= 0) {
			return 0;
		}
		const int days = days_in_month(tm1.tm_year, tm1.tm_mon);
		if (tm1.tm_mday > days) {
			return 0;
		}

		vstr2 = utils::split(vstr[1], ':');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_hour = utils::to_int(vstr2[0]);
		tm1.tm_min = utils::to_int(vstr2[1]);
		tm1.tm_sec = utils::to_int(vstr2[2]);
		if (tm1.tm_hour < 0 || tm1.tm_hour >= 24 || tm1.tm_min < 0 || tm1.tm_min >= 60 || tm1.tm_sec < 0 || tm1.tm_sec >= 60) {
			return 0;
		}
	}
	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst =-1;
  
	return mktime(&tm1);
}

int64_t date_str_2_ts(const std::string& date)
{
	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));
	bool align = false;

	if (align) {
		const std::string example = "2018-04-20";
		if (date.size() != example.size()) {
			return 0;
		}
		sscanf(date.c_str(), "%4d-%2d-%2d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);        
	} else {
		std::vector<std::string> vstr2 = utils::split(date, '-');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_year = utils::to_int(vstr2[0]);
		tm1.tm_mon = utils::to_int(vstr2[1]);
		tm1.tm_mday = utils::to_int(vstr2[2]);
		if (tm1.tm_year <= 1900 || tm1.tm_mon <= 0 || tm1.tm_mon > 12 || tm1.tm_mday <= 0) {
			return 0;
		}
		const int days = days_in_month(tm1.tm_year, tm1.tm_mon);
		if (tm1.tm_mday > days) {
			return 0;
		}
	}
	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst =-1;
  
	return mktime(&tm1);
}

// replace src_str in src_file, and generate to dst_file.
bool file_replace_string(const std::string& src_file, const std::string& dst_file, const std::vector<std::pair<std::string, std::string> >& replaces)
{
	tfile file(src_file,  GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}
	fsize = file.replace_string(fsize, replaces, NULL);
	
	// write data to new file
	tfile file2(dst_file,  GENERIC_WRITE, CREATE_ALWAYS);
	if (file2.valid()) {
		posix_fseek(file2.fp, 0);
		posix_fwrite(file2.fp, file.data, fsize);
	}

	return true;
}

bool file_replace_string(const std::string& src_file, const std::pair<std::string, std::string>& replace)
{
	std::vector<std::pair<std::string, std::string> > replaces;
	replaces.push_back(replace);
	return file_replace_string(src_file, replaces);
}

// replace src_str in src_file, and generate to dst_file.
bool file_replace_string(const std::string& src_file, const std::vector<std::pair<std::string, std::string> >& replaces)
{
	tfile file(src_file,  GENERIC_WRITE, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}

	bool dirty = false;
	fsize = file.replace_string(fsize, replaces, &dirty);

	if (dirty) {
		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
		file.truncate(fsize);
	}
	return true;
}

//
// encrypt/decrypt
//
// if return isn't NULL, caller need free heap.
char* saes_encrypt_heap(const char* ptext, int size, unsigned char* key)
{
	if (size == 0) {
		return NULL;
	}
	char* ctext = (char*)malloc((size & 1)? size + 1: size);
	saes_encrypt_stream((const unsigned char*)ptext, size, key, ctext);
	return ctext;
}

// if return isn't NULL, caller need free heap.
char* saes_decrypt_heap(const char* ctext, int size, unsigned char* key)
{
	if (size == 0 || (size & 1)) {
		return NULL;
	}
	char* ptext = (char*)malloc(size);
	saes_decrypt_stream((const unsigned char*)ctext, size, key, ptext);
	return ptext;
}

tsaes_encrypt::tsaes_encrypt(const char* ctext, int s, unsigned char* key)
	: size(0)
{
	buf = saes_encrypt_heap(ctext, s, key);
	if (buf) {
		size = s & 1? s + 1: s;
	}
}

tsaes_decrypt::tsaes_decrypt(const char* ptext, int s, unsigned char* key)
	: size(s)
{
	buf = saes_decrypt_heap(ptext, size, key);
	if (buf && !buf[size - 1]) {
		size --;
	}
}

tfile::tfile(const std::string& file, uint32_t desired_access, uint32_t create_disposition)
	: fp(INVALID_FILE)
	, data(NULL)
	, data_size(0)
	, truncate_size_(0)
	, can_truncate_(create_disposition == CREATE_ALWAYS && (desired_access & GENERIC_WRITE))
{
	VALIDATE(create_disposition == OPEN_EXISTING || create_disposition == CREATE_ALWAYS, null_str);
	// CREATE_ALWAYS: create always. if file exist, it will truncate to 0 immediately.
	posix_fopen(file.c_str(), desired_access, create_disposition, fp);
}

void tfile::close()
{
	if (data) {
		free(data);
		data = NULL;
		data_size = 0;
	}
	if (fp == INVALID_FILE) {
		return;
	}

	if (!can_truncate_ && truncate_size_ > 0) {
		// when visual studio is using apps.sln, cannot delete it.
		// so must truncate apps.sln.
		// now SDL doesn't support truncate file. if I add, it necessary modify more.
		// in windows, use SetEndOfFile directly.
		// if linux-like, use ftruncate to truncate a file.
		int64_t fsize = posix_fsize(fp);
		if (fsize > truncate_size_) {
#ifdef _WIN32
			posix_fseek(fp, truncate_size_);
			SetEndOfFile(fp->hidden.windowsio.h);
#else
			// http://www.cnblogs.com/sky-heaven/p/4663630.html
			fflush(fp->hidden.stdio.fp);
			ftruncate(fileno(fp->hidden.stdio.fp), truncate_size_);
			rewind(fp->hidden.stdio.fp);
#endif
		}
	}

	posix_fclose(fp);
	fp = INVALID_FILE;
}

void tfile::resize_data(int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size) {
		char* tmp = (char*)malloc(size);
		if (data) {
			if (vsize) {
				memcpy(tmp, data, vsize);
			}
			free(data);
		}
		data = tmp;
		data_size = size;
	}
}

int tfile::replace_span(const int start, int original_size, const char* new_data, int new_size, const int fsize)
{
	VALIDATE(start >= 0, null_str);

	int new_fsize = fsize + new_size - original_size;
	resize_data(new_fsize + 1, fsize); // extra 1 is for string terminate: '\0'.

	if (new_size <= original_size) {
		memcpy(data + start + new_size, data + start + original_size, fsize - start - original_size);
	} else {
		memmove(data + start + new_size, data + start + original_size, fsize - start - original_size);
	}
	if (new_data) {
		memcpy(data + start, new_data, new_size);
	}

	data[new_fsize] = '\0';
	return new_fsize; 
}

int64_t tfile::read_2_data(const int reserve_pre_bytes, const int reserve_post_bytes)
{
	VALIDATE(reserve_pre_bytes >= 0 && reserve_post_bytes >= 0, null_str);
	if (!valid()) {
		return 0;
	}
	int64_t fsize = posix_fsize(fp);
	if (!fsize) {
		return 0;
	}
	VALIDATE(fsize, null_str);

	posix_fseek(fp, 0);
	resize_data(reserve_pre_bytes + fsize + 1 + reserve_post_bytes);
	posix_fread(fp, data + reserve_pre_bytes, fsize);
	// let easy change to std::string.
	data[reserve_pre_bytes + fsize] = '\0';
	return fsize;
}

// caller require fill data before it.
// only modify data. doesn't touch file data.
int tfile::replace_string(int fsize, const std::vector<std::pair<std::string, std::string> >& replaces, bool* dirty)
{
	VALIDATE(data && data_size >= fsize, null_str);
	resize_data(fsize + 1, fsize);
	data[fsize] = '\0';

	const char* start2 = data; // search from 0.
	if (utils::bom_magic_started((const uint8_t*)data, fsize)) {
		start2 += BOM_LENGTH;
	}

	if (dirty) {
		*dirty = false;
	}
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
		const std::pair<std::string, std::string>& replace = *it;
		if (replace.first != replace.second) {
			const std::string& src = replace.first;
			const std::string& dst = replace.second;
			const char* ptr = strstr(start2, src.c_str());
			while (ptr) {
				// replace_span may remalloc data!
				int pos = ptr - data;
				fsize = replace_span(pos, src.size(), dst.c_str(), dst.size(), fsize);
				// replace next
				ptr = strstr(data + pos + dst.size(), src.c_str());
				if (dirty) {
					*dirty = true;
				}
			}
		}
	}

	return fsize;
}

tbakreader::tbakreader(const std::string& src_name, bool use_backup, const std::function<bool (tfile& file, int64_t dsize, bool bak)>& did_read)
	: tfile(src_name, GENERIC_READ, OPEN_EXISTING)
	, src_name_(src_name)
	, use_backup_(use_backup)
	, did_read_(did_read)
{
	if (use_backup) {
		bak_name_ = src_name_ + ".bak";
	}
}

int64_t tbakreader::read()
{
	int64_t fsize = 0;
	if (valid()) {
		fsize = posix_fsize(fp);
	}
	if (fsize > 0) {
		if (did_read_ == NULL) {
			int64_t fsize2 = read_2_data();
			if (fsize2 == fsize) {
				return fsize;
			}
		} else {
			posix_fseek(fp, 0);
			if (did_read_(*this, fsize, false)) {
				return fsize;
			}
		}
	}

	if (!use_backup_) {
		return 0;
	}

	VALIDATE(!bak_name_.empty(), null_str);
	tfile bak(bak_name_, GENERIC_READ, OPEN_EXISTING);
	fsize = 0;
	if (bak.valid()) {
		fsize = posix_fsize(bak.fp);
	}
	if (fsize > 0) {
		if (did_read_ == NULL) {
			int64_t fsize2 = bak.read_2_data();
			if (fsize2 == fsize) {
				return fsize;
			}
		} else {
			posix_fseek(bak.fp, 0);
			if (did_read_(bak, fsize, true)) {
				return fsize;
			}
		}
	}
	return 0;
}

int64_t tsha1reader::read()
{
	int64_t fsize = verify_sha1(*this);
	if (fsize > 0) {
		posix_fseek(fp, 0);
		if (did_read_(*this, fsize - SHA_DIGEST_LENGTH, false)) {
			return fsize;
		}
	}

	if (!use_backup_) {
		return 0;
	}

	VALIDATE(!bak_name_.empty(), null_str);
	tfile bak(bak_name_, GENERIC_READ, OPEN_EXISTING);
	fsize = verify_sha1(bak);
	if (fsize > 0) {
		posix_fseek(bak.fp, 0);
		if (did_read_(bak, fsize - SHA_DIGEST_LENGTH, true)) {
			return fsize;
		}
	}
	return 0;
}

int64_t tsha1reader::verify_sha1(tfile& file)
{
	if (!file.valid()) {
		return 0;
	}
	posix_fseek(file.fp, 0);
	int64_t fsize = posix_fsize(file.fp);
	posix_fseek(file.fp, 0);
	if (fsize < SHA_DIGEST_LENGTH) {
		return 0;
	}
	// allow dsize = 0.

	const int one_block = 2 * 1024 * 1024;
	file.resize_data(one_block);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	int64_t pos = 0;
	const int64_t end = fsize - SHA_DIGEST_LENGTH;
	while (pos < end) {
		int bytes = one_block;
		if (pos + bytes > end) {
			bytes = end - pos;
		}
		posix_fread(file.fp, file.data, bytes);
		SHA1_Update(&ctx, file.data, bytes);

		pos += bytes;
	}

	SHA1_Final(md, &ctx);
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	posix_fread(file.fp, file.data, SHA_DIGEST_LENGTH);
	posix_fseek(file.fp, 0);

	return memcmp(md, file.data, SHA_DIGEST_LENGTH) == 0? fsize: 0;
}

void extract_file_data(const std::string& src, const std::string& dst, int64_t from, int64_t size)
{
	tfile file(src, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);

	int64_t fsize = posix_fsize(file.fp);
	if (!fsize) {
		return;
	}

	tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(dst_file.valid(), null_str);

	const int block_size = 4096;
	int actual_size;
	file.resize_data(block_size);

	int64_t stop = fsize <= from + size? fsize: from + size;
	posix_fseek(file.fp, from);

	int64_t pos = from;
	while (pos < stop) {
		actual_size = block_size <= stop - pos? block_size: stop - pos;
		posix_fread(file.fp, file.data, actual_size);
		posix_fwrite(dst_file.fp, file.data, actual_size);
		pos += actual_size;
	}
}

void single_open_copy(const std::string& src, const std::string& dst)
{
	// in common, to effect, copy file use "little" block, it require open both file at same time.
	// if shutdown, it maybe breakdown both file.
	// this function open at most one file at same time. it require more memory.
	std::unique_ptr<tfile> file;
	file.reset(new tfile(src, GENERIC_READ, OPEN_EXISTING));
	int fsize = file->read_2_data();
	if (fsize == 0) {
		return;
	}
	std::string data(file->data, fsize);
	file.reset();

	file.reset(new tfile(dst, GENERIC_WRITE, CREATE_ALWAYS));
	VALIDATE(file->valid(), null_str);
	posix_fwrite(file->fp, data.c_str(), fsize);
	file.reset();
}

tbakwriter::tbakwriter(const std::string& src_name, bool use_backup, const std::function<bool (tfile& file)>& did_write, const std::function<bool ()>& did_pre_write)
	: src_name_(src_name)
	, use_backup_(use_backup)
	, did_pre_write_(did_pre_write)
	, did_write_(did_write)
{
	VALIDATE(did_write_ != NULL, null_str);
	if (use_backup) {
		bak_name_ = src_name_ + ".bak";
	}
}

void tbakwriter::write()
{
	bool continue_write = true;
	if (did_pre_write_ != NULL) {
		continue_write = did_pre_write_();
	}

	if (continue_write) {
		tfile file(src_name_, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);

		if (!did_write_(file)) {
			file.close();
			SDL_DeleteFiles(src_name_.c_str());
			return;
		}
	} else {
		return;
	}

	if (use_backup_) {
		tfile file(src_name_, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = posix_fsize(file.fp);
		file.close();

		if (fsize > 0) {
			if (fsize <= INT64_C(4) * 1024 * 1024) {
				// make sure at more one file for writting.
				single_open_copy(src_name_, bak_name_);
			} else {
				SDL_CopyFiles(src_name_.c_str(), bak_name_.c_str());
			}
		}
	}
}

void tsha1writer::write()
{
	{
		tfile file(src_name_, GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);

		if (!did_write_(file)) {
			file.close();
			SDL_DeleteFiles(src_name_.c_str());
			return;
		}

		uint8_t md[SHA_DIGEST_LENGTH];
		memset(md, 0, sizeof(md));
		SHA_CTX ctx;
		SHA1_Init(&ctx);

		posix_fseek(file.fp, 0);
		const int one_block = 2 * 1024 * 1024;
		int read_bytes = one_block;
		file.resize_data(one_block);
		while (read_bytes == one_block) {
			read_bytes = posix_fread(file.fp, file.data, one_block);
			if (read_bytes > 0) {
				SHA1_Update(&ctx, file.data, read_bytes);
			}
		}

		SHA1_Final(md, &ctx);
		OPENSSL_cleanse(&ctx, sizeof(ctx));
		posix_fwrite(file.fp, md, SHA_DIGEST_LENGTH);
	}

	if (use_backup_) {
		bool require_copy = false;
		{
			tsha1reader reader(src_name_, false, NULL);
			require_copy = reader.verify_sha1() > 0;
		}
		if (require_copy) {
			// make sure at more one file for writting.
			SDL_CopyFiles(src_name_.c_str(), bak_name_.c_str());
		}
	}
}

void convert_yuv_2_argb(const std::string& filename, const int width, const int height, int format, const std::string& out_png)
{
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (format == libyuv::FOURCC_YUY2) {
		VALIDATE(fsize == width * height * 2, null_str);
	} else if (format == libyuv::FOURCC_NV12 || format == libyuv::FOURCC_NV21) {
		VALIDATE(fsize == width * height * 3 / 2, null_str);
	}

	surface res = SDL_CreateRGBSurface(0, width, height, 4 * 8,
			0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	uint8_t* pixels = reinterpret_cast<uint8_t*>(res->pixels);

	if (format == libyuv::FOURCC_YUY2) {
		libyuv::YUY2ToARGB((uint8_t*)(file.data), width * 2,
               pixels, 4 * width,
               width, height);
	} else if (format == libyuv::FOURCC_NV12) {
		libyuv::NV12ToARGB((uint8_t*)(file.data), width,
               (uint8_t*)(file.data) + width * height, width,
               pixels, 4 * width,
               width, height);

	} else {
		VALIDATE(format == libyuv::FOURCC_NV21, null_str);

		libyuv::NV21ToARGB((uint8_t*)(file.data), width,
            (uint8_t*)(file.data) + width * height, width,
            pixels, 4 * width,
            width, height);
	}
	imwrite(res, out_png);
}

int posix_align_ceil2(int dividend, int divisor)
{
	int remainer = dividend % divisor;
	return dividend + (remainer? divisor - remainer: 0);
}

int posix_pages(int dividend, int divisor)
{
	VALIDATE(dividend >= 0 && divisor > 0, null_str);
	return dividend / divisor + (dividend % divisor? 1: 0);
}

void SDL_SimplerMB(const char* fmt, ...)
{
	char text[512];
	va_list ap;
    int retval;

    va_start(ap, fmt);
    retval = SDL_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, game_config::get_app_msgstr(null_str).c_str(), text, nullptr);
}