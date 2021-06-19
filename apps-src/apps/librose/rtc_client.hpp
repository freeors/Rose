/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef LIBROSE_RTC_CLIENT_H_INCLUDED
#define LIBROSE_RTC_CLIENT_H_INCLUDED


#include "util.hpp"
#include "thread.hpp"
#include "live555d.hpp"

#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/signalthread.h"
#include "rtc_base/sigslot.h"

#include "sdl_utils.hpp"
#include "gui/dialogs/dialog.hpp"

#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "media/engine/webrtcvideoengine.h"

#include "modules/video_coding/video_coding_impl.h"
#include "system_wrappers/include/clock.h"
#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/frame_buffer2.h"
#include "video/send_statistics_proxy.h"
#include "video/video_stream_encoder.h"

// #include "libyuv/video_common.h"

#include <net/base/io_buffer.h>

class trtsp_encoder;

class DummySetSessionDescriptionObserver
	: public webrtc::SetSessionDescriptionObserver {
public:
	static DummySetSessionDescriptionObserver* Create() {
		return
			new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
	}
	virtual void OnSuccess() {
		RTC_LOG(INFO) << __FUNCTION__;
	}
	virtual void OnFailure(const std::string& error) {
		RTC_LOG(INFO) << __FUNCTION__ << " " << error;
	}

protected:
	DummySetSessionDescriptionObserver() {}
	~DummySetSessionDescriptionObserver() {}
};

class trtc_client: public webrtc::PeerConnectionObserver, public webrtc::CreateSessionDescriptionObserver,
	public sigslot::has_slots<>, public live555d::tvidcap, public gui2::tdialog::ttexture_holder
{
	friend class trtsp_encoder;
public:
	enum {ncode_startlive555, ncode_startlive555finished};

	struct tusing_vidcap {
		tusing_vidcap(const std::string& name, bool encode)
			: name(name)
			, encode(encode)
		{}
		const std::string name; // must equal to item of device_names.
		const bool encode;
	};

	class VideoRenderer: public rtc::VideoSinkInterface<webrtc::VideoFrame>
	{
		friend trtc_client;
	public:
		explicit VideoRenderer(trtc_client& client, webrtc::VideoTrackInterface* track_to_render, const std::string& name, bool remote, int at, bool encode);
		virtual ~VideoRenderer();

		void set_app_size(int width, int height) 
		{
			app_width_ = width;
			app_height_ = height;
		}
		// VideoSinkInterface implementation
		void OnFrame(const webrtc::VideoFrame& frame) override;
		int rtsp_sessions() const;

		void clear_new_frame() 
		{ 
			new_frame = false;
			frame_thread_new_frame_ = false;
		}
		bool texture_allocated() const { return app_width_ != nposm && app_height_ != nposm; }
		void set_sample_interval(int interval)
		{
			VALIDATE(!requrie_encode && interval > 0, null_str);
			sample_interval_ = interval;
		}

	private:
		void clear_texture();

	public:
		std::string name;
		const bool requrie_encode;
		int app_width_;
		int app_height_;

		std::unique_ptr<trtsp_encoder> encoder_;
		texture tex_;
		texture cv_tex_;
		uint8_t* pixels_;
		texture app_tex_;

		// app will use below variable, in order to avoid sync, these quire evaluate in main thread.
		bool new_frame;
		// frames must <= frame_thread_frames_.
		int frames;
		// >0: when create
		// >0: receive mew frame, update it.
		uint32_t last_frame_ticks;
		bool hflip_;

	private:
		trtc_client& client_;
		const bool remote_;
		const int at_;
		int bytesperpixel_;
		rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
		bool frame_thread_new_frame_;
		int frame_thread_frames_;
		int sample_interval_;

		uint32_t startup_verbose_ms_;
		uint32_t last_verbose_ms_;
		int last_verbose_frames_;
	};
	VideoRenderer* vrenderer(bool remote, int at) const { return remote? remote_vrenderer_[at]: local_vrenderer_[at]; } 
	int using_vidcap_cout() const { return local_vrenderer_count_ + remote_vrenderer_count_; }

	void allocate_vrenderers(bool remote, int count);
	void free_vrenderers(bool remote);

	int id() const { return id_; }

	class tadapter
	{
	public:
		// name of tusing_vidcap must equal to item of device_names.
		virtual std::vector<tusing_vidcap> app_video_capturer(int id, bool remote, const std::vector<std::string>& device_names);
		virtual void app_handle_notify(int id, int ncode, int at, int64_t i64_value, const std::string& str_value);

		// -1 indicate caulcate by webrtc's GetMaxDefaultVideoBitrateKbps(...)
		// reference to EncoderStreamFactory::CreateEncoderStreams in <webrtc>/media/engine/webrtcvideoengine.cc
		// return value:
		//   value is kbps, for example, 4000 indicate 4Mbps.
		virtual int app_get_max_bitrate_kbps(int id, VideoRenderer& vrenderer) const { return -1; }

		virtual VideoRenderer* app_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track_to_render, const std::string& name, bool remote, int at, bool encode);

		virtual void did_draw_slice(int id, SDL_Renderer* renderer, VideoRenderer** locals, int locals_count, VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect);
	};

	static const uint32_t start_live555_threshold;
	static std::string ice_connection_state_str(webrtc::PeerConnectionInterface::IceConnectionState state);
	static std::string ice_gathering_state_str(webrtc::PeerConnectionInterface::IceGatheringState state);
	static std::string webrtc_set_preferred_codec(const std::string& sdp, const std::string& codec);

	trtc_client(int id, gui2::tdialog& dialog, tadapter& adapter, const tpoint& desire_size, bool local_only, const std::vector<trtsp_settings>& rtsps, int desire_fps, bool rtsp_exclude_local, bool idle_screen_saver);
	virtual ~trtc_client();

	void set_adapter(tadapter& adapter) { adapter_ = &adapter; }
	bool rtsp() const { return !rtsps_.empty(); }
	const std::vector<trtsp_settings>& rtsp_settings() const { return rtsps_; }
	const trtsp_settings& rtsp_settings(int at) const { return rtsps_[at]; }


	threading::mutex& get_sink_mutex(bool remote) { return remote? remote_sink_mutex_: local_sink_mutex_; }
	void draw_slice(SDL_Renderer* renderer, const SDL_Rect& draw_rect);

	bool can_safe_stop() const;

