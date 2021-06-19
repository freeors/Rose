/* $Id: string_utils.cpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Copyright (C) 2005 - 2013 by Philippe Plantier <ayin@anathas.org>


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
 * Various string-routines.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/parser.hpp"
#include "util.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"

#include <algorithm>
#include <iomanip>

#include <url/gurl.h>

#include <openssl/aes.h>
#include <openssl/sha.h>

#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>

const std::string null_str = "";

#ifndef _WIN32
void CoCreateGuid(GUID* pguid)
{
	VALIDATE(false, null_str); // now CoCreateGuid only call when windows.
	memset(pguid, 0, sizeof(GUID));
}
#endif

std::string trtsp_settings::to_preferences() const
{
	VALIDATE(valid(), null_str);
	std::stringstream ss;
	ss << name << "," << (tcp? "tcp": "udp") << "," << url;
	return ss.str();
}

namespace utils {

const std::string ellipsis = "...";

const std::string unicode_minus = "-";
const std::string unicode_en_dash = "-";
const std::string unicode_em_dash = "-";
const std::string unicode_figure_dash = "-";
const std::string unicode_multiplication_sign = "-";
const std::string unicode_bullet = "-";

bool to_bool(const std::string& str, bool def)
{
	if (str.empty()) {
		return def;
	}

	//  priority of use 'no'.
	if (str == "no" || str == "false") {
		return false;
	}
	return true;
}

int to_int(const std::string& str, bool hex)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	int ret = 0;
	if (hex || (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X'))) {
		// hex
		for (int at = hex? 0: 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			at = 1;
			flag = -1;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

uint32_t to_uint32(const std::string& str, bool hex)
{
	return uint32_t(to_int(str, hex));
}

uint64_t to_uint64(const std::string& str, bool hex)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	uint64_t ret = 0;
	if (hex || (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X'))) {
		// hex
		for (int at = hex? 0: 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			return 0;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

config to_config(const std::string& str)
{
	config cfg;
	try {
		::read(cfg, str);
	} catch(config::error& e) {
		SDL_Log("%s", e.message.c_str());
		cfg.clear();
	} catch (twml_exception& e) {
		SDL_Log("%s", e.user_message.c_str());
		cfg.clear();
	} 
	return cfg;
}

std::wstring to_wstring(const std::string& src)
{
	const char* tocode = sizeof(wchar_t) == 2? "UTF-16LE": "UTF-32LE";
	// param4 Inbytesleft indicates the number of input bytes to be used for conversion. 
	// '+1' make sure that converted string ends with '\0'. 
	// For std::string, the string returned by the c_str() always ends in a \0.
	// if result isn't nullptr, use SDL_free to free.
	wchar_t* wchar_ptr = (wchar_t *)SDL_iconv_string(tocode, "UTF-8", src.c_str(), src.size() + 1);
	std::wstring result(wchar_ptr);
	SDL_free(wchar_ptr);
	return result;
}

void test_from_double()
{
	std::vector<double> vec;
	vec.push_back(-9328.000019);
	vec.push_back(-9328.00001);
	vec.push_back(-9328.00002);
	vec.push_back(-9328.0001);
	vec.push_back(-9328.001);
	vec.push_back(-9328.01);
	vec.push_back(-9328.1);
	vec.push_back(-9328.00009);
	vec.push_back(-9328.999999);
	vec.push_back(0);
	vec.push_back(9328.000019);
	vec.push_back(9328.00001);
	vec.push_back(9328.00002);
	vec.push_back(9328.0001);
	vec.push_back(9328.001);
	vec.push_back(9328.01);
	vec.push_back(9328.1);
	vec.push_back(9328.00009);
	vec.push_back(9328.999999);

	vec.push_back(0.000019);
	vec.push_back(0.00001);
	vec.push_back(0.00002);
	vec.push_back(0.0001);
	vec.push_back(0.001);
	vec.push_back(0.01);
	vec.push_back(0.1);
	vec.push_back(0.00009);
	vec.push_back(0.0101);
	vec.push_back(0.999999);
	vec.push_back(-0.000019);
	vec.push_back(-0.00001);
	vec.push_back(-0.00002);
	vec.push_back(-0.0001);
	vec.push_back(-0.001);
	vec.push_back(-0.01);
	vec.push_back(-0.1);
	vec.push_back(-0.00009);
	vec.push_back(-0.0101);
	vec.push_back(-0.999999);

	for (std::vector<double>::const_iterator it = vec.begin(); it != vec.end(); ++ it) {
		double v = *it;
		std::string str = utils::from_double(v);
		SDL_Log("%.7f ==> %s", v, str.c_str());
	}
}

std::string from_double(double v)
{
	// max 5 decimal, but #5 maybe -1.
	// -9328.00002(memory:-9328.0000199) ==> -9328.00001
	char buf[40];
	char* ptr = buf;
	size_t maxlen = sizeof(buf);

	double integered = 0.0;
	int decimal = 0;
	if (v >= 0.0) {
		integered = SDL_floor(v);
		decimal = (v - integered) * 1000000;
	} else {
		integered = SDL_ceil(v);
		decimal = (v - integered) * -1000000;
		ptr[0] = '-';
		ptr ++;
		maxlen --;
		// if v is -0.01, intergered will be 0. but it is negative, and 0 hasn't -0.
		// so print (-1 + intergered)
		integered *= -1;
	}

	if ((decimal % 10 == 9) && decimal != 999999) {
		// Although the literal value is -9328.00002, the value stored in the memory may be -9328.000019.
		// 000019 + 1 ==> 00002;
		decimal ++;
	}
	decimal /= 10;

	if (decimal == 0) {
		SDL_snprintf(ptr, maxlen, "%.0f", integered);
		return buf;
	}
	// 99999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%05d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 9999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%04d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%03d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 99
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%02d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 9
	SDL_snprintf(ptr, maxlen, "%.0f.%d", integered, decimal);
	return buf;
}

// 0x7301a8c0 => 192.168.1.115
std::string from_ipv4(uint32_t ipv4)
{
	char buf[32];
	SDL_snprintf(buf, sizeof(buf), "%i.%i.%i.%i",
		posix_lo8(posix_lo16(ipv4)), posix_hi8(posix_lo16(ipv4)),
		posix_lo8(posix_hi16(ipv4)), posix_hi8(posix_hi16(ipv4)));
	return buf;
}

// 192.168.1.115 => 0x7301a8c0
uint32_t to_ipv4(const std::string& str)
{
	std::vector<std::string> vstr = utils::split(str, '.', 0);
	if (vstr.size() != 4) {
		return 0;
	}
	uint32_t ret = 0;
	for (int n = 0; n < (int)vstr.size(); n ++) {
		const std::string& s = vstr[n];
		if (!utils::isinteger(s)) {
			return 0;
		}
		uint32_t val = to_int(s);
		if (val > 255) {
			return 0;
		}
		ret |= val << (n * 8);
	}
	return ret;
}

// @ip1, ip2: 192.168.1.1 => 0x0101a8c0
bool is_same_net(uint32_t ip1, uint32_t ip2, int prefixlen)
{
	VALIDATE(prefixlen >= 1 && prefixlen <= 31, null_str);
	uint32_t netmask_le = ~((1u << (32 - prefixlen)) - 1);
	ip1 = SDL_Swap32(ip1);
	ip2 = SDL_Swap32(ip2);
	return (ip1 & netmask_le) == (ip2 & netmask_le);
}

// @24 => 255.255.255.0
std::string from_prefixlen(int prefixlen)
{
	if (prefixlen == 0) {
		return "0.0.0.0";
	} else if (prefixlen < 0 || prefixlen > 32) {
		return str_cast(prefixlen);
	}
	VALIDATE(prefixlen >= 1 && prefixlen <= 32, null_str);
	// 24 => netmask_le: 0xffffff00;
	uint32_t netmask_le = ~((1u << (32 - prefixlen)) - 1);
	// 24 => netmask_be: 0x00ffffff;
	uint32_t netmask_be = SDL_Swap32(netmask_le);
	const uint8_t* ptr = (const uint8_t*)&netmask_be;
	std::stringstream ss;
	for (int n = 0; n < 4; n ++) {
		if (n != 0) {
			ss << ".";
		}
		ss << (int)(ptr[n]);
	}
	// SDL_Log("from_refixlen, %i => %s(0x%08x)", prefixlen, ss.str().c_str(), netmask_be);
	return ss.str();
}

std::string from_wchar_ptr(const wchar_t* src)
{
	if (src == nullptr || src[0] == 0) {
		return null_str;
	}
	// param4 Inbytesleft indicates the number of input bytes to be used for conversion. 
	// '+1' make sure that converted string ends with '\0'. 
	// if result isn't nullptr, use SDL_free to free.
	const char* fromcode = sizeof(wchar_t) == 2? "UTF-16LE": "UTF-32LE";
	char* utf8_ptr = SDL_iconv_string("UTF-8", fromcode, (const char*)src, (SDL_wcslen(src) + 1) * sizeof(wchar_t));
	std::string result(utf8_ptr);
	SDL_free(utf8_ptr);
	return result;
}

// WCHAR* == > utf8. sizeof(WCHAR) always be 2.
std::string from_uint16_ptr(const uint16_t* src)
{
	if (src == nullptr || src[0] == 0) {
		return null_str;
	}

	int len = 0;
	while (src[len] != 0) {
		len ++;
	}
	char* utf8_ptr = SDL_iconv_string("UTF-8", "UTF-16LE", (const char*)src, (len + 1) * sizeof(uint16_t));
	std::string result(utf8_ptr);
	SDL_free(utf8_ptr);
	return result;
}

// @dst, src: caller must make sure them allocated.
// @maxlen: include terminal '\0'.
size_t wchar_ptr_2_uint16_ptr(uint16_t *dst, const wchar_t *src, size_t maxlen)
{
	if (sizeof(wchar_t) == sizeof(uint16_t)) {
		return SDL_wcslcpy((wchar_t*)dst, src, maxlen);
	}

    size_t srclen = SDL_wcslen(src);
    if (maxlen > 0) {
        size_t len = SDL_min(srclen, maxlen - 1);
		for (size_t n = 0; n < len; n ++) {
			dst[n] = src[n];
		}
        dst[len] = '\0';
    }
    return srclen;
}

std::string join_with_null(const std::vector<std::string>& vstr)
{
	int len = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		if (str.empty()) {
			continue;
		}
		len += str.size() + 1;
	}
	if (len != 0) {
		len ++;
	} else {
		return null_str;
	}

	char* buf = (char*)malloc(len);
	memset(buf, 0, len);
	int pos = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		if (str.empty()) {
			continue;
		}
		memcpy(buf + pos, str.c_str(), str.size());
		pos += str.size() + 1;
	}
	VALIDATE(pos == len - 1, null_str);
	return std::string(buf, len);
}

std::string join_int_string(int n, const std::string& str, const char connector)
{
	size_t str_size = str.size();
	const int max_int_size = 32;
	char* ptr = (char*)malloc(max_int_size + 1 + str_size);
	SDL_itoa(n, ptr, 10);

	size_t na_size = strlen(ptr);
	ptr[na_size] = connector;
	if (str_size > 0) {
		memcpy(ptr + na_size + 1, str.c_str(), str_size);
	}
	ptr[na_size + 1 + str_size] = '\0';
	std::string ret = ptr;
	free(ptr);
	return ret;
}

bool isnewline(const char c)
{
	return c == '\r' || c == '\n';
}

// Make sure that we can use Mac, DOS, or Unix style text files on any system
// and they will work, by making sure the definition of whitespace is consistent
bool portable_isspace(const char c)
{
	// returns true only on ASCII spaces
	if (static_cast<unsigned char>(c) >= 128)
		return false;
	return isnewline(c) || isspace(c);
}

// Make sure we regard '\r' and '\n' as a space, since Mac, Unix, and DOS
// all consider these differently.
bool notspace(const char c)
{
	return !portable_isspace(c);
}

std::string& strip(std::string &str)
{
	// If all the string contains is whitespace,
	// then the whitespace may have meaning, so don't strip it
	std::string::iterator it = std::find_if(str.begin(), str.end(), notspace);
	if (it == str.end())
		return str;

	str.erase(str.begin(), it);
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::string &strip_end(std::string &str)
{
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::vector< std::string > split(std::string const &val, const char c, const int flags)
{
	std::vector< std::string > res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
			
	while (i2 != val.end()) {
		if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip_end(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

std::vector< std::string > square_parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;
	std::vector<std::string::const_iterator> square_left;
	std::vector<std::string::const_iterator> square_right;
	std::vector< std::string > square_expansion;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	std::string::const_iterator j1;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	j1=i1;

	if (i1 == val.end()) return res;
	
	if (!separator) {
		// "Separator must be specified for square bracket split funtion.\n";
		return res;
	}

	if(left.size()!=right.size()){
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (true) {
		if(i2 == val.end() || (!in_parenthesis && *i2 == separator)) {
			//push back square contents
			size_t size_square_exp = 0;
			for (size_t i=0; i < square_left.size(); i++) {
				std::string tmp_val(square_left[i]+1,square_right[i]);
				std::vector< std::string > tmp = split(tmp_val);
				std::vector<std::string>::const_iterator itor = tmp.begin();
				for(; itor != tmp.end(); ++itor) {
					size_t found_tilde = (*itor).find_first_of('~');
					if (found_tilde == std::string::npos) {
						size_t found_asterisk = (*itor).find_first_of('*');
						if (found_asterisk == std::string::npos) {
							std::string tmp = (*itor);
							square_expansion.push_back(strip(tmp));
						}
						else { //'*' multiple expansion
							std::string s_begin = (*itor).substr(0,found_asterisk);
							s_begin = strip(s_begin);
							std::string s_end = (*itor).substr(found_asterisk+1);
							s_end = strip(s_end);
							for (int ast=atoi(s_end.c_str()); ast>0; --ast)
								square_expansion.push_back(s_begin);
						}
					}
					else { //expand number range
						std::string s_begin = (*itor).substr(0,found_tilde);
						s_begin = strip(s_begin);
						int begin = atoi(s_begin.c_str());
						size_t padding = 0;
						while (padding<s_begin.size() && s_begin[padding]=='0') {
							padding++;
						}
						std::string s_end = (*itor).substr(found_tilde+1);
						s_end = strip(s_end);
						int end = atoi(s_end.c_str());
						if (padding==0) {
							while (padding<s_end.size() && s_end[padding]=='0') {
								padding++;
							}
						}
						int increment = (end >= begin ? 1 : -1);
						end+=increment; //include end in expansion
						for (int k=begin; k!=end; k+=increment) {
							std::string pb = boost::lexical_cast<std::string>(k);
							for (size_t p=pb.size(); p<=padding; p++)
								pb = std::string("0") + pb;
							square_expansion.push_back(pb);
						}
					}
				}
				if (i*square_expansion.size() != (i+1)*size_square_exp ) {
					std::string tmp(i1, i2);
					// "Square bracket lengths do not match up: "+tmp+"\n";
					return res;
				}
				size_square_exp = square_expansion.size();
			}
			
			//combine square contents and rest of string for comma zone block
			size_t j = 0;
			size_t j_max = 0;
			if (square_left.size() != 0)
				j_max = square_expansion.size() / square_left.size();
			do {
				j1 = i1;
				std::string new_val;
				for (size_t i=0; i < square_left.size(); i++) {
					std::string tmp_val(j1, square_left[i]);
					new_val.append(tmp_val);
					size_t k = j+i*j_max;
					if (k < square_expansion.size())
						new_val.append(square_expansion[k]);
					j1 = square_right[i]+1;
				}
				std::string tmp_val(j1, i2);
				new_val.append(tmp_val);
				if (flags & STRIP_SPACES)
					strip_end(new_val);
				if (!(flags & REMOVE_EMPTY) || !new_val.empty())
					res.push_back(new_val);
				j++;
			} while (j<j_max);
			
			if (i2 == val.end()) //escape loop
				break;
			++i2;
			if (flags & STRIP_SPACES) { //strip leading spaces
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}
			i1=i2;
			square_left.clear();
			square_right.clear();
			square_expansion.clear();
			continue;
		}
		if(!part.empty() && *i2 == part.back()) {
			part.pop_back();
			if (*i2 == ']') square_right.push_back(i2);
			if (part.empty())
				in_parenthesis = false;
			++i2;
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++) {
			if (*i2 == lp[i]){
				if (*i2 == '[')
					square_left.push_back(i2);
				++i2;
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

std::vector< std::string > parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	
	if (left.size()!=right.size()) {
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (i2 != val.end()) {
		if(!in_parenthesis && separator && *i2 == separator){
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}
			i1=i2;
			continue;
		}
		if(!part.empty() && *i2 == part.back()){
			part.pop_back();
			if(!separator && part.empty()){
				std::string new_val(i1, i2);
				if (flags & STRIP_SPACES)
					strip(new_val);
				res.push_back(new_val);
				++i2;
				i1=i2;
			}else{
				if (part.empty())
					in_parenthesis = false;
				++i2;
			}
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++){
			if (*i2 == lp[i]){
				if (!separator && part.empty()){
					std::string new_val(i1, i2);
					if (flags & STRIP_SPACES)
						strip(new_val);
					res.push_back(new_val);
					++i2;
					i1=i2;
				}else{
					++i2;
				}
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

// Modify a number by string representing integer difference, or optionally %
int apply_modifier( const int number, const std::string &amount, const int minimum ) {
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(number * value);
	}
	value += number;
	if (( minimum > 0 ) && ( value < minimum ))
	    value = minimum;
	return value;
}

int apply_modifier( const int number, const std::string &amount, const int minimum, const int maximum)
{
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(maximum * value);
	}
	value += number;
	if (value < minimum)
	    value = minimum;
	if (value > maximum)
	    value = maximum;
	return value;
}

std::string escape(const std::string &str, const char *special_chars)
{
	std::string::size_type pos = str.find_first_of(special_chars);
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.insert(pos, 1, '\\');
		pos = res.find_first_of(special_chars, pos + 2);
	} while (pos != std::string::npos);
	return res;
}

std::string unescape(const std::string &str)
{
	std::string::size_type pos = str.find('\\');
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.erase(pos, 1);
		pos = res.find('\\', pos + 1);
	} while (pos != std::string::npos);
	return str;
}

bool string_bool(const std::string& str, bool def) {
	if (str.empty()) return def;

	// yes/no is the standard, test it first
	if (str == "yes") return true;
	if (str == "no"|| str == "false" || str == "off" || str == "0" || str == "0.0")
		return false;

	// all other non-empty string are considered as true
	return true;
}

std::string signed_value(int val)
{
	std::ostringstream oss;
	oss << (val >= 0 ? "+" : unicode_minus) << abs(val);
	return oss.str();
}

std::string half_signed_value(int val)
{
	std::ostringstream oss;
	if (val < 0)
		oss << unicode_minus;
	oss << abs(val);
	return oss.str();
}

static void si_string_impl_stream_write(std::stringstream &ss, double input) {
#ifdef _MSC_VER
	// Visual C++ makes 'precision' set the number of decimal places.
	// Other platforms make it set the number of significant figures
	ss.precision(1);
	ss << std::fixed
	   << input;
#else
	// Workaround to display 1023 KiB instead of 1.02e3 KiB
	if (input >= 1000)
		ss.precision(4);
	else
		ss.precision(3);
	ss << input;
#endif
}

std::string si_string(double input, bool base2, std::string unit) {
	const double multiplier = base2 ? 1024 : 1000;

	typedef boost::array<std::string, 9> strings9;

	strings9 prefixes;
	strings9::const_iterator prefix;
	if (input < 1.0) {
		strings9 tmp = { {
			"",
			_("prefix_milli^m"),
			_("prefix_micro^u"),
			_("prefix_nano^n"),
			_("prefix_pico^p"),
			_("prefix_femto^f"),
			_("prefix_atto^a"),
			_("prefix_zepto^z"),
			_("prefix_yocto^y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input < 1.0  && *prefix != prefixes.back()) {
			input *= multiplier;
			++prefix;
		}
	} else {
		strings9 tmp = { {
			"",
			(base2 ?
				_("prefix_kibi^K") :
				_("prefix_kilo^k")
			),
			_("prefix_mega^M"),
			_("prefix_giga^G"),
			_("prefix_tera^T"),
			_("prefix_peta^P"),
			_("prefix_exa^E"),
			_("prefix_zetta^Z"),
			_("prefix_yotta^Y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input > multiplier && *prefix != prefixes.back()) {
			input /= multiplier;
			++prefix;
		}
	}

	std::stringstream ss;
	si_string_impl_stream_write(ss, input);
	ss << ' '
	   << *prefix
	   << (base2 && (*prefix != "") ? _("infix_binary^i") : "")
	   << unit;
	return ss.str();
}

bool word_completion(std::string& text, std::vector<std::string>& wordlist) {
	std::vector<std::string> matches;
	const size_t last_space = text.rfind(" ");
	// If last character is a space return.
	if (last_space == text.size() -1) {
		wordlist = matches;
		return false;
	}

	bool text_start;
	std::string semiword;
	if (last_space == std::string::npos) {
		text_start = true;
		semiword = text;
	} else {
		text_start = false;
		semiword.assign(text, last_space + 1, text.size());
	}

	std::string best_match = semiword;
	for (std::vector<std::string>::const_iterator word = wordlist.begin();
			word != wordlist.end(); ++word)
	{
		if (word->size() < semiword.size()
		|| !std::equal(semiword.begin(), semiword.end(), word->begin(),
				chars_equal_insensitive))
		{
			continue;
		}
		if (matches.empty()) {
			best_match = *word;
		} else {
			int j = 0;
			while (toupper(best_match[j]) == toupper((*word)[j])) j++;
			if (best_match.begin() + j < best_match.end()) {
				best_match.erase(best_match.begin() + j, best_match.end());
			}
		}
		matches.push_back(*word);
	}
	if(!matches.empty()) {
		text.replace(last_space + 1, best_match.size(), best_match);
	}
	wordlist = matches;
	return text_start;
}

static bool is_word_boundary(char c) {
	return (c == ' ' || c == ',' || c == ':' || c == '\'' || c == '"' || c == '-');
}

bool word_match(const std::string& message, const std::string& word) {
	size_t first = message.find(word);
	if (first == std::string::npos) return false;
	if (first == 0 || is_word_boundary(message[first - 1])) {
		size_t next = first + word.size();
		if (next == message.size() || is_word_boundary(message[next])) {
			return true;
		}
	}
	return false;
}

bool wildcard_string_match(const std::string& str, const std::string& match) {
	const bool wild_matching = (!match.empty() && match[0] == '*');
	const std::string::size_type solid_begin = match.find_first_not_of('*');
	const bool have_solids = (solid_begin != std::string::npos);
	// Check the simple case first
	if(str.empty() || !have_solids) {
		return wild_matching || str == match;
	}
	const std::string::size_type solid_end = match.find_first_of('*', solid_begin);
	const std::string::size_type solid_len = (solid_end == std::string::npos)
		? match.length() - solid_begin : solid_end - solid_begin;
	std::string::size_type current = 0;
	bool matches;
	do {
		matches = true;
		// Now try to place the str into the solid space
		const std::string::size_type test_len = str.length() - current;
		for(std::string::size_type i=0; i < solid_len && matches; ++i) {
			char solid_c = match[solid_begin + i];
			if(i > test_len || !(solid_c == '?' || solid_c == str[current+i])) {
				matches = false;
			}
		}
		if(matches) {
			// The solid space matched, now consume it and attempt to find more
			const std::string consumed_match = (solid_begin+solid_len < match.length())
				? match.substr(solid_end) : "";
			const std::string consumed_str = (solid_len < test_len)
				? str.substr(current+solid_len) : "";
			matches = wildcard_string_match(consumed_str, consumed_match);
		}
	} while(wild_matching && !matches && ++current < str.length());
	return matches;
}

std::vector< std::string > quoted_split(std::string const &val, char c, int flags, char quote)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while (i2 != val.end()) {
		if (*i2 == quote) {
			// Ignore quoted character
			++i2;
			if (i2 != val.end()) ++i2;
		} else if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while(i2 != val.end() && *i2 == ' ')
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

std::pair< int, int > parse_range(std::string const &str)
{
	const std::string::const_iterator dash = std::find(str.begin(), str.end(), '-');
	const std::string a(str.begin(), dash);
	const std::string b = dash != str.end() ? std::string(dash + 1, str.end()) : a;
	std::pair<int,int> res(atoi(a.c_str()), atoi(b.c_str()));
	if (res.second < res.first)
		res.second = res.first;

	return res;
}

std::vector< std::pair< int, int > > parse_ranges(std::string const &str)
{
	std::vector< std::pair< int, int > > to_return;
	std::vector<std::string> strs = utils::split(str);
	std::vector<std::string>::const_iterator i, i_end=strs.end();
	for(i = strs.begin(); i != i_end; ++i) {
		to_return.push_back(parse_range(*i));
	}
	return to_return;
}

static int byte_size_from_utf8_first(unsigned char ch)
{
	int count;

	if ((ch & 0x80) == 0)
		count = 1;
	else if ((ch & 0xE0) == 0xC0)
		count = 2;
	else if ((ch & 0xF0) == 0xE0)
		count = 3;
	else if ((ch & 0xF8) == 0xF0)
		count = 4;
	else if ((ch & 0xFC) == 0xF8)
		count = 5;
	else if ((ch & 0xFE) == 0xFC)
		count = 6;
	else
		throw invalid_utf8_exception(); // Stop on invalid characters

	return count;
}

utf8_iterator::utf8_iterator(const std::string& str) :
	current_char(0),
	string_end(str.end()),
	current_substr(std::make_pair(str.begin(), str.begin()))
{
	update();
}

utf8_iterator::utf8_iterator(std::string::const_iterator const &beg,
		std::string::const_iterator const &end) :
	current_char(0),
	string_end(end),
	current_substr(std::make_pair(beg, beg))
{
	update();
}

utf8_iterator utf8_iterator::begin(std::string const &str)
{
	return utf8_iterator(str.begin(), str.end());
}

utf8_iterator utf8_iterator::end(const std::string& str)
{
	return utf8_iterator(str.end(), str.end());
}

bool utf8_iterator::operator==(const utf8_iterator& a) const
{
	return current_substr.first == a.current_substr.first;
}

utf8_iterator& utf8_iterator::operator++()
{
	current_substr.first = current_substr.second;
	update();
	return *this;
}

wchar_t utf8_iterator::operator*() const
{
	return current_char;
}

bool utf8_iterator::next_is_end()
{
	if(current_substr.second == string_end)
		return true;
	return false;
}

const std::pair<std::string::const_iterator, std::string::const_iterator>& utf8_iterator::substr() const
{
	return current_substr;
}

void utf8_iterator::update()
{
	// Do not try to update the current unicode char at end-of-string.
	if(current_substr.first == string_end)
		return;

	size_t size = byte_size_from_utf8_first(*current_substr.first);
	current_substr.second = current_substr.first + size;

	current_char = static_cast<unsigned char>(*current_substr.first);
	// Convert the first character
	if(size != 1) {
		current_char &= 0xFF >> (size + 1);
	}

	// Convert the continuation bytes
	for(std::string::const_iterator c = current_substr.first+1;
			c != current_substr.second; ++c) {
		// If the string ends occurs within an UTF8-sequence, this is bad.
		if (c == string_end)
			throw invalid_utf8_exception();

		if ((*c & 0xC0) != 0x80)
			throw invalid_utf8_exception();

		current_char = (current_char << 6) | (static_cast<unsigned char>(*c) & 0x3F);
	}
}


std::string wstring_to_string(const wide_string &src)
{
	std::string ret;

	try {
		for(wide_string::const_iterator i = src.begin(); i != src.end(); ++i) {
			unsigned int count;
			wchar_t ch = *i;

			// Determine the bytes required
			count = 1;
			if(ch >= 0x80)
				count++;

			Uint32 bitmask = 0x800;
			for(unsigned int j = 0; j < 5; ++j) {
				if(static_cast<Uint32>(ch) >= bitmask) {
					count++;
				}

				bitmask <<= 5;
			}

			if(count > 6) {
				throw invalid_utf8_exception();
			}

			if(count == 1) {
				ret.push_back(static_cast<char>(ch));
			} else {
				for(int j = static_cast<int>(count) - 1; j >= 0; --j) {
					unsigned char c = (ch >> (6 * j)) & 0x3f;
					c |= 0x80;
					if(j == static_cast<int>(count) - 1) {
						c |= 0xff << (8 - count);
					}
					ret.push_back(c);
				}
			}

		}

		return ret;
	}
	catch (invalid_utf8_exception&) {
		// "Invalid wide character string\n";
		return ret;
	}
}

std::string wchar_to_string(const wchar_t c)
{
	wide_string s;
	s.push_back(c);
	return wstring_to_string(s);
}

wide_string string_to_wstring(const std::string &src)
{
	wide_string res;

	try {
		utf8_iterator i1(src);
		const utf8_iterator i2(utf8_iterator::end(src));

		// Equivalent to res.insert(res.end(),i1,i2) which doesn't work on VC++6.
		while(i1 != i2) {
			res.push_back(*i1);
			++i1;
		}
	}
	catch (invalid_utf8_exception&) {
		// "Invalid UTF-8 string: \"" << src << "\"\n";
		return res;
	}

	return res;
}

std::string utf8str_insert(const std::string& src, const int from, const std::string& insert)
{
	if (insert.empty()) {
		return src;
	}
	VALIDATE(from >= 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	int at = 0;
	for (; it != end; ++ it, at ++) {
		if (at == from) {
			ret.append(insert);
		}
		ret.append(it.substr().first, it.substr().second);
	}
	if (at == from) {
		ret.append(insert);
	}
	return ret;
}

std::string utf8str_erase(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			continue;
		}
		ret.append(it.substr().first, it.substr().second);
	}
	return ret;
}

std::string utf8str_substr(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			ret.append(it.substr().first, it.substr().second);
			continue;
		}
	}
	return ret;
}

// for utf8 continuation bytes must be rule: b7 must 1, b6 must 0.
// caller must define some variable:
// const uint8_t* data_ptr = (const uint8_t*)utf8str.c_str();
// int pos; when call, pos is offset utf8-char's first byte. after macro, pos will be offset first byte after uftf8-char.
// int continuation_pos;
#define check_utf8_continuation_byte(c_bytes, fail_res)	\
	for (++ pos, continuation_pos = 0; continuation_pos < c_bytes; continuation_pos ++, pos ++) { \
		if (((uint8_t)(data_ptr[pos]) & 0xc0) != 0x80) { \
			return fail_res; \
		} \
	}

size_t utf8str_len(const std::string& utf8str)
{
	size_t size = 0;
	const int len = utf8str.size();
	const uint8_t* data_ptr = (const uint8_t*)utf8str.c_str();
	int pos = 0;
	int continuation_pos;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			check_utf8_continuation_byte(1, 0);
			// pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			check_utf8_continuation_byte(2, 0);
			// pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			check_utf8_continuation_byte(3, 0);
			// pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			check_utf8_continuation_byte(4, 0);
			// pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			check_utf8_continuation_byte(5, 0);
			// pos += 6;
		} else {
			return 0;
		}
		size ++;
	}
	if (pos != len) {
		// utf8str isn't valid utf-8 format string
		size = 0;
	}

	return size;
}

int utf8str_bytes(const std::string& utf8str, int chars)
{
	VALIDATE(chars >= 0, null_str);
	const int len = utf8str.size();
	if (chars >= len) {
		return len;
	}

	const uint8_t* c_str = (const uint8_t*)utf8str.c_str();
	int pos = 0, this_bytes = 0, chars2 = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return 0;
		}
		pos += this_bytes;
		if (++ chars2 == chars) {
			break;
		}
	}

	return pos;
}

bool is_utf8str(const char* data, int64_t len)
{
	VALIDATE(len >= 0, null_str);
	const uint8_t* data_ptr = (const uint8_t*)data;
	int64_t pos = 0;
	int continuation_pos;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			check_utf8_continuation_byte(1, false);
			// pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			check_utf8_continuation_byte(2, false);
			// pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			check_utf8_continuation_byte(3, false);
			// pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			check_utf8_continuation_byte(4, false);
			// pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			check_utf8_continuation_byte(5, false);
			// pos += 6;
		} else {
			return false;
		}
	}

	return pos == len;
}

std::string truncate_to_max_bytes(const char* utf8str, const int utf8str_size, int max_bytes)
{
	VALIDATE((utf8str_size == nposm || utf8str_size >= 0) && max_bytes >= 1, null_str);

	const int len = utf8str_size == nposm? INT32_MAX: utf8str_size;
	if (len <= max_bytes) {
		return std::string(utf8str, len);
	}

	const uint8_t* c_str = (const uint8_t*)utf8str;
	int pos = 0, this_bytes = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if (ch == '\0') {
			break;
		} else if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return std::string((const char*)c_str, pos);
		}
		if (pos + this_bytes > max_bytes) {
			break;
		}
		pos += this_bytes;
	}

	return std::string((const char*)c_str, pos);
}

std::string truncate_to_max_chars(const char* utf8str, const int utf8str_size, int max_chars)
{
	VALIDATE((utf8str_size == nposm || utf8str_size >= 0) && max_chars >= 1, null_str);

	const int len = utf8str_size == nposm? INT32_MAX: utf8str_size;
	if (len <= max_chars) {
		return std::string(utf8str, len);
	}

	const uint8_t* c_str = (const uint8_t*)utf8str;
	int pos = 0, this_bytes = 0, chars = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if (ch == '\0') {
			break;
		} else if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return std::string((const char*)c_str, pos);
		}
		pos += this_bytes;
		if (++ chars == max_chars) {
			break;
		}
	}

	return std::string((const char*)c_str, pos);
}

void truncate_as_wstring(std::string& str, const size_t size)
{
	wide_string utf8_str = utils::string_to_wstring(str);
	if(utf8_str.size() > size) {
		utf8_str.resize(size);
		str = utils::wstring_to_string(utf8_str);
	}
}

void ellipsis_truncate(std::string& str, const size_t size)
{
	const size_t prev_size = str.length();

	truncate_as_wstring(str, size);

	if(str.length() != prev_size) {
		str += ellipsis;
	}
}

bool utf8str_compare(const std::string& str1, const std::string& str2)
{
	utils::utf8_iterator itor1(str1);
	utils::utf8_iterator itor1_end = utils::utf8_iterator::end(str1);
	utils::utf8_iterator itor2(str2);
	utils::utf8_iterator itor2_end = utils::utf8_iterator::end(str2);

	for (; itor1 != itor1_end; ++ itor1, ++ itor2) {
		if (itor2 == itor2_end) {
			return false;
		}
		wchar_t key1 = *itor1;
		wchar_t key2 = *itor2;
		if (key1 < key2) {
			return true;
		} else if (key1 > key2) {
			return false;
		}
	}
	return true;
}

uint32_t check_color(const std::string& color_str)
{
	std::vector<std::string> fields = utils::split(color_str);
	if (fields.size() != 4) {
		return 0;
	}

	int val;
	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		
		val = utils::to_int(fields[i]);
		if (val < 0 || val > 255) {
			return 0;
		}
		result |= utils::to_int(fields[i]);
	}
	if (!(result & 0xff000000)) {
		// avoid to confuse with special color.
		return 0;
	}
	return result;
}

std::set<int> to_set_int(const std::string& value)
{
	std::vector<std::string> vstr = split(value);
	std::set<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.insert(lexical_cast_default<int>(*it));
	}
	return ret;
}

std::vector<int> to_vector_int(const std::string& value)
{
	std::vector<std::string> vstr = split(value);
	std::vector<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.push_back(lexical_cast_default<int>(*it));
	}
	return ret;
}

std::string lowercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void lowercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			str[i] = c_str[i] + diff;
		}
	}
}

std::string uppercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void uppercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			str[i] = c_str[i] + diff;
		}
	}
}


// condition id(only ASCII)/variable(only ASCII)/username
static bool is_id_char(char c)
{
	return ((c == '_') || (c == '-') || (c == ' '));
}

static bool is_variable_char(char c) 
{
	return ((c == '_') || (c == '-') || (c == '.'));
}

typedef bool (*is_xxx_char)(char c);

std::string errstr;
bool isvalid_id_base(const std::string& id, bool first_must_alpha, is_xxx_char fn, int min, int max)
{
	utils::string_map symbols;
	int s = (int)id.size();
	if (!s) {
		if (min > 0) {
			errstr = _("Can not empty!");
 			return false;
		}
		return true;
	}
	if (s < min) {
		symbols["min"] = str_cast(min);
		errstr = vgettext2("At least $min characters!", symbols);
		return false;
	}
	if (s > max) {
		symbols["max"] = str_cast(max);
		errstr = vgettext2("Can not be larger than $max characters!", symbols);
		return false;
	}
	char c = id.at(0);
	if (c == ' ') {
		errstr = _("First character can not empty!");
		return false;
	}
	if (first_must_alpha && !isalpha(c)) {
		errstr = _("First character must be alpha!");
		return false;
	}
	if (id == "null") {
		symbols["str"] = id;
		errstr = vgettext2("$str is reserved string!", symbols);
		return false;
	}

	const size_t alnum = std::count_if(id.begin(), id.end(), isalnum);
	const size_t valid_char = std::count_if(id.begin(), id.end(), fn);
	if ((alnum + valid_char != s) || valid_char == id.size()) {
		errstr = _("Contains invalid characters!");
		return false;
	}
	return true;
}

bool isvalid_id(const std::string& id, bool first_must_alpha, int min, int max)
{
	return isvalid_id_base(id, first_must_alpha, is_id_char, min, max);
}

bool isvalid_variable(const std::string& id, int min, int max)
{
	return isvalid_id_base(id, true, is_variable_char, min, max);
}

bool isvalid_nick(const std::string& nick)
{ 
	const int max_nick_size = 31;
	return isvalid_id_base(nick, false, is_id_char, 1, max_nick_size);
}

static bool is_username_char(char c) {
	return ((c == '_') || (c == '-'));
}

static bool is_wildcard_char(char c) {
    return ((c == '?') || (c == '*'));
}

bool isvalid_username(const std::string& username) 
{
	size_t alnum = 0, valid_char = 0, chars = 0;
	try {
		utils::utf8_iterator itor(username);
		for (; itor != utils::utf8_iterator::end(username); ++ itor) {
			wchar_t w = *itor;
			
			if ((w & 0xff00) || isalnum(w)) {
				alnum ++;
			} else if (is_username_char(w)) {
				valid_char ++;
			}
			chars ++;
		}
		if ((alnum + valid_char != chars) || valid_char == chars || !chars) {
			errstr = _("Contains invalid characters!");
			return false;
		}
	} catch (utils::invalid_utf8_exception&) {
		errstr = _("Invalid UTF-8 string!");
		return false;
	}
	return true;
}

bool isvalid_wildcard(const std::string& username) 
{
    const size_t alnum = std::count_if(username.begin(), username.end(), isalnum);
	const size_t valid_char =
			std::count_if(username.begin(), username.end(), is_username_char);
    const size_t wild_char =
            std::count_if(username.begin(), username.end(), is_wildcard_char);
	if ((alnum + valid_char + wild_char != username.size())
			|| valid_char == username.size() || username.empty() )
	{
		return false;
	}
	return true;
}

static bool is_filename_char(char c) {
	return c == '_' || c == '-' || c == ' ' || c == '.' || c == '(' || c == ')';
}

bool isvalid_filename(const std::string& filename)
{
	if (filename.empty()) {
		return false;
	}
	bool first = true;
	size_t alnum = 0, valid_char = 0, chars = 0;
	wchar_t last_w = 0;
	try {
		utils::utf8_iterator itor(filename);
		for (; itor != utils::utf8_iterator::end(filename); ++ itor) {
			wchar_t w = *itor;
			last_w = w;

			if (first) {
				if ((w & 0xff00) == 0) {
					uint8_t c = posix_lo8(w);
					if (c == '.' || c == ' ') {
						return false;
					}
				}
				first = false;
			}

			if ((w & 0xff00) || isalnum(w)) {
				alnum ++;
			} else if (is_filename_char(w)) {
				valid_char ++;
			} else {
				return false;
			}
			chars ++;
		}
		if (valid_char == chars) {
			return false;
		}
		if ((last_w & 0xff00) == 0) {
			uint8_t c = posix_lo8(last_w);
			if (c == '.' || c == ' ') {
				return false;
			}
		}
	} catch (utils::invalid_utf8_exception&) {
		return false;
	}
	return true;
}

bool isinteger(const std::string& str)
{
	const int len = (int)str.size();
	if (!len) {
		return false;
	}

	const char* c_str = str.c_str();
	char ch = c_str[0];
	int at = 0;
	if (ch == '-' || ch == '+') {
		if (len == 1) {
			return false;
		}
		at = 1;
	}
	for (; at < len; at ++) {
		if (c_str[at] < '0' || c_str[at] > '9') {
			return false;
		}
	}
	return true;
}

// if cstr is NULL, 'return cstr' directly will result 'access violation'.
std::string cstr_2_str(const char* cstr)
{
	return (cstr)? (cstr): null_str;
}

// below to replace_all is copy from StringReplace(<protobuf>/src/google/protobuf/stubs/strutil.cc)
std::string replace_all(const std::string &s, const std::string& oldsub, const std::string& newsub)
{
	if (oldsub.empty()) {
		return s; // if empty, return the given string.
	}

	std::string ret;
	std::string::size_type start_pos = 0;
	std::string::size_type pos;
	do {
		pos = s.find(oldsub, start_pos);
		if (pos == std::string::npos) {
			break;
		}
		ret.append(s, start_pos, pos - start_pos);
		ret.append(newsub);
		start_pos = pos + oldsub.size();  // start searching again after the "old"
	} while (true);
	ret.append(s, start_pos, s.length() - start_pos);
	return ret;
}

void replace_all2(std::string& s, const std::string& oldsub, const std::string& newsub)
{
	while (true) {
		std::string::size_type pos(0);
		if ((pos = s.find(oldsub)) != std::string::npos) {
			s.replace(pos, oldsub.length(), newsub);
		} else {
			break;
		}
	}
}

std::string replace_all_char(const std::string& s, char oldchar, char newchar)
{
	if (s.empty() || oldchar == newchar) {
		return s; // if empty, return the given string.
	}

	int size = s.size();
	char* tmpstr = (char*)malloc(size + 1);
	memcpy(tmpstr, s.c_str(), size);
	tmpstr[size] = '\0';

	char* ptr = tmpstr;
	do {
		ptr = strchr(ptr, oldchar);
		if (ptr == nullptr) {
			break;
		}
		ptr[0] = newchar;
		ptr ++;
	} while (true);

	std::string result(tmpstr, size);
	free(tmpstr);
	return result;
}

std::string generate_app_prefix_id(const std::string& app, const std::string& id)
{
	if (!app.empty()) {
		return app + "__" + id;
	}
	return id;
}

std::pair<std::string, std::string> split_app_prefix_id(const std::string& id2)
{
	size_t pos = id2.find("__");
	if (pos == std::string::npos) {
		return std::make_pair(null_str, id2);
	}
	return std::make_pair(id2.substr(0, pos), id2.substr(pos + 2));
}

bool bom_magic_started(const uint8_t* data, int size)
{
	return size >= BOM_LENGTH && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf;
}

// find the pos that non-blank character at it.
const char* skip_blank_characters(const char* start)
{
	while (start[0] == '\r' || start[0] == '\n' || start[0] == '\t' || start[0] == ' ') {
		start ++;
	}
	return start;
}

// find the pos that non-blank character at it.
const char* skip_blank_lines(const char* start, int lines)
{
	VALIDATE(lines > 0, null_str);

	int skiped_line_feeds = 0;
	while (start[0] == '\r' || start[0] == '\n' || start[0] == '\t' || start[0] == ' ') {
		if (start[0] == '\n') {
			skiped_line_feeds ++;
			if (skiped_line_feeds == lines) {
				start ++;
				if (start[0] == '\r' && start[1] != '\n') {
					start ++;
				}
				return start;
			}
		}
		start ++;
	}
	return start;
}

// find the pos that blank character(\r\n\t ) at it.
const char* until_blank_characters(const char* start)
{
	while (start[0] != '\r' && start[0] != '\n' && start[0] != '\t' && start[0] != ' ') {
		start ++;
	}
	return start;
}

// find the pos that c-style word terminate character(\r\n\t ;) at it.
const char* until_c_style_characters(const char* start)
{
	while (start[0] != '\r' && start[0] != '\n' && start[0] != '\t' && start[0] != ' ' && start[0] != ';') {
		start ++;
	}
	return start;
}

// 1. single block. cannot only attribute.
// 2. may exist space char before [name].
// 3. may exist space char after [/name].
bool is_single_cfg(const std::string& str, std::string* element_name)
{
	const int len = str.size();
	const char* cstr = str.c_str();
	int ch;

	if (len < 7) { // [1][1/]
		return false;
	}
	int pos = 0;
	int name_start = -1;
	for (pos = 0; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		if (name_start == -1) {
			if (ch != '[') {
				// non-space char must be begin with [.
				return false;
			}
			name_start = pos;
		} else if (ch == ']') {
			break;
		}
	}
	if (pos == len) {
		return false;
	}
	std::string key(cstr + name_start, pos + 1 - name_start); // key = [name].
	key.insert(1, "/");
	const char* ptr = strstr(cstr, key.c_str());
	if (ptr == NULL) {
		// cannot find [/name]
		return false;
	}
	for (pos = ptr + key.size() - cstr; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		// all characters after [name] must space char.
		return false;
	}
	if (element_name) {
		*element_name = key.substr(2, key.size() - 3);
	}
	return true;
}

void verify_splited_intergate(const std::string& text, const std::map<int, tcfg_string_pair>& segments)
{
	if (text.empty()) {
		VALIDATE(segments.empty(), null_str);
		return;
	}
	VALIDATE(!segments.empty(), null_str);

	const char* c_str = text.c_str();
	const int size = text.size();
	int next_item_start = 0;
	for (std::map<int, tcfg_string_pair>::const_iterator it = segments.begin(); it !=segments.end(); ++ it) {
		const int item_start = it->first;
		const tcfg_string_pair& segment = it->second;
		VALIDATE(item_start >= 0 && item_start < size, null_str);
		if (next_item_start != -1) {
			VALIDATE(item_start == next_item_start, null_str);
		}

		if (segment.iscfg) {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + (text_size + 2) * 2 < size, null_str);
			std::stringstream s;
			s << "<" << segment.text << ">";
			VALIDATE(!SDL_memcmp(c_str + item_start, s.str().c_str(), s.str().size()), null_str); 
			next_item_start = -1;
		} else {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + text_size <= size, null_str);
			VALIDATE(!SDL_memcmp(c_str + item_start, segment.text.c_str(), text_size), null_str);
			next_item_start = item_start + text_size;
		}
	}
}

static config convert_to_wml(const char* contents, const int size)
{
	std::stringstream ss;
	tpoint quotes_pos(-1, -1);
	bool in_quotes = false;
	bool last_char_escape = false;
	const char escape_char = '\\';

	VALIDATE(contents, null_str);

	if (!size || !notspace(contents[0]) || !notspace(contents[size - 1])) {
		return null_cfg;
	}

	std::vector<std::pair<std::string, tpoint> > attributes;
	// Find the different attributes.
	// No checks are made for the equal sign or something like that.
	// Attributes are just separated by spaces or newlines.
	// Attributes that contain spaces must be in single quotes.
	for (int pos = 0; pos < size; ++ pos) {
		const char c = contents[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		} else {
			if (c == '"' && !last_char_escape) {
				if (in_quotes) {
					quotes_pos.y = ss.str().size();
				} else {
					quotes_pos.x = ss.str().size();
				}
				ss << '"';
				in_quotes = !in_quotes;
			} else if ((c == ' ' || c == '\n') && !last_char_escape && !in_quotes) {
				// Space or newline, end of attribute.
				attributes.push_back(std::make_pair(ss.str(), quotes_pos));
				quotes_pos.x = quotes_pos.y = -1;
				ss.str("");
			} else {
				ss << c;
			}
			last_char_escape = false;
		}
	}
	if (in_quotes) {
		// Unterminated single quote after: 'ss.str()'
		return null_cfg;
	}
	if (ss.str() != "") {
		attributes.push_back(std::make_pair(ss.str(), quotes_pos));
	}

	config ret;
	for (std::vector<std::pair<std::string, tpoint> >::const_iterator it = attributes.begin(); it != attributes.end(); ++ it) {
		const std::string& text = it->first;
		const tpoint quotes_pos = it->second;
		const char* c_str = text.c_str();
		const int size = text.size();
		if (size <= 2) {
			return null_cfg;
		}
		size_t equal_pos = text.find('=');
		if (equal_pos == std::string::npos) {
			return null_cfg;
		}
		if (quotes_pos.x != -1 && quotes_pos.y <= (int)equal_pos) {
			return null_cfg;
		}
		std::string key = text.substr(0, equal_pos);
		strip(key);

		if (key.empty() || !notspace(key[0])) {
			// If all the string contains is whitespace,
			// then the whitespace may have meaning, strip will don't strip it
			return null_cfg;
		}
		
		std::string value;
		if (quotes_pos.x != -1) {
			value.assign(text, quotes_pos.x + 1, quotes_pos.y - quotes_pos.x - 1);
		} else {
			value = text.substr(equal_pos + 1);
			strip(value);
		}
		ret[key] = value;
	}

	return ret;
}

void split_integrate_src(const std::string& text, const std::set<std::string>& support_markups, std::map<int, tcfg_string_pair>& ret)
{
	ret.clear();

	int item_start = 0;
	int maybe_cfg_item_start = 0;
	const char* c_str = text.c_str();
	const int size = text.size();
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (int pos = 0; pos < size; ++ pos) {
		const char c = c_str[pos];
		if (state == OTHER) {
			if (c == '<') {
				maybe_cfg_item_start = pos;
				state = ELEMENT_NAME;
			}

		} else if (state == ELEMENT_NAME) {
			if (c == '/') {
				// Erroneous / in element name.
				// maybe_cfg_item_start = -1;
				state = OTHER;

			} else if (c == '<') {
				// overload
				maybe_cfg_item_start = pos;

			} else if (c == '>') {
				// End of this name.
				std::stringstream s;
				const std::string element_name(text.substr(maybe_cfg_item_start + 1, pos - maybe_cfg_item_start - 1));
				if (support_markups.find(element_name) != support_markups.end()) {
					s << "</" << element_name << ">";
					const std::string end_element_name = s.str();
					size_t end_pos = text.find(end_element_name, pos);
					if (end_pos != std::string::npos) {
						const int size = end_pos - pos - 1;
						const config cfg = convert_to_wml(text.c_str() + pos + 1, size);
						if (!cfg.empty()) {
							std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
							if (item_start != maybe_cfg_item_start) {
								VALIDATE(item_start < maybe_cfg_item_start, null_str);
								// previous
								ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
								ins.first->second.iscfg = false;
								ins.first->second.text = text.substr(item_start, maybe_cfg_item_start - item_start);

								item_start = maybe_cfg_item_start;
							}

							ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
							ins.first->second.iscfg = true;
							ins.first->second.text = element_name;
							ins.first->second.cfg = cfg;

							pos = end_pos + end_element_name.size() - 1;
							item_start = pos + 1;
						}
					}
				}

				state = OTHER;
			}
		}
	}
	if (item_start != size) {
		std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
		ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
		ins.first->second.iscfg = false;
		ins.first->second.text = text.substr(item_start, size - item_start);
	}

	verify_splited_intergate(text, ret);
}

// when tintegrate::insert_str, string will insert in <format>...</format>
// because must not markup-nest, inserted string shoulu be drop markup.
std::string ht_drop_markup(const std::string& text, const std::set<std::string>& support_markups, const std::string& text_markup)
{
	if (text.empty()) {
		return null_str;
	}
	std::map<int, tcfg_string_pair> splited_items;
	split_integrate_src(text, support_markups, splited_items);
	if (splited_items.size() == 1 && !splited_items[0].iscfg) {
		return text;
	}

	std::stringstream ss;
	std::map<int, utils::tcfg_string_pair>::const_iterator it = splited_items.begin();
	std::map<int, utils::tcfg_string_pair>::const_iterator before_it = it;
	for (++ it; ; before_it = it, ++ it) {
		if (before_it->second.iscfg) {
			if (before_it->second.text == text_markup) {
				// string shoulde be in <format>...</format>, require escape.
				std::string stuffed = tintegrate::stuff_escape(before_it->second.cfg["text"].str(), true);
				ss << stuffed;
			}

		} else {
			if (it != splited_items.end()) {
				ss << text.substr(before_it->first, it->first - before_it->first);
			} else {
				ss << text.substr(before_it->first);
			}
		}
		if (it == splited_items.end()) {
			break;
		}
	}
	return ss.str();
}

// @dosstyle: false-->'/', true-->'\'
// get rid of all /.. and /. and //
std::string normalize_path(const std::string& src, bool dosstyle)
{
	// because of thread-safe, must not use static variable.
	char* data = NULL;
	int size = (int)src.size();

	const char separator = dosstyle? '\\': '/';
	const char require_modify_separator = dosstyle? '/': '\\';
	const char* c_str = src.c_str();
	bool has_dot = false;
	bool has_continue_separator = false;
	int sep = -2;

	// 1. replace all require_modify_separator to separator.
	for (int at = 0; at < size; at++) {
		const char ch = c_str[at];
		if (ch == separator) {
			if (data) {
				data[at] = ch;
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			sep = at;
		} else if (ch == require_modify_separator) {
			if (!data) {
				data = (char*)malloc(posix_align_ceil(size + 1, 128));
				if (at) {
					memcpy(data, c_str, at);
				}
				data[size] = '\0';
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			data[at] = separator;
			sep = at;
		} else {
			if (!has_dot && ch == '.' && (sep + 1 == at)) {
				// only consider '.' next to /.
				// filename maybe exist '.', ex: _main.cfg
				has_dot = true;
			}
			if (data) {
				data[at] = ch;
			}
		}
	}

	if (!has_dot && !has_continue_separator) {
		if (!data) {		
			return src;
		}
		std::string ret(data, size);
		free(data);
		return ret;
	}

	if (!data) {
		data = (char*)malloc(posix_align_ceil(size + 1, 128));
		memcpy(data, c_str, size);
		data[size] = '\0';
	}
	
	// 2. delete all /.. and /. and //
	while (1) {
		sep = -1;
		bool exit = true;
		for (int at = 0; at < size - 1; at++) {
			if (data[at] == separator) {
				if (data[at + 1] == '.') {
					if (data[at + 2] == '.') {
						// /..
						VALIDATE(sep != -1, null_str);
						if (size - at - 3) {
							SDL_memcpy(data + sep, data + at + 3, (size - at - 3));
						}
						size -= at - sep + 3;
						data[size] = '\0';
						exit = false;

					} else if (at + 2 >= size || data[at + 2] == '/') {
						// /.    ===>next is '/', or '.' is last char. delete these tow chars.
						if (size - at - 2) {
							SDL_memcpy(data + at, data + at + 2, (size - at - 2));
						}
						size -= 2;
						data[size] = '\0';
						exit = false;
					}
					
				} else if (data[at + 1] == '/') {
					// //    ===>delete second '/'
					if (size - at - 1) {
						SDL_memcpy(data + at, data + at + 1, (size - at - 1));
					}
					size -= 1;
					data[size] = '\0';
					exit = false;
				}

				if (!exit) {
					break;
				}
				sep = at;
			}
		}
		if (exit) {
			break;
		}
	}

	std::string ret(data, size);
	free(data);
	return ret;
}

void hex_encode2(uint8_t* data, int len, char* out)
{
	int pos = 0;
	for (int at = 0; at < len; at ++) {
		uint8_t u8 = data[at];
		uint8_t lo;
		uint8_t hi = (u8 & 0xf0) >> 4;
		if (hi <= 9) {
			out[pos ++] = hi + '0';
		} else {
			out[pos ++] = (hi - 10) + 'a';
		}
		lo = u8 & 0xf;
		if (lo <= 9) {
			out[pos ++] = lo + '0';
		} else {
			out[pos ++] = (lo - 10) + 'a';
		}
		out[pos ++] = ' ';
	}
	out[pos] = '\0';
}

std::string format_guid(const GUID& guid)
{
	// ==> 3641E31E-36BF-4E03-8879-DE33ADC07D68
	std::stringstream ss;

	ss << std::setw(8) << std::setfill('0') << std::setbase(16) << guid.Data1 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data2 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data3 << "-";
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[0];
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[1] << "-";
	for (int at = 2; at < 8; at ++) {
		ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[at];
	}

	return uppercase(ss.str());
}

bool is_valid_phone(const std::string& phone)
{
	const int size = phone.size();
	const int min_chars = 6;
	if (size < min_chars) {
		return false;
	}
	const char* c_str = phone.c_str();
	for (int at = 0; at < size; at ++) {
		if (!isdigit(c_str[at]) && c_str[at] != '-') {
			return false;
		}
	}
	return true;
}

bool is_valid_plateno(const std::string& plateno)
{
	if (plateno.empty()) {
		return false;
	}
	const char* c_str = plateno.c_str();
	if (strchr(c_str, '|') != nullptr) {
		return false;
	}
	if (strchr(c_str, ',') != nullptr) {
		return false;
	}
	return true;
}

std::string edge_desensitize_str(const std::string& str, int prefix_len, int postfix_len)
{
	VALIDATE(prefix_len >= 0 && postfix_len >= 0 && prefix_len + postfix_len > 0, null_str);
	int stars = 0;

	const int len = utils::utf8str_len(str);
	if (len <= prefix_len + postfix_len) {
		return str;
	} else {
		stars = len - prefix_len - postfix_len;
	}

	std::string ret;
	if (prefix_len > 0) {
		ret = utils::utf8str_substr(str, 0, prefix_len);
	}
	ret.append(std::string(stars, '*'));
	if (postfix_len > 0) {
		ret.append(utils::utf8str_substr(str, prefix_len + stars, postfix_len));
	}
	return ret;
}

std::string desensitize_idnumber(const std::string& idnumber)
{
	return edge_desensitize_str(idnumber, 3, 4);
}

std::string desensitize_phone(const std::string& phone)
{
	if (!is_valid_phone(phone)) {
		// if it is invaoid phone, not blur.
		return phone;
	}
	// chinese mobile is 11.
	return edge_desensitize_str(phone, 3, 4);
}

std::string desensitize_name(const std::string& name)
{
	int len = utils::utf8str_len(name);
	if (len < 2) {
		return name;
	}

	const char* stars = "*";
	const int stars_len = 1;
	size_t size = 0;
	len = name.size();
	const uint8_t* data_ptr = (const uint8_t*)name.c_str();
	int pos = 0;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			pos += 6;
		} else {
			VALIDATE(false, null_str);
		}
		size ++;
		// now only first is orignal.
		break;
	}
	std::string ret = name.substr(0, pos);
	ret.append(stars, stars_len);

	return ret;
}

void aes256_encrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out)
{
	// In java, corresponding algorihm string: CIPHER_ALGORITHM = "AES/CBC/NoPadding"
	// if aes256, key must 32bytes. for aes256/192/128, all size are 16(AES_BLOCK_SIZE).
	VALIDATE(bytes > 0 && (bytes % AES_BLOCK_SIZE) == 0, null_str);

	AES_KEY enckey;
	AES_set_encrypt_key(key, 256, &enckey);

	// The IV used to encrypt block[n] is ciphertext(block[n-1]) unless n == 0, which is when the input IV is used. 
	// This copy makes sense if you think about calling this API in a loop. 
	// The output of the previous block is the IV for the next block. 
	uint8_t iv2[AES_BLOCK_SIZE];
    memcpy(iv2, iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(in, out, bytes, &enckey, iv2, AES_ENCRYPT);
}

void aes256_decrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out)
{
	// In java, corresponding algorihm string: CIPHER_ALGORITHM = "AES/CBC/NoPadding"
	// if aes256, key must 32bytes. for aes256/192/128, all size are 16(AES_BLOCK_SIZE).
	VALIDATE(bytes > 0 && (bytes % AES_BLOCK_SIZE) == 0, null_str);

	AES_KEY deckey;
	AES_set_decrypt_key(key, 256, &deckey);

    // memset(iv, 0x00, AES_BLOCK_SIZE);
	uint8_t iv2[AES_BLOCK_SIZE];
    memcpy(iv2, iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(in, out, bytes, &deckey, iv2, AES_DECRYPT);
}

std::unique_ptr<uint8_t[]> sha1(const uint8_t* in, const int len)
{
	VALIDATE(in && len > 0, null_str);
	std::unique_ptr<uint8_t[]> md(new uint8_t[SHA_DIGEST_LENGTH]);
	uint8_t* mdptr = md.get();
    SHA1(in, len, mdptr);

	// std::string str = rtc::hex_encode((const char*)mdptr, SHA_DIGEST_LENGTH);

	return md;
}

std::unique_ptr<uint8_t[]> sha256(const uint8_t* in, const int len)
{
	VALIDATE(in && len > 0, null_str);
	std::unique_ptr<uint8_t[]> md(new uint8_t[SHA256_DIGEST_LENGTH]);
    SHA256(in, len, md.get());
	return md;
}

bool verity_sha1(const uint8_t* in, int len, const uint8_t* desire_md)
{
	VALIDATE(in != nullptr && len > 0, null_str);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));

	SHA1(in, len, md);
	return memcmp(md, desire_md, SHA_DIGEST_LENGTH) == 0; 
}

std::vector<trtsp_settings> parse_rtsp_string(const std::string& str, bool check_0, bool unique_url)
{
	std::vector<trtsp_settings> ret;
	if (str.empty()) {
		return ret;
	}
	std::vector<std::string> vstr = utils::split(str, ',', STRIP_SPACES);
	const int fields_per_settings = 3;
	if (vstr.empty() || vstr.size() % fields_per_settings != 0) {
		return ret;
	}

	std::set<std::string> parsed_url;
	bool has_error = false;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); at ++) {
		const std::string name = *it;
		++ it;

		bool require_check = check_0 || at != 0;
		const std::string tcp_str = utils::lowercase(*it);
		bool tcp = true;
		if (require_check) {
			if (tcp_str == "tcp") {
				tcp = true;
			} else if (tcp_str == "udp") {
				tcp = false;
			} else {
				has_error = true;
				break;
			}
		}
		++ it;

		std::string url = *it;
		if (require_check) {
			std::replace(url.begin(), url.end(), ' ', '+');
			if (url.find("rtsp://") != 0 || (unique_url && parsed_url.count(url) > 0)) {
				has_error = true;
				break;
			}
			GURL gurl(url);
			if (!gurl.is_valid()) {
				has_error = true;
				break;
			}
		}
		parsed_url.insert(url);
		++ it;

		ret.push_back(trtsp_settings(name, tcp, url));
	}
	if (has_error) {
		ret.clear();
	}
	return ret;
}

bool is_short_app_dir(const std::string& str)
{
	const std::string prefix = "app-";
	const int should_connectors = 2;
	const int max_chars = 24;
	const int min_chars_one_segment = 2;

	int chars = 0;
	int connectors = 0;
	int size = str.size();
	if (size <= (int)prefix.size()) {
		return false;
	}
	const char* c_str = str.c_str();
	const char* ptr = strstr(c_str, prefix.c_str());
	if (ptr != c_str) {
		return false;
	}
	if (size - prefix.size() > max_chars) {
		return false;
	}
	for (int i = prefix.size(); i < size; i ++) {
		char ch = c_str[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) {
/*
			if (connectors == 0 && (ch >= '0' && ch <= '9')) {
				// <a> must not digit
				return false;
			}
*/
			chars ++;

		} else if (ch == '_') {
			if (connectors == should_connectors) {
				return false;
			}
			if (chars < min_chars_one_segment) {
				return false;
			}
			connectors ++;
			chars = 0;
		} else {
			return false;
		}
	}
	return connectors <= should_connectors && chars >= min_chars_one_segment;
}

