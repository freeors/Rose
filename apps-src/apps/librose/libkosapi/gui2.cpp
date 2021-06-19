#define GETTEXT_DOMAIN "rose-lib"

#include "filesystem.hpp"
#include "serialization/string_utils.hpp"
#include "sdl_utils.hpp"
#include "rtc_client.hpp"
#include "rose_config.hpp"
#include "wml_exception.hpp"

#include <SDL.h>
#include <opencv2/imgproc.hpp>

using namespace std::placeholders;
#include <numeric>
#include <iomanip>

#include "libyuv/video_common.h"
#include <kosapi/gui.h>

#ifndef _WIN32
#error "This file is impletement of libkosapi.so on windows!"
#endif

#ifndef WEBRTC_WIN
#error "Compire this file require predefined macro: WEBRTC_WIN"
#endif

// webrtc
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_region.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/logging.h"

#include "modules/desktop_capture/win/screen_capturer_win_directx.h"

extern SDL_Point get_screen_size();

namespace webrtc {

const int kTestSharedMemoryId = 123;

class ScreenCapturerTest;

class tscreen_encoder: public twebrtc_encoder
{
public:
    tscreen_encoder(ScreenCapturerTest& screen, uint32_t bitrate_kbps)
        : twebrtc_encoder("H264", bitrate_kbps)
		, screen_(screen)
    {}

private:
    void app_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image) override;

private:
    ScreenCapturerTest& screen_;
};

class MockDesktopCapturerCallback : public DesktopCapturer::Callback
{
public:
	MockDesktopCapturerCallback(const SDL_Point& screen_size);
	~MockDesktopCapturerCallback() override;

	void set_encoder(tscreen_encoder& encoder)
	{
		encoder_ptr_ = &encoder;
	}

	void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) final;

private:
	const SDL_Point screen_size_;
	tscreen_encoder* encoder_ptr_;
	RTC_DISALLOW_COPY_AND_ASSIGN(MockDesktopCapturerCallback);
};

MockDesktopCapturerCallback::MockDesktopCapturerCallback(const SDL_Point& screen_size)
	: screen_size_(screen_size)
	, encoder_ptr_(nullptr)
{}

MockDesktopCapturerCallback::~MockDesktopCapturerCallback() = default;

void MockDesktopCapturerCallback::OnCaptureResult(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame> frame)
{
	VALIDATE(encoder_ptr_ != nullptr, null_str);
	if (result == DesktopCapturer::Result::SUCCESS) {
		const int width = frame->size().width();
		const int height = frame->size().height();
		VALIDATE(width == screen_size_.x && height == screen_size_.y, null_str);
		encoder_ptr_->encode_frame_fourcc(frame->data(), frame->stride() * height, width, height, libyuv::FOURCC_ARGB);
	} else {
		SDL_Log("%u MockDesktopCapturerCallback::OnCaptureResult, fail, result: %i", SDL_GetTicks(), result);
	}
}

class ScreenCapturerTest
{
public:
	ScreenCapturerTest(const SDL_Point& screen_size, uint8_t* pixel_buf, fdid_gui2_screen_captured did, void* user)
		: tid_(SDL_ThreadID())
		, callback_(screen_size)
		, pixel_buf_(pixel_buf)
		, did_(did)
		, user_(user)
		, pixel_buf_vsize_(0)
	{}

public:
	// Enable allow_directx_capturer in DesktopCaptureOptions, but let
	// DesktopCapturer::CreateScreenCapturer to decide whether a DirectX capturer
	// should be used.
	void MaybeCreateDirectxCapturer() {
		DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
		options.set_allow_directx_capturer(true);
		capturer_ = DesktopCapturer::CreateScreenCapturer(options);
	}

	bool CreateDirectxCapturer(uint32_t bitrate_kbps) {
		VALIDATE_CALLED_ON_THIS_THREAD(tid_);
		if (!ScreenCapturerWinDirectx::IsSupported()) {
			RTC_LOG(LS_WARNING) << "Directx capturer is not supported";
			return false;
		}

		encoder_.reset(new tscreen_encoder(*this, bitrate_kbps));
		callback_.set_encoder(*encoder_.get());

		MaybeCreateDirectxCapturer();
		return true;
	}

	void CreateMagnifierCapturer() {
		DesktopCaptureOptions options(DesktopCaptureOptions::CreateDefault());
		options.set_allow_use_magnification_api(true);
		capturer_ = DesktopCapturer::CreateScreenCapturer(options);
	}

