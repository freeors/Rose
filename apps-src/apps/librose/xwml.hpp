/* $Id: loadscreen.hpp 47261 2010-10-28 21:06:14Z mordante $ */
/*
   Copyright (C) 2005 - 2010 by Joeri Melis <joeri_melis@hotmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef LIBROSE_XWML_HPP
#define LIBROSE_XWML_HPP

#include <string>
#include <map>
#include "builder.hpp"

class config;

#define BASENAME_DATA		"data.bin"
#define BASENAME_GUI		"gui.bin"
#define BASENAME_LANGUAGE	"language.bin"
#define BASENAME_APLT		"aplt.bin"

void wml_config_to_file(const std::string &fname, const config &cfg, uint32_t nfiles = 0, uint32_t sum_size = 0, uint32_t modified = 0, const std::map<std::string, std::string>& owners = std::map<std::string, std::string>());
void wml_config_from_file(const std::string &fname, config &cfg, uint32_t* nfiles = NULL, uint32_t* sum_size = NULL, uint32_t* modified = NULL);
bool wml_checksum_from_file(const std::string &fname, uint32_t* nfiles = NULL, uint32_t* sum_size = NULL, uint32_t* modified = NULL);
void collect_msgid_from_cfg(const std::string& fname, const config &cfg, const std::map<std::string, std::string>& owners, bool owner_is_app);
unsigned char calcuate_xor_from_file(const std::string &fname);
terrain_builder::building_rule* wml_building_rules_from_file(const std::string& fname, uint32_t* rules_size_ptr);

#endif
