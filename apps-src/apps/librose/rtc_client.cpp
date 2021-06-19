/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "rtc_client.hpp"

#include "gettext.hpp"
#include "video.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"
#include "base_instance.hpp"
#include "live555.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"

#include "api/video/i420_buffer.h"
#include "api/test/fakeconstraints.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "modules/video_capture/video_capture_factory.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert.h"
#include "rtc_base/stringutils.h"

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"

#if defined(__APPLE__) && TARGET_OS_IPHONE
// iOS use nullptr for both video decoder_factory and video encoder_factory.
#include "modules/video_coding/codecs/test/objc_codec_h264_test.h"
#elif defined(ANDROID)
#ifdef _KOS
#include "sdk/android/src/jni/androidmediadecoder_kos.h"
#include "sdk/android/src/jni/androidmediaencoder_kos.h"
#else
#include "sdk/android/src/jni/androidmediadecoder_jni.h"
#include "sdk/android/src/jni/androidmediaencoder_jni.h"
#endif
#endif

#include <base/rose/task_environment.h>
#include <base/threading/thread_task_runner_handle.h>

// Names used for a IceCandidate JSON object.
static const char kCandidateSdpMidName[] = "sdpMid";
static const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
static const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
static const char kSessionDescriptionTypeName[] = "type";
static const char kSessionDescriptionSdpName[] = "sdpOffer";

std::vector<trtc_client::tusing_vidcap> trtc_client::tadapter::app_video_capturer(int id, bool remote, const std::vector<std::string>& device_names)
{
	std::vector<tusing_vidcap> ret;
	for (std::vector<std::string>::const_iterator it = device_names.begin(); it != device_names.end(); ++ it) {
		const std::string& name = *it;
		ret.push_back(tusing_vidcap(name, false));
		break;
	}
	return ret;
}

void trtc_client::tadapter::app_handle_notify(int id, int ncode, int at, int64_t i64_value, const std::string& str_value)
{
	std::string msg;
	utils::string_map symbols;
	if (at != nposm) {
		symbols["number"] = str_cast(at + 1);
	}

	if (ncode == ncode_startlive555finished) {
		if (at != nposm && i64_value == 0) {
			symbols["url"] = str_value;
			msg = vgettext2("Camera#$number start fail. Try to play $url with VLC to see if can succeed.", symbols);
		}
	}
	if (!msg.empty()) {
		gui2::show_error_message(msg);
	}
}

trtc_client::VideoRenderer* trtc_client::tadapter::app_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track_to_render, const std::string& name, bool remote, int at, bool encode)
{
	return new VideoRenderer(client, track_to_render, name, remote, at, encode);
}

void trtc_client::tadapter::did_draw_slice(int id, SDL_Renderer* renderer, VideoRenderer** locals, int locals_count, VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	texture tex = locals_count > 0? locals[0]->tex_: remotes[0]->tex_;
	SDL_RenderCopy(renderer, tex.get(), nullptr, &draw_rect);
}

static trtc_client::tadapter adapter;

const uint32_t trtc_client::start_live555_threshold = 30000;

std::string trtc_client::ice_connection_state_str(webrtc::PeerConnectionInterface::IceConnectionState state)
{
	switch (state) {
	case webrtc::PeerConnectionInterface::kIceConnectionNew:
		return "NEW";
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
		return "CHECKING";
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
		return "CONNECTED";
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
		return "COMPLETED";
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
		return "FAILED";
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
		return "DISCONNECTED";
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
		return "CLOSED";
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
		return "COUNT";
	}
	return "UNKNOWN";
}

std::string trtc_client::ice_gathering_state_str(webrtc::PeerConnectionInterface::IceGatheringState state)
{
	switch (state) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
		return "NEW";
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
		return "GATHERING";
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
		return "COMPLETE";
	}
	return "UNKNOWN";
}

#define DTLS_ON  true
#define DTLS_OFF false

trtc_client::trtc_client(int id, gui2::tdialog& dialog, tadapter& adapter, const tpoint& desire_size, bool local_only, const std::vector<trtsp_settings>& rtsps, int desire_fps, bool rtsp_exclude_local, bool idle_screen_saver)
	: gui2::tdialog::ttexture_holder(dialog)
	, id_(id)
	, dialog_(dialog)
	, desire_size_(desire_size)
	, desire_fps_(desire_fps)
	, local_only_(local_only)
	, rtsps_(rtsps)
	, rtsp_exclude_local_(rtsp_exclude_local)
	, idle_screen_saver_(idle_screen_saver)
	, adapter_(&adapter)
	, message_handler_(*this)
	, local_vrenderer_(nullptr)
	, local_vrenderer_count_(0)
	, remote_vrenderer_(nullptr)
	, remote_vrenderer_count_(0)
	, caller_(true)
	, resolver_(nullptr)
	, state_(NOT_CONNECTED)
	, my_id_(-1)
	, last_msg_should_size_(-1)
	, recv_data_(nullptr)
	, recv_data_size_(0)
	, recv_data_vsize_(0)
	, send_data_(nullptr)
	, send_data_size_(0)
	, send_data_vsize_(0)
	, preferred_codec_(cricket::kH264CodecName) // cricket::kVp9CodecName
	, ref_count_(1)
{
	VALIDATE(desire_size_.x == nposm || desire_size_.x >= 640, null_str);
	VALIDATE(desire_size_.y == nposm || desire_size_.y >= 480, null_str);

	if (desire_size_.x != nposm && desire_size_.y != nposm) {
		// desire_size must be landscape size.
		VALIDATE(desire_size.x >= desire_size.y, null_str);
	}

	VALIDATE(instance->rtc_client() == nullptr, null_str);
	instance->set_rtc_client(this);
}

trtc_client::~trtc_client()
{
	SDL_Log("trtc_client::~trtc_client(), E, state: %i, peer_connection_: %p", state_, peer_connection_.get());

	// prevent frame thread from Send() late.
	// Although DeletePeerConnection will also perform clear_msg(), but it cannot cover all conditions,
	// perform it here first.
	// message_handler_.send_helper.clear_msg();

	if (state_ == CONNECTED) {
		if (peer_connection_.get()) {
			SDL_Log("trtc_client::~trtc_client(), will DeletePeerConnection()");
			DeletePeerConnection();
			if (!rtsp() && !local_only_) {
				// {"id":"stop"}
				Json::StyledWriter writer;
				Json::Value jmessage;

				jmessage["id"] = "stop";
				msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
			}
		}
		Close();
	}

	VALIDATE(local_vrenderer_ == nullptr && local_vrenderer_count_ == 0 && remote_vrenderer_ == nullptr && remote_vrenderer_count_ == 0, null_str);

	SDL_Log("trtc_client::~trtc_client(), will free recv_data_");

	if (recv_data_) {
		free(recv_data_);
		recv_data_ = nullptr;
	}
	if (send_data_) {
		free(send_data_);
		send_data_ = nullptr;
	}

	VALIDATE(instance->rtc_client(), null_str);
	instance->set_rtc_client(nullptr);
	SDL_Log("trtc_client::~trtc_client(), X");
}

trtc_client::VideoRenderer::VideoRenderer(trtc_client& client, webrtc::VideoTrackInterface* track_to_render, const std::string& name, bool remote, int at, bool encode)
	: client_(client)
	, rendered_track_(track_to_render)
	, name(name)
	, remote_(remote)
	, at_(at)
	, requrie_encode(encode)
	, bytesperpixel_(4) // ARGB
	// , frame_thread_new_frame_(false)
	, frame_thread_frames_(0)
	// , frames(0)
	// , new_frame(false)
	, last_frame_ticks(SDL_GetTicks())
	, hflip_(false)
	, sample_interval_(1)
	, startup_verbose_ms_(0)
	, last_verbose_ms_(0)
	, last_verbose_frames_(0)
{
	clear_texture();

	rtc::VideoSinkWants wants;
	wants.rotation_applied = false;
	if (rendered_track_.get() != nullptr) {
		rendered_track_->AddOrUpdateSink(this, wants);
	}
}

trtc_client::VideoRenderer::~VideoRenderer()
{
	if (rendered_track_.get() != nullptr) {
		rendered_track_->RemoveSink(this);
		rendered_track_ = nullptr;
	}
	encoder_.reset();
}

void trtc_client::VideoRenderer::clear_texture()
{
	app_width_ = nposm;
	app_height_ = nposm;

	tex_ = nullptr;
	pixels_ = nullptr;
	cv_tex_ = nullptr;

	clear_new_frame();
	frames = 0;
}

void trtc_client::VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame)
{
	twebrtc_send_helper::tlock send_lock(client_.message_handler_.send_helper);
	if (!send_lock.can_send()) {
		return;
	}

	if (!pixels_) {
		// must execute before lock sink_mutex, because main thread maybe waiting for sink_mutex. 
		// for exmaple, did_draw_slice.
		tmessage_handler::tmsg_data_firstframe* pdata = new tmessage_handler::tmsg_data_firstframe(remote_, at_, video_frame.width(), video_frame.height());
		instance->sdl_thread().Send(RTC_FROM_HERE, &client_.message_handler_, tmessage_handler::POST_MSG_FIRSTFRAME, pdata);
	}
	
	threading::lock lock2(client_.get_sink_mutex(remote_));
	if (!pixels_) {
		// maybe in change rotation
		return;
	}
	VALIDATE(app_width_ != nposm && app_height_ != nposm, null_str);

	frame_thread_frames_ ++;
	if (!requrie_encode && frame_thread_frames_ % sample_interval_ != 0) {
		return;
	}

	// const webrtc::VideoFrame frame(webrtc::I420Buffer::Rotate(video_frame.video_frame_buffer(), video_frame.rotation()),
	//	webrtc::kVideoRotation_0, video_frame.timestamp_us());
	rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(video_frame.video_frame_buffer()->ToI420());

	if (buffer->width() != app_width_) {
		// android: back-camera(90), front-camera(270),
		webrtc::VideoRotation rotation = video_frame.rotation() == webrtc::kVideoRotation_90? webrtc::kVideoRotation_90: webrtc::kVideoRotation_270;
		buffer = webrtc::I420Buffer::Rotate(*buffer, rotation);
	}

	if (encoder_.get() != nullptr) {
		encoder_->encode_frame(buffer);
	}

	int pitch = buffer->width() * bytesperpixel_;

	libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
        buffer->DataU(), buffer->StrideU(),
        buffer->DataV(), buffer->StrideV(),
        pixels_,
		pitch,
        buffer->width(), buffer->height());

	if (hflip_) {
		int offset, index1, index2;
		uint8_t* src;
		uint8_t* dst;
		uint8_t tmp;
		for (int y = 0; y != app_height_; ++y) {
			offset = y * app_width_;
			for (int x = 0; x != app_width_ / 2; ++x) {
				index1 = offset + x;
				index2 = offset + app_width_ - x - 1;
				src = pixels_ + (index1 << 2);
				dst = pixels_ + (index2 << 2);
				tmp = src[0];
				src[0] = dst[0];
				dst[0] = tmp;
				tmp = src[1];
				src[1] = dst[1];
				dst[1] = tmp;
				tmp = src[2];
				src[2] = dst[2];
				dst[2] = tmp;
				tmp = src[3];
				src[3] = dst[3];
				dst[3] = tmp;
			}
		}
	}

	frame_thread_new_frame_ = true;

	uint32_t now = SDL_GetTicks();
    if (startup_verbose_ms_ == 0) {
		startup_verbose_ms_ = now;
		last_verbose_ms_ = now;
    }
    if (now - last_verbose_ms_ >= 10000) {
		uint32_t elapsed_second = (now - last_verbose_ms_) / 1000;
		SDL_Log("[rtc_client]#%i, total: %u, (%uS)%u*%i fps, %ix%i/%ix%i, rotation(%i)", 
			at_, now - startup_verbose_ms_, elapsed_second, (frame_thread_frames_ - last_verbose_frames_) / elapsed_second, sample_interval_, buffer->width(), buffer->height(), video_frame.width(), video_frame.height(), video_frame.rotation());
		last_verbose_ms_ = now;
		last_verbose_frames_ = frame_thread_frames_;
    }
}

int trtc_client::VideoRenderer::rtsp_sessions() const
{
	const trtsp_encoder* encoder_ptr = encoder_.get();
	if (encoder_ptr == nullptr) {
		return 0;
	}
	return encoder_ptr->sessions_.size();
}

void trtc_client::allocate_vrenderers(bool remote, int count)
{
	VideoRenderer*** vrenderers = remote? &remote_vrenderer_: &local_vrenderer_;
	int& internal_count = remote? remote_vrenderer_count_: local_vrenderer_count_;

	VALIDATE(*vrenderers == nullptr && internal_count == 0 && count > 0, null_str);
	*vrenderers = (VideoRenderer**)malloc(count * sizeof(VideoRenderer));
	memset(*vrenderers, 0, sizeof(VideoRenderer*) * count);
	internal_count = count;
}

void trtc_client::free_vrenderers(bool remote)
{
	VideoRenderer*** vrenderers = remote? &remote_vrenderer_: &local_vrenderer_;
	int& count = remote? remote_vrenderer_count_: local_vrenderer_count_;
	if (*vrenderers == nullptr) {
		VALIDATE(count == 0, null_str);
		return;
	}
	free(*vrenderers);
	*vrenderers = nullptr;
	count = 0;
}

void trtc_client::draw_slice(SDL_Renderer* renderer, const SDL_Rect& draw_rect)
{
	VALIDATE_IN_MAIN_THREAD();

	{
		threading::lock lock(local_sink_mutex_);
		threading::lock lock2(remote_sink_mutex_);

		uint32_t now = SDL_GetTicks();
		bool has_frame = false;
		for (int i = 0; i < local_vrenderer_count_; i ++) {
			VideoRenderer* sink = local_vrenderer_[i];
			if (sink) {
				if (sink->frame_thread_new_frame_) {
					SDL_UnlockTexture(sink->tex_.get());
					sink->new_frame = true;
					sink->frames ++;
					sink->last_frame_ticks = now;
				}
				if (!has_frame) {
					has_frame = sink->frames > 0;
				}
			}
		}
		for (int i = 0; i < remote_vrenderer_count_; i ++) {
			VideoRenderer* sink = remote_vrenderer_[i];
			if (sink) {
				// local sink always valid, but remote sink not.
				if (sink->frame_thread_new_frame_) {
					SDL_UnlockTexture(sink->tex_.get());
					sink->new_frame = true;
					sink->frames ++;
					sink->last_frame_ticks = now;
				}
				if (!has_frame) {
					has_frame = sink->frames > 0;
				}
			}
		}
		
		if (!has_frame) {
			// no videorenderer has received frame.
			return;
		}

		// item in remote_vrenderer_ maybe non-nullptr, change to nullptr later.
		adapter_->did_draw_slice(id_, renderer, local_vrenderer_, local_vrenderer_count_, remote_vrenderer_, remote_vrenderer_count_, draw_rect);

		for (int i = 0; i < local_vrenderer_count_; i ++) {
			VideoRenderer* sink = local_vrenderer_[i];
			if (sink) {
				sink->clear_new_frame();
			}
		}

		for (int i = 0; i < remote_vrenderer_count_; i ++) {
			VideoRenderer* sink = remote_vrenderer_[i];
			if (sink) {
				sink->clear_new_frame();
			}
		}
	}
}

