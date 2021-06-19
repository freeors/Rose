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
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/

#include "liveMedia/include/liveMedia.hh"
#include "BasicUsageEnvironment/include/BasicUsageEnvironment.hh"
#include "live555.hpp"
#include <stdlib.h>
#include <SDL_log.h>
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"
#include <iomanip>
#include "thread.hpp"
#include "gui/dialogs/dialog.hpp"
#include "base_instance.hpp"
#include "filesystem.hpp"

#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "url/gurl.h"
#include <base/threading/thread_task_runner_handle.h>

namespace live555 {

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

std::string log_RTSPClient(const RTSPClient& rtspClient) 
{
	std::stringstream ss;
	ss << "[URL:\"" << rtspClient.url() << "\"]: ";
	return ss.str();
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
std::string log_MediaSubsession(const MediaSubsession& subsession) 
{
	std::stringstream ss;
	ss << subsession.mediumName() << "/" << subsession.codecName();
	return ss.str();
}

void usage(UsageEnvironment& env, char const* progName) {
  env << "Usage: " << progName << " <rtsp-url-1> ... <rtsp-url-N>\n";
  env << "\t(where each <rtsp-url-i> is a \"rtsp://\" URL)\n";
}

class StreamClientState {
public:
  StreamClientState();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

static bool use_chromium = true;
static base::RunLoop* runloop_ptr = nullptr;

class ourRTSPClient: public RTSPClient 
{
public:
	static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
					int verbosityLevel = 0,
					char const* applicationName = NULL,
					portNumBits tunnelOverHTTPPortNum = 0);

	void sendTeardownCommand2(MediaSession* session, responseHandler* responseHandler)
	{
		sendTeardownCommand(*session, responseHandler, nullptr);
	}

	void socket_io_fail() override
	{
		resetTCPSockets();
		runloop_ptr->Quit();
	}

	void reset2()
	{
		reset();
		e_.Set();
	}

protected:
	ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
		int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
	// called only by createNew();
	virtual ~ourRTSPClient(); // if want to destroy ourRTSPClient, must call shutdownStream.

public:
	StreamClientState scs;
	std::string httpd_address;
	rtc::Event e_;
};

class tstart_singleton
{
public:
	tstart_singleton()
		: eventLoopWatchVariable(0)
		, rtsp(nullptr)
	{}
	
	char eventLoopWatchVariable;
	ourRTSPClient* rtsp;
	uint32_t setup_request_ticks;
	bool setup_success;

};

static std::unique_ptr<tstart_singleton> singleton;

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class DummySink: public MediaSink
{
public:
	static DummySink* createNew(UsageEnvironment& env,
					MediaSubsession& subsession, // identifies the kind of data that's being received
					char const* streamId = NULL); // identifies the stream itself (optional)

	void set_receive_ready() { receive_ready_ = true; }

	// redefined virtual functions:
	Boolean continuePlaying() override;

	void continuePlaying_chromium(net::StreamSocket* netSock)
	{
		continuePlaying();
		fSubsession.rtpSource()->chromium_slice(-1, netSock);
	}

private:
	DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
	// called only by "createNew()"
	virtual ~DummySink();

	static void afterGettingFrame(void* clientData, unsigned frameSize,
								unsigned numTruncatedBytes,
				struct timeval presentationTime,
								unsigned durationInMicroseconds);
	void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				struct timeval presentationTime, unsigned durationInMicroseconds);

public:
	int received_frames;
	int interrupts;
	char* eventLoopWatchVariable_ptr;
	uint8_t* fReceiveBuffer;
	unsigned int fReceiveBufferSize;
	unsigned int fFrameSize;
	MediaSubsession& fSubsession;
	base::RunLoop* loop;

private:
	char* fStreamId;
	bool receive_ready_;
	std::unique_ptr<tfile> file_;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
	std::stringstream ss;
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			if (use_chromium) {
				runloop_ptr->Quit();
			}
			env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
			delete[] resultString;
			break;
		}

		ss.str("");
		char* const sdpDescription = resultString;
		ss << log_RTSPClient(*rtspClient) << "Got a SDP description:\n" << sdpDescription << "\n";
		SDL_Log("%s", ss.str().c_str());

