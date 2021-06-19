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
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// Usage Environment
// Implementation

#include "UsageEnvironment/include/UsageEnvironment.hh"
#include <stdlib.h>

#include "net/base/io_buffer.h"

Boolean UsageEnvironment::reclaim() {
  // We delete ourselves only if we have no remainining state:
  if (liveMediaPriv == NULL && groupsockPriv == NULL) {
    delete this;
    return True;
  }

  return False;
}

UsageEnvironment::UsageEnvironment(TaskScheduler& scheduler, bool use_chromium, int id, bool send_rtcp)
  : liveMediaPriv(NULL), groupsockPriv(NULL), fScheduler(scheduler)
  , use_chromium(use_chromium)
  , id(id)
  , send_rtcp(send_rtcp)
  , iocomplete_RTSPClient(true)
  , iocomplete_arg0(nullptr)
  , setup_finished(false)
  , in_io_pending(false) {
}

UsageEnvironment::~UsageEnvironment() {
}

// By default, we handle 'should not occur'-type library errors by calling abort().  Subclasses can redefine this, if desired.
// (If your runtime library doesn't define the "abort()" function, then define your own (e.g., that does nothing).)
void UsageEnvironment::internalError() {
  abort();
}


TaskScheduler::TaskScheduler() {
}

TaskScheduler::~TaskScheduler() {
}

void TaskScheduler::rescheduleDelayedTask(TaskToken& task,
					  int64_t microseconds, TaskFunc* proc,
					  void* clientData) {
  unscheduleDelayedTask(task);
  task = scheduleDelayedTask(microseconds, proc, clientData);
}

// By default, we handle 'should not occur'-type library errors by calling abort().  Subclasses can redefine this, if desired.
void TaskScheduler::internalError() {
  abort();
}
