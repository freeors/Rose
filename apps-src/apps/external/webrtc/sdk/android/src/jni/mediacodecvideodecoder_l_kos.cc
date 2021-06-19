/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "mediaCodecvideodecoder_l_kos.h"

#include "sdk/android/src/jni/androidmediacodeccommon.h"
#include <SDL_log.h>
#include <SDL_timer.h>

const std::vector<std::string> MediaCodecVideoDecoder_l::supportedVp8HwCodecPrefixes = {
	"OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel."};
const std::vector<std::string> MediaCodecVideoDecoder_l::supportedVp9HwCodecPrefixes = {
	"OMX.qcom.", "OMX.Exynos."};
const std::vector<std::string> MediaCodecVideoDecoder_l::supportedH264HwCodecPrefixes = {
	"OMX.qcom.", "OMX.Intel.", "OMX.Exynos.", "OMX.rk."};
const std::vector<std::string> MediaCodecVideoDecoder_l::supportedQcomH264HighProfileHwCodecPrefix = {
	"OMX.qcom."};
const std::vector<std::string> MediaCodecVideoDecoder_l::supportedExynosH264HighProfileHwCodecPrefix = {
	"OMX.Exynos."};

const std::set<int> MediaCodecVideoDecoder_l::supportedColorList = {
      webrtc::jni::COLOR_FormatYUV420Planar, webrtc::jni::COLOR_FormatYUV420SemiPlanar,
      webrtc::jni::COLOR_QCOM_FormatYUV420SemiPlanar,
      webrtc::jni::COLOR_QCOM_FORMATYVU420PackedSemiPlanar32m4ka, 
	  webrtc::jni::COLOR_QCOM_FORMATYVU420PackedSemiPlanar16m4ka,
      webrtc::jni::COLOR_QCOM_FORMATYVU420PackedSemiPlanar64x32Tile2m8ka,
	  webrtc::jni::COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m};


	// Functions to query if HW decoding is supported.
bool MediaCodecVideoDecoder_l::isVp8HwSupported() 
{
	return !findDecoder(KOS_MIMETYPE_VIDEO_VP8, supportedVp8HwCodecPrefixes).codecName.empty();
}

bool MediaCodecVideoDecoder_l::isVp9HwSupported() 
{
	return !findDecoder(KOS_MIMETYPE_VIDEO_VP9, supportedVp9HwCodecPrefixes).codecName.empty();
}

bool MediaCodecVideoDecoder_l::isH264HwSupported()
{
	return !findDecoder(KOS_MIMETYPE_VIDEO_AVC, supportedH264HwCodecPrefixes).codecName.empty();
}

bool MediaCodecVideoDecoder_l:: isH264HighProfileHwSupported() 
{
	// Support H.264 HP decoding on QCOM chips for Android L and above.
	if (!findDecoder(KOS_MIMETYPE_VIDEO_AVC, supportedQcomH264HighProfileHwCodecPrefix).codecName.empty()) {
		return true;
	}

	// Support H.264 HP decoding on Exynos chips for Android M and above.
	if (!findDecoder(KOS_MIMETYPE_VIDEO_AVC, supportedExynosH264HighProfileHwCodecPrefix).codecName.empty()) {
		return true;
	}
	return false;
}