bool trtc_client::can_safe_stop() const
{
	bool can_stop = true;
	for (int i = 0; i < local_vrenderer_count_; i ++) {
		VideoRenderer* sink = local_vrenderer_[i];
		if (sink) {
			if (!sink->texture_allocated()) {
				can_stop = false;
				break;
			}
		}
	}
	return can_stop;
}

void trtc_client::allocate_texture(bool remote, int at, int width, int height)
{
	VALIDATE_IN_MAIN_THREAD();

	SDL_Renderer* renderer = get_renderer();

	VideoRenderer& vsink = *vrenderer(remote, at);
	VALIDATE(vsink.encoder_.get() == nullptr, null_str);
	if (vsink.requrie_encode) {
		vsink.encoder_.reset(new trtsp_encoder(*this, vsink, cricket::kH264CodecName));
		VALIDATE(vsink.encoder_.get() != nullptr, null_str);
	}

	int app_video_width = width;
	int app_video_height = height;
	if (game_config::mobile && !remote) {
		// mobile device && local camera
		// if remove video, peer need make sure "right" before send.
		if (height > width) {
			int tmp = width;
			width = height;
			height = tmp;
		}

		bool landscape = game_config::mobile? gui2::twidget::current_landscape: true;
		if (!gui2::twidget::should_conside_orientation(gui2::settings::screen_width, gui2::settings::screen_height)) {
			landscape = true;
		}
		// calculate app_width, app_height
		app_video_width = landscape? width: height;
		app_video_height = landscape? height: width;
	}

	VALIDATE(vsink.app_width_ == nposm && vsink.app_height_ == nposm, null_str);
	VALIDATE(vsink.tex_.get() == nullptr && vsink.pixels_ == nullptr && vsink.cv_tex_.get() == nullptr, null_str);

	// It is required that vsink.tex_ and vsink.cv_tex_ all be valid, 
	// exit the function as soon as one is invalid, and declare this execute failure.
	vsink.tex_ = SDL_CreateTexture(renderer, get_screen_format().format, SDL_TEXTUREACCESS_STREAMING, app_video_width, app_video_height);
	if (vsink.tex_.get() == nullptr) {
		return;
	}

	// some platform, SDL_TEXTUREACCESS_STREAMING isn't SupportedFormat, for example opengl.
	// on those platform, allocated pixels maybe 4 bytes as if we want 3 bytes!
	// vsink.tex_ = SDL_CreateTexture(get_renderer(), SDL_TEXTUREACCESS_STREAMING, SDL_TEXTUREACCESS_STREAMING, width, height);

	// opencv texture
	vsink.cv_tex_ = SDL_CreateTexture(renderer, get_screen_format().format, SDL_TEXTUREACCESS_STREAMING, app_video_width, app_video_height);
	if (vsink.cv_tex_.get() == nullptr) {
		vsink.tex_ = nullptr;
		return;
	}

	int pitch = 0;
	// SDL_LockTexture isn't relative with opengl, it always returns 0 when app is background(android)
	SDL_LockTexture(vsink.tex_.get(), NULL, (void**)&vsink.pixels_, &pitch);

	vsink.set_app_size(app_video_width, app_video_height);
}

void trtc_client::clear_texture()
{
	VALIDATE_IN_MAIN_THREAD();

	// inform all vrender, immediate return in OnFrame.
	message_handler_.send_helper.clear_msg();

	// make surfe no OnFrame in executing.
	threading::lock lock(local_sink_mutex_);
	threading::lock lock2(remote_sink_mutex_);

	for (int i = 0; i < local_vrenderer_count_; i ++) {
		VideoRenderer* renderer = local_vrenderer_[i];
		renderer->clear_texture();
	}
	for (int i = 0; i < remote_vrenderer_count_; i ++) {
		VideoRenderer* renderer = remote_vrenderer_[i];
		renderer->clear_texture();
	}

	message_handler_.send_helper.reset_deconstructed();
}

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamLabel[] = "stream_label";
const uint16_t kDefaultServerPort = 8888;

std::string GetStunConnectionString()
{
	return "stun:133.130.113.73:3478";
}

std::string GetTurnConnectionString()
{
	return "turn:test@133.130.113.73:3478";
}

// Updates the original SDP description to instead prefer the specified video
// codec. We do this by placing the specified codec at the beginning of the
// codec list if it exists in the sdp.
std::string trtc_client::webrtc_set_preferred_codec(const std::string& sdp, const std::string& codec)
{
	std::string result;

	// a=rtpmap:126 H264/90000
	const std::string codec2 = std::string(" ") + codec + "/";
	const std::string lineSeparator = "\n";
	const std::string mLineSeparator = " ";

	// Copied from PeerConnectionClient.java.
	// TODO(tkchin): Move this to a shared C++ file.
	const char* mvideo = strstr(sdp.c_str(), "\nm=video ");
	if (!mvideo) {
		// No m=video line, so can't prefer @codec
		return sdp;
	}
	mvideo += 1;
	const std::string prefix = "a=rtpmap:";
	std::string line, payload;
	const char* rtpmap = strstr(mvideo, prefix.c_str());
	const char* until;
	while (rtpmap) {
		until = rtpmap + prefix.size();
		while (*until != '\r' && *until != '\n') {
			until ++;
		}
		line.assign(rtpmap, until - rtpmap);
		if (line.find(codec2) != std::string::npos) {
			payload.assign(rtpmap + prefix.size(), strchr(rtpmap, ' ') - rtpmap - prefix.size());
			break;
		}
		rtpmap = until;
		rtpmap = strstr(rtpmap, prefix.c_str());
	}
	if (payload.empty()) {
		return sdp;
	}
	line.assign(mvideo, strchr(mvideo, '\n') - mvideo);
	std::vector<std::string> vstr = utils::split(line, ' ', utils::REMOVE_EMPTY);
	// m=video 9 UDP/TLS/RTP/SAVPF 126 120 97
	std::vector<std::string>::iterator it = std::find(vstr.begin(), vstr.end(), payload);
	const size_t require_index = 3;
	size_t index = std::distance(vstr.begin(), it);
	if (index == require_index) {
		return sdp;
	}
	vstr.erase(it);
	it = vstr.begin();
	std::advance(it, require_index);
	vstr.insert(it, payload);

	result = sdp.substr(0, mvideo - sdp.c_str());
	result += utils::join(vstr, " ");
	result += sdp.substr(mvideo - sdp.c_str() + line.size());
	
	return result;
}

bool trtc_client::InitializePeerConnection()
{
	message_handler_.send_helper.reset_deconstructed();
	relay_only_ = app_relay_only();

	connection_state_ = webrtc::PeerConnectionInterface::kIceConnectionNew;
	gathering_state_ = webrtc::PeerConnectionInterface::kIceGatheringNew;

	VALIDATE(local_vrenderer_count_ == 0 && remote_vrenderer_count_ == 0, null_str);
	VALIDATE(peer_connection_factory_.get() == NULL, null_str);
	VALIDATE(peer_connection_.get() == NULL, null_str);
	RTC_CHECK(_networkThread.get() == NULL);
	RTC_CHECK(_workerThread.get() == NULL);
	RTC_CHECK(_signalingThread.get() == NULL);

	_networkThread = rtc::Thread::CreateWithSocketServer();
    bool result = _networkThread->Start();
    RTC_CHECK(result);

    _workerThread = rtc::Thread::Create();
    result = _workerThread->Start();
    RTC_CHECK(result);

    rtc::Thread* signalingThread;
// #ifdef _WIN32
//	signalingThread = rtc::Thread::Current();
// #else
    _signalingThread = rtc::Thread::Create();
    result = _signalingThread->Start();
    signalingThread = _signalingThread.get();
// #endif
    RTC_CHECK(result);

	cricket::WebRtcVideoEncoderFactory* encoder_factory = NULL;
	cricket::WebRtcVideoDecoderFactory* decoder_factory = NULL;
#if defined(__APPLE__) && TARGET_OS_IPHONE

#elif defined(ANDROID)
	encoder_factory = new webrtc::jni::MediaCodecVideoEncoderFactory();
    decoder_factory = new webrtc::jni::MediaCodecVideoDecoderFactory();
#endif

	peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        _networkThread.get(), _workerThread.get(), signalingThread,
        nullptr, 
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		encoder_factory, decoder_factory);

	// peer_connection_factory_ = webrtc::CreatePeerConnectionFactory();

	if (!peer_connection_factory_.get()) {
		DeletePeerConnection();
		return false;
	}

	if (!CreatePeerConnection(DTLS_ON)) {
		DeletePeerConnection();
	}
	AddStreams();

	if (idle_screen_saver_) {
		disable_idle_lock_.reset(new tdisable_idle_lock);
	}
	return peer_connection_.get() != NULL;
}

bool trtc_client::CreatePeerConnection(bool dtls) 
{
	VALIDATE(peer_connection_factory_.get() != NULL, null_str);
	VALIDATE(peer_connection_.get() == NULL, null_str);

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServer server;
	// server.uri = GetStunConnectionString();
	server.uri = GetTurnConnectionString();
	server.username = "test";
	server.password = "123";
	// server.password = "123456";
	config.servers.push_back(server);

	webrtc::FakeConstraints constraints;
	if (dtls) {
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
			"true");
	} else {
		constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp,
			"false");
	}

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(
		config, &constraints, NULL, NULL, this);
	return peer_connection_.get() != NULL;
}

void trtc_client::DeletePeerConnection() 
{
	SDL_Log("trtc_client::DeletePeerConnection(), E");

	// prevent frame thread froming Send() late.
	message_handler_.send_helper.clear_msg();

	if (peer_connection_.get()) {
		state_ = NOT_CONNECTED;
		SDL_Log("trtc_client::DeletePeerConnection(), Close(), E");
		peer_connection_->Close();
		SDL_Log("trtc_client::DeletePeerConnection(), Close(), X");
	} else {
		VALIDATE(state_ != CONNECTED, null_str);
	}

	peer_connection_ = NULL;
	active_streams_.clear();
	StopLocalRenderer();
	StopRemoteRenderer(nposm);
	peer_connection_factory_ = NULL;


	_networkThread.reset();
	_workerThread.reset();
	_signalingThread.reset();

	disable_idle_lock_.reset();
	SDL_Log("trtc_client::DeletePeerConnection(), X");
}

void trtc_client::ConnectToPeer(const std::string& peer_nick, bool caller, const std::string& offer)
{
	VALIDATE(!peer_nick.empty(), null_str);

	if (peer_connection_.get()) {
		// main_wnd_->MessageBox("Error", "We only support connecting to one peer at a time", true);
		return;
	}

	if (InitializePeerConnection()) {
		if (caller) {
			peer_connection_->CreateOffer(this, NULL);

		} else {
			// Replace message type from "offer" to "answer"
			const std::string offer2 = webrtc_set_preferred_codec(offer, preferred_codec_);
			webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("offer", offer2, nullptr));
			peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description);
			peer_connection_->CreateAnswer(this, NULL);
		}
	} else {
		// main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
	}
}

void trtc_client::AddStreams() 
{
	if (rtsp() && rtsp_exclude_local_) {
		return;
	}
	if (active_streams_.find(kStreamLabel) != active_streams_.end()) {
		return;  // Already added.
	}

	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
	audio_track = peer_connection_factory_->CreateAudioTrack(
			kAudioLabel, peer_connection_factory_->CreateAudioSource(NULL));

	webrtc::FakeConstraints video_constraints;
	if (desire_fps_ != nposm) {
		VALIDATE(desire_fps_ >= 1, null_str);
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMaxFrameRate, str_cast(desire_fps_));
	}
	if (desire_size_.x != nposm) {
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMinWidth, str_cast(desire_size_.x));
	}
	if (desire_size_.y != nposm) {
		video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMinHeight, str_cast(desire_size_.y));
	}

	// although support set framt rate use kMinFrameRate, kMaxFrameRate, but some os not support in fact. include windows(directshow), iOS.
	// in order to cross-platform, caller don't set frame rate.
	// video_constraints.AddOptional(webrtc::MediaConstraintsInterface::kMaxFrameRate, "7");

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = peer_connection_factory_->CreateLocalMediaStream(kStreamLabel);

	//
	// enmuate desire video capture, and insert them to stream.
	//
	std::vector<std::string> device_names;

	{
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(webrtc::VideoCaptureFactory::CreateDeviceInfo());
		VALIDATE(info.get(), "Construct VideoCaptureModule::DeviceInfo fail.");

		int num_devices = info->NumberOfDevices();
		for (int i = 0; i < num_devices; ++i) {
			const uint32_t kSize = 256;
			char name[kSize] = { 0 };
			char id[kSize] = { 0 };
			if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
				device_names.push_back(name);
			}
		}
	}
	existed_vidcaps_ = device_names;

	// device_names.empty() maybe is empty. let app handle this fault.
	std::vector<tusing_vidcap> using_vidcaps = adapter_->app_video_capturer(id_, false, device_names);
	if (using_vidcaps.empty()) {
		// app may accept no camera, for settings.
		return;
	}
	allocate_vrenderers(false, using_vidcaps.size());

	cricket::WebRtcVideoDeviceCapturerFactory factory;
	std::unique_ptr<cricket::VideoCapturer> capturer;

	std::stringstream err;
	int at = 0;
	bool has_encode_vidcap = false;
	for (std::vector<tusing_vidcap>::const_iterator it = using_vidcaps.begin(); it != using_vidcaps.end(); ++ it, at ++) {
		const tusing_vidcap& using_vidcap = *it;
		capturer = factory.Create(cricket::Device(using_vidcap.name, 0));
		err.str("");
		err << "Create video capturer(" << using_vidcap.name << ") fail.";
		VALIDATE(capturer.get(), err.str());

		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
			peer_connection_factory_->CreateVideoTrack(
				kVideoLabel,
				peer_connection_factory_->CreateVideoSource(std::move(capturer),
					&video_constraints)));

		VALIDATE(video_track.get(), "create video track fail.");
		StartLocalRenderer(video_track, using_vidcap.name, at, using_vidcap.encode);
		if (using_vidcap.encode) {
			has_encode_vidcap = true;
		}
		stream->AddTrack(video_track);
	}
	if (has_encode_vidcap) {
		live555d::register_vidcap(*this);
	}

	//
	// insert audio to stream.
	//
	if (audio_track.get()) {
		stream->AddTrack(audio_track);
	}
	if (!peer_connection_->AddStream(stream)) {
		RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
	}
	typedef std::pair<std::string,
		rtc::scoped_refptr<webrtc::MediaStreamInterface> >
		MediaStreamPair;
	active_streams_.insert(MediaStreamPair(stream->label(), stream));
}