bool is_rose_bundleid(const std::string& str, const char separator)
{
	const int should_connectors = 2;
	const int max_chars = 24;
	const int min_chars_one_segment = 2;

	int chars = 0;
	int connectors = 0;
	int size = str.size();
	if (size > max_chars) {
		return false;
	}
	const char* c_str = str.c_str();
	for (int i = 0; i < size; i ++) {
		char ch = c_str[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) {
			if (connectors == 0 && (ch >= '0' && ch <= '9')) {
				// <a> must not digit
				return false;
			}
			chars ++;

		} else if (ch == separator) {
			if (connectors == should_connectors) {
				return false;
			}
			if (chars < min_chars_one_segment) {
				return false;
			}
			connectors ++;
			chars = 0;
		} else {
			return false;
		}
	}
	return connectors == should_connectors && chars >= min_chars_one_segment;
}

bool is_bundleid2(const std::string& bundleid, const std::set<std::string>& exclude, char exclude_separator)
{
	if (!is_bundleid(bundleid)) {
		return false;
	}
	if (!exclude.empty()) {
		if (exclude_separator != '.') {
			const std::string label = utils::replace_all_char(bundleid, '.', exclude_separator);
			return exclude.count(label) == 0;
		} else {
			return exclude.count(bundleid) == 0;
		}
	}
	return true;
}