MediaCodecVideoDecoder_l::DecoderProperties MediaCodecVideoDecoder_l::findDecoder(const std::string& mime, const std::vector<std::string>& supportedCodecPrefixes)
{
	SDL_Log("Trying to find HW decoder for mime %s", mime.c_str());
	for (int i = 0; i < MediaCodecList_getCodecCount(); ++i) {
		kosMediaCodecInfo info;
		MediaCodecList_getCodecInfoAt(i, &info);
		if (info.name[0] == '\0') {
			continue;
		}

		if (info.isEncoder) {
			continue;
		}

		const kosMediaCodecCap* hitted_cap = nullptr;
		for (int at = 0; at < KOS_MAX_CAP_SIZE; at ++) {
			const kosMediaCodecCap& cap = info.caps[at];
			if (strcmp(cap.mime, mime.c_str()) == 0) {
				hitted_cap = &cap;
				break;
			}
		}
		if (hitted_cap == nullptr) {
			continue;
		}
		

		const std::string name = info.name;

		SDL_Log("Found candidate decoder %s, next check colorFormat", name.c_str());

		// Check if this is supported decoder.
		bool supportedCodec = false;
		for (std::vector<std::string>::const_iterator it = supportedCodecPrefixes.begin(); it != supportedCodecPrefixes.end(); ++ it) {
			const std::string& codecPrefix = *it;
			if (name.find(codecPrefix) == 0) {
				supportedCodec = true;
				break;
			}
		}
		if (!supportedCodec) {
			continue;
		}

		// Check if codec supports either yuv420 or nv12.
		for (std::set<int>::const_iterator it = supportedColorList.begin(); it != supportedColorList.end(); ++ it) {
			int codecColorFormat = *it;

			for (int at = 0; at < KOS_MAX_COLORFORMAT; at ++) {
				if (codecColorFormat == hitted_cap->colorFormats[at]) {
					// Found supported HW decoder.
					SDL_Log("Found target decoder %s. Color: 0x%x", name.c_str(), codecColorFormat);
					return DecoderProperties(name, codecColorFormat);
				}
			}
		}
	}
	SDL_Log("No HW decoder found for mime %s", mime.c_str());
	return DecoderProperties("", 0); // No HW decoder.
}

MediaCodecVideoDecoder_l::MediaCodecVideoDecoder_l()
	: mediaCodec(-1)
	, colorFormat(0)
	, width(0)
	, height(0)
	, stride(0)
	, sliceHeight(0)
	, hasDecodedFirstFrame(0)
{}

void MediaCodecVideoDecoder_l::checkOnMediaCodecThread()
{
	if (mediaCodecThread.get()) {
		RTC_CHECK(mediaCodecThread->CalledOnValidThread());
	}
}

  // Pass null in |surfaceTextureHelper| to configure the codec for ByteBuffer output.
bool MediaCodecVideoDecoder_l::initDecode(webrtc::VideoCodecType type, int _width, int _height)
{
	RTC_CHECK(mediaCodecThread.get() == nullptr);

	std::string mime;
	std::vector<std::string> supportedCodecPrefixes;
	if (type == webrtc::kVideoCodecVP8) {
		mime = KOS_MIMETYPE_VIDEO_VP8;
		supportedCodecPrefixes = supportedVp8HwCodecPrefixes;
	} else if (type == webrtc::kVideoCodecVP9) {
		mime = KOS_MIMETYPE_VIDEO_VP9;
		supportedCodecPrefixes = supportedVp9HwCodecPrefixes;
	} else if (type == webrtc::kVideoCodecH264) {
		mime = KOS_MIMETYPE_VIDEO_AVC;
		supportedCodecPrefixes = supportedH264HwCodecPrefixes;
	} else {
		RTC_CHECK(false);
	}
	DecoderProperties properties = findDecoder(mime, supportedCodecPrefixes);
	if (properties.codecName.empty()) {
		return false;
	}

	SDL_Log("Java initDecode: %i, %i x %i. Color: 0x%x", type, width, height, properties.colorFormat);

	mediaCodecThread.reset(new rtc::ThreadChecker);

	width = _width;
	height = _height;
	stride = width;
	sliceHeight = height;

	kosMediaFormat format = MediaFormat_create(mime.c_str(), width, height);
	format.colorFormat = properties.colorFormat;
	// format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);

	SDL_Log("  Format: 0x%x, size: %i x %i", format.colorFormat, format.width, format.height);

	mediaCodec = MediaCodec_setup(properties.codecName.c_str());
	if (mediaCodec < 0) {
		SDL_Log("Can not create media decoder");
		return false;
	}
	MediaCodec_configure(mediaCodec, &format, KOS_MEDIACODEC_CONFIGURE_FLAG_DECODE);
	MediaCodec_start(mediaCodec);

	colorFormat = properties.colorFormat;
	decodeStartTimeMs = std::queue<TimeStamps>();
	hasDecodedFirstFrame = false;
	SDL_Log("Input buffers: %i. Output buffers: %i", MediaCodec_getBufferLength(mediaCodec, true), MediaCodec_getBufferLength(mediaCodec, false));
	return true;
}

  // Resets the decoder so it can start decoding frames with new resolution.
  // Flushes MediaCodec and clears decoder output buffers.