//
// PeerConnectionObserver implementation.
//

void trtc_client::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
	// thread context: _signalingThread
	if (new_state == webrtc::PeerConnectionInterface::kClosed) {
	}
}

// Called when a remote stream is added
void trtc_client::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{
	// thread context: _signalingThread
	RTC_LOG(INFO) << __FUNCTION__ << " " << stream->label();

	webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
	// Only render the first track.
	if (!tracks.empty()) {
		webrtc::VideoTrackInterface* track = tracks[0];
		StartRemoteRenderer(track, "remote camera", 0);
		// once release, this MediaStream will not be ~MediaStream!
		// stream.release();
	}
}

void trtc_client::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) 
{
	// thread context: _signalingThread
	RTC_LOG(INFO) << __FUNCTION__ << " " << stream->label();
}

void trtc_client::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	// thread context: _signalingThread
	connection_state_ = new_state;
	app_OnIceConnectionChange();
}

void trtc_client::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
	// thread context: _signalingThread
	gathering_state_ = new_state;
	app_OnIceGatheringChange();
}

void trtc_client::OnIceConnectionReceivingChange(bool receiving)
{
	// thread context: _signalingThread
}

void trtc_client::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	// thread context: _signalingThread
	const std::string& type = candidate->candidate().type();
	if (relay_only_ && type != cricket::RELAY_PORT_TYPE) {
		return;
	}
	
	if (rtsp()) {
		return;
	}

	std::string sdp;
	if (!candidate->ToString(&sdp)) {
		RTC_LOG(LS_ERROR) << __FUNCTION__ << " Failed to serialize candidate";
		return;
	}

	RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index() << " " << sdp;

	const std::string sdp_mid = candidate->sdp_mid();
	const int sdp_mline_index = candidate->sdp_mline_index();

	Json::StyledWriter writer;
	Json::Value jmessage, jcandidate;

	jcandidate[kCandidateSdpMidName] = candidate->sdp_mid();
	jcandidate[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	jcandidate["candidate"] = sdp;

	jmessage["id"] = "onIceCandidate";
	jmessage["candidate"] = jcandidate;

	msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
}

#include <webrtc/pc/mediasession.h>

void trtc_client::OnSuccess(webrtc::SessionDescriptionInterface* desc) 
{
	const cricket::SessionDescription& desc2 = *desc->description();
	const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(&desc2);
	const cricket::ContentInfo* video = cricket::GetFirstVideoContent(&desc2);
	const cricket::ContentInfo* data = cricket::GetFirstDataContent(&desc2);

	if (local_only_) {
		// if avcapture, don't call SetLocalDescription, don't start local network transport logic.
/*
		std::string sdp;
		desc->ToString(&sdp);
		{
			tfile file(game_config::preferences_dir + "/desc.sdp", GENERIC_WRITE, CREATE_ALWAYS);
			VALIDATE(file.valid(), null_str);
			posix_fwrite(file.fp, sdp.c_str(), sdp.size());
		}
		{
			sdp = webrtc_set_preferred_codec(sdp, cricket::kH264CodecName);
			tfile file(game_config::preferences_dir + "/desc-h264.sdp", GENERIC_WRITE, CREATE_ALWAYS);
			VALIDATE(file.valid(), null_str);
			posix_fwrite(file.fp, sdp.c_str(), sdp.size());
		}
*/
		return;
	}

	if (rtsp()) {
/*
		// 1. start rtsp client.
		// 2. SetRemoteDescription.
		// see reference to <librose>/gui/dialogs/chat.cpp
		std::string sdp;
		{
			tfile file(game_config::preferences_dir + "/desc.sdp", GENERIC_READ, OPEN_EXISTING);
			int fsize = file.read_2_data();
			VALIDATE(fsize > 0, null_str);
			sdp.assign(file.data, fsize);
		}
		// Replace message type from "offer" to "answer"
		sdp = webrtc_set_preferred_codec(sdp, preferred_codec_);
		webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription("answer", sdp, nullptr));
		peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description);
*/
		// start_rtsp();
		instance->sdl_thread().Invoke<void>(RTC_FROM_HERE, rtc::Bind(&trtc_client::start_rtsp, this));
		return;
	}

	// thread context: _signalingThread
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

	std::string sdp;
	desc->ToString(&sdp);

	Json::StyledWriter writer;
	Json::Value jmessage;

	if (caller_) {
		jmessage["id"] = "call";
		jmessage["from"] = my_nick_;
		jmessage["to"] = peer_nick_;
		jmessage[kSessionDescriptionSdpName] = sdp;
	} else {
		jmessage["id"] = "incomingCallResponse";
		jmessage["from"] = peer_nick_;
		jmessage["callResponse"] = "accept";
		jmessage["sdpAnswer"] = sdp;
	}

	msg_2_signaling_server(control_socket_.get(), writer.write(jmessage));
}

void trtc_client::OnFailure(const std::string& error) 
{
	// thread context: _signalingThread
	RTC_LOG(LERROR) << error;
}

//
// PeerConnectionClientObserver implementation.
//

void trtc_client::OnServerConnectionFailure()
{
	RTC_LOG(INFO) << "Failed to connect to " << server_;
}

rtc::AsyncSocket* CreateClientSocket(int family) 
{
	rtc::Thread* thread = rtc::Thread::Current();
	VALIDATE(thread != NULL, null_str);
	return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
}

void trtc_client::InitSocketSignals() 
{
	VALIDATE(control_socket_.get() != NULL, null_str);

	control_socket_->SignalCloseEvent.connect(this, &trtc_client::OnClose);
	control_socket_->SignalConnectEvent.connect(this, &trtc_client::OnConnect);
	control_socket_->SignalReadEvent.connect(this, &trtc_client::OnRead);
}

void trtc_client::Connect(const std::string& server, int port) 
{
	VALIDATE(!server.empty(), null_str);

	if (state_ != NOT_CONNECTED) {
		RTC_LOG(WARNING)
			<< "The client must not be connected before you can call Connect()";
		OnServerConnectionFailure();
		return;
	}

	if (server.empty()) {
		OnServerConnectionFailure();
		return;
	}

	if (port <= 0) {
		port = kDefaultServerPort;
	}

	server_address_.SetIP(server);
	server_address_.SetPort(port);

	if (server_address_.IsUnresolvedIP()) {
		state_ = RESOLVING;
		resolver_ = new rtc::AsyncResolver();
		resolver_->SignalDone.connect(this, &trtc_client::OnResolveResult);
		resolver_->Start(server_address_);
	} else {
		DoConnect();
	}
}

void trtc_client::OnResolveResult(rtc::AsyncResolverInterface* resolver) 
{
	if (resolver_->GetError() != 0) {
		OnServerConnectionFailure();
		resolver_->Destroy(false);
		resolver_ = NULL;
		state_ = NOT_CONNECTED;
	} else {
		server_address_ = resolver_->address();
		DoConnect();
	}
}

void trtc_client::DoConnect() 
{
	control_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));
	InitSocketSignals();

	bool ret = ConnectControlSocket();
	if (ret) {
		state_ = SIGNING_IN;
	} else {
		OnServerConnectionFailure();
	}
}

void trtc_client::Close() 
{
	SDL_Log("trtc_client::Close(), E");

	if (!rtsp() && !local_only_) {
		control_socket_->Close();
	}
	if (resolver_ != NULL) {
		resolver_->Destroy(false);
		resolver_ = NULL;
	}
	my_id_ = -1;
	state_ = NOT_CONNECTED;

	existed_vidcaps_.clear();

	SDL_Log("trtc_client::Close(), X");
}

bool trtc_client::ConnectControlSocket() 
{
	VALIDATE(control_socket_->GetState() == rtc::Socket::CS_CLOSED, null_str);
	int err = control_socket_->Connect(server_address_);
	if (err == SOCKET_ERROR) {
		Close();
		return false;
	}
	return true;
}

void trtc_client::OnConnect(rtc::AsyncSocket* socket) 
{
	Json::StyledWriter writer;
	Json::Value jmessage;

	jmessage["id"] = "register";
	jmessage["name"] = my_nick_;

	msg_2_signaling_server(socket, writer.write(jmessage));
}

void trtc_client::msg_2_signaling_server(rtc::AsyncSocket* socket, const std::string& msg)
{
	const int prefix_bytes = 2;

	VALIDATE(!msg.empty(), null_str);

	resize_send_data(prefix_bytes + msg.size());
	unsigned short n = SDL_SwapBE16((uint16_t)msg.size());
	memcpy(send_data_, &n, sizeof(short));
	memcpy(send_data_ + 2, msg.c_str(), msg.size());

	size_t sent = socket->Send(send_data_, prefix_bytes + msg.size());
	VALIDATE(sent == prefix_bytes + msg.length(), null_str);
	// RTC_UNUSED(sent);
}

// every call at least return one message, as if exist more message in this triger.
// caller require use while to call it.
const char* trtc_client::read_2_buffer(rtc::AsyncSocket* socket, bool first, size_t* content_length)
{
	const int prefix_bytes = 2;
	const int chunk_size = 1024;
	int ret_size = 0;

	if (last_msg_should_size_ != -1 && recv_data_vsize_ >= prefix_bytes + last_msg_should_size_) {
		if (recv_data_vsize_ > prefix_bytes + last_msg_should_size_) {
			memmove(recv_data_, recv_data_ + prefix_bytes + last_msg_should_size_, recv_data_vsize_ - prefix_bytes - last_msg_should_size_);
		}
		recv_data_vsize_ -= prefix_bytes + last_msg_should_size_;
		last_msg_should_size_ = -1;
	}

	if (!first) {
		if (recv_data_vsize_ == 0) {
			return NULL;
		} else if (last_msg_should_size_ == -1 && recv_data_vsize_ >= prefix_bytes) {
			last_msg_should_size_ = SDL_SwapBE16(*(reinterpret_cast<uint16_t*>(recv_data_)));
			if (recv_data_vsize_ >= prefix_bytes + last_msg_should_size_) {
				// if residue bytes is enough to one message, return directory.
				*content_length = last_msg_should_size_;
				return recv_data_ + prefix_bytes;
			}
		}
	}

	do {
		if (recv_data_size_ < recv_data_vsize_ + chunk_size) {
			resize_recv_data(recv_data_size_ + chunk_size);
		}
		ret_size = socket->Recv(recv_data_ + recv_data_vsize_, chunk_size, nullptr);
		if (ret_size <= 0) {
			break;
		}
		recv_data_vsize_ += ret_size;
		if (last_msg_should_size_ == -1 && recv_data_vsize_ >= prefix_bytes) {
			last_msg_should_size_ = SDL_SwapBE16(*(reinterpret_cast<uint16_t*>(recv_data_)));
		}
	} while (ret_size == chunk_size);

	if (last_msg_should_size_ == -1 || recv_data_vsize_ < prefix_bytes + last_msg_should_size_) {
		return NULL;
	}

	*content_length = last_msg_should_size_;
	return recv_data_ + prefix_bytes;
}

void trtc_client::OnClose(rtc::AsyncSocket* socket, int err) 
{
	RTC_LOG(INFO) << __FUNCTION__;

	// must call it, for example remove it from ss_
	socket->Close();

	rtc::Thread* thread = rtc::Thread::Current();
	thread->Post(RTC_FROM_HERE, &dialog_, MSG_SIGNALING_CLOSE, NULL);
}

bool trtc_client::chat_OnMessage(rtc::Message* msg)
{
	if (msg->message_id == MSG_SIGNALING_CLOSE) {
		DeletePeerConnection();
		Close();
		return true;
	}
	return false;
}

void trtc_client::StartLocalRenderer(webrtc::VideoTrackInterface* local_video, const std::string& name, int at, bool encode)
{
	VALIDATE(local_vrenderer_ != nullptr && at >= 0 && at < local_vrenderer_count_ && !local_vrenderer_[at], null_str);
	local_vrenderer_[at] = adapter_->app_create_video_renderer(*this, local_video, name, false, at, encode);
}

void trtc_client::StopLocalRenderer()
{
	bool has_encode_vidcap = false;
	for (int i = 0; i < local_vrenderer_count_; i ++) {
		VideoRenderer* renderer = local_vrenderer_[i];
		if (renderer) {
			if (renderer->requrie_encode) {
				has_encode_vidcap = true;
			}
		}
	}
	if (has_encode_vidcap) {
		// trtc::consume require VideoRenderer valid. so set vidcap=nullptr before destruct VideoRenderer.
		// make sure no consume is running.
		live555d::deregister_vidcap();
	}

	for (int i = 0; i < local_vrenderer_count_; i ++) {
		VideoRenderer* renderer = local_vrenderer_[i];
		if (renderer) {
			delete renderer;
			local_vrenderer_[i] = nullptr;
		}
	}
	free_vrenderers(false);
}

void trtc_client::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video, const std::string& name, int at) 
{
	VALIDATE(remote_vrenderer_ != nullptr && at >= 0 && at < remote_vrenderer_count_ && !remote_vrenderer_[at], null_str);
	remote_vrenderer_[at] = adapter_->app_create_video_renderer(*this, remote_video, name, true, at, false);
}

void trtc_client::StopRemoteRenderer(int at)
{
	SDL_Log("trtc_client::StopRemoteRenderer#%i, E", at);
	for (int i = 0; i < remote_vrenderer_count_; i ++) {
		VideoRenderer* renderer = remote_vrenderer_[i];
		if (renderer && (at == nposm || i == at)) {
			delete renderer;
			remote_vrenderer_[i] = nullptr;
		}
	}
	if (at == nposm) {
		free_vrenderers(true);
	}
	SDL_Log("trtc_client::StopRemoteRenderer#%i, X", at);
}

void trtc_client::resize_recv_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > recv_data_size_) {
		char* tmp = (char*)malloc(size);
		if (recv_data_) {
			if (recv_data_vsize_) {
				memcpy(tmp, recv_data_, recv_data_vsize_);
			}
			free(recv_data_);
		}
		recv_data_ = tmp;
		recv_data_size_ = size;
	}
}

void trtc_client::resize_send_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > send_data_size_) {
		char* tmp = (char*)malloc(size);
		if (send_data_) {
			if (send_data_vsize_) {
				memcpy(tmp, send_data_, send_data_vsize_);
			}
			free(send_data_);
		}
		send_data_ = tmp;
		send_data_size_ = size;
	}
}

