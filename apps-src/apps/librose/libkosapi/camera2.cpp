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

#include <kosapi/camera.h>

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

class tcamera2_executor : public tworker 
{
public:
	tcamera2_executor(void* user) : user_(user) { thread_->Start(); }

private:
	void DoWork() override;
	void OnWorkStart() override {}
	void OnWorkDone() override {}

private:
	void* user_;
};

static std::unique_ptr<tcamera2_executor> executor;
static bool exit_executor_loop = false;
static bool preview_started = false;
static fdid_camera2_frame_captured did_camera2_frame_captured = nullptr;
static void* user = nullptr;

int kosGetNumberOfCameras() 
{
	return 0;
}

int kosGetCameraInfo(int cameraId, char* info) 
{
	return 0;
}

void tcamera2_executor::DoWork() 
{
	int frames = 0;
	while (!exit_executor_loop) {
		if (preview_started && did_camera2_frame_captured) {
			std::string filename = game_config::preferences_dir + "/nv21.dat";
			if (frames & 1) {
				filename = game_config::preferences_dir + "/nv21-2.dat";
            }
			tfile file(filename, GENERIC_READ, OPEN_EXISTING);
			int fsize = file.read_2_data();
			if (fsize > 0) {
				did_camera2_frame_captured(file.data, fsize, 1280, 720, 0, 0, user);
			}
			frames ++;
        }
		SDL_Delay(1000);
	}
}

int kosSetupCamera(int cameraId, const char* clientPackageName) 
{
	VALIDATE(!executor.get(), null_str);
	VALIDATE(!preview_started, null_str);
	executor.reset(new tcamera2_executor(nullptr));
	return 0;
}

void reset_executor()
{
	executor.reset();
}

void kosReleaseCamera(int cameraId) 
{
	VALIDATE(executor.get(), null_str);
	exit_executor_loop = true;

	// tworker must be destroy in main thread. so it will throw exception.
	executor.reset();
	// instance->sdl_thread().Invoke<void>(RTC_FROM_HERE, rtc::Bind(&reset_executor));

	exit_executor_loop = false;
}

int kosStartPreviewCamera(int cameraId, fdid_camera2_frame_captured did, void* _user) 
{
	VALIDATE(!preview_started, null_str);

	did_camera2_frame_captured = did;
	user = _user;
	preview_started = true;
	return 0;
}

void kosStopPreviewCamera(int cameraId) 
{
	VALIDATE(preview_started, null_str);
	preview_started = false;
	did_camera2_frame_captured = nullptr;
	user = nullptr;
}