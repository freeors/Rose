/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "mediaCodecvideoencoder_l_kos.h"

#include "sdk/android/src/jni/androidmediacodeccommon.h"
#include <SDL_log.h>
#include <SDL_timer.h>

const double MediaCodecVideoEncoder_l::BITRATE_CORRECTION_SEC = 3.0;
const double MediaCodecVideoEncoder_l::BITRATE_CORRECTION_MAX_SCALE = 4;
int MediaCodecVideoEncoder_l::codecErrors = 0;

static MediaCodecProperties qcomVp8HwProperties("OMX.qcom.", NO_ADJUSTMENT);

// List of supported HW VP8 encoders.
static std::vector<MediaCodecProperties> vp8HwList = {
	MediaCodecProperties("OMX.qcom.", NO_ADJUSTMENT),
	MediaCodecProperties("OMX.Exynos.", DYNAMIC_ADJUSTMENT),
	MediaCodecProperties("OMX.Intel.", NO_ADJUSTMENT)};
	
// List of supported HW VP9 encoders.
static std::vector<MediaCodecProperties> vp9HwList = {
	MediaCodecProperties("OMX.qcom.", NO_ADJUSTMENT), 
	MediaCodecProperties("OMX.Exynos.", FRAMERATE_ADJUSTMENT)};

// List of supported HW H.264 encoders.
static std::vector<MediaCodecProperties> h264HwList = {
	MediaCodecProperties("OMX.qcom.", NO_ADJUSTMENT), 
	MediaCodecProperties("OMX.Exynos.", FRAMERATE_ADJUSTMENT), 
	MediaCodecProperties("OMX.rk.", FRAMERATE_ADJUSTMENT)};

// List of supported HW H.264 high profile encoders.
static std::vector<MediaCodecProperties> h264HighProfileHwList = {
	MediaCodecProperties("OMX.Exynos.", FRAMERATE_ADJUSTMENT)};

const std::set<std::string> MediaCodecVideoEncoder_l::H264_HW_EXCEPTION_MODELS = {"SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4"};

const std::set<int> MediaCodecVideoEncoder_l::supportedColorList = {
	webrtc::jni::COLOR_FormatYUV420Planar,
	webrtc::jni::COLOR_FormatYUV420SemiPlanar,
	webrtc::jni::COLOR_QCOM_FormatYUV420SemiPlanar,
	COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m};

// Functions to query if HW encoding is supported.
bool MediaCodecVideoEncoder_l::isVp8HwSupported()
{
	return !findHwEncoder(KOS_MIMETYPE_VIDEO_VP8, vp8HwList, supportedColorList).codecName.empty();
}

bool MediaCodecVideoEncoder_l::isVp9HwSupported()
{
	return !findHwEncoder(KOS_MIMETYPE_VIDEO_VP9, vp9HwList, supportedColorList).codecName.empty();
}

bool MediaCodecVideoEncoder_l::isH264HwSupported()
{
	return !findHwEncoder(KOS_MIMETYPE_VIDEO_AVC, h264HwList, supportedColorList).codecName.empty();
}

bool MediaCodecVideoEncoder_l::isH264HighProfileHwSupported()
{
	return !findHwEncoder(KOS_MIMETYPE_VIDEO_AVC, h264HighProfileHwList, supportedColorList).codecName.empty();
}