protected:
	void allocate_texture(bool remote, int at, int width, int height);
	void clear_texture() override;

	bool InitializePeerConnection();

	void ConnectToPeer(const std::string& peer_nick, bool caller, const std::string& offer);
	bool CreatePeerConnection(bool dtls);
	void DeletePeerConnection();
	void AddStreams();

	void StartLocalRenderer(webrtc::VideoTrackInterface* local_video, const std::string& name, int at, bool encode);
	void StopLocalRenderer();
	void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video, const std::string& name, int at);
	void StopRemoteRenderer(int at);

	// CreateSessionDescriptionObserver implementation.
	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
	void OnFailure(const std::string& error) override;

	// socket
	void Connect(const std::string& server, int port);

	void DoConnect();
	void Close();
	void InitSocketSignals();
	bool ConnectControlSocket();
	void OnConnect(rtc::AsyncSocket* socket);
	void msg_2_signaling_server(rtc::AsyncSocket* socket, const std::string& msg);

	// Returns true if the whole response has been read.
	const char* read_2_buffer(rtc::AsyncSocket* socket, bool first, size_t* content_length);

	virtual void OnRead(rtc::AsyncSocket* socket) {}
	void OnClose(rtc::AsyncSocket* socket, int err);

	void OnResolveResult(rtc::AsyncResolverInterface* resolver);

	virtual void OnServerConnectionFailure();

	void resize_recv_data(int size);
	void resize_send_data(int size);

	enum {MSG_SIGNALING_CLOSE = gui2::tdialog::POST_MSG_MIN_APP};
	bool chat_OnMessage(rtc::Message* msg);

	// 
	// live555d::tvidcap implementation.
	//
	void setup(int sessionid) override;
	uint32_t read(int sessionid, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize) override;
	void teardown(int sessionid) override;