		// Create a media session object from this SDP description:
		scs.session = MediaSession::createNew(env, sdpDescription);
		delete[] sdpDescription; // because we don't need it anymore
		if (scs.session == NULL) {
			env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
			break;
		} else if (!scs.session->hasSubsessions()) {
			env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
			break;
		}
		ourRTSPClient* client2 = (ourRTSPClient*)(rtspClient);
		CHECK(client2->httpd_address.empty());

		GURL gurl(scs.session->httpdUrl());
		if (gurl.is_valid()) {
			const std::string& spec = gurl.spec();
			size_t len = spec.size();
			if (spec[len - 1] == '/') {
				len --;
			}
			client2->httpd_address.assign(spec.c_str(), len);
		}

		// Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
		// calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
		// (Each 'subsession' will have its own data source.)
		scs.iter = new MediaSubsessionIterator(*scs.session);
		setupNextSubsession(rtspClient);
		return;
	} while (0);

	// An unrecoverable error occurred with this stream.
	// shutdownStream(rtspClient);
}

void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL)
{
	// Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
	// to receive (even if more than stream uses the same "rtsp://" URL).
	ourRTSPClient* rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName, 0);
	if (rtspClient == NULL) {
		env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
		return;
	}

	singleton->setup_success = false;
	// Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
	// Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
	// Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
	if (use_chromium) {
		singleton->setup_request_ticks = SDL_GetTicks();
		rtspClient->socket_thread_->task_runner()->PostTask(FROM_HERE, 
			base::BindOnce(&ourRTSPClient::Start_chromium, base::Unretained(rtspClient), continueAfterDESCRIBE, nullptr));

	} else {
		rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
	}
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP True

