/* $Id: sha1.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010 by Benoit Timbert <benoit.timbert@free.fr>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_PROTOBUF_HPP_INCLUDED
#define LIBROSE_PROTOBUF_HPP_INCLUDED

#include <string>
#include "SDL_types.h"
#include <google/protobuf/message_lite.h>

namespace protobuf {

// If id is null, it is an app call. 
// In order to app needn't pass id every time, give it a default value: ""

void load_pb(int type);
void load_sha1pb(const std::string& filepath, ::google::protobuf::MessageLite* lite, bool use_backup);
void load_sha1pb(int type, bool use_backup);

void write_pb(int type);
void write_sha1pb(const std::string& filepath, ::google::protobuf::MessageLite* lite, bool use_backup);
void write_sha1pb(int type, bool use_backup);

}

#endif
