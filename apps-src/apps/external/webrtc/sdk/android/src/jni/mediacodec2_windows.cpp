#include <kosapi/mediacodec.h>

#include <SDL_log.h>
#include <vector>
#include <set>
#include <string.h>

#include "sdk/android/src/jni/mediacodecvideodecoder_l_kos.h"

typedef enum OMX_COLOR_FORMATTYPE {
    OMX_COLOR_FormatUnused,
    OMX_COLOR_FormatMonochrome,
    OMX_COLOR_Format8bitRGB332,
    OMX_COLOR_Format12bitRGB444,
    OMX_COLOR_Format16bitARGB4444,
    OMX_COLOR_Format16bitARGB1555,
    OMX_COLOR_Format16bitRGB565,
    OMX_COLOR_Format16bitBGR565,
    OMX_COLOR_Format18bitRGB666,
    OMX_COLOR_Format18bitARGB1665,
    OMX_COLOR_Format19bitARGB1666,
    OMX_COLOR_Format24bitRGB888,
    OMX_COLOR_Format24bitBGR888,
    OMX_COLOR_Format24bitARGB1887,
    OMX_COLOR_Format25bitARGB1888,
    OMX_COLOR_Format32bitBGRA8888,
    OMX_COLOR_Format32bitARGB8888,
    OMX_COLOR_FormatYUV411Planar,
    OMX_COLOR_FormatYUV411PackedPlanar,
    OMX_COLOR_FormatYUV420Planar,
    OMX_COLOR_FormatYUV420PackedPlanar,
    OMX_COLOR_FormatYUV420SemiPlanar,
    OMX_COLOR_FormatYUV422Planar,
    OMX_COLOR_FormatYUV422PackedPlanar,
    OMX_COLOR_FormatYUV422SemiPlanar,
    OMX_COLOR_FormatYCbYCr,
    OMX_COLOR_FormatYCrYCb,
    OMX_COLOR_FormatCbYCrY,
    OMX_COLOR_FormatCrYCbY,
    OMX_COLOR_FormatYUV444Interleaved,
    OMX_COLOR_FormatRawBayer8bit,
    OMX_COLOR_FormatRawBayer10bit,
    OMX_COLOR_FormatRawBayer8bitcompressed,
    OMX_COLOR_FormatL2,
    OMX_COLOR_FormatL4,
    OMX_COLOR_FormatL8,
    OMX_COLOR_FormatL16,
    OMX_COLOR_FormatL24,
    OMX_COLOR_FormatL32,
    OMX_COLOR_FormatYUV420PackedSemiPlanar,
    OMX_COLOR_FormatYUV422PackedSemiPlanar,
    OMX_COLOR_Format18BitBGR666,
    OMX_COLOR_Format24BitARGB6666,
    OMX_COLOR_Format24BitABGR6666,
    OMX_COLOR_FormatKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_COLOR_FormatVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    /**<Reserved android opaque colorformat. Tells the encoder that
     * the actual colorformat will be  relayed by the
     * Gralloc Buffers.
     * FIXME: In the process of reserving some enum values for
     * Android-specific OMX IL colorformats. Change this enum to
     * an acceptable range once that is done.
     * */
    OMX_COLOR_FormatAndroidOpaque = 0x7F000789,
    OMX_COLOR_Format32BitRGBA8888 = 0x7F00A000,
    /** Flexible 8-bit YUV format.  Codec should report this format
     *  as being supported if it supports any YUV420 packed planar
     *  or semiplanar formats.  When port is set to use this format,
     *  codec can substitute any YUV420 packed planar or semiplanar
     *  format for it. */
    OMX_COLOR_FormatYUV420Flexible = 0x7F420888,

    OMX_TI_COLOR_FormatYUV420PackedSemiPlanar = 0x7F000100,
    OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00,
    OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03,
    OMX_SEC_COLOR_FormatNV12Tiled = 0x7FC00002,
    OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m = 0x7FA30C04,
    OMX_COLOR_FormatMax = 0x7FFFFFFF
} OMX_COLOR_FORMATTYPE;

class MediaCodecInfo2
{
public:
	MediaCodecInfo2(const std::string& name, bool isencoder, const std::string& mime);

	std::string codecName;
	bool isEncoder;

	struct tcap {
		std::string mime;
		std::set<int> colorFormats;
	};
	std::vector<tcap> caps;
};

MediaCodecInfo2::MediaCodecInfo2(const std::string& name, bool isencoder, const std::string& mime)
	: codecName(name)
	, isEncoder(isencoder)
{
	caps.push_back(tcap());
	tcap& cap = caps.back();

	cap.mime = mime;
	cap.colorFormats.insert(OMX_COLOR_FormatYUV420Flexible);
	cap.colorFormats.insert(OMX_COLOR_FormatYUV420Planar);
	cap.colorFormats.insert(OMX_COLOR_FormatYUV420SemiPlanar);
	if (isEncoder) {
		cap.colorFormats.insert(OMX_COLOR_FormatAndroidOpaque);
	}
}

class MediaCodecList2
{
public:
	static std::vector<MediaCodecInfo2> sRegularCodecInfos;
	static void initCodecList();

	static void getCodecInfoAt(int index, kosMediaCodecInfo* info);
	static bool IsSupportMime(int index, const char* mime);
	static bool IsSupportColorFormat(int index, const char* mime, int colorFormat);
};