MediaCodecVideoEncoder_l::EncoderProperties MediaCodecVideoEncoder_l::findHwEncoder(
	const std::string& mime, const std::vector<MediaCodecProperties>& supportedHwCodecProperties, const std::set<int>& colorList)
{
	// MediaCodec.setParameters is missing for JB and below, so bitrate
	// can not be adjusted dynamically.
/*	
	// Check if device is in H.264 exception list.
	if (strcmp(mime.c_str(), KOS_MIMETYPE_VIDEO_AVC) == 0) {
		if (H264_HW_EXCEPTION_MODELS.count(Build.MODEL) != 0) {
			SDL_Log("Model: %s has black listed H.264 encoder.", Build.MODEL);
			return EncoderProperties("", 0, NO_ADJUSTMENT); // No HW decoder.
		}
	}
*/
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

		SDL_Log("Found candidate encoder %s, next check colorFormat", name.c_str());

		// Check if this is supported decoder.
		bool supportedCodec = false;
		BitrateAdjustmentType bitrateAdjustmentType = NO_ADJUSTMENT;
		for (std::vector<MediaCodecProperties>::const_iterator it = supportedHwCodecProperties.begin(); it != supportedHwCodecProperties.end(); ++ it) {
			const MediaCodecProperties& codecProperties = *it;
			const std::string& codecPrefix = codecProperties.codecPrefix;
			if (name.find(codecPrefix) == 0) {
				if (codecProperties.bitrateAdjustmentType != NO_ADJUSTMENT) {
					bitrateAdjustmentType = codecProperties.bitrateAdjustmentType;
					SDL_Log("Codec %s requires bitrate adjustment: %i", name.c_str(), bitrateAdjustmentType);
				}
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
					SDL_Log("Found target encoder %s. Color: 0x%x", name.c_str(), codecColorFormat);
					return EncoderProperties(name, codecColorFormat, bitrateAdjustmentType);
				}
			}
		}
	}

	SDL_Log("No HW encoder found for mime %s", mime.c_str());
	return EncoderProperties("", 0, NO_ADJUSTMENT); // No HW decoder.
}

MediaCodecVideoEncoder_l::MediaCodecVideoEncoder_l()
	: bitrateAdjustmentType(NO_ADJUSTMENT)
	, configDataSize(0)
{}