void trtc_client::setup(int sessionid)
{
	RTC_DCHECK(live555d::tmanager::live555d_thread_checker->CalledOnValidThread() && sessionid != nposm);
	trtsp_encoder& encoder = *vrenderer(false, 0)->encoder_.get();
	encoder.setup(sessionid);
}

uint32_t trtc_client::read(int sessionid, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize)
{
	RTC_DCHECK(live555d::tmanager::live555d_thread_checker->CalledOnValidThread() && sessionid != nposm);

	trtsp_encoder& encoder = *vrenderer(false, 0)->encoder_.get();
	return encoder.consume(sessionid, *this, inGetAuxSDPLine, fTo, fMaxSize);
}

void trtc_client::teardown(int sessionid)
{
	RTC_DCHECK(live555d::tmanager::live555d_thread_checker->CalledOnValidThread() && sessionid != nposm);

	trtsp_encoder& encoder = *vrenderer(false, 0)->encoder_.get();
	return encoder.teardown(sessionid);
}

tavcapture::tavcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const tpoint& desire_size, const int desire_fps, bool idle_screen_saver)
	: trtc_client(id, dialog, adapter, desire_size, true, std::vector<trtsp_settings>(), desire_fps, false, idle_screen_saver)
{
	caller_ = true;
	state_ = CONNECTED;
	ConnectToPeer("fake", true, null_str);
}

//
//  H264ParseSPS.c(https://blog.csdn.net/lizhijian21/article/details/80982403)
//
//  Created by lzj<lizhijian_21@163.com> on 2018/7/6.
//  Copyright @ 2018 LZJ. All rights reserved.
//
//  See https://www.itu.int/rec/T-REC-H.264-201610-S
//

#include <string.h>
#include <stdlib.h>
#include <math.h>
 
typedef unsigned char BYTE;
typedef int INT;
typedef unsigned int UINT;
 
typedef struct
{
    const BYTE *data;   // sps bitstream
    UINT size;          // size of sps bitstream
    UINT index;         // current bit position in sps bitstream
} sps_bit_stream;
 
//
// remove H264 NAL's Anti competitive code(0x03)
// @param data sps bitstream
// @param dataSize sps bitstream
//
static void del_emulation_prevention(BYTE *data, UINT *dataSize)
{
    UINT dataSizeTemp = *dataSize;
    for (UINT i=0, j=0; i<(dataSizeTemp-2); i++) {
        int val = (data[i]^0x0) + (data[i+1]^0x0) + (data[i+2]^0x3);    // whether competitive code or not
        if (val == 0) {
            for (j=i+2; j<dataSizeTemp-1; j++) {    // remove competitive code
                data[j] = data[j+1];
            }
            
            (*dataSize)--;      // decrement data size
        }
    }
}
 
static void sps_bs_init(sps_bit_stream *bs, const BYTE *data, UINT size)
{
    if (bs) {
        bs->data = data;
        bs->size = size;
        bs->index = 0;
    }
}
 
//
// index is at end or not

// @param bs sps_bit_stream
// @return 1£ºyes£¬0£ºno
//
static INT eof(sps_bit_stream *bs)
{
    return (bs->index >= bs->size * 8);    // bit index was over bitstream
}
 
//
// get bitCount bit from bitstream's index
 
// @param bs sps_bit_stream
// @param bitCount bit
// @return value
//
static UINT u(sps_bit_stream *bs, BYTE bitCount)
{
    UINT val = 0;
    for (BYTE i=0; i<bitCount; i++) {
        val <<= 1;
        if (eof(bs)) {
            val = 0;
            break;
        } else if (bs->data[bs->index / 8] & (0x80 >> (bs->index % 8))) {
            val |= 1;
        }
        bs->index++; 
    }
    
    return val;
}
 
//
// read value that encoded with unsigned Golomb(UE)
// #2^LeadingZeroBits - 1 + (xxx)
// @param bs sps_bit_stream
// @return value
//
static UINT ue(sps_bit_stream *bs)
{
    UINT zeroNum = 0;
    while (u(bs, 1) == 0 && !eof(bs) && zeroNum < 32) {
        zeroNum ++;
    }
    
    return (UINT)((1 << zeroNum) - 1 + u(bs, zeroNum));
}
 
//
// read value that encoded with signed Golomb(SE)
// #(-1)^(k+1) * Ceil(k/2)
 
// @param bs sps_bit_stream
// @return value
//
INT se(sps_bit_stream *bs)
{
    INT ueVal = (INT)ue(bs);
    double k = ueVal;
    
    INT seVal = (INT)ceil(k / 2);     // ceil: integer that at least >= k/2
    if (ueVal % 2 == 0) {       // inverse even: (-1)^(k+1)
        seVal = -seVal;
    }
    
    return seVal;
}
 
//
// parse VUI(Video usability information)
// @param bs sps_bit_stream
// @param info sps's result
// @see E.1.1 VUI parameters syntax
//
void vui_para_parse(sps_bit_stream *bs, sps_info_struct *info)
{
    UINT aspect_ratio_info_present_flag = u(bs, 1);
    if (aspect_ratio_info_present_flag) {
        UINT aspect_ratio_idc = u(bs, 8);
        if (aspect_ratio_idc == 255) {      //Extended_SAR
            u(bs, 16);      //sar_width
            u(bs, 16);      //sar_height
        }
    }
    
    UINT overscan_info_present_flag = u(bs, 1);
    if (overscan_info_present_flag) {
        u(bs, 1);       //overscan_appropriate_flag
    }
    
    UINT video_signal_type_present_flag = u(bs, 1);
    if (video_signal_type_present_flag) {
        u(bs, 3);       //video_format
        u(bs, 1);       //video_full_range_flag
        UINT colour_description_present_flag = u(bs, 1);
        if (colour_description_present_flag) {
            u(bs, 8);       //colour_primaries
            u(bs, 8);       //transfer_characteristics
            u(bs, 8);       //matrix_coefficients
        }
    }
    
    UINT chroma_loc_info_present_flag = u(bs, 1);
    if (chroma_loc_info_present_flag) {
        ue(bs);     //chroma_sample_loc_type_top_field
        ue(bs);     //chroma_sample_loc_type_bottom_field
    }
 
    UINT timing_info_present_flag = u(bs, 1);
    if (timing_info_present_flag) {
        UINT num_units_in_tick = u(bs, 32);
        UINT time_scale = u(bs, 32);
        UINT fixed_frame_rate_flag = u(bs, 1);
        
        info->fps = (UINT)((float)time_scale / (float)num_units_in_tick);
        if (fixed_frame_rate_flag) {
            info->fps = info->fps/2;
        }
    }
    
    UINT nal_hrd_parameters_present_flag = u(bs, 1);
    if (nal_hrd_parameters_present_flag) {
        //hrd_parameters()  //see E.1.2 HRD parameters syntax
    }
    
    // below code require impletement function: hrd_parameters()
    UINT vcl_hrd_parameters_present_flag = u(bs, 1);
    if (vcl_hrd_parameters_present_flag) {
        // hrd_parameters()
    }
    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
        u(bs, 1);   //low_delay_hrd_flag
    }
    
    u(bs, 1);       //pic_struct_present_flag
    UINT bitstream_restriction_flag = u(bs, 1);
    if (bitstream_restriction_flag) {
        u(bs, 1);   //motion_vectors_over_pic_boundaries_flag
        ue(bs);     //max_bytes_per_pic_denom
        ue(bs);     //max_bits_per_mb_denom
        ue(bs);     //log2_max_mv_length_horizontal
        ue(bs);     //log2_max_mv_length_vertical
        ue(bs);     //max_num_reorder_frames
        ue(bs);     //max_dec_frame_buffering
    }
}
 
//See 7.3.1 NAL unit syntax
//See 7.3.2.1.1 Sequence parameter set data syntax
INT h264_parse_sps(const BYTE *data, UINT dataSize, sps_info_struct *info)
{
    if (!data || dataSize <= 0 || !info) return 0;
    INT ret = 0;
    
	memset(info, 0, sizeof(sps_info_struct));
    BYTE *dataBuf = (BYTE*)malloc(dataSize);
    memcpy(dataBuf, data, dataSize);        // clone data, prevents the removal of competitive codes from affecting raw data. 
    del_emulation_prevention(dataBuf, &dataSize);
 
    sps_bit_stream bs = {0};
    sps_bs_init(&bs, dataBuf, dataSize);   // init SPS bitstream
    
    u(&bs, 1);      //forbidden_zero_bit
    u(&bs, 2);      //nal_ref_idc
    UINT nal_unit_type = u(&bs, 5);
 
    if (nal_unit_type == 0x7) {     //Nal SPS Flag
        info->profile_idc = u(&bs, 8);
/*
        u(&bs, 1);      //constraint_set0_flag
        u(&bs, 1);      //constraint_set1_flag
        u(&bs, 1);      //constraint_set2_flag
        u(&bs, 1);      //constraint_set3_flag
        u(&bs, 1);      //constraint_set4_flag
        u(&bs, 1);      //constraint_set4_flag
        u(&bs, 2);      //reserved_zero_2bits
*/
		info->profile_iop = u(&bs, 8);

        info->level_idc = u(&bs, 8);
        
        ue(&bs);    //seq_parameter_set_id
        
        UINT chroma_format_idc = 1;     // normal format is 4:2:0
        if (info->profile_idc == 100 || info->profile_idc == 110 || info->profile_idc == 122 ||
            info->profile_idc == 244 || info->profile_idc == 44 || info->profile_idc == 83 ||
            info->profile_idc == 86 || info->profile_idc == 118 || info->profile_idc == 128 ||
            info->profile_idc == 138 || info->profile_idc == 139 || info->profile_idc == 134 || info->profile_idc == 135) {
            chroma_format_idc = ue(&bs);
            if (chroma_format_idc == 3) {
                u(&bs, 1);      //separate_colour_plane_flag
            }
            
            ue(&bs);        //bit_depth_luma_minus8
            ue(&bs);        //bit_depth_chroma_minus8
            u(&bs, 1);      //qpprime_y_zero_transform_bypass_flag
            UINT seq_scaling_matrix_present_flag = u(&bs, 1);
            if (seq_scaling_matrix_present_flag) {
                UINT seq_scaling_list_present_flag[8] = {0};
                for (INT i=0; i<((chroma_format_idc != 3)?8:12); i++) {
                    seq_scaling_list_present_flag[i] = u(&bs, 1);
                    if (seq_scaling_list_present_flag[i]) {
                        if (i < 6) {    // scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[i])
                        } else { // scaling_list(ScalingList8x8[i - 6], 64, UseDefaultScalingMatrix8x8Flag[i - 6])
                        }
                    }
                }
            }
        }
        
        ue(&bs);        //log2_max_frame_num_minus4
        UINT pic_order_cnt_type = ue(&bs);
        if (pic_order_cnt_type == 0) {
            ue(&bs);        //log2_max_pic_order_cnt_lsb_minus4
        } else if (pic_order_cnt_type == 1) {
            u(&bs, 1);      //delta_pic_order_always_zero_flag
            se(&bs);        //offset_for_non_ref_pic
            se(&bs);        //offset_for_top_to_bottom_field
            
            UINT num_ref_frames_in_pic_order_cnt_cycle = ue(&bs);
            INT *offset_for_ref_frame = (INT *)malloc((UINT)num_ref_frames_in_pic_order_cnt_cycle * sizeof(INT));
            for (INT i = 0; i < (INT)num_ref_frames_in_pic_order_cnt_cycle; i++) {
                offset_for_ref_frame[i] = se(&bs);
            }
            free(offset_for_ref_frame);
        }
        
        ue(&bs);      //max_num_ref_frames
        u(&bs, 1);      //gaps_in_frame_num_value_allowed_flag
        
        UINT pic_width_in_mbs_minus1 = ue(&bs);     // bit36
        UINT pic_height_in_map_units_minus1 = ue(&bs);      //47
        UINT frame_mbs_only_flag = u(&bs, 1);
        
        info->width = (INT)(pic_width_in_mbs_minus1 + 1) * 16;
        info->height = (INT)(2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16;
        
        if (!frame_mbs_only_flag) {
            u(&bs, 1);      //mb_adaptive_frame_field_flag
        }
        
        u(&bs, 1);     //direct_8x8_inference_flag
        UINT frame_cropping_flag = u(&bs, 1);
        if (frame_cropping_flag) {
            UINT frame_crop_left_offset = ue(&bs);
            UINT frame_crop_right_offset = ue(&bs);
            UINT frame_crop_top_offset = ue(&bs);
            UINT frame_crop_bottom_offset= ue(&bs);
            
            //See 6.2 Source, decoded, and output picture formats
            INT crop_unit_x = 1;
            INT crop_unit_y = 2 - frame_mbs_only_flag;      //monochrome or 4:4:4
            if (chroma_format_idc == 1) {   //4:2:0
                crop_unit_x = 2;
                crop_unit_y = 2 * (2 - frame_mbs_only_flag);
            } else if (chroma_format_idc == 2) {    //4:2:2
                crop_unit_x = 2;
                crop_unit_y = 2 - frame_mbs_only_flag;
            }
            
            info->width -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
            info->height -= crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset);
        }
        
        UINT vui_parameters_present_flag = u(&bs, 1);
        if (vui_parameters_present_flag) {
            vui_para_parse(&bs, info);
        }
     
        ret = 1;
    }
    free(dataBuf);
    
    return ret;
}
// <<< H264ParseSPS.c

#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/engine/convert_legacy_video_factory.h"
#include "media/base/h264_profile_level_id.h"
#include "system_wrappers/include/cpu_info.h"

namespace cricket {

static void AddDefaultFeedbackParams(VideoCodec* codec) {
  // Don't add any feedback params for RED and ULPFEC.
  if (codec->name == kRedCodecName || codec->name == kUlpfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
  codec->AddFeedbackParam(
      FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
  // Don't add any more feedback params for FLEXFEC.
  if (codec->name == kFlexfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamCcm, kRtcpFbCcmParamFir));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kRtcpFbNackParamPli));
}

