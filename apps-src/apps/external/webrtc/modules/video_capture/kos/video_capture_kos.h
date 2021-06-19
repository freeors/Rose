/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_ANDROID_H_

#include "rtc_base/thread_checker.h"
#include "rtc_base/timestampaligner.h"
#include "media/base/videoadapter.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "modules/video_capture/video_capture_impl.h"
#include "common_video/include/i420_buffer_pool.h"

namespace webrtc {
namespace videocapturemodule {

class VideoCapturekOS: public VideoCaptureImpl 
{
public:
	explicit VideoCapturekOS(int at);
	virtual ~VideoCapturekOS();

	static rtc::scoped_refptr<VideoCaptureModule> Create(const char* device_unique_id_utf8);

	// Implementation of VideoCaptureImpl.
	int32_t StartCapture(const VideoCaptureCapability& capability) override;
	int32_t StopCapture() override;
	bool CaptureStarted() override;
	int32_t CaptureSettings(VideoCaptureCapability& settings) override;

	void onByteBufferFrameCaptured(const void* frame_data, int length,
			int width, int height, VideoRotation rotation, int64_t timestamp_ns);

private:
	bool setup();
	void release();
	bool AdaptFrame(int width,
                                         int height,
                                         int64_t time_us,
                                         int* out_width,
                                         int* out_height,
                                         int* crop_width,
                                         int* crop_height,
                                         int* crop_x,
                                         int* crop_y);

private:
	int at_;
	bool is_capturing_;
	VideoCaptureCapability capability_;

	// Stores thread ID in the constructor.
	// We can then use ThreadChecker::CalledOnValidThread() to ensure that
	// other methods are called from the same thread.
	rtc::ThreadChecker thread_checker_;

	// Set to true by Init() and false by Close().
	bool initialized_;

	rtc::TimestampAligner timestamp_aligner_;
	cricket::VideoAdapter video_adapter_;

	NV12ToI420Scaler nv12toi420_scaler_;
	I420BufferPool buffer_pool_;
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_IOS_VIDEO_CAPTURE_IOS_H_