void MediaCodecVideoDecoder_l::reset(int _width, int _height)
{
	RTC_CHECK(mediaCodecThread.get() != nullptr && mediaCodec >= 0);
	SDL_Log("Java reset: %i x %i", _width, _height);

	MediaCodec_flush(mediaCodec);

	width = _width;
	height = _height;
	decodeStartTimeMs = std::queue<TimeStamps>();
	hasDecodedFirstFrame = false;
}

void MediaCodecVideoDecoder_l::release()
{
	SDL_Log("Java releaseDecoder. Total number of dropped frames: x");
	checkOnMediaCodecThread();

	// google's webrtc is call below stop/release in other thread.
	// but I think kOS has fixed this bug, call them in this thread.
	MediaCodec_stop(mediaCodec);
	MediaCodec_release(mediaCodec);

/*
	// Run Mediacodec stop() and release() on separate thread since sometime
	// Mediacodec.stop() may hang.
	final CountDownLatch releaseDone = new CountDownLatch(1);

	Runnable runMediaCodecRelease = new Runnable() {
		@Override
		public void run() {
		try {
			Logging.d(TAG, "Java releaseDecoder on release thread");
			mediaCodec.stop();
			mediaCodec.release();
			Logging.d(TAG, "Java releaseDecoder on release thread done");
		} catch (Exception e) {
			Logging.e(TAG, "Media decoder release failed", e);
		}
		releaseDone.countDown();
		}
	};
	new Thread(runMediaCodecRelease).start();

	if (!ThreadUtils.awaitUninterruptibly(releaseDone, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
		Logging.e(TAG, "Media decoder release timeout");
		codecErrors++;
	}
*/
	mediaCodec = -1;
	mediaCodecThread.reset();
	SDL_Log("Java releaseDecoder done");
}

// Dequeue an input buffer and return its index, -1 if no input buffer is
// available, or -2 if the codec is no longer operative.
int MediaCodecVideoDecoder_l::dequeueInputBuffer()
{
    checkOnMediaCodecThread();
    return MediaCodec_dequeueInputBuffer(mediaCodec, DEQUEUE_INPUT_TIMEOUT);
}

bool MediaCodecVideoDecoder_l::getInputBuffer(int index, kosABuffer& buffer)
{
	checkOnMediaCodecThread();
	return MediaCodec_getBuffer(mediaCodec, true, index, &buffer);
}

bool MediaCodecVideoDecoder_l::queueInputBuffer(int inputBufferIndex, int size, int64_t presentationTimeStamUs,
      int64_t timeStampMs, int64_t ntpTimeStamp) 
{
	checkOnMediaCodecThread();
	decodeStartTimeMs.push(TimeStamps(SDL_GetTicks(), timeStampMs, ntpTimeStamp));
	MediaCodec_queueInputBuffer(mediaCodec, inputBufferIndex, 0, size, presentationTimeStamUs);
	return true;
}

static std::string get_mediaformat_str(const kosMediaFormat& format)
{
	std::stringstream ss;

	ss << "{mime=" << ((const char*)format.mime);
	ss << ", width=" << format.width;
	ss << ", height=" << format.height;
	ss << ", stride=" << format.stride;
	ss << ", sliceHeight=" << format.sliceHeight << "}";
	return ss.str();
}