static std::vector<VideoCodec> AssignPayloadTypesAndDefaultCodecs(
    std::vector<webrtc::SdpVideoFormat> input_formats) {
  if (input_formats.empty())
    return std::vector<VideoCodec>();
  static const int kFirstDynamicPayloadType = 96;
  static const int kLastDynamicPayloadType = 127;
  int payload_type = kFirstDynamicPayloadType;

  input_formats.push_back(webrtc::SdpVideoFormat(kRedCodecName));
  input_formats.push_back(webrtc::SdpVideoFormat(kUlpfecCodecName));

  std::vector<VideoCodec> output_codecs;
  for (const webrtc::SdpVideoFormat& format : input_formats) {
    VideoCodec codec(format);
    codec.id = payload_type;
    AddDefaultFeedbackParams(&codec);
    output_codecs.push_back(codec);

    // Increment payload type.
    ++payload_type;
    if (payload_type > kLastDynamicPayloadType)
      break;

    // Add associated RTX codec for recognized codecs.
    // TODO(deadbeef): Should we add RTX codecs for external codecs whose names
    // we don't recognize?
    if (CodecNamesEq(codec.name, kVp8CodecName) ||
        CodecNamesEq(codec.name, kVp9CodecName) ||
        CodecNamesEq(codec.name, kH264CodecName) ||
        CodecNamesEq(codec.name, kRedCodecName)) {
      output_codecs.push_back(
          VideoCodec::CreateRtxCodec(payload_type, codec.id));

      // Increment payload type.
      ++payload_type;
      if (payload_type > kLastDynamicPayloadType)
        break;
    }
  }
  return output_codecs;
}

}

RoseDecoder::RoseDecoder() = default;
RoseDecoder::RoseDecoder(const RoseDecoder&) = default;
RoseDecoder::~RoseDecoder() = default;

std::string RoseDecoder::ToString() const 
{
	std::stringstream ss;
	ss << "{decoder: " << (decoder ? "(VideoDecoder)" : "nullptr");
	ss << ", payload_type: " << payload_type;
	ss << ", payload_name: " << payload_name;
	ss << ", codec_params: {";
	for (const auto& it : codec_params)
		ss << it.first << ": " << it.second;
	ss << '}';
	ss << '}';

	return ss.str();
}

namespace webrtc {
static VideoCodec CreateDecoderVideoCodec(const RoseDecoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  strncpy(codec.plName, decoder.payload_name.c_str(), sizeof(codec.plName));
  codec.codecType = PayloadStringToCodecType(decoder.payload_name);

  if (codec.codecType == kVideoCodecVP8) {
    *(codec.VP8()) = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecVP9) {
    *(codec.VP9()) = VideoEncoder::GetDefaultVp9Settings();
  } else if (codec.codecType == kVideoCodecH264) {
    *(codec.H264()) = VideoEncoder::GetDefaultH264Settings();
    codec.H264()->profile =
        H264::ParseSdpProfileLevelId(decoder.codec_params)->profile;
  } else if (codec.codecType == kVideoCodecStereo) {
    RoseDecoder associated_decoder = decoder;
    associated_decoder.payload_name = CodecTypeToPayloadString(kVideoCodecVP9);
    VideoCodec associated_codec = CreateDecoderVideoCodec(associated_decoder);
    associated_codec.codecType = kVideoCodecStereo;
    return associated_codec;
  }

  codec.width = 320;
  codec.height = 180;
  const int kDefaultStartBitrate = 300;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate =
      kDefaultStartBitrate;

  return codec;
}

void collect_video_decoder(webrtc::VideoEncoderFactory& encoder_factory, webrtc::VideoDecoderFactory& decoder_factory, uint8_t first_h264_payload_type, int clones, 
	std::vector<RoseDecoder>& decoders, DecoderMap& allocated_decoders)
{
	VALIDATE(clones >= 0, null_str);
	VALIDATE(decoders.empty() && allocated_decoders.empty(), null_str);

	std::vector<cricket::VideoCodec> recv_codecs = cricket::AssignPayloadTypesAndDefaultCodecs(encoder_factory.GetSupportedFormats());
	RTC_LOG(LS_INFO) << "collect_video_decoder, recv_codecs's size: " << static_cast<int>(recv_codecs.size());

	cricket::VideoCodec h264_codec;
	for (std::vector<cricket::VideoCodec>::iterator it = recv_codecs.begin(); it != recv_codecs.end(); ++ it) {
		cricket::VideoCodec& codec = *it;
		if (codec.name == cricket::kH264CodecName) {
			SDL_Log("collect_video_decoder, this codes.name(%s) == cricket::kH264CodecName, codec.id from %i to %i", codec.name.c_str(), codec.id, first_h264_payload_type);
			codec.id = first_h264_payload_type;
			h264_codec = codec;
			break;
		}
	}

	for (int at = 0; at < clones; at ++) {
		// Why add 1? --position 0 is reserved for the original playloadType
		h264_codec.id = first_h264_payload_type + at + 1;
		h264_codec.SetParam("rtsp", std::string("ch") + str_cast(at + 1));
		recv_codecs.push_back(h264_codec);
	}

	for (std::vector<cricket::VideoCodec>::const_iterator it = recv_codecs.begin(); it != recv_codecs.end(); ++ it) {
		const cricket::VideoCodec& codec = *it;

		webrtc::SdpVideoFormat video_format(codec.name, codec.params);
		std::unique_ptr<webrtc::VideoDecoder> new_decoder;

		if (codec.name == cricket::kH264CodecName) {
			std::map<std::string, std::string>::const_iterator it = codec.params.find("profile-level-id");
			SDL_Log("collect_video_decoder, codec: %s, id: %i, profile-level-id: %s", codec.ToString().c_str(), codec.id,
				it != codec.params.end()? it->second.c_str(): "<nil>");
		} else {
			RTC_LOG(LS_INFO) << "collect_video_decoder, codec: " << codec.ToString();
		}
		if (!new_decoder) {
			// decoder_factory_->SetReceiveStreamId(stream_params_.id);
			// decoder_factory_->SetReceiveStreamId("video");
			new_decoder = decoder_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(
				codec.name, codec.params));
		}

		// If we still have no valid decoder, we have to create a "Null" decoder
		// that ignores all calls. The reason we can get into this state is that
		// the old decoder factory interface doesn't have a way to query supported
		// codecs.
		if (!new_decoder) {
			RTC_LOG(LS_INFO) << "codec: " << codec.ToString() << ", CreateVideoDecoder fail";
			continue;
		}
		SDL_Log("codec: %s, CreateVideoDecoder success", codec.ToString().c_str());
		VALIDATE(new_decoder.get() != nullptr, null_str);

		RoseDecoder decoder;
		decoder.decoder = new_decoder.get();
		decoder.payload_type = codec.id;
		decoder.payload_name = codec.name;
		decoder.codec_params = codec.params;
		decoders.push_back(decoder);

		const bool did_insert =
		allocated_decoders
			.insert(std::make_pair(video_format, std::move(new_decoder)))
			.second;
		RTC_CHECK(did_insert);
	}
}

namespace video_coding {
class RtspFrameObject: public FrameObject 
{
public:
	RtspFrameObject(const uint8_t* data, size_t frame_size, int width, int height, uint8_t payloadType, uint32_t timestamp);

	bool GetBitstream(uint8_t* destination) const override
	{
		return true;
	}

	// The capture timestamp of this frame.
	uint32_t Timestamp() const override
	{
		return 0;
	}

	// When this frame was received.
	int64_t ReceivedTime() const override 
	{
		return 0;
	}

	// When this frame should be rendered.
	int64_t RenderTime() const override 
	{
		return 0;
	}

private:
	VideoCodecType codec_type_;
};

RtspFrameObject::RtspFrameObject(const uint8_t* data, size_t frame_size, int width, int height, uint8_t payloadType, uint32_t timestamp)
{
	codec_type_ = kVideoCodecH264;

	// Stereo codec appends CopyCodecSpecific to last packet to avoid copy.

	// TODO(philipel): Remove when encoded image is replaced by FrameObject.
	// VCMEncodedFrame members
	RTPVideoHeader video_header;
	video_header.codec = kRtpVideoH264;
	video_header.playout_delay.min_ms = -1;
	video_header.playout_delay.max_ms = -1;

	CopyCodecSpecific(&video_header);
	_completeFrame = true;
	// _payloadType = 100;
	_payloadType = payloadType;
	_timeStamp = timestamp;
	ntp_time_ms_ = -1;
	_frameType = kVideoFrameKey;

	// Setting frame's playout delays to the same values
	// as of the first packet's.
	SetPlayoutDelay(video_header.playout_delay);

	// Since FFmpeg use an optimized bitstream reader that reads in chunks of
	// 32/64 bits we have to add at least that much padding to the buffer
	// to make sure the decoder doesn't read out of bounds.
	// NOTE! EncodedImage::_size is the size of the buffer (think capacity of
	//       an std::vector) and EncodedImage::_length is the actual size of
	//       the bitstream (think size of an std::vector).
	if (codec_type_ == kVideoCodecH264)
		_size = frame_size + EncodedImage::kBufferPaddingBytesH264;
	else
		_size = frame_size;

	_buffer = new uint8_t[_size];
	_length = frame_size;

	// bool bitstream_copied = GetBitstream(_buffer);
	memcpy(_buffer, data, frame_size);
	bool bitstream_copied = true;

	RTC_DCHECK(bitstream_copied);
	_encodedWidth = width;
	_encodedHeight = height;

	// FrameObject members
	timestamp = _timeStamp;

	// http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
	// ts_126114v120700p.pdf Section 7.4.5.
	// The MTSI client shall add the payload bytes as defined in this clause
	// onto the last RTP packet in each group of packets which make up a key
	// frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
	// (HEVC)).
	rotation_ = kVideoRotation_0;
	_rotation_set = true;
	content_type_ = VideoContentType::UNSPECIFIED;
	
	timing_.flags = 255;
}

}  // namespace video_coding

}

trtspcapture::trtspcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const std::vector<trtsp_settings>& rtsps, bool rtsp_exclude_local, const tpoint& desire_local_size, bool idle_screen_saver)
	: trtc_client(id, dialog, adapter, desire_local_size, false, rtsps, nposm, rtsp_exclude_local, idle_screen_saver)
	, first_h264_payload_type_(200)
{
	VALIDATE(!rtsps_.empty(), null_str);

	cricket::WebRtcVideoEncoderFactory* encoder_factory = NULL;
	cricket::WebRtcVideoDecoderFactory* decoder_factory = NULL;

#if defined(__APPLE__) && TARGET_OS_IPHONE
    encoder_factory = webrtc::CreateObjCEncoderFactory2().release();
    decoder_factory = webrtc::CreateObjCDecoderFactory2().release();
#elif defined(ANDROID)
	encoder_factory = new webrtc::jni::MediaCodecVideoEncoderFactory();
    decoder_factory = new webrtc::jni::MediaCodecVideoDecoderFactory();
#endif

	std::unique_ptr<CricketDecoderWithParams> cricket_decoder_with_params(new CricketDecoderWithParams(std::move(std::unique_ptr<cricket::WebRtcVideoDecoderFactory>(decoder_factory))));
	decoder_factory_ = cricket::ConvertVideoDecoderFactory(std::move(cricket_decoder_with_params));
	encoder_factory_ = cricket::ConvertVideoEncoderFactory(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(encoder_factory));

	caller_ = true;
	state_ = CONNECTED;
	ConnectToPeer("fake", true, null_str);
}

trtspcapture::~trtspcapture()
{
	SDL_Log("trtspcapture::~trtspcapture(), E");

	// prevent frame-thread from Send() late.
	// Although following DeletePeerConnection() will also perform clear_msg(), 
	// but immediately followed "delete stream" require, perform it here first.
	message_handler_.send_helper.clear_msg();

	for (std::vector<VideoReceiveStream*>::const_iterator it = receive_streams_.begin(); it != receive_streams_.end(); ++ it) {
		VideoReceiveStream* stream = *it;
		delete stream;
	}
	SDL_Log("trtspcapture::~trtspcapture(), X");
}

void trtspcapture::CricketDecoderWithParams::DestroyVideoDecoder(webrtc::VideoDecoder* decoder)
{
	if (external_decoder_factory_) {
		external_decoder_factory_->DestroyVideoDecoder(decoder);
	} else {
		delete decoder;
	}
}

void trtspcapture::VideoReceiveStream::DecodeThreadFunction(void* ptr) 
{
	// base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);
	// message_loop.set_runloop_quit_with_delete_tasks(true);

	scoped_refptr<base::sequence_manager::TaskQueue> task_queue_;
	scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

	std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_ = base::test::CreateSequenceManagerForMainThreadType(base::test::TaskEnvironment::MainThreadType::IO);
	sequence_manager_->set_runloop_quit_with_delete_tasks(true);
	{
		task_queue_ = sequence_manager_->CreateTaskQueue(
			base::sequence_manager::TaskQueue::Spec("task_environment_default")
				.SetTimeDomain(nullptr));
		task_runner_ = task_queue_->task_runner();
		sequence_manager_->SetDefaultTaskRunner(task_runner_);
		// simple_task_executor_ = std::make_unique<SimpleTaskExecutor>(task_runner_);
		CHECK(base::ThreadTaskRunnerHandle::IsSet())
			<< "ThreadTaskRunnerHandle should've been set now.";
		// CompleteInitialization();
	}

	while (static_cast<trtspcapture::VideoReceiveStream*>(ptr)->Decode()) {}
}

trtspcapture::VideoReceiveStream::VideoReceiveStream(trtspcapture& capture, std::vector<RoseDecoder>& decoders, trtsp_settings& rtsp_settings, int at)
	: capture_(capture)
	, decoders_(decoders)
	, rtsp_settings_(rtsp_settings)
	, at_(at)
	, send_helper_(this)
	, clock_(webrtc::Clock::GetRealTimeClock())
	, decode_thread_(&DecodeThreadFunction,
                     this,
                     "DecodingThread",
                     rtc::kHighestPriority)
	, timing_(new webrtc::VCMTiming(clock_))
	, video_receiver_(clock_, nullptr, this, timing_.get(), this, this)
	, num_cpu_cores_(webrtc::CpuInfo::DetectNumberOfCores())
	, buffer_size_(0)
	, buffer_vsize_(0)
	, next_frame_ticks_(0)
	, stopped_(false)
	, try_restart_threshold_(30000)
	, next_live555_start_ticks_(0)
{
}

trtspcapture::VideoReceiveStream::~VideoReceiveStream()
{
	SDL_Log("VideoReceiveStream::~VideoReceiveStream#%i, E", at_);
	stop();
	SDL_Log("VideoReceiveStream::~VideoReceiveStream#%i, X", at_);
}

// Implements EncodedImageCallback.
webrtc::EncodedImageCallback::Result trtspcapture::VideoReceiveStream::OnEncodedImage(
	const webrtc::EncodedImage& encoded_image,
	const webrtc::CodecSpecificInfo* codec_specific_info,
	const webrtc::RTPFragmentationHeader* fragmentation)
{
	return Result(Result::OK, 0);
}

// Implements NackSender.
void trtspcapture::VideoReceiveStream::SendNack(const std::vector<uint16_t>& sequence_numbers)
{
}