void setupNextSubsession(RTSPClient* rtspClient) 
{
	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
	std::stringstream ss;

	scs.subsession = scs.iter->next();
	if (scs.subsession != NULL) {
		if (!scs.subsession->initiate(-1, use_chromium)) {
			ss << log_RTSPClient(*rtspClient) << "Failed to initiate the \"" << log_MediaSubsession(*scs.subsession) << "\" subsession: " << env.getResultMsg() << "\n";
			SDL_Log("%s", ss.str().c_str());
			setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
		} else {
			ss << log_RTSPClient(*rtspClient) << "Initiated the \"" << log_MediaSubsession(*scs.subsession) << "\" subsession (";
			if (scs.subsession->rtcpIsMuxed()) {
				ss << "client port " << scs.subsession->clientPortNum();
			} else {
				ss << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
			}
			ss << ")";
			SDL_Log("%s", ss.str().c_str());

			singleton->setup_request_ticks = SDL_GetTicks();
			// Continue setting up this subsession, by sending a RTSP "SETUP" command:
			rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
		}
		return;
	}

	singleton->setup_request_ticks = SDL_GetTicks();
	// We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
	if (scs.session->absStartTime() != NULL) {
		// Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
	} else {
		scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
		rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
	}
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) 
{
	std::stringstream ss;
	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			if (use_chromium) {
				runloop_ptr->Quit();
			}
			env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
			break;
		}

		ss.str("");
		ss << log_RTSPClient(*rtspClient) << "Set up the \"" << log_MediaSubsession(*scs.subsession) << "\" subsession (";
		if (scs.subsession->rtcpIsMuxed()) {
			ss << "client port " << scs.subsession->clientPortNum();
		} else {
			ss << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
		}
		ss << ")";
		SDL_Log("%s", ss.str().c_str());

		// Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
		// (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
		// after we've sent a RTSP "PLAY" command.)

		scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url());
			// perhaps use your own custom "MediaSink" subclass instead
		if (scs.subsession->sink == NULL) {
			env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
			<< "\" subsession: " << env.getResultMsg() << "\n";
			break;
		}

		ss.str("");
		ss << log_RTSPClient(*rtspClient) << "Created a data sink for the \"" << log_MediaSubsession(*scs.subsession) << "\" subsession\n";
		SDL_Log("%s", ss.str().c_str());
		scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession 
		scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
							subsessionAfterPlaying, scs.subsession);
		// Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
		if (scs.subsession->rtcpInstance() != NULL) {
			scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
		}
	} while (0);
	delete[] resultString;

	// Set up the next subsession, if any:
	setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) 
{
	std::stringstream ss;

	do {
		UsageEnvironment& env = rtspClient->envir(); // alias
		StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

		if (resultCode != 0) {
			if (use_chromium) {
				runloop_ptr->Quit();
			}
			env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
			break;
		}

		// Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
		// using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
		// 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
		// (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
		if (scs.duration > 0) {
			unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
			scs.duration += delaySlop;
			unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
			scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
		}

		ss.str("");
		ss << log_RTSPClient(*rtspClient) << "Started playing session";
		if (scs.duration > 0) {
			ss << " (for up to " << scs.duration << " seconds)";
		}
		ss << "...";
		SDL_Log("%s", ss.str().c_str());

		MediaSubsessionIterator iter(*scs.session);
		while ((scs.subsession = iter.next()) != NULL) {
			DummySink* sink = static_cast<DummySink*>(scs.subsession->sink);
			sink->set_receive_ready();
		}
		singleton->setup_success = true;

		if (use_chromium) {
			runloop_ptr->Quit();
		} else {
			// info doEventLoop(live555::start) exit.
			singleton->eventLoopWatchVariable = 1;
		}

	} while (0);
	delete[] resultString;
/*
	if (!singleton->setup_success) {
		// An unrecoverable error occurred with this stream.
		shutdownStream(rtspClient);
	}
*/
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  // shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) 
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(rtspClient != nullptr, null_str);

	UsageEnvironment& env = rtspClient->envir(); // alias
	StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
	std::stringstream ss;

	ourRTSPClient* rtspClient2 = static_cast<ourRTSPClient*>(rtspClient);

	// First, check whether any subsessions have still to be closed:
	if (scs.session != NULL) { 
		Boolean someSubsessionsWereActive = False;
		MediaSubsessionIterator iter(*scs.session);
		MediaSubsession* subsession;

		while ((subsession = iter.next()) != NULL) {
			if (subsession->sink != NULL) {
				Medium::close(subsession->sink);
				subsession->sink = NULL;

				if (subsession->rtcpInstance() != NULL) {
					subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
				}

				someSubsessionsWereActive = True;
			}
		}

		if (someSubsessionsWereActive) {
			// Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
			// Don't bother handling the response to the "TEARDOWN".
			if (!use_chromium) {
				rtspClient->sendTeardownCommand(*scs.session, NULL);
			} else {
/*
				rtspClient2->socket_thread_->task_runner()->PostTask(FROM_HERE, 
					base::BindOnce(&ourRTSPClient::sendTeardownCommand2, base::Unretained(rtspClient2), scs.session, nullptr));
*/
			}
		}
	}

	ss.str("");
	ss << log_RTSPClient(*rtspClient) << "Closing the stream.";
	SDL_Log("%s", ss.str().c_str());

	if (use_chromium) {
		rtspClient2->socket_thread_->task_runner()->PostTask(FROM_HERE, 
				base::BindOnce(&ourRTSPClient::reset2, base::Unretained(rtspClient2)));
		rtspClient2->e_.Wait(rtc::Event::kForever);
	}

	// Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
	Medium::close(rtspClient);

	if (singleton.get()) {
		// info doEventLoop exit.
		singleton->eventLoopWatchVariable = 1;
	}
}


// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
					int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
	: RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1, use_chromium)
	, e_(false, false)
{
	VALIDATE(singleton->rtsp == nullptr, null_str);
	singleton->rtsp = this;
}

ourRTSPClient::~ourRTSPClient() 
{
	if (singleton.get()) {
		VALIDATE(singleton->rtsp == this, null_str);
		singleton->rtsp = nullptr;
	}
}

// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}


// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1048576

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
{
	return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
	: MediaSink(env)
	, fSubsession(subsession)
	, receive_ready_(false)
	, received_frames(0)
	, interrupts(0)
	, eventLoopWatchVariable_ptr(nullptr)
	, fReceiveBuffer(nullptr)
	, fReceiveBufferSize(0)
	, fFrameSize(0)
	, loop(nullptr)
{
	fStreamId = strDup(streamId);
}

DummySink::~DummySink() 
{
	delete[] fStreamId;
	file_.reset();
}

void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds) {
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) 
{
	if (SDL_strcmp(fSubsession.codecName(), "H264") != 0) {
		return;
	}
/*
	if (envir().send_rtcp) {
		std::stringstream ss;
		// We've just received a frame of data.  (Optionally) print out information about it:
		ss << "#[" << received_frames << "]";
		if (fStreamId != NULL) {
			ss << "Stream \"" << fStreamId << "\"; ";
		}
		ss << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
		ss << "(" << std::setbase(16) << std::setfill('0') << std::setw(2) << (int)fReceiveBuffer[0] << ")";
		ss << std::setbase(10);
		if (numTruncatedBytes > 0) {
			ss << " (with " << numTruncatedBytes << " bytes truncated)";
		}
		char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
		sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
		ss << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
		if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
			ss << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
		}
	#ifdef DEBUG_PRINT_NPT
		ss << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
	#endif

		SDL_Log("%s", ss.str().c_str());
	}
*/
/*
	if (file_.get() == nullptr) {
		std::stringstream ss;
		ss << game_config::preferences_dir << "/realtime3r.264";
		file_.reset(new tfile(ss.str().c_str(), GENERIC_WRITE, CREATE_ALWAYS));
		VALIDATE(file_->valid(), null_str);
	}
*/
	if (file_.get() != nullptr) {
		uint8_t header[4] = {0x00, 0x00, 0x00, 0x01};
		posix_fwrite(file_->fp, header, 4);
		posix_fwrite(file_->fp, fReceiveBuffer, frameSize);
	}

	fFrameSize = frameSize;

	received_frames ++;
	if (use_chromium) {
		if (loop) {
			loop->Quit();
		}
	} else {
		*eventLoopWatchVariable_ptr = 1;
	}


	static uint32_t startup_ms = 0;
	static uint32_t last_ms = 0;
	static int last_received_frames = 0;
	uint32_t now = SDL_GetTicks();
    if (startup_ms == 0) {
		startup_ms = now;
		last_ms = startup_ms;
    }
    if (now - last_ms >= 10000) {
		last_ms = now;
		uint32_t elapsed_second = (now - startup_ms) / 1000;
		SDL_Log("[live555.cpp]#%u, %u fps, last 10s receive %i, frames", elapsed_second, received_frames / elapsed_second, received_frames - last_received_frames);
		last_received_frames = received_frames;
    }
}

Boolean DummySink::continuePlaying()
{
	if (!receive_ready_) {
		return False;
	}
	if (fSource == NULL) {
		return False; // sanity check (should not happen)
	}

	VALIDATE(fReceiveBuffer != nullptr && fReceiveBufferSize > 0 && fFrameSize == 0, null_str);

	// Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
	fSource->getNextFrame(fReceiveBuffer, fReceiveBufferSize,
						afterGettingFrame, this,
						onSourceClosure, this);
	return True;
}

void shutdownLive555(TaskScheduler* scheduler, UsageEnvironment* env, RTSPClient* rtspClient)
{
	VALIDATE(scheduler != nullptr && env != nullptr, null_str);
	if (rtspClient != nullptr) {
		shutdownStream(rtspClient);
	}

	bool ret = env->reclaim();
	if (!ret) {
		MediaLookupTable* table = MediaLookupTable::ourMedia(*env);
		table->dump();
	}
	VALIDATE(ret, null_str);
	delete scheduler;
}

class tlive555
{
public:
	tlive555(TaskScheduler* scheduler, UsageEnvironment* env, DummySink* video_sink)
		: scheduler(scheduler)
		, env(env)
		, rtsp(singleton->rtsp)
		, video_sink(video_sink)
		, eventLoopWatchVariable(0)
		, using_count(0)
	{
		VALIDATE(scheduler != nullptr && env != nullptr && video_sink != nullptr && rtsp != nullptr, null_str);
		video_sink->eventLoopWatchVariable_ptr = &eventLoopWatchVariable;
	}

