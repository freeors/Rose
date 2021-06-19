// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef BASE_LOGGING_BUILDFLAGS_H_
#define BASE_LOGGING_BUILDFLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_ENABLE_LOG_ERROR_NOT_REACHED() (0)

#endif  // BASE_LOGGING_BUILDFLAGS_H_