// Implements KeyFrameRequestSender.
void trtspcapture::VideoReceiveStream::RequestKeyFrame()
{
}

int32_t trtspcapture::VideoReceiveStream::FrameToRender(webrtc::VideoFrame& videoFrame,  // NOLINT
                                rtc::Optional<uint8_t> qp,
                                webrtc::VideoContentType content_type)
{
	capture_.vrenderer(true, at_)->OnFrame(videoFrame);

	const std::string file = game_config::preferences_dir + "/google.png";
	if (game_config::os == os_windows && !SDL_IsFile(file.c_str())) {
		rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(videoFrame.video_frame_buffer()->ToI420());
		int pitch = buffer->width() * 4;
		surface surf = create_neutral_surface(buffer->width(), buffer->height());
		{
			surface_lock dst_lock(surf);
			uint8_t* pixels = (uint8_t*)dst_lock.pixels();

			libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
				buffer->DataU(), buffer->StrideU(),
				buffer->DataV(), buffer->StrideV(),
				pixels,
				pitch,
				buffer->width(), buffer->height());
		}
		imwrite(surf, "google.png");
	}
	return 0;
}

void trtspcapture::start_rtsp()
{
	RTC_LOG(LS_INFO) << "trtspcapture::start_rtsp, E";
	VALIDATE(encoder_factory_.get() != nullptr && decoder_factory_.get() != nullptr, null_str);
	VALIDATE(!rtsps_.empty(), null_str);
	VALIDATE(decoders_.empty() && allocated_decoders_.empty(), null_str);
	webrtc::collect_video_decoder(*encoder_factory_.get(), *decoder_factory_.get(), first_h264_payload_type_, rtsps_.size() - 1, decoders_, allocated_decoders_);

	std::vector<std::string> device_names;
	for (std::vector<trtsp_settings>::const_iterator it = rtsps_.begin(); it != rtsps_.end(); ++ it) {
		const trtsp_settings& settings = *it;
		device_names.push_back(settings.name);
	}
	// thie session maybe use both local and remote vidcap.
	std::vector<tusing_vidcap> using_vidcaps = adapter_->app_video_capturer(id_, true, device_names);
	VALIDATE(!using_vidcaps.empty(), "app must select one video_capturer at least.");
	allocate_vrenderers(true, using_vidcaps.size());

	int at = 0;
	for (std::vector<trtsp_settings>::iterator it = rtsps_.begin(); it != rtsps_.end(); ++ it, at ++) {
		trtsp_settings& settings = *it;

		StartRemoteRenderer(nullptr, using_vidcaps[at].name, at);
		receive_streams_.push_back(new VideoReceiveStream(*this, decoders_, settings, at));
		receive_streams_.back()->start();
	}
	adapter_->app_handle_notify(id_, ncode_startlive555finished, nposm, 0, null_str);

	RTC_LOG(LS_INFO) << "trtspcapture::start_rtsp, X";
}

#define LIVE555_BUFFER_SIZE		1048576
#define LIVE555_BUFFER_HSIZE	524288

void trtspcapture::VideoReceiveStream::resize_buffer(int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= vsize && vsize == buffer_vsize_, null_str);

	if (size > buffer_size_) {
		uint8_t* tmp = new uint8_t[size];
		if (live555_buffer_.get() != nullptr) {
			uint8_t* ptr = live555_buffer_.get();
			if (vsize) {
				memcpy(tmp, ptr, vsize);
			}
		}
		live555_buffer_.reset(tmp);
		buffer_size_ = size;
	}
}

void trtspcapture::VideoReceiveStream::start()
{
	//
	// start
	// 
	VALIDATE(!decode_thread_.IsRunning(), null_str);
	VALIDATE(!live555_buffer_ && buffer_size_ == 0 && buffer_vsize_ == 0, null_str);

	RTC_LOG(LS_INFO) << "VideoReceiveStream::start, decoders_'s size: " << static_cast<int>(decoders_.size());
	for (const RoseDecoder& decoder : decoders_) {
		video_receiver_.RegisterExternalDecoder(decoder.decoder,
												decoder.payload_type);
		webrtc::VideoCodec codec = webrtc::CreateDecoderVideoCodec(decoder);

		RTC_LOG(LS_INFO) << "codec: plType: " << static_cast<int>(codec.plType) << ", plName: " << codec.plName;
/*
		RTC_CHECK(rtp_video_stream_receiver_.AddReceiveCodec(codec,
															 decoder.codec_params));
*/
		RTC_CHECK_EQ(VCM_OK, video_receiver_.RegisterReceiveCodec(
								 &codec, num_cpu_cores_, false));
	}
	video_receiver_.RegisterReceiveCallback(this);

	// Start the decode thread
	decode_thread_.Start();
	resize_buffer(LIVE555_BUFFER_SIZE, buffer_vsize_);
	uint8_t header[4] = {0x00, 0x00, 0x00, 0x01};
	buffer_vsize_ += sizeof(header);

	capture_.adapter_->app_handle_notify(capture_.id_, ncode_startlive555, at_, 0, rtsp_settings_.url);
	// start live555, allow start during start_live555_threshold.
	rtsp_settings_.id = live555::start(rtsp_settings_.url, rtsp_settings_.sendrtcp, rtsp_settings_.httpdurl, &rtsp_settings_.ipv4);
	next_live555_start_ticks_ = SDL_GetTicks() + try_restart_threshold_;

	capture_.adapter_->app_handle_notify(capture_.id_, ncode_startlive555finished, at_, rtsp_settings_.id != nposm? 1: 0, rtsp_settings_.url);
}

void trtspcapture::VideoReceiveStream::stop()
{
	stopped_ = true;
	// Prevent decoding thread from Send() to main thread
	send_helper_.clear_msg();
	if (decode_thread_.IsRunning()) {
		// TriggerDecoderShutdown will release any waiting decoder thread and make
		// it stop immediately, instead of waiting for a timeout. Needs to be called
		// before joining the decoder thread.
		video_receiver_.TriggerDecoderShutdown();

		decode_thread_.Stop();
		// Deregister external decoders so they are no longer running during
		// destruction. This effectively stops the VCM since the decoder thread is
		// stopped, the VCM is deregistered and no asynchronous decoder threads are
		// running.
		for (const RoseDecoder& decoder : decoders_) {
			video_receiver_.RegisterExternalDecoder(nullptr, decoder.payload_type);
		}
		live555_buffer_.reset();
		buffer_size_ = 0;
		buffer_vsize_ = 0;
	}
	if (rtsp_settings_.id != nposm) {
		live555::stop(rtsp_settings_.id);
	}
	capture_.StopRemoteRenderer(at_);
}

void trtspcapture::VideoReceiveStream::live555_start()
{
	SDL_Log("%u, trtspcapture::VideoReceiveStream::live555_start", SDL_GetTicks());
	VALIDATE(rtsp_settings_.id == nposm, null_str);
	rtsp_settings_.id = live555::start(rtsp_settings_.url, rtsp_settings_.sendrtcp, rtsp_settings_.httpdurl, &rtsp_settings_.ipv4);
}

void trtspcapture::VideoReceiveStream::live555_restart()
{
	SDL_Log("%u, trtspcapture::VideoReceiveStream::live555_restart", SDL_GetTicks());
	VALIDATE(rtsp_settings_.id != nposm, null_str);
	live555::stop(rtsp_settings_.id);
	rtsp_settings_.id = nposm;
	live555_start();
}

webrtc::video_coding::FrameBuffer::ReturnReason trtspcapture::VideoReceiveStream::NextFrame(std::unique_ptr<webrtc::video_coding::FrameObject>* frame_out) 
{
	if (stopped_) {
		return webrtc::video_coding::FrameBuffer::ReturnReason::kStopped;
	}

	const uint32_t now = SDL_GetTicks();
	if (next_live555_start_ticks_ != 0 && rtsp_settings_.id == nposm) {
		// ether start or stop, must be after first live555::start.
		if (now >= next_live555_start_ticks_) {
			twebrtc_send_helper::tlock lock(send_helper_);
			if (lock.can_send()) {
				instance->sdl_thread().Send(RTC_FROM_HERE, this, POST_MSG_STARTLIVE555, nullptr);
			}
			next_live555_start_ticks_ = now + try_restart_threshold_;
		}
	}

	resize_buffer(buffer_vsize_ + LIVE555_BUFFER_HSIZE, buffer_vsize_);
	const int frame_size = live555::frame_slice(rtsp_settings_.id, live555_buffer_.get() + buffer_vsize_, buffer_size_ - buffer_vsize_);
	if (frame_size == 0) {
		return webrtc::video_coding::FrameBuffer::ReturnReason::kTimeout;
	} else if (frame_size == nposm) {
		// in spethreshold, no video's rtp
		twebrtc_send_helper::tlock lock(send_helper_);
		if (lock.can_send()) {
			instance->sdl_thread().Send(RTC_FROM_HERE, this, POST_MSG_RESTARTLIVE555, nullptr);
		}
		next_live555_start_ticks_ = now + try_restart_threshold_;
		return webrtc::video_coding::FrameBuffer::ReturnReason::kTimeout;
	}

	uint8_t* buffer_ptr = live555_buffer_.get();
	uint8_t* buf = buffer_ptr + buffer_vsize_;
    uint8_t nal  = buf[0];
    uint8_t type = nal & 0x1f;

	if (type <= 23) {
		std::unique_ptr<webrtc::video_coding::RtspFrameObject> frame;
		uint8_t header[4] = {0x00, 0x00, 0x00, 0x01};
		memcpy(buf - sizeof(header), &header, sizeof(header));
		if (type == 1 || type == 5) {
			if (rtsp_settings_.width != nposm) {
				VALIDATE(rtsp_settings_.width > 0 && rtsp_settings_.height > 0, null_str);
				frame.reset(new webrtc::video_coding::RtspFrameObject(buffer_ptr, buffer_vsize_ + frame_size, rtsp_settings_.width, rtsp_settings_.height, capture_.first_h264_payload_type_ + at_, 0));
				buffer_vsize_ = sizeof(header);
				*frame_out = std::move(frame);
				return webrtc::video_coding::FrameBuffer::ReturnReason::kFrameFound;
			} else {
				// fault. but maybe enter it.
				VALIDATE(rtsp_settings_.height == nposm, null_str);
			}

		} else {
			if (type == 7 && rtsp_settings_.width == nposm) {
				// buf[3] = 32;
				sps_info_struct sps;
				memset(&sps, 0, sizeof(sps));
				h264_parse_sps(buf, frame_size, &sps);
				rtsp_settings_.width = sps.width;
				rtsp_settings_.height = sps.height;
				SDL_Log("rtsp's SPS, profile_idc: %u(0x%02x), profile_iop: %u(0x%02x), level_idc(0x%02x): %u, %ux%ux%u", 
					sps.profile_idc, sps.profile_idc, 
					sps.profile_iop, sps.profile_iop,
					sps.level_idc, sps.level_idc, sps.width, sps.height, sps.fps);
				VALIDATE(rtsp_settings_.width > 0 && rtsp_settings_.height > 0, null_str);
			}
			buffer_vsize_ += frame_size + sizeof(header);
		}
	}
	return webrtc::video_coding::FrameBuffer::ReturnReason::kTimeout;
}

bool trtspcapture::VideoReceiveStream::Decode()
{
	static int frames = 0;

	std::unique_ptr<webrtc::video_coding::FrameObject> frame;
	// TODO(philipel): Call NextFrame with |keyframe_required| argument when
	//                 downstream project has been fixed.
	webrtc::video_coding::FrameBuffer::ReturnReason res = NextFrame(&frame);

	if (res == webrtc::video_coding::FrameBuffer::ReturnReason::kStopped) {
		video_receiver_.DecodingStopped();
		return false;
	}

	if (frame) {
		int64_t now_ms = clock_->TimeInMilliseconds();
		RTC_DCHECK_EQ(res, webrtc::video_coding::FrameBuffer::ReturnReason::kFrameFound);

		// RTC_LOG(LS_INFO) << "#" << at_ << "(" << rtsp_settings_.id << "), VideoReceiveStream::Decode(), # " << frames << ", size: " << frame->Length();
		frames ++;
		int decode_result = video_receiver_.Decode(frame.get());
		// VALIDATE(decode_result == WEBRTC_VIDEO_CODEC_OK, null_str);
/*
		if (decode_result == WEBRTC_VIDEO_CODEC_OK ||
			decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME) {

			keyframe_required_ = false;
			frame_decoded_ = true;
			rtp_video_stream_receiver_.FrameDecoded(frame->picture_id);

			if (decode_result == WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME) {
				RequestKeyFrame();
			}
		} else if (!frame_decoded_ || !keyframe_required_ ||
					(last_keyframe_request_ms_ + kMaxWaitForKeyFrameMs < now_ms)) {
			keyframe_required_ = true;
			// TODO(philipel): Remove this keyframe request when downstream project
			//                 has been fixed.
			RequestKeyFrame();
			last_keyframe_request_ms_ = now_ms;
		}
*/
	} else {
		SDL_Delay(10);
	}
	return true;
}

twebrtc_encoder::twebrtc_encoder(const std::string& codec, int max_bitrate_kbps)
	: desire_codec_(codec)
	, max_bitrate_kbps_(max_bitrate_kbps)
	, content_type_(webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo) 
	, video_stream_encoder_(nullptr)
	, encoder_min_bitrate_bps_(0)
    , encoder_max_bitrate_bps_(-1)
    , encoder_target_rate_bps_(0)
	, encoded_frames(0)
	, max_image_length_(0)
	, startup_verbose_ticks_(0)
	, last_verbose_ticks_(0)
	, last_capture_frames_(0)
{
	VALIDATE(!desire_codec_.empty(), null_str);

	cricket::WebRtcVideoEncoderFactory* encoder_factory = NULL;
#if defined(__APPLE__) && TARGET_OS_IPHONE

#elif defined(ANDROID)
	encoder_factory = new webrtc::jni::MediaCodecVideoEncoderFactory();
#endif

	encoder_factory_ = cricket::ConvertVideoEncoderFactory(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(encoder_factory));

	start();
}

twebrtc_encoder::~twebrtc_encoder()
{
	if (video_stream_encoder_ != nullptr) {
		video_stream_encoder_->Stop();
		video_stream_encoder_ = nullptr;
	}
}