std::string create_guid(bool line)
{
	char* guid = SDL_CreateGUID(line? SDL_TRUE: SDL_FALSE);
	std::string result = guid;
	SDL_free(guid);
	return result;
}

bool is_guid(const std::string& str, bool line)
{
	std::string str2 = lowercase(str);
	// d1e09a45-9413-4ffc-986a-aaa41615faf6
	const int digits = 32;
	const int lines = 4;
	int size = str2.size();
	if (line) {
		if (size != digits + lines) {
			return false;
		}
		std::replace(str2.begin(), str2.end(), '-', '0');
	} else if (size != digits) {
		return false;
	}
	const char* c_str = str2.c_str();
	for (int i = 0; i < size; i ++) {
		char ch = c_str[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) {
		} else {
			return false;
		}
	}
	return true;
}

} // end namespace utils

namespace chinese {

const int fix_bytes_in_code = 12;
const wchar_t min_unicode = 0x4E00;
const wchar_t max_unicode = 0x9FA5;
static std::unique_ptr<char[]> pinyin_code;

const char* get_pinyin_code()
{ 
	return pinyin_code.get(); 
}

int get_pinyin_code_bytes()
{
	return max_unicode - min_unicode + 1;
}

void create_chinesefrequence_raw()
{
	VALIDATE(game_config::os == os_windows, null_str);
	std::set<wchar_t> label_gb2312_1th;
	{
		// reference to http://ash.jp/code/cn/gb2312tbl.htm
		for (wchar_t wch = 0xb0a0; wch <= 0xd7f9; ) {
			if ((wch & 0xff) == 0xa0) {
				wch ++;
				continue;
			} else if ((wch & 0xff) == 0xff) {
				wch ++;
				wch += 0xa0;
				continue;
			}
			// uint8_t data[3] = {wch & 0xff, (wch & 0xff00) >> 8, '\0'};
			uint8_t data[3] = {(uint8_t)((wch & 0xff00) >> 8), (uint8_t)(wch & 0xff), '\0'};
			std::string str((char*)data, 2);
			str = ansi_2_utf8(str);

			utils::utf8_iterator it(str);
			bool first = false;
			for (; it != utils::utf8_iterator::end(str); ++ it) {
				VALIDATE(!first, null_str);
				label_gb2312_1th.insert(*it);
				first = true;
			}
			wch ++;
		}
	}

	std::set<wchar_t> label_gb2312_2th;
	{
		for (wchar_t wch = 0xd8a0; wch <= 0xf7fe; ) {
			if ((wch & 0xff) == 0xa0) {
				wch ++;
				continue;
			} else if ((wch & 0xff) == 0xff) {
				wch ++;
				wch += 0xa0;
				continue;
			}
			// uint8_t data[3] = {wch & 0xff, (wch & 0xff00) >> 8, '\0'};
			uint8_t data[3] = {(uint8_t)((wch & 0xff00) >> 8), (uint8_t)(wch & 0xff), '\0'};
			std::string str((char*)data, 2);
			str = ansi_2_utf8(str);

			utils::utf8_iterator it(str);
			bool first = false;
			for (; it != utils::utf8_iterator::end(str); ++ it) {
				VALIDATE(!first, null_str);
				VALIDATE(label_gb2312_1th.count(*it) == 0, null_str);
				label_gb2312_2th.insert(*it);

				first = true;
			}
			wch ++;
		}
		// master gb2312 has 3755 chinese. but maybe gb2312 exited but 3500 not, 
		// so label_gb2312_extra.size() maybe larger 3755 - 3500.
	}

	std::set<wchar_t> label_3500_extra;
	{
		const std::string imagenet_comp_path = game_config::path + "/data/core/cert/chinese-3500.txt";
		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch & 0x80) {
				int wch = posix_mku16(file.data[at], file.data[at + 1]);
				std::string str(file.data + at, 2);
				str = ansi_2_utf8(str);

				utils::utf8_iterator it(str);
				bool first = false;
				for (; it != utils::utf8_iterator::end(str); ++ it) {
					VALIDATE(!first, null_str);
					wchar_t ch = *it;
					if (ch != 0x3000) {
						// may be exist chinese-space.
						if (label_gb2312_1th.count(ch) == 0 && label_gb2312_2th.count(ch) == 0) {
							label_3500_extra.insert(ch);
						}
					}
					first = true;
				}
				at ++;
			}
		}
	}

	tfile file(game_config::path + "/data/core/cert/chinesefrequence.raw", GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);
	std::stringstream fp_ss;

	fp_ss << "# total count: " << label_gb2312_1th.size() << " + " << label_gb2312_2th.size() << " + " << label_3500_extra.size();
	fp_ss << " = " << (label_gb2312_1th.size() + label_gb2312_2th.size() + label_3500_extra.size());
	fp_ss << "\n\n";

	fp_ss << "#\n";
	fp_ss << "# gb2312[1th] chinese section. count: " << label_gb2312_1th.size() << "\n";
	fp_ss << "#\n";

	int at = 0;
	for (std::set<wchar_t>::const_iterator it = label_gb2312_1th.begin(); it != label_gb2312_1th.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}
	fp_ss << "\n\n";
	fp_ss << std::setbase(10);

	fp_ss << "#\n";
	fp_ss << "# gb2312[2th] chinese section. count: " << label_gb2312_2th.size() << "\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_gb2312_2th.begin(); it != label_gb2312_2th.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}
	fp_ss << "\n\n";
	fp_ss << std::setbase(10);

	fp_ss << "#\n";
	fp_ss << "# 3500 chinese section that not in gb2312. reference <Chinese curriculum standards (2011 Edition)>. count: " << label_3500_extra.size() << "\n";
	fp_ss << "#\n";

	at = 0;
	for (std::set<wchar_t>::const_iterator it = label_3500_extra.begin(); it != label_3500_extra.end(); ++ it, at ++) {
		wchar_t ch = *it;
		fp_ss << UCS2_to_UTF8(ch) << "(";
		fp_ss << "0x"<< std::setbase(16) << std::setw(4) << std::setfill('0') << ch << ")\n";
	}

	posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().size());
}

