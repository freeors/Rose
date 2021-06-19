/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2018, Live Networks, Inc.  All rights reserved
// LIVE555 Media Server
// main program

#include <BasicUsageEnvironment/include/BasicUsageEnvironment.hh>
#include "DynamicRTSPServer.hh"
#include "version.hh"
#include "base_instance.hpp"

#include "live555d.hpp"
#include "base/logging.h"
#include "base/bind.h"

#include <groupsock/include/GroupsockHelper.hh>
#include "wml_exception.hpp"
using namespace std::placeholders;

uint32_t get_local_ipaddr()
{
	if (ReceivingInterfaceAddr != INADDR_ANY) {
		return (uint32_t)ReceivingInterfaceAddr;
	}

	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler, true, nposm, false);

	netAddressBits addr = ourIPAddress(*env);
	bool ret = env->reclaim();
	VALIDATE(ret, null_str);
	delete scheduler;

	return (uint32_t)addr;
}

bool fls_use_chromium = true;
#ifdef _WIN32
bool fls_verbos = false;
#else
bool fls_verbos = false;
#endif

namespace live555d {

BasicTaskScheduler* fls_scheduler = nullptr;
UsageEnvironment* fls_env = nullptr;
RTSPServer* fls_rtspServer = nullptr;

tvidcap* tmanager::vidcap = nullptr;
threading::mutex tmanager::vidcap_mutex;
std::unique_ptr<rtc::ThreadChecker> tmanager::live555d_thread_checker;

tmanager::tmanager()
	: tserver_(server_rtspd)
	, eventLoopWatchVariable_(0)
	, e_(false, false)
{}

void tmanager::did_set_event()
{
	e_.Set();
}

void tmanager::start_internal(uint32_t ipaddr)
{
	live555d_thread_checker.reset(new rtc::ThreadChecker);

	std::unique_ptr<tauto_destruct_executor> destruct_executor(new tauto_destruct_executor(std::bind(&tmanager::did_set_event, this)));

	// Begin by setting up our usage environment:
	BasicTaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler, true, ENVIR_SERVER_ID, false);
	fls_scheduler = scheduler;
	fls_env = env;

	UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
	// To implement client access control to the RTSP server, do the following:
	authDB = new UserAuthenticationDatabase;
	authDB->addUserRecord("username1", "password1"); // replace these with real strings
	// Repeat the above with each <username>, <password> that you wish to allow
	// access to the server.
#endif

	const bool use_chromium = fls_use_chromium;
	// Create the RTSP server.  Try first with the default port number (554),
	// and then with the alternative port number (8554):
	RTSPServer* rtspServer = nullptr;
	portNumBits rtspServerPortNum = 554;

	rtspServerPortNum = 8554;
	rtspServer = DynamicRTSPServer::createNew(*env, rtspServerPortNum, authDB, use_chromium);


	if (rtspServer == NULL) {
		LOG(INFO) << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		return;
	}
	fls_rtspServer = rtspServer;

	LOG(INFO) << "LIVE555 Media Server";
	LOG(INFO) << "\tversion " << MEDIA_SERVER_VERSION_STRING
		<< " (LIVE555 Streaming Media library version "
		<< LIVEMEDIA_LIBRARY_VERSION_STRING << ").";