// source code is from WebRtcVideoChannel::WebRtcVideoSendStream::CreateVideoEncoderConfig
webrtc::VideoEncoderConfig twebrtc_encoder::CreateVideoEncoderConfig(const cricket::VideoCodec& codec) const
{
  // RTC_DCHECK_RUN_ON(&thread_checker_);
  webrtc::VideoEncoderConfig encoder_config;
  bool is_screencast = false;
  if (is_screencast) {
    // encoder_config.min_transmit_bitrate_bps = 1000 * parameters_.options.screencast_min_bitrate_kbps.value_or(0);
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kScreen;
  } else {
    encoder_config.min_transmit_bitrate_bps = 0;
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
  }

  // By default, the stream count for the codec configuration should match the
  // number of negotiated ssrcs. But if the codec is blacklisted for simulcast
  // or a screencast (and not in simulcast screenshare experiment), only
  // configure a single stream.
  encoder_config.number_of_streams = 1;
/*
  if (IsCodecBlacklistedForSimulcast(codec.name) ||
      (is_screencast &&
       (!UseSimulcastScreenshare() || !parameters_.conference_mode))) {
    encoder_config.number_of_streams = 1;
  }
*/

  // int app_bitrate_kbps = client_.adapter_->app_get_max_bitrate_kbps(client_.id_, vrenderer_);
  encoder_config.max_bitrate_bps = max_bitrate_kbps_ > 0? max_bitrate_kbps_ * 1000: -1;

  const bool conference_mode = false;
  const int kDefaultQpMax = 56;
  int max_qp = kDefaultQpMax;
  codec.GetParam(cricket::kCodecParamMaxQuantization, &max_qp);
  encoder_config.video_stream_factory =
      new rtc::RefCountedObject<cricket::EncoderStreamFactory>(
          codec.name, max_qp, cricket::kDefaultVideoMaxFramerate, is_screencast,
          conference_mode);

  return encoder_config;
}

// source code is from WebRtcVideoChannel::WebRtcVideoSendStream::ConfigureVideoEncoderSettings
rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings> ConfigureVideoEncoderSettings(const cricket::VideoCodec& codec)
{
  // RTC_DCHECK_RUN_ON(&thread_checker_);
  bool is_screencast = false;
  // No automatic resizing when using simulcast or screencast.
  bool frame_dropping = !is_screencast;

  RTC_CHECK(cricket::CodecNamesEq(codec.name, cricket::kH264CodecName));
  if (cricket::CodecNamesEq(codec.name, cricket::kH264CodecName)) {
    webrtc::VideoCodecH264 h264_settings =
        webrtc::VideoEncoder::GetDefaultH264Settings();
    h264_settings.frameDroppingOn = frame_dropping;
	// deduce IRAM time to 5 seconds
	h264_settings.keyFrameInterval = SDL_min(30 * 5, h264_settings.keyFrameInterval);
    return new rtc::RefCountedObject<
        webrtc::VideoEncoderConfig::H264EncoderSpecificSettings>(h264_settings);
  }
  
  return nullptr;
}

void twebrtc_encoder::start()
{
	RTC_LOG(LS_INFO) << "trtc_encoder::start, E";
	RTC_CHECK(recv_codecs_.empty());
	recv_codecs_ = cricket::AssignPayloadTypesAndDefaultCodecs(encoder_factory_->GetSupportedFormats());
	RTC_LOG(LS_INFO) << "trtc_encoder::start, recv_codecs_'s size: " << static_cast<int>(recv_codecs_.size());

	for (std::vector<cricket::VideoCodec>::const_iterator it = recv_codecs_.begin(); it != recv_codecs_.end(); ++ it) {
		const cricket::VideoCodec& codec = *it;
		if (codec.name != desire_codec_) {
			// maybe some codec has same name, for example H264 and H264 high profile.
			// now is use first codec if same name.
			continue;
		}
		webrtc::SdpVideoFormat video_format(codec.name, codec.params);
		std::unique_ptr<webrtc::VideoEncoder> new_encoder;

		RTC_LOG(LS_INFO) << "codec: " << codec.ToString();
		if (!new_encoder && encoder_factory_) {
			// decoder_factory_->SetReceiveStreamId(stream_params_.id);
			// decoder_factory_->SetReceiveStreamId("video");
			new_encoder = encoder_factory_->CreateVideoEncoder(webrtc::SdpVideoFormat(
				codec.name, codec.params));
		}

		// If we still have no valid decoder, we have to create a "Null" decoder
		// that ignores all calls. The reason we can get into this state is that
		// the old decoder factory interface doesn't have a way to query supported
		// codecs.
		if (!new_encoder) {
			RTC_LOG(LS_INFO) << "codec: " << codec.ToString() << ", CreateVideoEncoder fail";
			continue;
		}
		RTC_LOG(LS_INFO) << "codec: " << codec.ToString() << ", CreateVideoEncoder success";
		VALIDATE(new_encoder.get() != nullptr, null_str);

		Encoder encoder;
		encoder.encoder = new_encoder.get();
		encoder.payload_type = codec.id;
		encoder.payload_name = codec.name;
		encoder.codec_params = codec.params;
		encoder.codec = codec;
		encoders_.push_back(encoder);

		const bool did_insert =
		allocated_encoders_
			.insert(std::make_pair(video_format, std::move(new_encoder)))
			.second;
		RTC_CHECK(did_insert);
		break;
	}

	VALIDATE(encoders_.size() == 1, null_str);
	const Encoder* encoder_ptr = &encoders_[0];

	encoder_config_ = CreateVideoEncoderConfig(encoder_ptr->codec);
	RTC_DCHECK_GT(encoder_config_.number_of_streams, 0);

	webrtc::VideoSendStream::Config config(nullptr);

	stats_proxy_.reset(new webrtc::SendStatisticsProxy(webrtc::Clock::GetRealTimeClock(),
                config,
                content_type_));

	// config.encoder_settings.internal_source = false;
	// config.encoder_settings.full_overuse_time = false;
	config.encoder_settings.encoder = encoder_ptr->encoder;

	webrtc::SdpVideoFormat format(encoder_ptr->codec.name, encoder_ptr->codec.params);
	const webrtc::VideoEncoderFactory::CodecInfo info = encoder_factory_->QueryVideoEncoder(format);
	config.encoder_settings.full_overuse_time = info.is_hardware_accelerated;
	config.encoder_settings.internal_source = info.has_internal_source;
	config.encoder_settings.payload_name = encoder_ptr->codec.name;
	config.encoder_settings.payload_type = encoder_ptr->codec.id;
	RTC_LOG(LS_INFO) << "encoder_settings: " << config.encoder_settings.ToString();

	encoder_config_.encoder_specific_settings = ConfigureVideoEncoderSettings(encoder_ptr->codec);
	// encoder_config_.encoder_specific_settings

	video_stream_encoder_ =
		new webrtc::VideoStreamEncoder(webrtc::CpuInfo::DetectNumberOfCores(), stats_proxy_.get(),
                            config.encoder_settings,
                            config.pre_encode_callback,
                            std::unique_ptr<webrtc::OveruseFrameDetector>());

	int start_bitrate_bps = 300000;
	video_stream_encoder_->SetStartBitrate(start_bitrate_bps);
	bool rotation_applied = false;
	video_stream_encoder_->SetSink(this, rotation_applied);

	size_t max_packet_size = 1200;
	int rtp_history_ms = 1000;
	video_stream_encoder_->ConfigureEncoder(std::move(encoder_config_),
                                        max_packet_size,
                                        rtp_history_ms > 0);
	video_stream_encoder_->SendKeyFrame();

	RTC_LOG(LS_INFO) << "trtc_encoder::start, X";
}

void twebrtc_encoder::encode_frame(const rtc::scoped_refptr<webrtc::I420BufferInterface>& buffer)
{
	webrtc::VideoFrame frame(buffer, webrtc::kVideoRotation_0, 0);

	const uint32_t now = SDL_GetTicks();
	if (startup_verbose_ticks_ == 0) {
		startup_verbose_ticks_ = now;
		last_verbose_ticks_ = startup_verbose_ticks_;
	}

	const uint32_t interval = 10000; // 10 second
	if (now - last_verbose_ticks_ >= interval) {
		uint32_t total_second = (now - startup_verbose_ticks_) / interval;
		uint32_t elapsed_second = (now - last_verbose_ticks_) / interval;
		last_verbose_ticks_ = now;
		SDL_Log("trtc_encoder::encode_frame %s, (%ix%i), rotation: %i, max_image_length: %u, last %u[s] %i frames",
			format_elapse_hms2(total_second, false).c_str(),
			buffer->width(), buffer->height(), frame.rotation(), (uint32_t)max_image_length_,
			elapsed_second, last_capture_frames_);
		last_capture_frames_ = 0;
	}

	rtc::VideoSinkInterface<webrtc::VideoFrame>* encoder = static_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>* >(video_stream_encoder_);
	encoder->OnFrame(frame);
}

void twebrtc_encoder::encode_frame_fourcc(const uint8_t* pixel_buf, int length, int width, int height, uint32_t libyuv_fourcc)
{
	int stride_y = width;
	int stride_uv = (width + 1) / 2;
	int target_width = width;
	int target_height = height;

	// TODO(nisse): Use a pool?
	rtc::scoped_refptr<webrtc::I420Buffer> buffer = webrtc::I420Buffer::Create(
		target_width, abs(target_height), stride_y, stride_uv, stride_uv);

	libyuv::RotationMode rotation_mode = libyuv::kRotate0;

	const int conversionResult = libyuv::ConvertToI420(
		pixel_buf, length, buffer.get()->MutableDataY(),
		buffer.get()->StrideY(), buffer.get()->MutableDataU(),
		buffer.get()->StrideU(), buffer.get()->MutableDataV(),
		buffer.get()->StrideV(), 0, 0,  // No Cropping
		width, height, target_width, target_height, rotation_mode,
		libyuv_fourcc);
	if (conversionResult < 0) {
		SDL_Log("Failed to convert capture frame from type 0x%08x to I420", libyuv_fourcc);
		return;
	}
	encode_frame(buffer);
}

static int GetEncoderMinBitrateBps()
{
	const int kDefaultEncoderMinBitrateBps = 30000;
	return kDefaultEncoderMinBitrateBps;
}

void twebrtc_encoder::OnEncoderConfigurationChanged(std::vector<webrtc::VideoStream> streams, int min_transmit_bitrate_bps)
{
	encoder_min_bitrate_bps_ = std::max(streams[0].min_bitrate_bps, GetEncoderMinBitrateBps());
	encoder_max_bitrate_bps_ = 0;
	for (const auto& stream : streams) {
		encoder_max_bitrate_bps_ += stream.max_bitrate_bps;
	}

	// encoder_max_bitrate_bps_ = SDL_max(encoder_max_bitrate_bps_, 8000000); // 8M bps

	// how to calculate encoder_target_rate_bps_? reference to VideoSendStreamImpl::OnBitrateUpdated.
	encoder_target_rate_bps_ = encoder_max_bitrate_bps_;

	encoder_target_rate_bps_ = std::min(encoder_max_bitrate_bps_, encoder_target_rate_bps_);
	video_stream_encoder_->OnBitrateUpdated(encoder_target_rate_bps_, 0, 0);
}

webrtc::VideoStreamEncoder::EncodedImageCallback::Result twebrtc_encoder::OnEncodedImage(
		const webrtc::EncodedImage& encoded_image,
		const webrtc::CodecSpecificInfo* codec_specific_info,
		const webrtc::RTPFragmentationHeader* fragmentation)
{
	EncodedImageCallback::Result result(EncodedImageCallback::Result::OK);
	encoded_frames ++;

	VALIDATE(encoded_image._length > 5, null_str);
	uint8_t nal = encoded_image._buffer[4];
	uint8_t type = nal & 0x1f;

	threading::lock lock(encoded_images_mutex);
	if (type == 0x7) {
		sps_info_struct sps;
		memset(&sps, 0, sizeof(sps));
		h264_parse_sps(encoded_image._buffer + 4, encoded_image._length - 4, &sps);
		SDL_Log("#%u, OnEncodedImage, receive SPS, profile_idc: %u, level_idc: %u, %ux%ux%u, _length: %i", encoded_frames, sps.profile_idc, sps.level_idc, sps.width, sps.height, sps.fps, encoded_image._length);

		buffered_encoded_images_.clear();
	}

	if (encoded_image._length > max_image_length_) {
		max_image_length_ = encoded_image._length;
		SDL_Log("trtc_encoder::OnEncodedImage, encoded_image: %u, max_image_length: %u", encoded_image._length, (uint32_t)max_image_length_);
	}

	scoped_refptr<net::IOBufferWithSize> image = new net::IOBufferWithSize(encoded_image._length);
	memcpy(image->data(), encoded_image._buffer, encoded_image._length);

	buffered_encoded_images_.push_back(image);
/*	
	for (std::map<int, tsession>::iterator it = sessions_.begin(); it != sessions_.end(); ++ it) {
		it->second.encoded_images.push_back(image);
	}
*/
	app_encoded_image(image);
	return result;
}

twebrtc_encoder::Encoder::Encoder() = default;
twebrtc_encoder::Encoder::Encoder(const Encoder&) = default;
twebrtc_encoder::Encoder::~Encoder() = default;

std::string twebrtc_encoder::Encoder::ToString() const 
{
	std::stringstream ss;
	ss << "{encoder: " << (encoder ? "(VideoDecoder)" : "nullptr");
	ss << ", payload_type: " << payload_type;
	ss << ", payload_name: " << payload_name;
	ss << ", codec_params: {";
	for (const auto& it : codec_params)
		ss << it.first << ": " << it.second;
	ss << '}';
	ss << '}';

	return ss.str();
}

trtsp_encoder::trtsp_encoder(trtc_client& client, trtc_client::VideoRenderer& vrenderer, const std::string& codec)
	: twebrtc_encoder(codec, client.adapter_->app_get_max_bitrate_kbps(client.id_, vrenderer))
	, client_(client)
	, vrenderer_(vrenderer)
{
}

trtsp_encoder::~trtsp_encoder()
{
}

void trtsp_encoder::setup(int sessionid)
{
	VALIDATE(sessions_.count(sessionid) == 0, null_str);
	std::pair<std::map<int, tsession>::iterator, bool> ins = sessions_.insert(std::make_pair(sessionid, tsession(*this)));
	ins.first->second.encoded_images = buffered_encoded_images_;
}