private:
	//
	// RefCountInterface implementation. CreateSessionDescriptionObserver derived from it.
	//
	void AddRef() const override { ref_count_.IncRef(); }
	rtc::RefCountReleaseStatus Release() const override
	{
		const auto status = ref_count_.DecRef();
		if (status == rtc::RefCountReleaseStatus::kDroppedLastRef) {
			delete this;
		}
		return status;
	}

	//
	// PeerConnectionObserver implementation.
	//
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override; // {}
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override; // {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceConnectionReceivingChange(bool receiving) override; // {}

	// thread context: main thread
	virtual bool app_relay_only() { return false; }

	// thread context: _signalingThread
	virtual void app_OnIceConnectionChange() {}
	virtual void app_OnIceGatheringChange() {}

	virtual void start_rtsp() {}

protected:
	const int id_;
	gui2::tdialog& dialog_;
	tpoint desire_size_;
	int desire_fps_;
	bool local_only_;
	std::vector<trtsp_settings> rtsps_;
	const bool rtsp_exclude_local_;
	bool relay_only_;
	tadapter* adapter_;

	bool caller_;
	//
	// webrtc
	//
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
	// PeerConnectionClient* client_;
	std::deque<std::string*> pending_messages_;
	std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> > active_streams_;
	std::string server_;

	enum State {
		NOT_CONNECTED,
		RESOLVING,
		SIGNING_IN,
		CONNECTED,
		SIGNING_OUT_WAITING,
		SIGNING_OUT,
	};

	rtc::SocketAddress server_address_;
	rtc::AsyncResolver* resolver_;
	std::unique_ptr<rtc::AsyncSocket> control_socket_;
	std::string onconnect_data_;
	std::string notification_data_;
	std::string my_nick_;
	std::string peer_nick_;
	State state_;
	int my_id_;

	std::unique_ptr<rtc::Thread> _networkThread;
	std::unique_ptr<rtc::Thread> _workerThread;
	std::unique_ptr<rtc::Thread> _signalingThread;

	const bool idle_screen_saver_;
	std::unique_ptr<tdisable_idle_lock> disable_idle_lock_;
	std::string preferred_codec_;

	webrtc::PeerConnectionInterface::IceConnectionState connection_state_;
	webrtc::PeerConnectionInterface::IceGatheringState gathering_state_;

	threading::mutex remote_sink_mutex_;
	threading::mutex local_sink_mutex_;

	char* send_data_;
	int send_data_size_;
	int send_data_vsize_;

	int last_msg_should_size_;

	char* recv_data_;
	int recv_data_size_;
	int recv_data_vsize_;

	mutable webrtc::webrtc_impl::RefCounter ref_count_{0};

	VideoRenderer** local_vrenderer_;
	int local_vrenderer_count_;
	VideoRenderer** remote_vrenderer_;
	int remote_vrenderer_count_;

	std::vector<std::string> existed_vidcaps_;

	class tmessage_handler: public rtc::MessageHandler
	{
	public:
		tmessage_handler(trtc_client& client)
			: send_helper(this)
			, client_(client)
		{}

		enum {POST_MSG_FIRSTFRAME};
		struct tmsg_data_firstframe: public rtc::MessageData {
			explicit tmsg_data_firstframe(bool remote, int at, int width, int height)
				: remote(remote)
				, at(at)
				, width(width)
				, height(height)
			{}

			bool remote;
			int at;
			int width;
			int height;
		};
		void OnMessage(rtc::Message* msg) override
		{
			switch (msg->message_id) {
			case POST_MSG_FIRSTFRAME:
				{
					tmsg_data_firstframe* pdata = static_cast<tmsg_data_firstframe*>(msg->pdata);
					client_.allocate_texture(pdata->remote, pdata->at, pdata->width, pdata->height);
				}
				break;
			}

			if (msg->pdata) {
				delete msg->pdata;
				msg->pdata = nullptr;
			}
		}

	public:
		twebrtc_send_helper send_helper;

	private:
		trtc_client& client_;
	};
	tmessage_handler message_handler_;
};

