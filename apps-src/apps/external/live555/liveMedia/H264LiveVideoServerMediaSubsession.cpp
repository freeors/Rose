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
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H264 video file.
// Implementation

#include "liveMedia/include/H264LiveVideoServerMediaSubsession.hh"
#include "liveMedia/include/H264VideoRTPSink.hh"
#include "liveMedia/include/ByteLiveVideoSource.hh"
#include "liveMedia/include/H264VideoStreamFramer.hh"

#include "SDL_log.h"
#include <base/logging.h>

H264LiveVideoServerMediaSubsession*
H264LiveVideoServerMediaSubsession::createNew(UsageEnvironment& env,
					      char const* fileName,
					      Boolean reuseFirstSource) {
  return new H264LiveVideoServerMediaSubsession(env, fileName, reuseFirstSource);
}

H264LiveVideoServerMediaSubsession::H264LiveVideoServerMediaSubsession(UsageEnvironment& env,
								       char const* fileName, Boolean reuseFirstSource)
  : FileServerMediaSubsession(env, fileName, reuseFirstSource),
    fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL) {
}

H264LiveVideoServerMediaSubsession::~H264LiveVideoServerMediaSubsession() {
  delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
  H264LiveVideoServerMediaSubsession* subsess = (H264LiveVideoServerMediaSubsession*)clientData;
  subsess->afterPlayingDummy1();
}

void H264LiveVideoServerMediaSubsession::afterPlayingDummy1() {
  // Unschedule any pending 'checking' task:
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
  // Signal the event loop that we're done:
  setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData)
{
	H264LiveVideoServerMediaSubsession* subsess = (H264LiveVideoServerMediaSubsession*)clientData;
	subsess->checkForAuxSDPLine1();
}

void H264LiveVideoServerMediaSubsession::checkForAuxSDPLine1()
{
	SDL_Log("checkForAuxSDPLine1, E");
  nextTask() = NULL;

  char const* dasl;
  if (fAuxSDPLine != NULL) {
    // Signal the event loop that we're done:
	  SDL_Log("checkForAuxSDPLine1, 1");
    setDoneFlag();
  } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
	  SDL_Log("checkForAuxSDPLine1, 2, dasl: %s", dasl);
    fAuxSDPLine = strDup(dasl);
    fDummyRTPSink = NULL;

    // Signal the event loop that we're done:
    setDoneFlag();
  } else if (!fDoneFlag) {
	  SDL_Log("checkForAuxSDPLine1, 3");
    // try again after a brief delay:
    int uSecsToDelay = 100000; // 100 ms
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
			      (TaskFunc*)checkForAuxSDPLine, this);
  }
  SDL_Log("checkForAuxSDPLine1, X");
}

std::set<int> inGetAuxSDPLines;

char const* H264LiveVideoServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
  if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

  SDL_Log("H264LiveVideoServerMediaSubsession::getAuxSDPLine(...), E, fSessionid: %i", inputSource->fSessionid);
  CHECK(inputSource->fSessionid != nposm);
  inGetAuxSDPLines.insert(inputSource->fSessionid);

  if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
    // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
    // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
    // and we need to start reading data from our file until this changes.
    fDummyRTPSink = rtpSink;

    // Start reading the file:
    fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

    // Check whether the sink's 'auxSDPLine()' is ready:
    checkForAuxSDPLine(this);
  }

  envir().taskScheduler().doEventLoop(&fDoneFlag);

  inGetAuxSDPLines.erase(inputSource->fSessionid);
  SDL_Log("H264LiveVideoServerMediaSubsession::getAuxSDPLine(...), X");
  return fAuxSDPLine;
}

FramedSource* H264LiveVideoServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  estBitrate = 500; // kbps, estimate

  SDL_Log("H264LiveVideoServerMediaSubsession::createNewStreamSource(...), E");

  // Create the video source:
  ByteLiveVideoSource* fileSource = ByteLiveVideoSource::createNew(envir(), fFileName);
  if (fileSource == NULL) return NULL;

  // Create a framer for the Video Elementary Stream:
  // return H264VideoStreamFramer::createNew(envir(), fileSource);

  FramedSource* source = H264VideoStreamFramer::createNew(envir(), fileSource);
  SDL_Log("H264LiveVideoServerMediaSubsession::createNewStreamSource(...), X");
  return source;
}

RTPSink* H264LiveVideoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, true);
}
