/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_SERVER_SHADOW_KOS_H
#define FREERDP_SERVER_SHADOW_KOS_H

#include <freerdp/assistance.h>

#include <freerdp/server/shadow.h>

#include <rtc_client.hpp>

// webrtc
#include "rtc_base/event.h"
#include "base/threading/thread.h"

#include "rose_config.hpp"
#include <kosapi/gui.h>

struct kosShadowSubsystem;
class trecord_screen;

struct tencoded_image
{
    // if ref of image is 0, image's data will be release.
    scoped_refptr<net::IOBufferWithSize> image;

    int orientation;
};

class trecord_screen
{
public:
	trecord_screen(kosShadowSubsystem& subsystem)
        : captured_is_h264(true)
        , max_encoded_threshold(3)
        , new_capture_ticks(0)
        , total_capture_frames(0)
        , last_capture_frames(0)
		, last_capture_bytes(0)
        , max_one_frame_bytes(0)
        , subsystem_(subsystem)
        , input_created_(false)
        , peer_(nullptr)
	{
        // pixel_buf_ is h264 data
		pixel_buf_ = new uint8_t[1920 * 1080 * 4];
	}
	~trecord_screen()
	{
		delete[] pixel_buf_;
	}

	void start(freerdp_peer* peer);
	void stop();
    bool thread_started() const { return thread_.get() != nullptr; }

    void did_screen_captured(uint8_t* pixel_buf, int length, int width, int height, uint32_t flags);

    threading::mutex& encoded_images_mutex() { return encoded_images_mutex_; }

public:
    const bool captured_is_h264;
	uint8_t* pixel_buf_;
    const int max_encoded_threshold;
    std::queue<tencoded_image> encoded_images;

    uint32_t new_capture_ticks;
    uint32_t total_capture_frames;
    int last_capture_frames;
	int last_capture_bytes;
    int max_one_frame_bytes;

private:
	void start_internal();
    void clear_session_variables();

private:
    kosShadowSubsystem& subsystem_;
	std::unique_ptr<base::Thread> thread_;
    threading::mutex encoded_images_mutex_;
    bool input_created_;
    freerdp_peer* peer_;
};

struct kosShadowSubsystem
{
	rdpShadowSubsystem base;

	trecord_screen* record_screen;
    uint32_t max_fps_to_encoder;
};

#ifdef __cplusplus
extern "C"
{
#endif

int rose_shadow_subsystem_start(rdpShadowSubsystem* arg, freerdp_peer* peer, uint32_t max_fps_to_encoder);
int rose_shadow_subsystem_stop(rdpShadowSubsystem* arg);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_KOS_H */