class tavcapture: public trtc_client
{
public:
	tavcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const tpoint& desire_size = tpoint(nposm, nposm), const int desire_fps = nposm, bool idle_screen_saver = true);
};

//
//  H264ParseSPS.h(https://blog.csdn.net/lizhijian21/article/details/80982403)
//
//  Created by lzj<lizhijian_21@163.com> on 2018/7/6.
//  Copyright @ 2018 LZJ. All rights reserved.
//
     
typedef struct
{
    unsigned int profile_idc;
	unsigned int profile_iop;
    unsigned int level_idc;
    
    unsigned int width;
    unsigned int height;
    unsigned int fps;       // SPS maybe no FPS
} sps_info_struct;
 
    
/**
 pase one SPS bitstream
 @param data SPS bitstream, require NAL prefix with type = 0x7(for exmaple: 67 42 00 28 ab 40 22 01 e3 cb cd c0 80 80 a9 02)
 @param dataSize of SPS bitstream.
 @param info SPS result of this SPS bitstream.
 @return success:1£¬fail:0
 */
int h264_parse_sps(const unsigned char *data, unsigned int dataSize, sps_info_struct *info);
 
// <<< H264ParseSPS_h

struct RoseDecoder
{
	RoseDecoder();
	RoseDecoder(const RoseDecoder&);
	~RoseDecoder();
	std::string ToString() const;

	// The actual decoder instance.
	webrtc::VideoDecoder* decoder = nullptr;

	// Received RTP packets with this payload type will be sent to this decoder
	// instance.
	int payload_type = 0;

	// Name of the decoded payload (such as VP8). Maps back to the depacketizer
	// used to unpack incoming packets.
	std::string payload_name;

	// This map contains the codec specific parameters from SDP, i.e. the "fmtp"
	// parameters. It is the same as cricket::CodecParameterMap used in
	// cricket::VideoCodec.
	std::map<std::string, std::string> codec_params;
};

namespace webrtc {

struct SdpVideoFormatCompare
{
    bool operator()(const webrtc::SdpVideoFormat& lhs,
                    const webrtc::SdpVideoFormat& rhs) const {
    return std::tie(lhs.name, lhs.parameters) <
            std::tie(rhs.name, rhs.parameters);
    }
};
typedef std::map<webrtc::SdpVideoFormat,
                    std::unique_ptr<webrtc::VideoDecoder>,
                    SdpVideoFormatCompare>
	DecoderMap;

typedef std::map<webrtc::SdpVideoFormat,
                    std::unique_ptr<webrtc::VideoEncoder>,
                    SdpVideoFormatCompare>
    EncoderMap;
}

class trtspcapture: public trtc_client
{
public:
	trtspcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const std::vector<trtsp_settings>& rtsps, bool rtsp_exclude_local, const tpoint& desire_local_size, bool idle_screen_saver = true);
	~trtspcapture();

	void start_rtsp() override;
	void stop_rtsp();