void chinesefrequence_raw_2_code()
{
	VALIDATE(game_config::os == os_windows, null_str);
	const int bytes_in_code = sizeof(wchar_t);

	std::string filename = game_config::path + "/data/core/cert/chinesefrequence.raw";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize > 0, null_str);

	std::vector<std::string> vstr = utils::split(file.data, '\n');
	char* data = (char*)malloc(vstr.size() * bytes_in_code);
	memset(data, 0, vstr.size() * bytes_in_code);

	std::set<int> unicodes;
	std::pair<int, size_t> max_size(0, 0);
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		std::string line = *it;
		utils::strip(line);
		const int size = line.size();
		const char* c_str = line.c_str();
		if (size == 0 || c_str[0] == '#') {
			continue;
		}
		size_t pos = line.find('(');
		VALIDATE(pos != std::string::npos, null_str);
		VALIDATE(size == (int)pos + 1 + 6 + 1, null_str);
		VALIDATE(c_str[size - 1] == ')', null_str);
		
		int code = utils::to_int(line.substr(pos + 1, 6));
		VALIDATE(unicodes.count(code) == 0, null_str);
		unicodes.insert(code);

		VALIDATE(code >= 0 && code <= 65535, null_str);
		wchar_t code2 = code;
		VALIDATE(code2 >= min_unicode && code2 <= max_unicode, null_str);
		memcpy(data + at * bytes_in_code, &code2, bytes_in_code); 
		at ++;
	}
	VALIDATE(at == unicodes.size(), null_str);

	filename = game_config::path + "/data/core/cert/chinesefrequence.code";
	tfile file2(filename, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file2.fp, data, unicodes.size() * bytes_in_code);
}

