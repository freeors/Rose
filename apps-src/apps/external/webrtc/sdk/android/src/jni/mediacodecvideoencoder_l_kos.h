/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_MEDIACOENCVIDEODECODER_KOS_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_MEDIACOENCVIDEODECODER_KOS_H_

#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "common_types.h"
#include <kosapi/mediacodec.h>

// Class describing supported media codec properties.
struct MediaCodecProperties
{
	MediaCodecProperties(const std::string& codecPrefix, BitrateAdjustmentType bitrateAdjustmentType)
		: codecPrefix(codecPrefix)
		, bitrateAdjustmentType(bitrateAdjustmentType)
	{}

	std::string codecPrefix;
	// Flag if encoder implementation does not use frame timestamps to calculate frame bitrate
	// budget and instead is relying on initial fps configuration assuming that all frames are
	// coming at fixed initial frame rate. Bitrate adjustment is required for this case.
	BitrateAdjustmentType bitrateAdjustmentType;
};

class MediaCodecVideoEncoder_l
{
public:
	static const int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000; // Timeout for codec releasing.
	static const int DEQUEUE_TIMEOUT = 0; // Non-blocking, no wait.
	static const int BITRATE_ADJUSTMENT_FPS = 30;
	static const int MAXIMUM_INITIAL_FPS = 30;
	static const double BITRATE_CORRECTION_SEC;
	// Maximum bitrate correction scale - no more than 4 times.
	static const double BITRATE_CORRECTION_MAX_SCALE;
	// Amount of correction steps to reach correction maximum scale.
	static const int BITRATE_CORRECTION_STEPS = 20;
	// Forced key frame interval - used to reduce color distortions on Qualcomm platform.
	static const long QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS = 15000;
	static const long QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS = 20000;
	static const long QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS = 15000;

	// Active running encoder instance. Set in initEncode() (called from native code)
	// and reset to null in release() call.
	static int codecErrors;

	std::unique_ptr<rtc::ThreadChecker> mediaCodecThread;
	int mediaCodec;
	int profile;
	int width;
	int height;

	static const int VIDEO_AVCProfileHigh = 8;
	static const int VIDEO_AVCLevel3 = 0x100;

	// Should be in sync with webrtc::H264::Profile.
	enum H264Profile {
		CONSTRAINED_BASELINE = 0,
		BASELINE = 1,
		MAIN = 2,
		CONSTRAINED_HIGH = 3,
		HIGH =4
	};

	// List of devices with poor H.264 encoder quality.
	// HW H.264 encoder on below devices has poor bitrate control - actual
	// bitrates deviates a lot from the target value.
	static const std::set<std::string> H264_HW_EXCEPTION_MODELS;

	// Bitrate modes - should be in sync with OMX_VIDEO_CONTROLRATETYPE defined
	// in OMX_Video.h
	static const int VIDEO_ControlRateConstant = 2;
	// NV12 color format supported by QCOM codec, but not declared in MediaCodec -
	// see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
	static const int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
	// Allowable color formats supported by codec - in order of preference.
	static const std::set<int> supportedColorList;
	webrtc::VideoCodecType type;
	int colorFormat;

	// Variables used for dynamic bitrate adjustment.
	BitrateAdjustmentType bitrateAdjustmentType;
	double bitrateAccumulator;
	double bitrateAccumulatorMax;
	double bitrateObservationTimeMs;
	int bitrateAdjustmentScaleExp;
	int targetBitrateBps;
	int targetFps;

	// Interval in ms to force key frame generation. Used to reduce the time of color distortions
	// happened sometime when using Qualcomm video encoder.
	int64_t forcedKeyFrameMs;
	int64_t lastKeyFrameMs;

	// SPS and PPS NALs (Config frame) for H.264.
	std::unique_ptr<uint8_t[]> configData;
	int configDataSize;

	// Functions to query if HW encoding is supported.
	static bool isVp8HwSupported();

	static bool isVp9HwSupported();

	static bool isH264HwSupported();

	static bool isH264HighProfileHwSupported();

	// Helper struct for findHwEncoder() below.
	struct EncoderProperties {
		EncoderProperties(const std::string& codecName, int colorFormat, BitrateAdjustmentType bitrateAdjustmentType)
			: codecName(codecName)
			, colorFormat(colorFormat)
			, bitrateAdjustmentType(bitrateAdjustmentType)
		{
		}
		std::string codecName; // OpenMax component name for HW codec.
		int colorFormat; // Color format supported by codec.
		BitrateAdjustmentType bitrateAdjustmentType; // Bitrate adjustment type
	};

	static EncoderProperties findHwEncoder(const std::string& mime, const std::vector<MediaCodecProperties>& supportedHwCodecProperties, const std::set<int>& colorList);

	MediaCodecVideoEncoder_l();

	void checkOnMediaCodecThread();

	bool initEncode(webrtc::VideoCodecType type, int profile, int width, int height, int kbps, int fps);

	void checkKeyFrameRequired(bool requestedKeyFrame, int64_t presentationTimestampUs);

	bool encodeBuffer(bool isKeyframe, int inputBuffer, int size, int64_t presentationTimestampUs);

	void release();

	bool setRates(int kbps, int frameRate);

	// Dequeue an input buffer and return its index, -1 if no input buffer is
	// available, or -2 if the codec is no longer operative.
	int dequeueInputBuffer();
	bool getInputBuffer(int index, kosABuffer& buffer);
	int getInputBufferLength() const;

	// Helper struct for dequeueOutputBuffer() below.
	struct OutputBufferInfo {
		OutputBufferInfo(
			int index, const uint8_t* buffer, size_t buffer_size, bool isKeyFrame, int64_t presentationTimestampUs) 
			: index(index)
			, buffer(buffer)
			, buffer_size(buffer_size)
			, isKeyFrame(isKeyFrame)
			, presentationTimestampUs(presentationTimestampUs)
		{}

		bool valid() const { return index >= 0; }

		int index;
		const uint8_t* buffer;
		size_t buffer_size;
		bool isKeyFrame;
		int64_t presentationTimestampUs;
	};

	// Dequeue and return an output buffer, or null if no output is ready.  Return
	// a fake OutputBufferInfo with index -1 if the codec is no longer operable.
	OutputBufferInfo dequeueOutputBuffer();
	bool getOutputBuffer(int index, kosABuffer& buffer);

	double getBitrateScale(int bitrateAdjustmentScaleExp);

	void reportEncodedFrame(int size);

	// Release a dequeued output buffer back to the codec for re-use.  Return
	// false if the codec is no longer operable.
	bool releaseOutputBuffer(int index);

	int getColorFormat() {
		return colorFormat;
	}
};

#endif // WEBRTC_SDK_ANDROID_SRC_JNI_MEDIACOENCVIDEODECODER_KOS_H_