private:
	// WebRtcVideoDecoderFactory implementation that allows to override
	// |receive_stream_id|.
	class CricketDecoderWithParams : public cricket::WebRtcVideoDecoderFactory {
	public:
		explicit CricketDecoderWithParams(
			std::unique_ptr<cricket::WebRtcVideoDecoderFactory> external_decoder_factory)
			: external_decoder_factory_(std::move(external_decoder_factory)) {}

		void SetReceiveStreamId(const std::string& receive_stream_id) {
			receive_stream_id_ = receive_stream_id;
		}

	private:
		webrtc::VideoDecoder* CreateVideoDecoderWithParams(
			const cricket::VideoCodec& codec,
			cricket::VideoDecoderParams params) override {
			if (!external_decoder_factory_)
				return nullptr;
			params.receive_stream_id = receive_stream_id_;
			return external_decoder_factory_->CreateVideoDecoderWithParams(codec,
																			params);
		}

		webrtc::VideoDecoder* CreateVideoDecoderWithParams(
			webrtc::VideoCodecType type,
			cricket::VideoDecoderParams params) override {
			RTC_NOTREACHED();
			return nullptr;
		}

		void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

		const std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory_;
		std::string receive_stream_id_;
	};

	// If |cricket_decoder_with_params_| is non-null, it's owned by
	// |decoder_factory_|.
	// CricketDecoderWithParams* const cricket_decoder_with_params_;

	std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory_;
	// const std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;
	std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;

	uint8_t first_h264_payload_type_;

	// Decoders for every payload that we can receive.
    std::vector<RoseDecoder> decoders_;

	webrtc::DecoderMap allocated_decoders_;

	class VideoReceiveStream: public webrtc::EncodedImageCallback
		, public webrtc::NackSender
		, public webrtc::KeyFrameRequestSender
		, public webrtc::VCMReceiveCallback
		, public rtc::MessageHandler
	{
	public:
		VideoReceiveStream(trtspcapture& capture, std::vector<RoseDecoder>& decoders, trtsp_settings& rtsp_settings, int at);
		~VideoReceiveStream();

		void start();
		void stop();

	private:
		static void DecodeThreadFunction(void* ptr);
		bool Decode();

		void resize_buffer(int size, int vsize);
		webrtc::video_coding::FrameBuffer::ReturnReason NextFrame(std::unique_ptr<webrtc::video_coding::FrameObject>* frame_out);
		void live555_start();
		void live555_restart();

	private:
		// Implements EncodedImageCallback.
		webrtc::EncodedImageCallback::Result OnEncodedImage(
			const webrtc::EncodedImage& encoded_image,
			const webrtc::CodecSpecificInfo* codec_specific_info,
			const webrtc::RTPFragmentationHeader* fragmentation) override;

		// Implements NackSender.
		void SendNack(const std::vector<uint16_t>& sequence_numbers) override;

		// Implements KeyFrameRequestSender.
		void RequestKeyFrame() override;

		// Implements VCMReceiveCallback.
		int32_t FrameToRender(webrtc::VideoFrame& videoFrame,  // NOLINT
                                rtc::Optional<uint8_t> qp,
                                webrtc::VideoContentType content_type) override;

		enum {POST_MSG_STARTLIVE555, POST_MSG_RESTARTLIVE555};
		void OnMessage(rtc::Message* msg) override
		{
			switch (msg->message_id) {
			case POST_MSG_STARTLIVE555:
				{
					live555_start();
				}
				break;
			case POST_MSG_RESTARTLIVE555:
				{
					live555_restart();
				}
				break;
			}

			if (msg->pdata) {
				delete msg->pdata;
				msg->pdata = nullptr;
			}
		}

	private:
		trtspcapture& capture_;
		std::vector<RoseDecoder>& decoders_;
		trtsp_settings& rtsp_settings_;
		const int at_;
		twebrtc_send_helper send_helper_;
		const int num_cpu_cores_;
		// webrtc::ProcessThread* const process_thread_;
		webrtc::Clock* const clock_;
		rtc::PlatformThread decode_thread_;
		std::unique_ptr<webrtc::VCMTiming> timing_;  // Jitter buffer experiment.
		webrtc::vcm::VideoReceiver video_receiver_;

		bool stopped_;

		// use live555's buffer
		std::unique_ptr<uint8_t[]> live555_buffer_;
		int buffer_size_;
		int buffer_vsize_;

		// use local file
		uint32_t next_frame_ticks_;
		
		int try_restart_threshold_;
		uint32_t next_live555_start_ticks_;
	};

	std::vector<VideoReceiveStream*> receive_streams_;
};

// user guide: http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=377&extra=page%3D1
class twebrtc_encoder: public webrtc::VideoStreamEncoder::EncoderSink
{
	friend class webrtc::VideoStreamEncoder;

public:
	std::vector<scoped_refptr<net::IOBufferWithSize> > buffered_encoded_images_;
	threading::mutex encoded_images_mutex;

