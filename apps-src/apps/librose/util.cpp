/*
   Copyright (C) 2005 - 2015 by Philippe Plantier <ayin@anathas.org>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  String-routines - Templates for lexical_cast & lexical_cast_default.
 */

#include "util.hpp"
#include "util_c.h"
#include "rose_config.hpp"
#include "version.hpp"
#include "wml_exception.hpp"

#include <SDL_video.h>
#include <cstdlib>
template<>
size_t lexical_cast<size_t, const std::string&>(const std::string& a)
{
	char* endptr;
	size_t res = strtoul(a.c_str(), &endptr, 10);

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

#ifndef MSVC_DO_UNIT_TESTS
template<>
size_t lexical_cast<size_t, const char*>(const char* a)
{
	char* endptr;
	size_t res = strtoul(a, &endptr, 10);

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}
#endif
template<>
size_t lexical_cast_default<size_t, const std::string&>(const std::string& a, size_t def)
{
	if(a.empty()) {
		return def;
	}

	char* endptr;
	size_t res = strtoul(a.c_str(), &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}
template<>
size_t lexical_cast_default<size_t, const char*>(const char* a, size_t def)
{
	if(*a == '\0') {
		return def;
	}

	char* endptr;
	size_t res = strtoul(a, &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}
template<>
long lexical_cast<long, const std::string&>(const std::string& a)
{
	char* endptr;
	long res = strtol(a.c_str(), &endptr, 10);

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
long lexical_cast<long, const char*>(const char* a)
{
	char* endptr;
	long res = strtol(a, &endptr, 10);

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}
template<>
long lexical_cast_default<long, const std::string&>(const std::string& a, long def)
{
	if(a.empty()) {
		return def;
	}

	char* endptr;
	long res = strtol(a.c_str(), &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}
template<>
long lexical_cast_default<long, const char*>(const char* a, long def)
{
	if(*a == '\0') {
		return def;
	}

	char* endptr;
	long res = strtol(a, &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}
template<>
int lexical_cast<int, const std::string&>(const std::string& a)
{
	char* endptr;
	int res = strtol(a.c_str(), &endptr, 10);

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

#ifndef MSVC_DO_UNIT_TESTS
template<>
int lexical_cast<int, const char*>(const char* a)
{
	char* endptr;
	int res = strtol(a, &endptr, 10);

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}
#endif

template<>
int lexical_cast_default<int, const std::string&>(const std::string& a, int def)
{
	if(a.empty()) {
		return def;
	}

	char* endptr;
	int res = strtol(a.c_str(), &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

template<>
int lexical_cast_default<int, const char*>(const char* a, int def)
{
	if(*a == '\0') {
		return def;
	}

	char* endptr;
	int res = strtol(a, &endptr, 10);

	if (*endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

template<>
double lexical_cast<double, const std::string&>(const std::string& a)
{
	char* endptr;
	double res = strtod(a.c_str(), &endptr);

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
double lexical_cast<double, const char*>(const char* a)
{
	char* endptr;
	double res = strtod(a, &endptr);

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
double lexical_cast_default<double, const std::string&>(const std::string& a, double def)
{
	char* endptr;
	double res = strtod(a.c_str(), &endptr);

	if (a.empty() || *endptr != '\0') {
		return def;;
	} else {
		return res;
	}
}

template<>
double lexical_cast_default<double, const char*>(const char* a, double def)
{
	char* endptr;
	double res = strtod(a, &endptr);

	if (*a == '\0' || *endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

template<>
float lexical_cast<float, const std::string&>(const std::string& a)
{
	char* endptr;
	float res = static_cast<float>(strtod(a.c_str(), &endptr));

	if (a.empty() || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}

template<>
float lexical_cast<float, const char*>(const char* a)
{
	char* endptr;
	float res = static_cast<float>(strtod(a, &endptr));

	if (*a == '\0' || *endptr != '\0') {
		throw bad_lexical_cast();
	} else {
		return res;
	}
}
template<>
float lexical_cast_default<float, const std::string&>(const std::string& a, float def)
{
	char* endptr;
	float res = static_cast<float>(strtod(a.c_str(), &endptr));

	if (a.empty() || *endptr != '\0') {
		return def;;
	} else {
		return res;
	}
}

template<>
float lexical_cast_default<float, const char*>(const char* a, float def)
{
	char* endptr;
	float res = static_cast<float>(strtod(a, &endptr));

	if (*a == '\0' || *endptr != '\0') {
		return def;
	} else {
		return res;
	}
}

tdisable_idle_lock::tdisable_idle_lock()
	: original_(SDL_IsScreenSaverEnabled())
{
	SDL_DisableScreenSaver();
}

tdisable_idle_lock::~tdisable_idle_lock()
{
	if (original_) {
		SDL_EnableScreenSaver();
	} else {
		SDL_DisableScreenSaver();
	}
}

thome_indicator_lock::thome_indicator_lock(int level)
	: original_(0)
{
	// default is 0
	VALIDATE(level == 1 || level == 2, null_str);
	const char* ptr = SDL_GetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR);
	const std::string old = ptr != nullptr? ptr: "0";
	original_ = utils::to_int(old, false);

	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, str_cast(level).c_str());
}

thome_indicator_lock::~thome_indicator_lock()
{
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, str_cast(original_).c_str());
}

std::ostream &operator<<(std::ostream &stream, const tpoint& point)
{
	stream << point.x << ',' << point.y;
	return stream;
}

SDL_Rect lua_unpack_rect(uint64_t u64)
{
	// [(SHRT_MIN)-32768, (SHRT_MAX)32767]
	uint32_t lo32 = posix_lo32(u64);
	uint32_t hi32 = posix_hi32(u64);

	int x = (short)posix_lo16(lo32);
	int y = (short)posix_hi16(lo32);
	int w = (short)posix_lo16(hi32);
	int h = (short)posix_hi16(hi32);
	return SDL_Rect{x, y, w, h};
}

const tpoint null_point(construct_null_coordinate());

// same as live555's seqNumLT(u_int16_t s1, u_int16_t s2)
bool uint16_lt(uint16_t s1, uint16_t s2)
{
	// ==> s1 < s2?
	// a 'less-than' on 16-bit sequence numbers
	int diff = s2 - s1;
	if (diff > 0) {
		return (diff < 0x8000);
	} else if (diff < 0) {
		return (diff < -0x8000);
	} else { // diff == 0
		return false;
	}
}

uint16_t uint16_minus(uint16_t s1, uint16_t s2)
{
	// ==> think s1 always >= s2, calculate s1 - s2?
	uint32_t minuend = s1;
	if (s1 == s2) {
		return 0;
	}
	if (s1 < s2) {
		minuend += 0x10000;
	}
	return minuend - s2;
}

// Convert a UCS-2 value to a UTF-8 string
std::string UCS2_to_UTF8(const wchar_t ch)
{
	uint8_t dst[4] = {0, 0, 0, 0};

    if (ch <= 0x7F) {
        dst[0] = (Uint8) ch;
    } else if (ch <= 0x7FF) {
        dst[0] = 0xC0 | (Uint8) ((ch >> 6) & 0x1F);
        dst[1] = 0x80 | (Uint8) (ch & 0x3F);
    } else {
        dst[0] = 0xE0 | (Uint8) ((ch >> 12) & 0x0F);
        dst[1] = 0x80 | (Uint8) ((ch >> 6) & 0x3F);
        dst[2] = 0x80 | (Uint8) (ch & 0x3F);
    }
	return (char*)dst;
}

bool os_is_mobile(int os)
{
	return os == os_ios || os == os_android;
}

// from <freerdp>/include/freerdp/constants.h
// osMajorType (2 bytes): A 16-bit, unsigned integer. The type of platform.
/**
 * OSMajorType
 */
#define OSMAJORTYPE_UNSPECIFIED 0x0000
#define OSMAJORTYPE_WINDOWS 0x0001
#define OSMAJORTYPE_OS2 0x0002
#define OSMAJORTYPE_MACINTOSH 0x0003
#define OSMAJORTYPE_UNIX 0x0004
#define OSMAJORTYPE_IOS 0x0005
#define OSMAJORTYPE_OSX 0x0006
#define OSMAJORTYPE_ANDROID 0x0007

int freerdp_os()
{
	if (game_config::os == os_windows) {
		return OSMAJORTYPE_WINDOWS;

	} else if (game_config::os == os_ios) {
		return OSMAJORTYPE_IOS;

	} else if (game_config::os == os_android) {
		return OSMAJORTYPE_ANDROID;

	}
	return OSMAJORTYPE_UNSPECIFIED;
}

int gethostname2(char* name, int namelen)
{
	std::stringstream ss;
	if (game_config::os == os_ios) {
		ss << "ios";
		
	} else if (game_config::os == os_android) {
		ss << "android";

	} else if (game_config::os == os_windows) {
		ss << "windows";

	} else {
		// now only support above os.
		// VALIDATE(false, null_str);
		ss << "generic";
	}

	ss << "_" << game_config::version.str(true);

	strcpy(name, ss.str().c_str());
	// return value same as gethostname: 0 else SOCKET_ERROR
	return 0;
}

int hostname_2_os(const char* name)
{
	if (name == nullptr) {
		return os_windows;
	}
	if (strstr(name, "ios_") == name) {
		return os_ios;
	}
	if (strstr(name, "android_") == name) {
		return os_android;
	}
	return os_windows;
}

bool hostname_is_mobile(const char* name)
{
	int os = hostname_2_os(name);
	return os_is_mobile(os);
}

const char* app_version_from_hostname(const char* name, int* len_ptr)
{
	if (len_ptr != nullptr) {
		*len_ptr = 0;
	}
	if (name == nullptr) {
		return nullptr;
	}
	const int len = strlen(name);
	const char* result = strchr(name, '_');
	if (result == nullptr) {
		return nullptr;
	}
	result ++;
	if (len_ptr != nullptr) {
		const char* end = strchr(result, '_');
		if (end == nullptr) {
			*len_ptr = len - (result - name);
		} else {
			*len_ptr = end - result;
		}
	}
	return result;
}