void chinesepinyin_raw_2_code()
{
	VALIDATE(game_config::os == os_windows, null_str);
	std::string filename = game_config::path + "/data/core/cert/chinesepinyin.raw";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize > 0, null_str);

	std::vector<std::string> vstr = utils::split(file.data, '\n');
	char* data = (char*)malloc(vstr.size() * fix_bytes_in_code);
	memset(data, 0, vstr.size() * fix_bytes_in_code);

	std::pair<int, size_t> max_size(0, 0);
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it, at ++) {
		std::vector<std::string> vstr2 = utils::split(*it, ':');
		VALIDATE(vstr2.size() == 2, null_str);
		std::vector<std::string> pinyins = utils::split(vstr2[1], '|');
		std::string pinyin = utils::lowercase(pinyins[0]);
		const char* c_str = pinyin.c_str();
		const int size = pinyin.size();
		for (int at2 = 0; at2 < size; at2 ++) {
			char ch = c_str[at2];
			if (at2 != size - 1) {
				VALIDATE(ch >= 'a' && ch <= 'z', null_str);
			} else {
				VALIDATE((ch >= 'a' && ch <= 'z') || (ch >= '1' && ch <= '5'), null_str);
			}
		}
		if (pinyin.size() > max_size.second) {
			max_size = std::make_pair(min_unicode + at, pinyin.size());
		}
		VALIDATE((int)max_size.second < fix_bytes_in_code, null_str);
		memcpy(data + at * fix_bytes_in_code, pinyin.c_str(), pinyin.size()); 
	}


	filename = game_config::path + "/data/core/cert/chinesepinyin.code";
	tfile file2(filename, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file2.fp, data, vstr.size() * fix_bytes_in_code);
}