	struct Encoder {
	Encoder();
		Encoder(const Encoder&);
		~Encoder();
		std::string ToString() const;

		// The actual decoder instance.
		webrtc::VideoEncoder* encoder = nullptr;

		// Received RTP packets with this payload type will be sent to this decoder
		// instance.
		int payload_type = 0;

		// Name of the decoded payload (such as VP8). Maps back to the depacketizer
		// used to unpack incoming packets.
		std::string payload_name;

		// This map contains the codec specific parameters from SDP, i.e. the "fmtp"
		// parameters. It is the same as cricket::CodecParameterMap used in
		// cricket::VideoCodec.
		std::map<std::string, std::string> codec_params;

		cricket::VideoCodec codec;
	};
	twebrtc_encoder(const std::string& codec, int max_bitrate_kbps);
	~twebrtc_encoder();

	void start();
	void encode_frame(const rtc::scoped_refptr<webrtc::I420BufferInterface>& buffer);
	void encode_frame_fourcc(const uint8_t* pixel_buf, int length, int width, int height, uint32_t libyuv_fourcc);

private:
	void OnEncoderConfigurationChanged(std::vector<webrtc::VideoStream> streams, int min_transmit_bitrate_bps) override;
	EncodedImageCallback::Result OnEncodedImage(
		const webrtc::EncodedImage& encoded_image,
		const webrtc::CodecSpecificInfo* codec_specific_info,
		const webrtc::RTPFragmentationHeader* fragmentation) override;

	webrtc::VideoEncoderConfig CreateVideoEncoderConfig(const cricket::VideoCodec& codec) const;

	virtual void app_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image) {}

public:
	int encoded_frames;

protected:
	const std::string desire_codec_;
	const int max_bitrate_kbps_;
	std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;
	std::vector<cricket::VideoCodec> recv_codecs_;

	// Decoders for every payload that we can receive.
    std::vector<Encoder> encoders_;

	webrtc::EncoderMap allocated_encoders_;

	std::unique_ptr<webrtc::SendStatisticsProxy> stats_proxy_;
	const webrtc::VideoEncoderConfig::ContentType content_type_;
	webrtc::VideoEncoderConfig encoder_config_;
	webrtc::VideoStreamEncoder* video_stream_encoder_;

	int encoder_min_bitrate_bps_;
	uint32_t encoder_max_bitrate_bps_;
	uint32_t encoder_target_rate_bps_;

	size_t max_image_length_;
	uint32_t startup_verbose_ticks_;
	uint32_t last_verbose_ticks_;
	uint32_t last_capture_frames_;
};

class trtsp_encoder: public twebrtc_encoder
{
public:
	struct tsession {
		friend class trtc_client;
		tsession(trtsp_encoder& encoder)
			: encoder(encoder)
			, valid_offset(0)
			, inGetAuxSDPLine_images_at(0)
		{}
		uint32_t consume(const trtc_client& client, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize);

		trtsp_encoder& encoder;
		std::vector<scoped_refptr<net::IOBufferWithSize> > encoded_images;
		uint32_t valid_offset;
		uint32_t inGetAuxSDPLine_images_at;
	};
	std::map<int, tsession> sessions_;

	trtsp_encoder(trtc_client& client, trtc_client::VideoRenderer& vrenderer, const std::string& codec);
	~trtsp_encoder();

	void setup(int sessionid);
	uint32_t consume(int sessionid, const trtc_client& client, bool inGetAuxSDPLine, unsigned char* fTo, uint32_t fMaxSize);
	void teardown(int sessionid);

private:
	void app_encoded_image(const scoped_refptr<net::IOBufferWithSize>& image) override;

private:
	trtc_client& client_;
	trtc_client::VideoRenderer& vrenderer_;
};

class tmemcapture: public trtc_client
{
public:
	static const std::string mem_url;
	// rtsp://192.168.1.180:8554/0+
	tmemcapture(int id, gui2::tdialog& dialog, tadapter& adapter, const std::vector<trtsp_settings>& rtsps, bool rtsp_exclude_local, const tpoint& desire_local_size, bool idle_screen_saver = true);
	~tmemcapture();

