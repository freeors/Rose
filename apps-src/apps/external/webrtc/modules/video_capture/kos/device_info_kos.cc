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
#include <string>

#include "modules/video_capture/kos/device_info_kos.h"
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/logging.h"

using namespace webrtc;
using namespace videocapturemodule;

#define KOS_UNSUPPORTED()                                                        \
  SDL_Log("%s is not supported on the Android platform.", __FUNCTION__); \
  return -1;

std::vector<std::string> DeviceInfokOS::device_names;

#ifdef ANDROID
VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {
	return new DeviceInfokOS();
}
#endif

DeviceInfokOS::DeviceInfokOS()
    : DeviceInfoImpl() {
	this->Init();
}

DeviceInfokOS::~DeviceInfokOS() {}

int32_t DeviceInfokOS::Init() 
{
	device_names.clear();
/*
	int count = kosGetNumberOfCameras();
	for (int n = 0; n < count; n ++) {
		getCameraInfo(n, CameraInfo* info);
	}
*/
    // const char* filesUTF8 = env->GetStringUTFChars(mEnv, files_str, 0);
	const char* filesUTF8 = "1|cam0-test|";
	const int len = SDL_strlen(filesUTF8);

	SDL_Log("DeviceInfoAndroid::Init, filesUTF8: %p, filesUTF8's len: %i", filesUTF8, len);

	char* data = (char*)SDL_malloc(len);
	SDL_memcpy(data, filesUTF8, len);

	int device_count = 0;
	int start = 0;
	int at = 0;
	for (; at < len; at ++) {
		const char ch = data[at];
		if (ch == '|') {
			data[at] = '\0';
			device_count = SDL_atoi(data);
			break;
		}
	}

	start = at + 1;
	for (at = start; at < len; at ++) {
		const char ch = data[at];
		if (ch == '|') {
			data[at] = '\0';
			device_names.push_back(data + start);
			start = at + 1;
		}
	}

	for (int i = 0; i < device_count; i++) {
		// AVCaptureDevice *avDevice = [DeviceInfoIosObjC captureDeviceForIndex:i];
		VideoCaptureCapabilities capabilityVector;

		// for (NSString *preset in camera_presets) {
		for (int ncap = 0; ncap < 1; ncap ++) {
			VideoCaptureCapability capability;

			capability.width = 352;
			capability.height = 288;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);

			capability.width = 640;
			capability.height = 480;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);

			capability.width = 1280;
			capability.height = 720;
			capability.maxFPS = 29;
			capability.videoType = VideoType::kYUY2;
			capability.interlaced = false;
			capabilityVector.push_back(capability);
		}

		char deviceNameUTF8[256];
		char deviceId[256];
		this->GetDeviceName(i, deviceNameUTF8, 256, deviceId, 256);
		std::string deviceIdCopy(deviceId);
		std::pair<std::string, VideoCaptureCapabilities> mapPair = std::pair<std::string, VideoCaptureCapabilities>(deviceIdCopy, capabilityVector);
		_capabilitiesMap.insert(mapPair);
	}

	return 0;
}

uint32_t DeviceInfokOS::NumberOfDevices() 
{
	return device_names.size();
}

int32_t DeviceInfokOS::GetDeviceName(uint32_t deviceNumber,
                                     char* deviceNameUTF8,
                                     uint32_t deviceNameUTF8Length,
                                     char* deviceUniqueIdUTF8,
                                     uint32_t deviceUniqueIdUTF8Length,
                                     char* productUniqueIdUTF8,
                                     uint32_t productUniqueIdUTF8Length) 
{
	strncpy(deviceNameUTF8, device_names[deviceNumber].c_str(), deviceNameUTF8Length);
	deviceNameUTF8[deviceNameUTF8Length - 1] = '\0';

	strncpy(deviceUniqueIdUTF8,
			device_names[deviceNumber].c_str(),
			deviceUniqueIdUTF8Length);
	deviceUniqueIdUTF8[deviceUniqueIdUTF8Length - 1] = '\0';

	return 0;
}

int32_t DeviceInfokOS::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
	int32_t numberOfCapabilities = 0;
	std::string deviceUniqueId(deviceUniqueIdUTF8);
	std::map<std::string, VideoCaptureCapabilities>::iterator it =
		_capabilitiesMap.find(deviceUniqueId);

	if (it != _capabilitiesMap.end()) {
		numberOfCapabilities = it->second.size();
	}
	return numberOfCapabilities;
}

int32_t DeviceInfokOS::GetCapability(const char* deviceUniqueIdUTF8,
                                     const uint32_t deviceCapabilityNumber,
                                     VideoCaptureCapability& capability) {
	std::string deviceUniqueId(deviceUniqueIdUTF8);
	std::map<std::string, VideoCaptureCapabilities>::iterator it =
		_capabilitiesMap.find(deviceUniqueId);

	if (it != _capabilitiesMap.end()) {
		VideoCaptureCapabilities deviceCapabilities = it->second;

		if (deviceCapabilityNumber < deviceCapabilities.size()) {
			VideoCaptureCapability cap;
			cap = deviceCapabilities[deviceCapabilityNumber];
			capability = cap;
			return 0;
		}
	}

	return -1;
}

int32_t DeviceInfokOS::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8,
    void* parentWindow,
    uint32_t positionX,
    uint32_t positionY) {
	KOS_UNSUPPORTED();
}

int32_t DeviceInfokOS::GetOrientation(const char* deviceUniqueIdUTF8,
                                      VideoRotation& orientation) {
	if (strcmp(deviceUniqueIdUTF8, "Front Camera") == 0) {
		orientation = kVideoRotation_0;
	} else {
		// orientation = kVideoRotation_90;
		orientation = kVideoRotation_0;
	}
	return orientation;
}

int32_t DeviceInfokOS::CreateCapabilityMap(const char* deviceUniqueIdUTF8) 
{
	std::string deviceName(deviceUniqueIdUTF8);
	std::map<std::string, std::vector<VideoCaptureCapability>>::iterator it =
		_capabilitiesMap.find(deviceName);
	VideoCaptureCapabilities deviceCapabilities;
	if (it != _capabilitiesMap.end()) {
		_captureCapabilities = it->second;
		return 0;
	}

	return -1;
}
