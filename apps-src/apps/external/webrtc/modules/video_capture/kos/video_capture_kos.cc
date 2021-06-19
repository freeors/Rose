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

#include "api/video/i420_buffer.h"
#include "modules/video_capture/kos/device_info_kos.h"
#include "modules/video_capture/kos/video_capture_kos.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"
#include <kosapi/camera.h>

using namespace webrtc;
using namespace videocapturemodule;

#ifdef ANDROID
rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(const char* deviceUniqueIdUTF8) 
{
	return VideoCapturekOS::Create(deviceUniqueIdUTF8);
}
#endif

VideoCapturekOS::VideoCapturekOS(int at)
    : VideoCaptureImpl()
	, at_(at)
	, is_capturing_(false)
	, initialized_(false)
{
	capability_.width = kDefaultWidth;
	capability_.height = kDefaultHeight;
	capability_.maxFPS = kDefaultFrameRate;
}

VideoCapturekOS::~VideoCapturekOS() 
{
	release();
}

rtc::scoped_refptr<VideoCaptureModule> VideoCapturekOS::Create(const char* deviceUniqueIdUTF8) 
{
	if (!deviceUniqueIdUTF8[0]) {
		return NULL;
	}

	int id = -1;
	for (std::vector<std::string>::iterator it = DeviceInfokOS::device_names.begin(); it != DeviceInfokOS::device_names.end(); ++ it) {
		const std::string& name = *it;
		if (!strcmp(name.c_str(), deviceUniqueIdUTF8)) {
			id = std::distance(DeviceInfokOS::device_names.begin(), it);
			break;
		}
	}
	SDL_Log("Create, deviceUniqueIdUTF8: %s, id: %i", deviceUniqueIdUTF8, id);

	rtc::scoped_refptr<VideoCapturekOS> capture_module(new rtc::RefCountedObject<VideoCapturekOS>(id));

	const int32_t name_length = strlen(deviceUniqueIdUTF8);
	if (name_length > kVideoCaptureUniqueNameLength) {
		return nullptr;
	}

	capture_module->_deviceUniqueId = new char[name_length + 1];
	strncpy(capture_module->_deviceUniqueId, deviceUniqueIdUTF8, name_length + 1);
	capture_module->_deviceUniqueId[name_length] = '\0';

	return capture_module;
}

static void onByteBufferFrameCaptured_static(const void* frame_data,
                                                    int length,
                                                    int width,
                                                    int height,
                                                    int rotation,
                                                    int64_t timestamp_ns, void* user)
{
	VideoCapturekOS* capture = reinterpret_cast<VideoCapturekOS*>(user);
	capture->onByteBufferFrameCaptured(frame_data, length, width, height, (VideoRotation)rotation, timestamp_ns);
}

int32_t VideoCapturekOS::StartCapture(const VideoCaptureCapability& capability)
{
	// RTC_CHECK(thread_checker_.CalledOnValidThread());

	SDL_Log("VideoCapturekOS::StartCapture, (%i x %i x %i), E", capability.width, capability.height, capability.maxFPS);

	capability_ = capability;

	if (!setup() || kosStartPreviewCamera(at_, onByteBufferFrameCaptured_static, this) != 0) {
		release();
		SDL_Log("VideoCapturekOS::StartCapture, (%i x %i x %i), X, Fail", capability.width, capability.height, capability.maxFPS);
		return -1;
	}

	is_capturing_ = true;

	SDL_Log("VideoCapturekOS::StartCapture, (%i x %i x %i), X", capability.width, capability.height, capability.maxFPS);
	return 0;
}

int32_t VideoCapturekOS::StopCapture() 
{
	// RTC_CHECK(thread_checker_.CalledOnValidThread());
	SDL_Log("VideoCapturekOS::StopCapture, E");

	if (!is_capturing_) {
		SDL_Log("VideoCapturekOS::StopCapture, X, not capturing, do nothing");
		return 0;
	}
	kosStopPreviewCamera(at_);

	is_capturing_ = false;
	SDL_Log("VideoCapturekOS::StopCapture, X");
	return 0;
}

bool VideoCapturekOS::CaptureStarted() 
{
	return is_capturing_;
}

