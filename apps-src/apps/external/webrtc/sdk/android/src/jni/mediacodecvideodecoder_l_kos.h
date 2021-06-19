/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_MEDIACODECVIDEODECODER_KOS_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_MEDIACODECVIDEODECODER_KOS_H_

#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "common_types.h"
#include <kosapi/mediacodec.h>

class MediaCodecVideoDecoder_l
{
public:
	// This class is constructed, operated, and destroyed by its C++ incarnation,
	// so the class and its methods have non-public visibility.  The API this
	// class exposes aims to mimic the webrtc::VideoDecoder API as closely as
	// possibly to minimize the amount of translation work necessary.

	static const long MAX_DECODE_TIME_MS = 200;

	// Timeout for input buffer dequeue.
	static const int DEQUEUE_INPUT_TIMEOUT = 500000;
	// Timeout for codec releasing.
	static const int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;
	// Max number of output buffers queued before starting to drop decoded frames.
	static const int MAX_QUEUED_OUTPUTBUFFERS = 3;
	
	// List of supported HW VP8 decoders.
	static const std::vector<std::string> supportedVp8HwCodecPrefixes;
	// List of supported HW VP9 decoders.
	static const std::vector<std::string> supportedVp9HwCodecPrefixes;
	// List of supported HW H.264 decoders.
	static const std::vector<std::string> supportedH264HwCodecPrefixes;
	// List of supported HW H.264 high profile decoders.
	static const std::vector<std::string> supportedQcomH264HighProfileHwCodecPrefix;
	static const std::vector<std::string> supportedExynosH264HighProfileHwCodecPrefix;

	// Allowable color formats supported by codec - in order of preference.
	static const std::set<int> supportedColorList;
	
	struct TimeStamps {
		TimeStamps(int64_t decodeStartTimeMs, int64_t timeStampMs, int64_t ntpTimeStampMs)
			: decodeStartTimeMs(decodeStartTimeMs)
			, timeStampMs(timeStampMs)
			, ntpTimeStampMs(ntpTimeStampMs)
		{}
		// Time when this frame was queued for decoding.
		int64_t decodeStartTimeMs;
		// Only used for bookkeeping in Java. Stores C++ inputImage._timeStamp value for input frame.
		int64_t timeStampMs;
		// Only used for bookkeeping in Java. Stores C++ inputImage.ntp_time_ms_ value for input frame.
		int64_t ntpTimeStampMs;
	};
  
	static bool isVp8HwSupported();
	static bool isVp9HwSupported();
	static bool isH264HwSupported();
	static bool isH264HighProfileHwSupported();

	// Helper struct for findDecoder() below.
	struct DecoderProperties 
	{
		DecoderProperties(const std::string& _codecName, int _colorFormat)
			: codecName(_codecName)
			, colorFormat(_colorFormat) 
		{}
		const std::string codecName; // OpenMax component name for VP8 codec.
		const int colorFormat; // Color format supported by codec.
	};

	static DecoderProperties findDecoder(const std::string& mime, const std::vector<std::string>& supportedCodecPrefixes);

	MediaCodecVideoDecoder_l();

	void checkOnMediaCodecThread();

	// Pass null in |surfaceTextureHelper| to configure the codec for ByteBuffer output.
	bool initDecode(webrtc::VideoCodecType type, int width, int height);

	// Resets the decoder so it can start decoding frames with new resolution.
	// Flushes MediaCodec and clears decoder output buffers.
	void reset(int width, int height);

	void release();
	// Dequeue an input buffer and return its index, -1 if no input buffer is
	// available, or -2 if the codec is no longer operative.
	int dequeueInputBuffer();
	bool getInputBuffer(int index, kosABuffer& buffer);

	bool queueInputBuffer(int inputBufferIndex, int size, int64_t presentationTimeStamUs,
		int64_t timeStampMs, int64_t ntpTimeStamp);


	// Helper struct for dequeueOutputBuffer() below.
	struct DecodedOutputBuffer {
		DecodedOutputBuffer()
			: index(-1)
			, offset(0)
			, size(0)
			, presentationTimeStampMs(0)
			, timeStampMs(0)
			, ntpTimeStampMs(0)
			, decodeTimeMs(0)
			, endDecodeTimeMs(0)
		{}

		DecodedOutputBuffer(int index, int offset, int size, int64_t presentationTimeStampMs,
			int64_t timeStampMs, int64_t ntpTimeStampMs, int64_t decodeTime, int64_t endDecodeTime)
			: index(index)
			, offset(offset)
			, size(size)
			, presentationTimeStampMs(presentationTimeStampMs)
			, timeStampMs(timeStampMs)
			, ntpTimeStampMs(ntpTimeStampMs)
			, decodeTimeMs(decodeTime)
			, endDecodeTimeMs(endDecodeTime)
		{}

		bool valid() const { return index >= 0; }

		int index;
		int offset;
		int size;
		// Presentation timestamp returned in dequeueOutputBuffer call.
		int64_t presentationTimeStampMs;
		// C++ inputImage._timeStamp value for output frame.
		int64_t timeStampMs;
		// C++ inputImage.ntp_time_ms_ value for output frame.
		int64_t ntpTimeStampMs;
		// Number of ms it took to decode this frame.
		int64_t decodeTimeMs;
		// System time when this frame decoding finished.
		int64_t endDecodeTimeMs;
	};

	// Returns null if no decoded buffer is available, and otherwise a DecodedByteBuffer.
	// Throws IllegalStateException if call is made on the wrong thread, if color format changes to an
	// unsupported format, or if |mediaCodec| is not in the Executing state. Throws CodecException
	// upon codec error.
	DecodedOutputBuffer dequeueOutputBuffer(int dequeueTimeoutMs);
	bool getOutputBuffer(int index, kosABuffer& buffer);

	// Release a dequeued output byte buffer back to the codec for re-use. Should only be called for
	// non-surface decoding.
	// Throws IllegalStateException if the call is made on the wrong thread, if codec is configured
	// for surface decoding, or if |mediaCodec| is not in the Executing state. Throws
	// MediaCodec.CodecException upon codec error.
	void returnDecodedOutputBuffer(int index);

	int getColorFormat()
	{
		return colorFormat;
	}

	int getWidth() {
		return width;
	}

	int getHeight() {
		return height;
	}

	int getStride() {
		return stride;
	}

	int getSliceHeight() {
		return sliceHeight;
	}

private:
	std::unique_ptr<rtc::ThreadChecker> mediaCodecThread;
	int mediaCodec;

	int colorFormat;
	int width;
	int height;
	int stride;
	int sliceHeight;
	bool hasDecodedFirstFrame;
	std::queue<TimeStamps> decodeStartTimeMs;
};

#endif