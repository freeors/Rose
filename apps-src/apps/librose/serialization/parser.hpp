/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2015 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#ifndef SERIALIZATION_PARSER_HPP_INCLUDED
#define SERIALIZATION_PARSER_HPP_INCLUDED

#include "global.hpp"
#include "config.hpp"


class abstract_validator;
// Read data in, clobbering existing data.
void read(config &cfg, std::istream &in,
		  abstract_validator * validator = NULL); 	// Throws config::error
void read(config &cfg, const std::string &in,
		  abstract_validator * validator = NULL); 	// Throws config::error
void read_gz(config &cfg, std::istream &in,
			 abstract_validator * validator = NULL);
void read_bz2(config &cfg, std::istream &in,
			 abstract_validator * validator = NULL);

void write(std::ostream &out, config const &cfg, unsigned int level=0, const std::string& def_textdomain = "rose-lib");
void write_gz(std::ostream &out, config const &cfg);
void write_bz2(std::ostream &out, config const &cfg);
void write_key_val(std::ostream &out, const std::string &key, const config::attribute_value &value, unsigned level, std::string &textdomain);
void write_open_child(std::ostream &out, const std::string &child, unsigned int level);
void write_close_child(std::ostream &out, const std::string &child, unsigned int level);

#endif
