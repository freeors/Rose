/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <SDL_log.h>
#include <utility>

#include <kosapi/net.h>

#include "serialization/string_utils.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "base_instance.hpp"
#include "rtc_base/bind.h"

#ifndef _WIN32
#error "This file is impletement of libkosapi.so on windows!"
#endif

static fkosNetReceiveBroadcast kosNetReceiveBroadcast = nullptr;
static void* kosNetReceiveBroadcastUser = nullptr;

void kosNetSetReceiveBroadcast(fkosNetReceiveBroadcast did, void* user)
{
	kosNetReceiveBroadcast = did;
	kosNetReceiveBroadcastUser = user;
}

int kosNetSendMsg(const char* msg, char* result, int maxBytes)
{
	VALIDATE(msg != nullptr && msg[0] != '\0', null_str);
	VALIDATE(result != nullptr && maxBytes > 0, null_str);

	result[0] = '\0';
	SDL_Log("kosNetSendMsg, msg: %s, maxBytes: %i", msg, maxBytes);
	return 200;
}

void kosNetGetCfg(char* result, int max_bytes)
{
	VALIDATE(result != nullptr, null_str);
}

bool kosNetSetCfg(const char* msg)
{
	VALIDATE(msg != nullptr, null_str);
	VALIDATE(msg[0] != '\0', null_str);

	return true;
}