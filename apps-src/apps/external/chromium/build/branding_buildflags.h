// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef BUILD_BRANDING_BUILDFLAGS_H_
#define BUILD_BRANDING_BUILDFLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_CHROMIUM_BRANDING() (1)
#define BUILDFLAG_INTERNAL_GOOGLE_CHROME_BRANDING() (0)

#endif  // BUILD_BRANDING_BUILDFLAGS_H_