	void start_rtsp() override;
	// void stop_rtsp();

	void decode(int at, const uint8_t* data, size_t frame_size);

private:
	// WebRtcVideoDecoderFactory implementation that allows to override
	// |receive_stream_id|.
	class CricketDecoderWithParams : public cricket::WebRtcVideoDecoderFactory {
	public:
		explicit CricketDecoderWithParams(
			std::unique_ptr<cricket::WebRtcVideoDecoderFactory> external_decoder_factory)
			: external_decoder_factory_(std::move(external_decoder_factory)) {}

		void SetReceiveStreamId(const std::string& receive_stream_id) {
			receive_stream_id_ = receive_stream_id;
		}

	private:
		webrtc::VideoDecoder* CreateVideoDecoderWithParams(
			const cricket::VideoCodec& codec,
			cricket::VideoDecoderParams params) override {
			if (!external_decoder_factory_)
				return nullptr;
			params.receive_stream_id = receive_stream_id_;
			return external_decoder_factory_->CreateVideoDecoderWithParams(codec,
																			params);
		}

		webrtc::VideoDecoder* CreateVideoDecoderWithParams(
			webrtc::VideoCodecType type,
			cricket::VideoDecoderParams params) override {
			RTC_NOTREACHED();
			return nullptr;
		}

		void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

		const std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory_;
		std::string receive_stream_id_;
	};

	// If |cricket_decoder_with_params_| is non-null, it's owned by
	// |decoder_factory_|.
	// CricketDecoderWithParams* const cricket_decoder_with_params_;

	std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory_;
	// const std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;
	std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory_;

	std::vector<cricket::VideoCodec> recv_codecs_;
	uint8_t first_h264_payload_type_;

	// Decoders for every payload that we can receive.
    std::vector<RoseDecoder> decoders_;

	webrtc::DecoderMap allocated_decoders_;

	class VideoReceiveStream: public webrtc::EncodedImageCallback
		, public webrtc::NackSender
		, public webrtc::KeyFrameRequestSender
		, public webrtc::VCMReceiveCallback
	{
	public:
		VideoReceiveStream(tmemcapture& capture, std::vector<RoseDecoder>& decoders, trtsp_settings& rtsp_settings, int at);
		~VideoReceiveStream();

		void start();
		void stop();

	// private:
		bool Decode(const uint8_t* data, size_t len);

	private:
		// Implements EncodedImageCallback.
		webrtc::EncodedImageCallback::Result OnEncodedImage(
			const webrtc::EncodedImage& encoded_image,
			const webrtc::CodecSpecificInfo* codec_specific_info,
			const webrtc::RTPFragmentationHeader* fragmentation) override;

		// Implements NackSender.
		void SendNack(const std::vector<uint16_t>& sequence_numbers) override;

		// Implements KeyFrameRequestSender.
		void RequestKeyFrame() override;

		// Implements VCMReceiveCallback.
		int32_t FrameToRender(webrtc::VideoFrame& videoFrame,  // NOLINT
                                rtc::Optional<uint8_t> qp,
                                webrtc::VideoContentType content_type) override;

	private:
		tmemcapture& capture_;
		std::vector<RoseDecoder>& decoders_;
		trtsp_settings& rtsp_settings_;
		const int at_;
		const int num_cpu_cores_;
		// webrtc::ProcessThread* const process_thread_;
		webrtc::Clock* const clock_;
		std::unique_ptr<webrtc::VCMTiming> timing_;  // Jitter buffer experiment.
		webrtc::vcm::VideoReceiver video_receiver_;

		bool stopped_;
		threading::mutex decoder_mutex_;


		// use local file
		uint32_t next_frame_ticks_;
		
		int try_restart_threshold_;
		uint32_t next_live555_start_ticks_;
	};

	std::vector<VideoReceiveStream*> receive_streams_;
};

#endif // LIBROSE_RTC_CLIENT_H_INCLUDED