	~tlive555()
	{
		VALIDATE(env != nullptr && scheduler != nullptr, null_str);

		shutdownLive555(scheduler, env, rtsp);
		env = nullptr;
		scheduler = nullptr;
	}

public:
	UsageEnvironment* env;
	TaskScheduler* scheduler;
	ourRTSPClient* rtsp;
	DummySink* video_sink;
	char eventLoopWatchVariable;
	threading::mutex mutex;
	int using_count;
};

static int next_id = 0;
static std::map<int, tlive555*> live555s;
static threading::mutex live555s_mutex;

struct tlive555_using_lock
{
	tlive555_using_lock(int id, const std::string& scene)
		: ptr(nullptr)
		, id(id)
		, scene(scene)
	{
		threading::lock lock(live555s_mutex);
		std::map<int, tlive555*>::iterator live555_it = live555s.find(id);
		if (live555_it == live555s.end()) {
			// this rtsp client is destroyed.
			return ;
		}
		ptr = live555_it->second;
		VALIDATE(ptr->using_count >= 0, null_str);
		// SDL_Log("[%u] tlive555_using_lock::tlive555_using_lock [%s] pre id: %i, using_count: %i", SDL_GetTicks(), scene.c_str(), id, ptr->using_count);
		rtc::AtomicOps::Increment(&ptr->using_count);
	}
	~tlive555_using_lock()
	{
		if (ptr != nullptr) {
			rtc::AtomicOps::Decrement(&ptr->using_count);
			VALIDATE(ptr->using_count >= 0, null_str);
			// SDL_Log("[%u] tlive555_using_lock::~tlive555_using_lock [%s] post id: %i, using_count: %i", SDL_GetTicks(), scene.c_str(), id, ptr->using_count);
		}
	}
	tlive555* ptr;
	const int id;
	const std::string scene;
};

void rtsp_setup_slice(gui2::tprogress_* progress, base::RunLoop* loop, int timeout)
{
	if (progress) {
		progress->show_slice();
	}

	if (timeout != nposm && (int)(SDL_GetTicks() - singleton->setup_request_ticks) > timeout) {
		// require call after executor->progress->show_slice().
		loop->Quit();
		return;
	}

	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&rtsp_setup_slice, progress, loop, timeout));
}

struct tsingleton_lock
{
	tsingleton_lock()
	{
		VALIDATE(!singleton.get(), null_str);
		singleton.reset(new tstart_singleton());
	}

	~tsingleton_lock()
	{
		singleton.reset();
	}
};

struct tRunLoop_lock
{
	tRunLoop_lock()
	{
		VALIDATE(runloop_ptr == nullptr, null_str);
		if (use_chromium) {
			runloop_ptr = new base::RunLoop();
		}
	}

	~tRunLoop_lock()
	{
		if (use_chromium) {
			VALIDATE(runloop_ptr != nullptr, null_str);
			delete runloop_ptr;
			runloop_ptr = nullptr;
		}
	}
};