std::vector<MediaCodecInfo2> MediaCodecList2::sRegularCodecInfos;

void MediaCodecList2::initCodecList()
{
	if (sRegularCodecInfos.empty()) {

		sRegularCodecInfos.push_back(MediaCodecInfo2("OMX.rk.video_decoder.avc", false, "video/avc"));
		sRegularCodecInfos.push_back(MediaCodecInfo2("OMX.rk.video_decoder.vp8", false, "video/x-vnd.on2.vp8"));
		sRegularCodecInfos.push_back(MediaCodecInfo2("OMX.rk.video_decoder.hevc", false, "video/hevc"));
		sRegularCodecInfos.push_back(MediaCodecInfo2("OMX.rk.video_encoder.avc", true, "video/avc"));
	}
}

void MediaCodecList2::getCodecInfoAt(int index, kosMediaCodecInfo* info)
{
	MediaCodecList2::initCodecList();
	memset(info, 0, sizeof(kosMediaCodecInfo));
	if (index < 0 || index >= (int)sRegularCodecInfos.size()) {
		return;
	}

	const MediaCodecInfo2& info2 = sRegularCodecInfos[index];
	SDL_strlcpy(info->name, info2.codecName.c_str(), sizeof(info->name));
	info->isEncoder = info2.isEncoder;

	int at = 0;
	for (std::vector<MediaCodecInfo2::tcap>::const_iterator it = info2.caps.begin(); it != info2.caps.end(); ++ it, at ++) {
		const MediaCodecInfo2::tcap& cap = *it;
		kosMediaCodecCap& cap2 = info->caps[at];
		SDL_strlcpy(cap2.mime, cap.mime.c_str(), sizeof(cap2.mime));
		int at2 = 0;
		for (std::set<int>::const_iterator it = cap.colorFormats.begin(); it != cap.colorFormats.end(); ++ it, at2 ++) {
			cap2.colorFormats[at2] = *it;
		}
	}
}

bool MediaCodecList2::IsSupportMime(int index, const char* mime)
{
	if (index < 0 || index >= (int)sRegularCodecInfos.size()) {
		return false;
	}
	const MediaCodecInfo2& info2 = sRegularCodecInfos[index];
	for (std::vector<MediaCodecInfo2::tcap>::const_iterator it = info2.caps.begin(); it != info2.caps.end(); ++ it) {
		const std::string& mime2 = it->mime;
		if (strcmp(mime, mime2.c_str()) == 0) {
			return true;
		}
	}
	return false;
}

bool MediaCodecList2::IsSupportColorFormat(int index, const char* mime, int colorFormat)
{
	if (index < 0 || index >= (int)sRegularCodecInfos.size()) {
		return false;
	}
	const MediaCodecInfo2& info2 = sRegularCodecInfos[index];
	const MediaCodecInfo2::tcap* hit = NULL;
	for (std::vector<MediaCodecInfo2::tcap>::const_iterator it = info2.caps.begin(); it != info2.caps.end(); ++ it) {
		const std::string& mime2 = it->mime;
		if (strcmp(mime, mime2.c_str()) == 0) {
			hit = &*it;
		}
	}
	if (!hit) {
		return false;
	}
	return hit->colorFormats.find(colorFormat) != hit->colorFormats.end();
}

int MediaCodecList_getCodecCount()
{
	MediaCodecList2::initCodecList();
	return MediaCodecList2::sRegularCodecInfos.size();
}

void MediaCodecList_getCodecInfoAt(int index, kosMediaCodecInfo* info)
{	
	MediaCodecList2::getCodecInfoAt(index, info);
}


kosMediaFormat MediaFormat_create(const char* mime, int width, int height)
{
	kosMediaFormat format;
	memset(&format, 0, sizeof(kosMediaFormat));
	return format;
}

int MediaCodec_setup(const char* name)
{
	return 0;
}

int MediaCodec_configure(int handle, kosMediaFormat* format, int flags)
{
	return 0;
}

void MediaCodec_start(int handle)
{
}

void MediaCodec_stop(int handle)
{
}

void MediaCodec_flush(int handle)
{
}

void MediaCodec_release(int handle)
{
}

int MediaCodec_getBufferLength(int handle, bool input)
{
	return 0;
}

bool MediaCodec_getBuffer(int handle, bool input, int index, kosABuffer* buffer)
{
	return true;
}

int MediaCodec_dequeueInputBuffer(int handle, int64_t timeoutUs)
{
	return 0;
}

bool MediaCodec_queueInputBuffer(int handle, int index, size_t offset, size_t size, int64_t presentationTimeStamUs)
{
	return true;
}

int MediaCodec_dequeueOutputBuffer(int handle, int64_t timeoutUs, kosBufferInfo* bufferInfo)
{
	return 0;
}

void MediaCodec_releaseOutputBuffer(int handle, int index)
{
	return;
}

bool MediaCodec_getOutputFormat(int handle, kosMediaFormat* format)
{
	return true;
}

void MediaCodec_setParameters(int handle, kosMediaCodecParameters* params)
{
}

//
// test
//
void test_mediacodeclist()
{
	MediaCodecVideoDecoder_l::isVp8HwSupported();
	MediaCodecVideoDecoder_l::isVp9HwSupported();
	MediaCodecVideoDecoder_l::isH264HwSupported();
	MediaCodecVideoDecoder_l::isH264HighProfileHwSupported();
}