void MediaCodecVideoEncoder_l::checkOnMediaCodecThread()
{
	if (mediaCodecThread.get()) {
		RTC_CHECK(mediaCodecThread->CalledOnValidThread());
	}
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

bool MediaCodecVideoEncoder_l::initEncode(webrtc::VideoCodecType _type, int _profile, int _width, int _height, int kbps, int fps) {
	SDL_Log("Java initEncode: %i. Profile: %i. %i x %i. @ %i kbps. Fps: %i.", type, _profile, _width, _height, kbps, fps);

	type = _type;
	profile = _profile;
	width = _width;
	height = _height;
	RTC_CHECK(mediaCodecThread.get() == nullptr);

	EncoderProperties properties("", 0, NO_ADJUSTMENT); 
	std::string mime;
	int keyFrameIntervalSec = 0;
	bool configureH264HighProfile = false;
	if (type == webrtc::kVideoCodecVP8) {
		mime = KOS_MIMETYPE_VIDEO_VP8;
		properties = findHwEncoder(
			KOS_MIMETYPE_VIDEO_VP8, vp8HwList, supportedColorList);
		keyFrameIntervalSec = 100;
	} else if (type == webrtc::kVideoCodecVP9) {
		mime = KOS_MIMETYPE_VIDEO_VP9;
		properties = findHwEncoder(
			KOS_MIMETYPE_VIDEO_VP9, vp9HwList, supportedColorList);
		keyFrameIntervalSec = 100;
	} else if (type == webrtc::kVideoCodecH264) {
		mime = KOS_MIMETYPE_VIDEO_AVC;
		properties = findHwEncoder(KOS_MIMETYPE_VIDEO_AVC, h264HwList, supportedColorList);
		if (profile == CONSTRAINED_HIGH) {
			EncoderProperties h264HighProfileProperties = findHwEncoder(KOS_MIMETYPE_VIDEO_AVC,
				h264HighProfileHwList, supportedColorList);
			if (!h264HighProfileProperties.codecName.empty()) {
				SDL_Log("High profile H.264 encoder supported.");
				configureH264HighProfile = true;
			} else {
				SDL_Log("High profile H.264 encoder requested, but not supported. Use baseline.");
			}
		}
		keyFrameIntervalSec = 20;
	}
	if (properties.codecName.empty()) {
		SDL_Log("Can not find HW encoder for %i", type);
		return false;
	}
	colorFormat = properties.colorFormat;
	bitrateAdjustmentType = properties.bitrateAdjustmentType;
	if (bitrateAdjustmentType == FRAMERATE_ADJUSTMENT) {
		fps = BITRATE_ADJUSTMENT_FPS;
	} else {
		fps = SDL_min(fps, MAXIMUM_INITIAL_FPS);
	}

	forcedKeyFrameMs = 0;
	lastKeyFrameMs = -1;
	if (type == webrtc::kVideoCodecVP8 && properties.codecName.find(qcomVp8HwProperties.codecPrefix) == 0) {
		forcedKeyFrameMs = QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS;
	}

	SDL_Log("Color format: 0x%x. Bitrate adjustment: %i. Key frame interval: %lld. Initial fps: %i",
		colorFormat, bitrateAdjustmentType, forcedKeyFrameMs, fps);
	targetBitrateBps = 1000 * kbps;
	targetFps = fps;
	bitrateAccumulatorMax = targetBitrateBps / 8.0;
	bitrateAccumulator = 0;
	bitrateObservationTimeMs = 0;
	bitrateAdjustmentScaleExp = 0;

	mediaCodecThread.reset(new rtc::ThreadChecker);

	kosMediaFormat format = MediaFormat_create(mime.c_str(), width, height);
	format.bitrate = targetBitrateBps;
	format.bitrateMode = VIDEO_ControlRateConstant;
	format.colorFormat = properties.colorFormat;
	format.frameRate = targetFps;
	format.iFrameIntervalSec = keyFrameIntervalSec;
	if (configureH264HighProfile) {
		format.profile = VIDEO_AVCProfileHigh;
		format.level = VIDEO_AVCLevel3;
	}
	SDL_Log("  Format: %s", get_mediaformat_str(format).c_str());

	mediaCodec = MediaCodec_setup(properties.codecName.c_str());
	if (mediaCodec < 0) {
		SDL_Log("Can not create media decoder");
		release();
		return false;
	}
	MediaCodec_configure(mediaCodec, &format, KOS_MEDIACODEC_CONFIGURE_FLAG_ENCODE);
	MediaCodec_start(mediaCodec);

	SDL_Log("Input buffers: %i. Output buffers: %i", MediaCodec_getBufferLength(mediaCodec, true), MediaCodec_getBufferLength(mediaCodec, false));

	return true;
}

void MediaCodecVideoEncoder_l::checkKeyFrameRequired(bool requestedKeyFrame, int64_t presentationTimestampUs)
{
	int64_t presentationTimestampMs = (presentationTimestampUs + 500) / 1000;
	if (lastKeyFrameMs < 0) {
		lastKeyFrameMs = presentationTimestampMs;
	}
	bool forcedKeyFrame = false;
	if (!requestedKeyFrame && forcedKeyFrameMs > 0
		&& presentationTimestampMs > lastKeyFrameMs + forcedKeyFrameMs) {
		forcedKeyFrame = true;
	}
	if (requestedKeyFrame || forcedKeyFrame) {
		// Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
		// indicate this in queueInputBuffer() below and guarantee _this_ frame
		// be encoded as a key frame, but sadly that flag is ignored.  Instead,
		// we request a key frame "soon".
		if (requestedKeyFrame) {
			SDL_Log("Sync frame request");
		} else {
			SDL_Log("Sync frame forced");
		}
		kosMediaCodecParameters params = MediaCodec_createParameters(KOS_PARAMETER_FLAG_REQUEST_SYNC_FRAME);
		params.request_sync_frame = 0;
		MediaCodec_setParameters(mediaCodec, &params);
		lastKeyFrameMs = presentationTimestampMs;
	}
}

bool MediaCodecVideoEncoder_l::encodeBuffer(bool isKeyframe, int inputBuffer, int size, int64_t presentationTimestampUs) 
{
	checkOnMediaCodecThread();

	checkKeyFrameRequired(isKeyframe, presentationTimestampUs);
	MediaCodec_queueInputBuffer(mediaCodec, inputBuffer, 0, size, presentationTimestampUs);
	return true;
}

void MediaCodecVideoEncoder_l::release()
{
	SDL_Log("Java releaseEncoder");
	checkOnMediaCodecThread();

	bool stopHung = false;

	if (mediaCodec >= 0) {
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
			Logging.d(TAG, "Java releaseEncoder on release thread");
			try {
			mediaCodec.stop();
			} catch (Exception e) {
			Logging.e(TAG, "Media encoder stop failed", e);
			}
			try {
			mediaCodec.release();
			} catch (Exception e) {
			Logging.e(TAG, "Media encoder release failed", e);
			caughtException.e = e;
			}
			Logging.d(TAG, "Java releaseEncoder on release thread done");

			releaseDone.countDown();
		}
		};
		new Thread(runMediaCodecRelease).start();

		if (!ThreadUtils.awaitUninterruptibly(releaseDone, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
			Logging.e(TAG, "Media encoder release timeout");
			stopHung = true;
		}
*/
		mediaCodec = -1;
	}

	mediaCodecThread.reset();

	if (stopHung) {
		codecErrors++;
	}

	SDL_Log("Java releaseEncoder done");
}

