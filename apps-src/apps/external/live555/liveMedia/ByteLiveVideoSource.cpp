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
// A file source that is a plain byte stream (rather than frames)
// Implementation

#include "liveMedia/include/ByteLiveVideoSource.hh"
#include "liveMedia/include/InputFile.hh"
#include "groupsock/include/GroupsockHelper.hh"

#include "filesystem.hpp"
#include "SDL.h"
#include "SDL_log.h"
////////// ByteLiveVideoSource //////////

ByteLiveVideoSource*
ByteLiveVideoSource::createNew(UsageEnvironment& env, char const* fileName,
				unsigned preferredFrameSize,
				unsigned playTimePerFrame)
{
	ByteLiveVideoSource* newSource = new ByteLiveVideoSource(env, preferredFrameSize, playTimePerFrame);
	return newSource;
}
/*
void ByteLiveVideoSource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
  fNumBytesToStream = numBytesToStream;
  fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteLiveVideoSource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
  fNumBytesToStream = numBytesToStream;
  fLimitNumBytesToStream = fNumBytesToStream > 0;
}

void ByteLiveVideoSource::seekToEnd() {
}
*/
ByteLiveVideoSource::ByteLiveVideoSource(UsageEnvironment& env, unsigned preferredFrameSize,  unsigned playTimePerFrame)
  : FramedFileSource(env, nullptr), fPreferredFrameSize(preferredFrameSize),
    fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0)
	, fHaveStartedReading(False)
	, fLimitNumBytesToStream(False)
	, fNumBytesToStream(0)
	, fTotalReadBytes(0)
	, fDoReadCount(0)
{
  // Test whether the file is seekable
  fFidIsSeekable = false;
}

ByteLiveVideoSource::~ByteLiveVideoSource() {
}

void ByteLiveVideoSource::doGetNextFrame() {
  if (fLimitNumBytesToStream && fNumBytesToStream == 0) {
    handleClosure();
    return;
  }

  doReadFromFile();
}

void ByteLiveVideoSource::doStopGettingFrames() {
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
}

#include "live555d.hpp"

extern bool fls_use_chromium;
extern std::set<int> inGetAuxSDPLines;
#include "base/threading/thread_task_runner_handle.h"
#include "base/location.h"

void ByteLiveVideoSource::doReadFromFile()
{
	// Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
	if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fMaxSize) {
		fMaxSize = (unsigned)fNumBytesToStream;
	}
	if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}

	bool inGetAuxSDPLine = inGetAuxSDPLines.count(fSessionid) != 0;
	{
		threading::lock lock(live555d::tmanager::vidcap_mutex);
		if (live555d::tmanager::vidcap) {
			fFrameSize = live555d::tmanager::vidcap->read(fSessionid, inGetAuxSDPLine, fTo, fMaxSize);
		} else {
			fFrameSize = 0;
		}
	}
	if (inGetAuxSDPLine) {
		SDL_Log("T:%u, #%u, doReadFromFile[X], fTotalReadBytes: %u, fFrameSize: %u", SDL_GetTicks(), fDoReadCount ++, fTotalReadBytes, fFrameSize);
	}
	fTotalReadBytes += fFrameSize;

	if (fFrameSize == 0) {
		handleClosure();
		return;
	}
	fNumBytesToStream -= fFrameSize;

	// Set the 'presentation time':
	if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
		if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
			// This is the first frame, so use the current time:
			gettimeofday(&fPresentationTime, NULL);
		} else {
			// Increment by the play time of the previous data:
			unsigned uSeconds	= fPresentationTime.tv_usec + fLastPlayTime;
			fPresentationTime.tv_sec += uSeconds/1000000;
			fPresentationTime.tv_usec = uSeconds%1000000;
		}

		// Remember the play time of this data:
		fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
		fDurationInMicroseconds = fLastPlayTime;
	} else {
		// We don't know a specific play time duration for this data,
		// so just record the current time as being the 'presentation time':
		gettimeofday(&fPresentationTime, NULL);
	}

	// Inform the reader that he has data:
	// To avoid possible infinite recursion, we need to return to the event loop to do this:
	if (!fls_use_chromium || inGetAuxSDPLine) {
		nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
				(TaskFunc*)FramedSource::afterGetting, this);
	} else {
		base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::BindOnce(&FramedSource::afterGetting_chromium, fSessionid, base::Unretained(this)));
	}
}
