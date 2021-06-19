/*
   Copyright (C) 2003 - 2015 by David White <dave@whitevine.net>


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
 *  Templates and utility-routines for strings and numbers.
 */

#ifndef LIBROSE_UTIL_C_H_INCLUDED
#define LIBROSE_UTIL_C_H_INCLUDED

#include <SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool os_is_mobile(int os);
int freerdp_os();
int gethostname2(char* name, int namelen);
int hostname_2_os(const char* name);
bool hostname_is_mobile(const char* name);
const char* app_version_from_hostname(const char* name, int* len_ptr);

#ifdef __cplusplus
}
#endif

#endif