int start(const std::string& rtsp_url, bool send_rtcp, std::string& httpd_url, uint32_t* ipv4_ptr)
{
	VALIDATE_IN_MAIN_THREAD();
	instance->will_longblock();

	httpd_url.clear();
	VALIDATE(!rtsp_url.empty(), null_str);

	SDL_Log("%u, live555::start--- rtsp_url: %s", SDL_GetTicks(), rtsp_url.c_str());

	if (base::RunLoop::IsRunningOnCurrentThread()) {
		// has one RunLoop is running, must not nest.
		SDL_Log("---live555::start X, rtsp_url: %s, base::RunLoop::IsRunningOnCurrentThread() is true", rtsp_url.c_str());
		return nposm;
	}

	// all rtsp must start in same thread, can not consider synchronization code.
	tsingleton_lock start_lock;

	// Begin by setting up our usage environment:
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler, use_chromium, next_id, send_rtcp);

	// after new ourRTSPClient, openURL will Post ourRTSPClient::Start_chromium.
	// Start_chromium will use runloop. ready runloop.
	tRunLoop_lock RunLoop_lock;

	// There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
	openURL(*env, "librose", rtsp_url.c_str());

	if (singleton->rtsp == nullptr) {
		// construct ourRTSPClient fail.
		shutdownLive555(scheduler, env, nullptr);
		SDL_Log("---live555::start X, rtsp_url: %s, construct ourRTSPClient fail", rtsp_url.c_str());
		return nposm;
	}

	if (use_chromium) {
		const int timeout = 4500;
		base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&rtsp_setup_slice, nullptr, runloop_ptr, timeout));
		runloop_ptr->Run();
	} else {
		// All subsequent activity takes place within the event loop:
		env->taskScheduler().doEventLoop(&(singleton->eventLoopWatchVariable));
		// This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.
	}

	if (singleton->rtsp == nullptr) {
		// DESCRIBE, SETUP, PLAY maybe fail. 
		SDL_Log("---live555::start X, rtsp_url: %s, DESCRIBE, SETUP, PLAY maybe fail", rtsp_url.c_str());
		shutdownLive555(scheduler, env, nullptr);
		return nposm;
	}
	if (!singleton->setup_success) {
		shutdownLive555(scheduler, env, singleton->rtsp);
		SDL_Log("---live555::start X, rtsp_url: %s, singleton->setup_success is false", rtsp_url.c_str());
		return nposm;
	}

	DummySink* video_sink = nullptr;
	StreamClientState& scs = singleton->rtsp->scs;
	MediaSubsessionIterator iter(*scs.session);
	while ((scs.subsession = iter.next()) != NULL) {
		std::string medium = utils::lowercase(scs.subsession->mediumName());
		if (medium.find("video") != std::string::npos) {
			video_sink = static_cast<DummySink*>(scs.subsession->sink);
			break; // this subsession is still active
		}
	}
	VALIDATE(video_sink != nullptr, null_str);

	tlive555* live555_ptr = new tlive555(scheduler, env, video_sink);

	threading::lock lock(live555s_mutex);
	live555_ptr->eventLoopWatchVariable = 1;
	std::pair<std::map<int, tlive555*>::iterator, bool> ins = live555s.insert(std::make_pair(next_id, live555_ptr));
	next_id ++;
	if (next_id == INT_MAX) {
		next_id = 0;
	}
	httpd_url = live555_ptr->rtsp->httpd_address;
	if (ipv4_ptr != nullptr) {
		*ipv4_ptr = live555_ptr->rtsp->ipv4();
	}

	SDL_Log("---live555::start X, rtsp_url: %s, httpd_url: %s", rtsp_url.c_str(), httpd_url.c_str());

	return ins.first->first;
}

void stop(int id)
{
	VALIDATE_IN_MAIN_THREAD();
	instance->will_longblock();

	SDL_Log("%u, live555::stop", SDL_GetTicks());

	tlive555* live555 = nullptr;
	{
		threading::lock lock(live555s_mutex);
		std::map<int, tlive555*>::iterator live555_it = live555s.find(id);
		VALIDATE(live555_it != live555s.end(), null_str);
		live555 = live555_it->second;
		live555s.erase(live555_it);
	}
	// rose make sure DecodingThread has exit. so doesn't require synchronization code.

	while (live555->using_count > 0) {
		SDL_Delay(10);
	}
	delete live555;
}

void OnWriteCompleted_chromium(int id, int rv)
{
	tlive555_using_lock using_lock(id, "OnWriteCompleted_chromium");
	if (using_lock.ptr == nullptr) {
		// this rtsp client is destroyed.
		return;
	}
	tlive555* live555 = using_lock.ptr;
	// threading::lock lock(live555->mutex);

	live555->rtsp->OnWriteCompleted(id, rv);
}