bool MediaCodecVideoEncoder_l::setRates(int kbps, int frameRate)
{
	checkOnMediaCodecThread();

	int codecBitrateBps = 1000 * kbps;
	if (bitrateAdjustmentType == DYNAMIC_ADJUSTMENT) {
		bitrateAccumulatorMax = codecBitrateBps / 8.0;
		if (targetBitrateBps > 0 && codecBitrateBps < targetBitrateBps) {
		// Rescale the accumulator level if the accumulator max decreases
		bitrateAccumulator = bitrateAccumulator * codecBitrateBps / targetBitrateBps;
		}
	}
	targetBitrateBps = codecBitrateBps;
	targetFps = frameRate;

	// Adjust actual encoder bitrate based on bitrate adjustment type.
	if (bitrateAdjustmentType == FRAMERATE_ADJUSTMENT && targetFps > 0) {
		codecBitrateBps = BITRATE_ADJUSTMENT_FPS * targetBitrateBps / targetFps;
		SDL_Log("setRates: %i -> %i kbps. Fps: %i", kbps, (codecBitrateBps / 1000), targetFps);
	} else if (bitrateAdjustmentType == DYNAMIC_ADJUSTMENT) {
		SDL_Log("setRates: %i -> %i kbps. Fps: %i. ExpScale: %i", 
			kbps, (codecBitrateBps / 1000), targetFps, bitrateAdjustmentScaleExp);
		if (bitrateAdjustmentScaleExp != 0) {
			codecBitrateBps = (int) (codecBitrateBps * getBitrateScale(bitrateAdjustmentScaleExp));
		}
	} else {
		SDL_Log("setRates: %i kbps. Fps: %i", kbps, targetFps);
	}

	kosMediaCodecParameters params = MediaCodec_createParameters(KOS_PARAMETER_FLAG_VIDEO_BITRATE);
	params.video_bitrate = codecBitrateBps;
	MediaCodec_setParameters(mediaCodec, &params);
	return true;
}

// Dequeue an input buffer and return its index, -1 if no input buffer is
// available, or -2 if the codec is no longer operative.
int MediaCodecVideoEncoder_l::dequeueInputBuffer() 
{
	checkOnMediaCodecThread();
	return MediaCodec_dequeueInputBuffer(mediaCodec, DEQUEUE_TIMEOUT);
}

bool MediaCodecVideoEncoder_l::getInputBuffer(int index, kosABuffer& buffer)
{
	checkOnMediaCodecThread();
	return MediaCodec_getBuffer(mediaCodec, true, index, &buffer);
}