	char* urlPrefix = rtspServer->rtspURLPrefix();
	server_url_ = urlPrefix;
	LOG(INFO) << "Play streams from this server using the URL";
	LOG(INFO) << "\t" << urlPrefix << "<filename>\nwhere <filename> is a file present in the current directory.";
	LOG(INFO) << "Each file's type is inferred from its name suffix:";
	LOG(INFO) << "\t\".264\" => a H.264 Video Elementary Stream file";
	LOG(INFO) << "\t\".265\" => a H.265 Video Elementary Stream file";
	LOG(INFO) << "\t\".aac\" => an AAC Audio (ADTS format) file";
	LOG(INFO) << "\t\".ac3\" => an AC-3 Audio file";
	LOG(INFO) << "\t\".amr\" => an AMR Audio file";
	LOG(INFO) << "\t\".dv\" => a DV Video file";
	LOG(INFO) << "\t\".m4e\" => a MPEG-4 Video Elementary Stream file";
	LOG(INFO) << "\t\".mkv\" => a Matroska audio+video+(optional)subtitles file";
	LOG(INFO) << "\t\".mp3\" => a MPEG-1 or 2 Audio file";
	LOG(INFO) << "\t\".mpg\" => a MPEG-1 or 2 Program Stream (audio+video) file";
	LOG(INFO) << "\t\".ogg\" or \".ogv\" or \".opus\" => an Ogg audio and/or video file";
	LOG(INFO) << "\t\".ts\" => a MPEG Transport Stream file";
	LOG(INFO) << "\t\t(a \".tsx\" index file - if present - provides server 'trick play' support)";
	LOG(INFO) << "\t\".vob\" => a VOB (MPEG-2 video with AC-3 audio) file";
	LOG(INFO) << "\t\".wav\" => a WAV Audio file";
	LOG(INFO) << "\t\".webm\" => a WebM audio(Vorbis)+video(VP8) file";
	LOG(INFO) << "See http://www.live555.com/mediaServer/ for additional documentation.";

	// Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
	// Try first with the default HTTP port (80), and then with the alternative HTTP
	// port numbers (8000 and 8080).
/*
	if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
		LOG(INFO) << "(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling, or for HTTP live streaming (for indexed Transport Stream files only).)";
	} else {
		LOG(INFO) << "(RTSP-over-HTTP tunneling is not available.)";
	}
*/
	if (use_chromium) {
		rtspServer->setup_chromium(ipaddr);

	}
	destruct_executor.reset();
	if (!use_chromium) {
		env->taskScheduler().doEventLoop(&eventLoopWatchVariable_); // does not return
	}
}

void tmanager::stop_internal()
{
	tauto_destruct_executor destruct_executor(std::bind(&tmanager::did_set_event, this));
	VALIDATE(fls_scheduler != nullptr && fls_env != nullptr, null_str);

	if (fls_rtspServer) {
		Medium::close(fls_rtspServer);
		fls_rtspServer = nullptr;
	}

	bool ret = fls_env->reclaim();
	if (!ret) {
		MediaLookupTable* table = MediaLookupTable::ourMedia(*fls_env);
		table->dump();
	}
	VALIDATE(ret, null_str);
	delete fls_scheduler;
	fls_env = nullptr;
	fls_scheduler = nullptr;

	// new and delte delegate_ must be in same thread.
}

void tmanager::start(uint32_t ipaddr)
{
	CHECK(thread_.get() == nullptr && eventLoopWatchVariable_ == 0);
	base::Thread* thread = new base::Thread("Live555dThread");

	base::Thread::Options socket_thread_options;

	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread->StartWithOptions(socket_thread_options));

	thread->task_runner()->PostTask(FROM_HERE, base::BindOnce(&tmanager::start_internal, base::Unretained(this), ipaddr));
	e_.Wait(rtc::Event::kForever);
	thread_.reset(thread);
}

void tmanager::stop()
{
	CHECK(eventLoopWatchVariable_ == 0);
	if (thread_.get() == nullptr) {
		return;
	}
	CHECK(eventLoopWatchVariable_ == 0);
	eventLoopWatchVariable_ = 1;

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&tmanager::stop_internal, base::Unretained(this)));
	e_.Wait(rtc::Event::kForever);

	thread_.reset();
	eventLoopWatchVariable_ = 0;
	server_url_.clear();
}

void register_vidcap(tvidcap& _vidcap)
{
	threading::lock lock(tmanager::vidcap_mutex);
	VALIDATE(tmanager::vidcap == nullptr, null_str);
	tmanager::vidcap = &_vidcap;
}

void deregister_vidcap()
{
	threading::lock lock(tmanager::vidcap_mutex);
	VALIDATE(tmanager::vidcap != nullptr, null_str);
	tmanager::vidcap = nullptr;
}

} // end live555d