void rtcp_onExpire_chromium(int id, RTCPInstance* instance)
{
	tlive555_using_lock using_lock(id, "rtcp_onExpire_chromium");
	if (using_lock.ptr == nullptr) {
		// this rtsp client is destroyed.
		return;
	}
	RTCPInstance::onExpire(instance);
}

bool sendDataOverTCP_client_chromium(net::StreamSocket* netSock, const uint8_t* data, unsigned dataSize)
{
	for (std::map<int, tlive555*>::const_iterator it = live555s.begin(); it != live555s.end(); ++ it) {
		tlive555& live555 = *(it->second);
		if (live555.rtsp->getNetSock() == netSock) {
			live555.rtsp->sendData_chromium(netSock, data, dataSize);
/*
			{
				int ii = 0;
				tfile file(game_config::preferences_dir + "/1-action-request-2.dat", GENERIC_WRITE, OPEN_EXISTING);
				int fsize = file.read_2_data();
				VALIDATE(fsize > 0, null_str);
				live555.rtsp->sendData_chromium(netSock, (const uint8_t*)file.data, fsize);
			}
*/
			return true;
		}
	}
	return false;
}

static void TaskInterruptData(void* p_private)
{
	DummySink* video_sink = (DummySink*)p_private;
	video_sink->interrupts ++;

	video_sink->source()->cancelGetNextFrame();
    // Avoid lock
    *video_sink->eventLoopWatchVariable_ptr = 1;
}

void quit_runloop(base::RunLoop* loop)
{
	loop->Quit();
}

int frame_slice(int id, uint8_t* buffer, int buffer_size)
{
	if (id == nposm) {
		return 0;
	}
	
	tlive555_using_lock live555_lock(id, "frame_slice");
	VALIDATE(live555_lock.ptr != nullptr, null_str);

	tlive555& live555 = *live555_lock.ptr;
	// threading::lock lock(live555.mutex);

	if (use_chromium) {
		VALIDATE(live555.eventLoopWatchVariable != 0, null_str);
	}

	live555.video_sink->fReceiveBuffer = buffer;
	live555.video_sink->fReceiveBufferSize = buffer_size;
	live555.video_sink->fFrameSize = 0;

	std::unique_ptr<base::RunLoop> loop;
	if (use_chromium) {
		loop.reset(new base::RunLoop());
		live555.video_sink->loop = loop.get();
	} else {
		live555.eventLoopWatchVariable = 0;
	}

	// SDL_Log("%u, live555.video_sink->continuePlaying()", SDL_GetTicks());

	if (use_chromium) {
		live555.rtsp->socket_thread_->task_runner()->PostTask(FROM_HERE, 
			base::BindOnce(&DummySink::continuePlaying_chromium, 
				base::Unretained(live555.video_sink), live555.rtsp->getNetSock()));

		// const base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(5000); // 5 second.
		const base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(10000); // 10 second.
		base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::Bind(&quit_runloop, loop.get()), timeout);
		loop->Run();
		loop.reset();

		live555.video_sink->loop = nullptr;
		// SDL_Log("%u, live555.video_sink->sync_event.Wait(500)", SDL_GetTicks());

	} else {
		live555.video_sink->continuePlaying();

		// Create a task that will be called if we wait more than 400ms(400000)/10second(10000000)
		TaskToken task = live555.scheduler->scheduleDelayedTask(10000000, TaskInterruptData, live555.video_sink);

		// Do the read
		live555.env->taskScheduler().doEventLoop(&(live555.eventLoopWatchVariable));

		// remove the task
		live555.scheduler->unscheduleDelayedTask(task );
	}

	if (live555.video_sink->fFrameSize == 0) {
		// no frame in 10 second, think disconnect.
		live555.video_sink->source()->cancelGetNextFrame();
		dbg_variable.fSendFails ++;
		SDL_Log("%u, live555::frame_slice, no frame in 10 second.", SDL_GetTicks());
		return nposm;
	}

	if (live555.video_sink->received_frames % 500 == 0) {
		SDL_Log("%s, Total received %i frames, interrupts: %i", live555.rtsp->url(), live555.video_sink->received_frames, live555.video_sink->interrupts);
	}
	return live555.video_sink->fFrameSize;
}

}