	void did_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image)
	{
		threading::lock lock(encoded_images_mutex_);
		memcpy(pixel_buf_, image->data(), image->size());
		pixel_buf_vsize_ = image->size();
	}

public:
	const SDL_threadID tid_;
	std::unique_ptr<tscreen_encoder> encoder_;
	MockDesktopCapturerCallback callback_;
	std::unique_ptr<DesktopCapturer> capturer_;
	threading::mutex encoded_images_mutex_;

	uint8_t* pixel_buf_;
	fdid_gui2_screen_captured did_;
	void* user_;
	int pixel_buf_vsize_;
};

void tscreen_encoder::app_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image)
{
	screen_.did_encoded_image(image);
}

class FakeSharedMemory : public SharedMemory {
 public:
  FakeSharedMemory(char* buffer, size_t size)
    : SharedMemory(buffer, size, 0, kTestSharedMemoryId),
      buffer_(buffer) {
  }
  virtual ~FakeSharedMemory() {
    delete[] buffer_;
  }
 private:
  char* buffer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemory);
};

class FakeSharedMemoryFactory : public SharedMemoryFactory
{
public:
	FakeSharedMemoryFactory() {}
	~FakeSharedMemoryFactory() override {}

	std::unique_ptr<SharedMemory> CreateSharedMemory(size_t size) override {
	return std::unique_ptr<SharedMemory>(
		new FakeSharedMemory(new char[size], size));
	}

private:
	RTC_DISALLOW_COPY_AND_ASSIGN(FakeSharedMemoryFactory);
};

}

static bool gPause = false;

static bool request_quit_record_screen = false;
static int gui2_record_screen_loop_h264_frame(uint32_t bitrate_kbps, uint8_t* pixel_buf, fdid_gui2_screen_captured did, void* user)
{
	request_quit_record_screen = false;

	const SDL_Point screen_size = get_screen_size();
	webrtc::ScreenCapturerTest capturer2(screen_size, pixel_buf, did, user);
	bool ret = capturer2.CreateDirectxCapturer(bitrate_kbps);
	VALIDATE(ret, null_str);

	capturer2.capturer_->Start(&capturer2.callback_);
	capturer2.capturer_->SetSharedMemoryFactory(
		std::unique_ptr<webrtc::SharedMemoryFactory>(new webrtc::FakeSharedMemoryFactory));

	const uint32_t interval_ms = 50;
	uint32_t last_send_ticks = 0;
	while (!request_quit_record_screen) {
		if (gPause) {
			SDL_Delay(20);
			continue;
		}
		if (capturer2.pixel_buf_vsize_ != 0) {
			threading::lock lock(capturer2.encoded_images_mutex_);
			did(pixel_buf, capturer2.pixel_buf_vsize_, screen_size.x, screen_size.y, KOS_DISPLAY_ORIENTATION_0, user);
			capturer2.pixel_buf_vsize_ = 0;
		}
		const uint32_t now = SDL_GetTicks();
 		if (now - last_send_ticks < interval_ms) {
			// SDL_Log("%u UseDirectxCapturerWithSharedBuffers.encoding_count_(%i) >= 3, delay 10 ms", SDL_GetTicks(), UseDirectxCapturerWithSharedBuffers.encoding_count_);
			SDL_Delay(10); // always 10 ms
			continue;
		}
		capturer2.capturer_->CaptureFrame();
		last_send_ticks = now + (SDL_GetTicks() - now) / 2;
	}
	capturer2.encoder_.reset();
	gPause = false;
	SDL_Log("%u ---kosRecordScreenLoop XXX", SDL_GetTicks());
	return 0;
}

int kosRecordScreenLoop(uint32_t bitrate_kbps, uint32_t max_fps_to_encoder, uint8_t* pixel_buf, fdid_gui2_screen_captured did, void* user)
{
	// bitrate_kbps[1Mbps, 10Mbps]
	VALIDATE(bitrate_kbps >= 1000 && bitrate_kbps <= 10000, null_str);
	return gui2_record_screen_loop_h264_frame(bitrate_kbps, pixel_buf, did, user);
}

void kosStopRecordScreen()
{
	request_quit_record_screen = true;
}

void kosPauseRecordScreen(bool pause)
{
	gPause = pause;
}

bool kosRecordScreenPaused()
{
	return gPause;
}

void kosGetDisplayInfo(KosDisplayInfo* info)
{
	memset(info, 0, sizeof(KosDisplayInfo));

	SDL_Point screen_size = get_screen_size();
	info->w = screen_size.x;
	info->h = screen_size.y;
	info->orientation = KOS_DISPLAY_ORIENTATION_0;
}