uint32_t trtsp_encoder::tsession::consume(const trtc_client& client, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize)
{
	// SDL_Log("trtc_encoder::tsession::consume, inGetAuxSDPLine: %s", inGetAuxSDPLine? "true": "false");

	uint32_t images_at = 0;
	if (inGetAuxSDPLine) {
		images_at = inGetAuxSDPLine_images_at;

	} else if (inGetAuxSDPLine_images_at > 0) {
		inGetAuxSDPLine_images_at = 0;
		valid_offset = 0;
	}

	uint32_t copy_bytes = 0;
	uint32_t pos_in_to = 0;
	while (copy_bytes == 0 || !encoded_images.empty()) {
		if (client.message_handler_.send_helper.deconstructed()) {
			// vidcap has been deconstructed. exit this function, and release vidcap_mutex.
			copy_bytes = 0;
			break;
		}
		if (!encoded_images.empty()) {
			if (inGetAuxSDPLine && images_at == (int)encoded_images.size()) {
				SDL_Delay(5);
				continue;
			}

			VALIDATE(copy_bytes <= fMaxSize && pos_in_to <= fMaxSize, null_str);

			threading::lock lock(encoder.encoded_images_mutex);

			const net::IOBufferWithSize* image = encoded_images[images_at].get();
			const uint32_t encodec_image_size = image->size();
			VALIDATE(valid_offset < encodec_image_size, null_str);
			uint32_t this_copy_bytes = encodec_image_size - valid_offset;
			if (this_copy_bytes > fMaxSize - pos_in_to) {
				this_copy_bytes = fMaxSize - pos_in_to;
			}
			memcpy(fTo + pos_in_to, image->data() + valid_offset, this_copy_bytes);
			valid_offset += this_copy_bytes;
			if (valid_offset >= encodec_image_size) {
				VALIDATE(valid_offset == encodec_image_size, null_str);
				if (!inGetAuxSDPLine) {
					// delete image.data();
					encoded_images.erase(encoded_images.begin());
				} else {
					images_at ++;
				}
				valid_offset = 0;
			}
			pos_in_to += this_copy_bytes;
			copy_bytes += this_copy_bytes;
			VALIDATE(copy_bytes <= fMaxSize && pos_in_to <= fMaxSize, null_str);
			if (copy_bytes == fMaxSize) {
				break;
			}
			if (inGetAuxSDPLine && images_at == (int)encoded_images.size()) {
				break;
			}
		} else {
			SDL_Delay(5);
		}
	}

	if (inGetAuxSDPLine) {
		inGetAuxSDPLine_images_at = images_at;

	}

	return copy_bytes;
}

uint32_t trtsp_encoder::consume(int sessionid, const trtc_client& client, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize)
{
	VALIDATE(sessionid != nposm && fMaxSize > 0, null_str);
	
	// trtc_encoder::OnImageEncoded insert/erase encoded_images_map_.second, must not insert/erase encoded_images_map_.
	// so result of encoded_images_map_.find(sessionid) is safe.	
	std::map<int, tsession>::iterator map_it = sessions_.find(sessionid);
	VALIDATE(map_it != sessions_.end(), null_str);

	return map_it->second.consume(client, inGetAuxSDPLine, fTo, fMaxSize);
}

void trtsp_encoder::teardown(int sessionid)
{
	threading::lock lock(encoded_images_mutex);

	std::map<int, tsession>::iterator it = sessions_.find(sessionid);
	if (it == sessions_.end()) {
		return;
	}
	sessions_.erase(it);
}

void trtsp_encoder::app_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image)
{
	for (std::map<int, tsession>::iterator it = sessions_.begin(); it != sessions_.end(); ++ it) {
		it->second.encoded_images.push_back(image);
	}
}

//
// tmemcapture section
//
const std::string tmemcapture::mem_url("mem://127.0.0.1");

tmemcapture::tmemcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const std::vector<trtsp_settings>& rtsps, bool rtsp_exclude_local, const tpoint& desire_local_size, bool idle_screen_saver)
	: trtc_client(id, dialog, adapter, desire_local_size, false, rtsps, nposm, rtsp_exclude_local, idle_screen_saver)
	, first_h264_payload_type_(200)
{
	VALIDATE(!rtsps_.empty(), null_str);

	cricket::WebRtcVideoEncoderFactory* encoder_factory = NULL;
	cricket::WebRtcVideoDecoderFactory* decoder_factory = NULL;

#if defined(__APPLE__) && TARGET_OS_IPHONE
    encoder_factory = webrtc::CreateObjCEncoderFactory2().release();
    decoder_factory = webrtc::CreateObjCDecoderFactory2().release();
#elif defined(ANDROID)
	encoder_factory = new webrtc::jni::MediaCodecVideoEncoderFactory();
    decoder_factory = new webrtc::jni::MediaCodecVideoDecoderFactory();
#endif

	std::unique_ptr<CricketDecoderWithParams> cricket_decoder_with_params(new CricketDecoderWithParams(std::move(std::unique_ptr<cricket::WebRtcVideoDecoderFactory>(decoder_factory))));
	decoder_factory_ = cricket::ConvertVideoDecoderFactory(std::move(cricket_decoder_with_params));
	encoder_factory_ = cricket::ConvertVideoEncoderFactory(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(encoder_factory));

	caller_ = true;
	state_ = CONNECTED;
	ConnectToPeer("fake", true, null_str);
}

tmemcapture::~tmemcapture()
{
	SDL_Log("trtspcapture::~trtspcapture(), E");

	// prevent frame-thread from Send() late.
	// Although following DeletePeerConnection() will also perform clear_msg(), 
	// but immediately followed "delete stream" require, perform it here first.
	message_handler_.send_helper.clear_msg();

	for (std::vector<VideoReceiveStream*>::const_iterator it = receive_streams_.begin(); it != receive_streams_.end(); ++ it) {
		VideoReceiveStream* stream = *it;
		delete stream;
	}
	SDL_Log("trtspcapture::~trtspcapture(), X");
}

void tmemcapture::CricketDecoderWithParams::DestroyVideoDecoder(webrtc::VideoDecoder* decoder)
{
	if (external_decoder_factory_) {
		external_decoder_factory_->DestroyVideoDecoder(decoder);
	} else {
		delete decoder;
	}
}

tmemcapture::VideoReceiveStream::VideoReceiveStream(tmemcapture& capture, std::vector<RoseDecoder>& decoders, trtsp_settings& rtsp_settings, int at)
	: capture_(capture)
	, decoders_(decoders)
	, rtsp_settings_(rtsp_settings)
	, at_(at)
	, clock_(webrtc::Clock::GetRealTimeClock())
	, timing_(new webrtc::VCMTiming(clock_))
	, video_receiver_(clock_, nullptr, this, timing_.get(), this, this)
	, num_cpu_cores_(webrtc::CpuInfo::DetectNumberOfCores())
	, next_frame_ticks_(0)
	, stopped_(false)
	, try_restart_threshold_(30000)
	, next_live555_start_ticks_(0)
{
}

tmemcapture::VideoReceiveStream::~VideoReceiveStream()
{
	SDL_Log("VideoReceiveStream::~VideoReceiveStream#%i, E", at_);
	stop();
	SDL_Log("VideoReceiveStream::~VideoReceiveStream#%i, X", at_);
}

// Implements EncodedImageCallback.
webrtc::EncodedImageCallback::Result tmemcapture::VideoReceiveStream::OnEncodedImage(
	const webrtc::EncodedImage& encoded_image,
	const webrtc::CodecSpecificInfo* codec_specific_info,
	const webrtc::RTPFragmentationHeader* fragmentation)
{
	return Result(Result::OK, 0);
}

// Implements NackSender.
void tmemcapture::VideoReceiveStream::SendNack(const std::vector<uint16_t>& sequence_numbers)
{
}

// Implements KeyFrameRequestSender.
void tmemcapture::VideoReceiveStream::RequestKeyFrame()
{
}

int32_t tmemcapture::VideoReceiveStream::FrameToRender(webrtc::VideoFrame& videoFrame,  // NOLINT
                                rtc::Optional<uint8_t> qp,
                                webrtc::VideoContentType content_type)
{
	if (stopped_) {
		return 0;
	}
	capture_.vrenderer(true, at_)->OnFrame(videoFrame);
/*
	const std::string file = game_config::preferences_dir + "/google.png";
	if (game_config::os == os_windows && !SDL_IsFile(file.c_str())) {
		rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(videoFrame.video_frame_buffer()->ToI420());
		int pitch = buffer->width() * 4;
		surface surf = create_neutral_surface(buffer->width(), buffer->height());
		{
			surface_lock dst_lock(surf);
			uint8_t* pixels = (uint8_t*)dst_lock.pixels();

			libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
				buffer->DataU(), buffer->StrideU(),
				buffer->DataV(), buffer->StrideV(),
				pixels,
				pitch,
				buffer->width(), buffer->height());
		}
		imwrite(surf, "google.png");
	}
*/
	return 0;
}

void tmemcapture::start_rtsp()
{
	RTC_LOG(LS_INFO) << "trtspcapture::start_rtsp, E";

	VALIDATE(encoder_factory_.get() != nullptr && decoder_factory_.get() != nullptr, null_str);
	VALIDATE(!rtsps_.empty(), null_str);
	VALIDATE(decoders_.empty() && allocated_decoders_.empty(), null_str);
	webrtc::collect_video_decoder(*encoder_factory_.get(), *decoder_factory_.get(), first_h264_payload_type_, rtsps_.size() - 1, decoders_, allocated_decoders_);

	std::vector<std::string> device_names;
	for (std::vector<trtsp_settings>::const_iterator it = rtsps_.begin(); it != rtsps_.end(); ++ it) {
		const trtsp_settings& settings = *it;
		device_names.push_back(settings.name);
	}
	// thie session maybe use both local and remote vidcap.
	std::vector<tusing_vidcap> using_vidcaps = adapter_->app_video_capturer(id_, true, device_names);
	VALIDATE(!using_vidcaps.empty(), "app must select one video_capturer at least.");
	allocate_vrenderers(true, using_vidcaps.size());

	int at = 0;
	for (std::vector<trtsp_settings>::iterator it = rtsps_.begin(); it != rtsps_.end(); ++ it, at ++) {
		trtsp_settings& settings = *it;

		StartRemoteRenderer(nullptr, using_vidcaps[at].name, at);
		receive_streams_.push_back(new VideoReceiveStream(*this, decoders_, settings, at));
		receive_streams_.back()->start();
	}
	adapter_->app_handle_notify(id_, ncode_startlive555finished, nposm, 0, null_str);

	RTC_LOG(LS_INFO) << "trtspcapture::start_rtsp, X";
}

void tmemcapture::decode(int at, const uint8_t* data, size_t frame_size)
{
	VALIDATE(at >= 0 && at < (int)receive_streams_.size(), null_str);
	receive_streams_[at]->Decode(data, frame_size);
}

void tmemcapture::VideoReceiveStream::start()
{
	//
	// start
	// 
	RTC_LOG(LS_INFO) << "VideoReceiveStream::start, decoders_'s size: " << static_cast<int>(decoders_.size());
	for (const RoseDecoder& decoder : decoders_) {
		video_receiver_.RegisterExternalDecoder(decoder.decoder,
												decoder.payload_type);
		webrtc::VideoCodec codec = webrtc::CreateDecoderVideoCodec(decoder);

		RTC_LOG(LS_INFO) << "codec: plType: " << static_cast<int>(codec.plType) << ", plName: " << codec.plName;
		RTC_CHECK_EQ(VCM_OK, video_receiver_.RegisterReceiveCodec(
								 &codec, num_cpu_cores_, false));
	}
	video_receiver_.RegisterReceiveCallback(this);

	// Start the decode thread
	capture_.adapter_->app_handle_notify(capture_.id_, ncode_startlive555, at_, 0, rtsp_settings_.url);
	// start live555, allow start during start_live555_threshold.
	rtsp_settings_.id = at_;
	// rtsp_settings_.id = live555::start(rtsp_settings_.url, rtsp_settings_.sendrtcp, rtsp_settings_.httpdurl);
	next_live555_start_ticks_ = SDL_GetTicks() + try_restart_threshold_;

	capture_.adapter_->app_handle_notify(capture_.id_, ncode_startlive555finished, at_, rtsp_settings_.id != nposm? 1: 0, rtsp_settings_.url);
}

void tmemcapture::VideoReceiveStream::stop()
{
	stopped_ = true;
	{
		// TriggerDecoderShutdown will release any waiting decoder thread and make
		// it stop immediately, instead of waiting for a timeout. Needs to be called
		// before joining the decoder thread.
		video_receiver_.TriggerDecoderShutdown();

		threading::lock lock2(decoder_mutex_);

		// decode_thread_.Stop();
		// Deregister external decoders so they are no longer running during
		// destruction. This effectively stops the VCM since the decoder thread is
		// stopped, the VCM is deregistered and no asynchronous decoder threads are
		// running.
		for (const RoseDecoder& decoder : decoders_) {
			video_receiver_.RegisterExternalDecoder(nullptr, decoder.payload_type);
		}
	}
	if (rtsp_settings_.id != nposm) {
		// live555::stop(rtsp_settings_.id);
	}
	capture_.StopRemoteRenderer(at_);
}

bool tmemcapture::VideoReceiveStream::Decode(const uint8_t* data, size_t frame_size)
{
	threading::lock lock2(decoder_mutex_);
	if (stopped_) {
		return false;
	}

	uint8_t header[4] = {0x00, 0x00, 0x00, 0x01};
	const uint8_t* buf = data + sizeof(header);
    uint8_t nal  = buf[0];
    uint8_t type = nal & 0x1f;

	std::unique_ptr<webrtc::video_coding::RtspFrameObject> frame;
	if (type <= 23) {
		if (type == 7 && rtsp_settings_.width == nposm) {
			// buf[3] = 32;
			sps_info_struct sps;
			memset(&sps, 0, sizeof(sps));
			h264_parse_sps(buf, frame_size, &sps);
			rtsp_settings_.width = sps.width;
			rtsp_settings_.height = sps.height;
			SDL_Log("rtsp's SPS, profile_idc: %u(0x%02x), profile_iop: %u(0x%02x), level_idc(0x%02x): %u, %ux%ux%u", 
				sps.profile_idc, sps.profile_idc, 
				sps.profile_iop, sps.profile_iop,
				sps.level_idc, sps.level_idc, sps.width, sps.height, sps.fps);
			VALIDATE(rtsp_settings_.width > 0 && rtsp_settings_.height > 0, null_str);
		}
		if (type == 7 || type == 1 || type == 5) {
			if (rtsp_settings_.width != nposm) {
				VALIDATE(rtsp_settings_.width > 0 && rtsp_settings_.height > 0, null_str);
				frame.reset(new webrtc::video_coding::RtspFrameObject(data, frame_size, rtsp_settings_.width, rtsp_settings_.height, capture_.first_h264_payload_type_ + at_, 0));
			} else {
				// fault. but maybe enter it.
				VALIDATE(rtsp_settings_.height == nposm, null_str);
			}

		}
	}

	int decode_result = WEBRTC_VIDEO_CODEC_ERROR;
	if (frame.get() != nullptr) {
		decode_result = video_receiver_.Decode(frame.get());
		// WEBRTC_VIDEO_CODEC_OK, WEBRTC_VIDEO_CODEC_OK_REQUEST_KEYFRAME
	}
	return decode_result == WEBRTC_VIDEO_CODEC_OK;;
}