int MediaCodecVideoEncoder_l::getInputBufferLength() const
{
	return MediaCodec_getBufferLength(mediaCodec, true);
}

// Dequeue and return an output buffer, or null if no output is ready.  Return
// a fake OutputBufferInfo with index -1 if the codec is no longer operable.
MediaCodecVideoEncoder_l::OutputBufferInfo MediaCodecVideoEncoder_l::dequeueOutputBuffer()
{
	checkOnMediaCodecThread();

	kosBufferInfo info;
	kosABuffer abuff;
	int result = MediaCodec_dequeueOutputBuffer(mediaCodec, DEQUEUE_TIMEOUT, &info);
	// Check if this is config frame and save configuration data.
	if (result >= 0) {
		bool isConfigFrame = (info.flags & MediaCodec_BUFFER_FLAG_CODEC_CONFIG) != 0;
		if (isConfigFrame) {
			SDL_Log("Config frame generated. Offset: %i. Size: %i", info.offset, info.size);
			RTC_CHECK(info.size > 0);
			configData.reset(new uint8_t[info.size]);

			MediaCodec_getBuffer(mediaCodec, false, result, &abuff);
			memcpy(configData.get(), abuff.mData + info.offset, info.size);
			configDataSize = info.size;
/*
			// Log few SPS header bytes to check profile and level.
			std::stringstream spsData;
			for (int i = 0; i < (info.size < 8 ? info.size : 8); i++) {
				spsData << configData.get()[i] & 0xff) + " ";
			}
			SDL_Log("%s", spsData.str().c_str());
*/
			// Release buffer back.
			MediaCodec_releaseOutputBuffer(mediaCodec, result);
			// Query next output.
			result = MediaCodec_dequeueOutputBuffer(mediaCodec, DEQUEUE_TIMEOUT, &info);
		}
	}
	if (result >= 0) {
		// MediaCodec doesn't care about Buffer position/remaining/etc so we can
		// mess with them to get a slice and avoid having to pass extra
		// (BufferInfo-related) parameters back to C++.
		MediaCodec_getBuffer(mediaCodec, false, result, &abuff);

		uint8_t* data = abuff.mData + info.offset;
		reportEncodedFrame(info.size);

		// Check key frame flag.
		bool isKeyFrame = (info.flags & MediaCodec_BUFFER_FLAG_KEY_FRAME) != 0;
		if (isKeyFrame) {
			SDL_Log("Sync frame generated");
		}
		if (isKeyFrame && type == webrtc::kVideoCodecH264) {
			SDL_Log("Appending config frame of size %i to output buffer with offset %i, size %i", 
				configDataSize, info.offset, info.size);
			// For H.264 key frame append SPS and PPS NALs at the start
			std::unique_ptr<uint8_t[]> keyFrameBuffer(new uint8_t[configDataSize + info.size]);
			memcpy(keyFrameBuffer.get(), configData.get(), configDataSize);
			memcpy(keyFrameBuffer.get() + configDataSize, data, info.size);
			return OutputBufferInfo(result, keyFrameBuffer.get(), configDataSize + info.size, isKeyFrame, info.presentationTimeUs);
		} else {
			return OutputBufferInfo(result, data, info.size, isKeyFrame, info.presentationTimeUs);
		}
	} else if (result == DEQUEUE_INFO_OUTPUT_BUFFERS_CHANGED) {
		SDL_Log("Decoder output buffers changed: %i", MediaCodec_getBufferLength(mediaCodec, false));
		return dequeueOutputBuffer();

	} else if (result == DEQUEUE_INFO_OUTPUT_FORMAT_CHANGED) {
		kosMediaFormat format;
		MediaCodec_getOutputFormat(mediaCodec, &format);
		SDL_Log("Decoder format changed: %s", get_mediaformat_str(format).c_str());
		return dequeueOutputBuffer();

	} else {
		// result == DEQUEUE_INFO_TRY_AGAIN_LATER
		return OutputBufferInfo(result, nullptr, 0, false, 0);;
	}
}