void load_pinyin_code()
{
	std::string filename = game_config::path + "/data/core/cert/chinesepinyin.code";
	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize == (max_unicode - min_unicode + 1) * fix_bytes_in_code, null_str);

	pinyin_code.reset(new char[fsize]);
	memcpy(pinyin_code.get(), file.data, fsize);
}

const char* pinyin(wchar_t wch)
{
	if (wch < min_unicode || wch > max_unicode) {
		std::stringstream err;
		err << "0x" << std::setbase(16) << std::setfill('0') << std::setw(4) << wch << " is not chinese unicode.";
		VALIDATE(false, err.str());
		return nullptr;
	}
	return pinyin_code.get() + (wch - min_unicode) * fix_bytes_in_code;
}

bool key_matched(const std::string& src, const std::string& key)
{
	VALIDATE(!src.empty() && !key.empty(), null_str);
	
	utils::utf8_iterator key_ch(key);
	utils::utf8_iterator key_end = utils::utf8_iterator::end(key);
	wchar_t key_wch = *key_ch;
	if (key_wch >= 'A' && key_wch <= 'Z') {
		key_wch += 0x20;
	}

	bool at_least_one = false;
	bool require_calculate_key_ch = false;
	utils::utf8_iterator ch(src);
	for (utils::utf8_iterator end = utils::utf8_iterator::end(src); ch != end; ++ ch) {
		const wchar_t wch = *ch;
		while (true) {
			wchar_t wch2 = wch;
			if (wch2 >= 'A' && wch2 <= 'Z') {
				wch2 += 0x20;
			} else if (key_wch < 0x80 && chinese::is_chinese(wch2)) {
				wch2 = chinese::pinyin(wch2)[0];
			}

			require_calculate_key_ch = false;
			if (wch2 == key_wch) {
				++ key_ch;
				if (key_ch == key_end) {
					return true;
				}
				key_wch = *key_ch;
				if (key_wch >= 'A' && key_wch <= 'Z') {
					key_wch += 0x20;
				}
				at_least_one = true;

			} else if (at_least_one) {
				// if has one dis-match, reset compare.
				at_least_one = false;

				key_ch = utils::utf8_iterator::begin(key);
				key_wch = *key_ch;
				if (key_wch >= 'A' && key_wch <= 'Z') {
					key_wch += 0x20;
				}
				// previous match partial, this dismatch, compare last ch.
				continue;
			}
			break;
		}
	}
	return false;
}

std::string calculate_string_pinyin(const std::string& utf8str)
{
	std::stringstream ss;
	utils::utf8_iterator ch(utf8str);
	int at = 0;
	for (utils::utf8_iterator end = utils::utf8_iterator::end(utf8str); ch != end; ++ ch, at ++) {
		if (at > 0) {
			ss << " ";
		}
		wchar_t wch = *ch;
		if (chinese::is_chinese(wch)) {
			ss << chinese::pinyin(wch);
		} else {
			ss << std::string(ch.substr().first, ch.substr().second);
		}
	}
	return ss.str();
}

} // end namespace chinese