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

#include <kosapi/sys.h>

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

void kosGetVersion(char* ver, int max_bytes)
{
    strcpy(ver, "1.0.3-20210422");
}

static int screen_width = 0;
static int screen_height = 0;
bool kosCreateInput(bool keyboard, int _screen_width, int _screen_height)
{
    VALIDATE(_screen_width > 0 && _screen_height > 0, null_str);
    screen_width = _screen_width;
    screen_height = _screen_height;
    return true;
}

void kosDestroyInput()
{
    screen_width = 0;
    screen_height = 0;
}

uint32_t kosSendInput(uint32_t input_count, KosInput* inputs)
{
    VALIDATE(input_count > 0 && inputs != nullptr, null_str);
    if (screen_width == 0 || screen_height == 0) {
        return 0;
    }
    INPUT* inputs_dst = (INPUT*)malloc(sizeof(INPUT) * input_count);
    memset(inputs_dst, 0, sizeof(INPUT) * input_count);

    for (int n = 0; n < (int)input_count; n ++) {
        KosInput* src = inputs + n;
        INPUT* dst = inputs_dst + n;
        if (src->type == KOS_INPUT_MOUSE) {
            VALIDATE(screen_width > 0 && screen_height > 0, null_str);
            dst->type = INPUT_MOUSE;
		    dst->mi.dx = (int)((float)src->u.mi.dx * (65535.0f / screen_width));
		    dst->mi.dy = (int)((float)src->u.mi.dy * (65535.0f / screen_height));
            if (src->u.mi.flags & MOUSEEVENTF_LEFTDOWN) {
                int ii = 0;
                SDL_Log("kosSendInput, INPUT_MOUSE, (%i, %i) =>(%i, %i)", src->u.mi.dx, src->u.mi.dy, (int)dst->mi.dx, (int)dst->mi.dy);
            }
            dst->mi.mouseData = src->u.mi.mouse_data;
            dst->mi.dwFlags = src->u.mi.flags;
            dst->mi.time = src->u.mi.time;

        } else if (src->type == KOS_INPUT_KEYBOARD) {
            dst->type = INPUT_KEYBOARD;
            dst->ki.wVk = src->u.ki.virtual_key;
            dst->ki.wScan = src->u.ki.scan_code;
            dst->ki.dwFlags = src->u.ki.flags;
            dst->ki.time = src->u.ki.time;
        }
    }
    uint32_t ret = SendInput(input_count, inputs_dst, sizeof(INPUT));
    free(inputs_dst);
    return ret;
}