int32_t VideoCapturekOS::CaptureSettings(VideoCaptureCapability& settings) 
{
	settings = capability_;
	settings.videoType = VideoType::kNV12;
	return 0;
}

bool VideoCapturekOS::setup()
{
	// RTC_CHECK(thread_checker_.CalledOnValidThread());

	SDL_Log("VideoCaptureAndroid::setup, E");

	if (initialized_) {
		SDL_Log("VideoCaptureAndroid::setup, X, initialized, do nothing");
		return true;
	}

	const char* clientPackageName = "com.leagor.iaccess";
	int ret = kosSetupCamera(at_, clientPackageName);
	if (ret == 0) {
		initialized_ = true;
	}
	SDL_Log("VideoCaptureAndroid::setup, X, ret: %i", ret);
	return ret == 0? true: false;
}

void VideoCapturekOS::release()
{
	// RTC_CHECK(thread_checker_.CalledOnValidThread());

	SDL_Log("VideoCaptureAndroid::release, E");

	if (!initialized_) {
		SDL_Log("VideoCaptureAndroid::release, X, not initialized_, do nothing");
		return;
	}

	StopCapture();
	kosReleaseCamera(at_);
	initialized_ = false;
	SDL_Log("VideoCaptureAndroid::release, X");
}

void VideoCapturekOS::onByteBufferFrameCaptured(const void* frame_data,
                                                    int length,
                                                    int width,
                                                    int height,
                                                    VideoRotation rotation,
                                                    int64_t timestamp_ns) {
	// RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

	int64_t camera_time_us = timestamp_ns / rtc::kNumNanosecsPerMicrosec;
	int64_t translated_camera_time_us =
		timestamp_aligner_.TranslateTimestamp(camera_time_us, rtc::TimeMicros());

	int adapted_width;
	int adapted_height;
	int crop_width;
	int crop_height;
	int crop_x;
	int crop_y;

	if (!AdaptFrame(width, height, camera_time_us, &adapted_width,
					&adapted_height, &crop_width, &crop_height, &crop_x,
					&crop_y)) {
		return;
	}

	const uint8_t* y_plane = static_cast<const uint8_t*>(frame_data);
	const uint8_t* uv_plane = y_plane + width * height;
	const int uv_width = (width + 1) / 2;

	RTC_CHECK_GE(length, width * height + 2 * uv_width * ((height + 1) / 2));

	// Can only crop at even pixels.
	crop_x &= ~1;
	crop_y &= ~1;
	// Crop just by modifying pointers.
	y_plane += width * crop_y + crop_x;
	uv_plane += uv_width * crop_y + crop_x;

	rtc::scoped_refptr<webrtc::I420Buffer> buffer =
		buffer_pool_.CreateBuffer(adapted_width, adapted_height);

	nv12toi420_scaler_.NV12ToI420Scale(
		y_plane, width, uv_plane, uv_width * 2, crop_width, crop_height,
		buffer->MutableDataY(), buffer->StrideY(),
		// Swap U and V, since we have NV21, not NV12.
		buffer->MutableDataV(), buffer->StrideV(), buffer->MutableDataU(),
		buffer->StrideU(), buffer->width(), buffer->height());

	VideoFrame converted_frame(buffer, rotation, translated_camera_time_us);
	DeliverCapturedFrame(converted_frame);
}

bool VideoCapturekOS::AdaptFrame(int width,
                                     int height,
                                     int64_t time_us,
                                     int* out_width,
                                     int* out_height,
                                     int* crop_width,
                                     int* crop_height,
                                     int* crop_x,
                                     int* crop_y) {
  /* {
     rtc::CritScope lock(&stats_crit_);
     stats_ = rtc::Optional<Stats>({width, height});
   }

   if (!broadcaster_.frame_wanted()) {
     return false;
   }
 */
  if (!video_adapter_.AdaptFrameResolution(
          width, height, time_us * rtc::kNumNanosecsPerMicrosec, crop_width,
          crop_height, out_width, out_height)) {
    // VideoAdapter dropped the frame.
    return false;
  }

  *crop_x = (width - *crop_width) / 2;
  *crop_y = (height - *crop_height) / 2;
  return true;
}