bool MediaCodecVideoEncoder_l::getOutputBuffer(int index, kosABuffer& buffer)
{
	checkOnMediaCodecThread();
	return MediaCodec_getBuffer(mediaCodec, false, index, &buffer);
}

double MediaCodecVideoEncoder_l::getBitrateScale(int bitrateAdjustmentScaleExp)
{
	return SDL_pow(BITRATE_CORRECTION_MAX_SCALE,
		(double) bitrateAdjustmentScaleExp / BITRATE_CORRECTION_STEPS);
}

void MediaCodecVideoEncoder_l::reportEncodedFrame(int size) 
{
	if (targetFps == 0 || bitrateAdjustmentType != DYNAMIC_ADJUSTMENT) {
		return;
	}

	// Accumulate the difference between actial and expected frame sizes.
	double expectedBytesPerFrame = targetBitrateBps / (8.0 * targetFps);
	bitrateAccumulator += (size - expectedBytesPerFrame);
	bitrateObservationTimeMs += 1000.0 / targetFps;

	// Put a cap on the accumulator, i.e., don't let it grow beyond some level to avoid
	// using too old data for bitrate adjustment.
	double bitrateAccumulatorCap = BITRATE_CORRECTION_SEC * bitrateAccumulatorMax;
	bitrateAccumulator = SDL_min(bitrateAccumulator, bitrateAccumulatorCap);
	bitrateAccumulator = SDL_max(bitrateAccumulator, -bitrateAccumulatorCap);

	// Do bitrate adjustment every 3 seconds if actual encoder bitrate deviates too much
	// form the target value.
	if (bitrateObservationTimeMs > 1000 * BITRATE_CORRECTION_SEC) {
		SDL_Log("Acc: %i. Max: %i. ExpScale: %i",
			(int)bitrateAccumulator, (int) bitrateAccumulatorMax, bitrateAdjustmentScaleExp);
		bool bitrateAdjustmentScaleChanged = false;
		if (bitrateAccumulator > bitrateAccumulatorMax) {
			// Encoder generates too high bitrate - need to reduce the scale.
			int bitrateAdjustmentInc = (int) (bitrateAccumulator / bitrateAccumulatorMax + 0.5);
			bitrateAdjustmentScaleExp -= bitrateAdjustmentInc;
			bitrateAccumulator = bitrateAccumulatorMax;
			bitrateAdjustmentScaleChanged = true;
		} else if (bitrateAccumulator < -bitrateAccumulatorMax) {
			// Encoder generates too low bitrate - need to increase the scale.
			int bitrateAdjustmentInc = (int) (-bitrateAccumulator / bitrateAccumulatorMax + 0.5);
			bitrateAdjustmentScaleExp += bitrateAdjustmentInc;
			bitrateAccumulator = -bitrateAccumulatorMax;
			bitrateAdjustmentScaleChanged = true;
		}
		if (bitrateAdjustmentScaleChanged) {
			bitrateAdjustmentScaleExp = SDL_min(bitrateAdjustmentScaleExp, BITRATE_CORRECTION_STEPS);
			bitrateAdjustmentScaleExp = SDL_max(bitrateAdjustmentScaleExp, -BITRATE_CORRECTION_STEPS);
			SDL_Log("Adjusting bitrate scale to %i. Value: %.2f", bitrateAdjustmentScaleExp, getBitrateScale(bitrateAdjustmentScaleExp));
			setRates(targetBitrateBps / 1000, targetFps);
		}
		bitrateObservationTimeMs = 0;
	}
}

// Release a dequeued output buffer back to the codec for re-use.  Return
// false if the codec is no longer operable.
bool MediaCodecVideoEncoder_l::releaseOutputBuffer(int index) 
{
	checkOnMediaCodecThread();

	MediaCodec_releaseOutputBuffer(mediaCodec, index);
	return true;
}