// Returns null if no decoded buffer is available, and otherwise a DecodedByteBuffer.
// Throws IllegalStateException if call is made on the wrong thread, if color format changes to an
// unsupported format, or if |mediaCodec| is not in the Executing state. Throws CodecException
// upon codec error.
MediaCodecVideoDecoder_l::DecodedOutputBuffer MediaCodecVideoDecoder_l::dequeueOutputBuffer(int dequeueTimeoutMs) 
{
    checkOnMediaCodecThread();

	DecodedOutputBuffer ret;
	if (decodeStartTimeMs.empty()) {
		return ret;
	}
	// Drain the decoder until receiving a decoded buffer or hitting
	// MediaCodec.INFO_TRY_AGAIN_LATER.
	kosBufferInfo info;
	kosMediaFormat format;
	const int64_t nowElapsedRealtime = SDL_GetTicks();
	TimeStamps timeStamps = decodeStartTimeMs.front();
	while (true) {
		int result = MediaCodec_dequeueOutputBuffer(mediaCodec, INT64_C(1000) * dequeueTimeoutMs, &info);
		switch (result) {
		case DEQUEUE_INFO_OUTPUT_BUFFERS_CHANGED:
			SDL_Log("Decoder output buffers changed: %i", MediaCodec_getBufferLength(mediaCodec, false));
			RTC_CHECK(!hasDecodedFirstFrame);
			break;

		case DEQUEUE_INFO_OUTPUT_FORMAT_CHANGED:
			MediaCodec_getOutputFormat(mediaCodec, &format);
			SDL_Log("Decoder format changed: %s", get_mediaformat_str(format).c_str());
			
			if (hasDecodedFirstFrame && (format.width != width || format.height != height)) {
				SDL_Log("Unexpected size change. Configured %i*%i. New %i*%i",
					width, height, format.width, format.height);
				RTC_CHECK(false);
			}
			width = format.width;
			height = format.height;

			if (format.colorFormat != 0) {
				colorFormat = format.colorFormat;
				SDL_Log("Color: 0x%x", format.colorFormat);

				if (supportedColorList.count(colorFormat) == 0) {
					SDL_Log("Non supported color format: 0x%x", format.colorFormat);
					RTC_CHECK(false);
				}
			}
			if (format.stride != 0) {
				stride = format.stride;
			}
			if (format.sliceHeight != 0) {
				sliceHeight = format.sliceHeight;
			}
			stride = SDL_max(width, stride);
			sliceHeight = SDL_max(height, sliceHeight);
			SDL_Log("Frame stride and slice height: %i x %i", stride, sliceHeight);
			break;

		case DEQUEUE_INFO_TRY_AGAIN_LATER:
			return ret;

		default: 
			hasDecodedFirstFrame = true;
			decodeStartTimeMs.pop();
			int64_t decodeTimeMs = nowElapsedRealtime - timeStamps.decodeStartTimeMs;
			if (decodeTimeMs > MAX_DECODE_TIME_MS) {
	/*
				SDL_Log("Very high decode time: " + decodeTimeMs + "ms"
					+ ". Q size: " + decodeStartTimeMs.size()
					+ ". Might be caused by resuming H264 decoding after a pause.");
	*/
				decodeTimeMs = MAX_DECODE_TIME_MS;
			}
			return DecodedOutputBuffer(result, info.offset, info.size,
				info.presentationTimeUs / 1000, timeStamps.timeStampMs,
				timeStamps.ntpTimeStampMs, decodeTimeMs, nowElapsedRealtime);
		}
	}
}

bool MediaCodecVideoDecoder_l::getOutputBuffer(int index, kosABuffer& buffer)
{
	checkOnMediaCodecThread();
	return MediaCodec_getBuffer(mediaCodec, false, index, &buffer);
}

// Release a dequeued output byte buffer back to the codec for re-use. Should only be called for
// non-surface decoding.
// Throws IllegalStateException if the call is made on the wrong thread, if codec is configured
// for surface decoding, or if |mediaCodec| is not in the Executing state. Throws
// MediaCodec.CodecException upon codec error.
void MediaCodecVideoDecoder_l::returnDecodedOutputBuffer(int index)
{
	checkOnMediaCodecThread();
	MediaCodec_releaseOutputBuffer(mediaCodec, index);
}