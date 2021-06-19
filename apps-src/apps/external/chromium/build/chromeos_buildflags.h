// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef BUILD_CHROMEOS_BUILDFLAGS_H_
#define BUILD_CHROMEOS_BUILDFLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_IS_LACROS() (0)
#define BUILDFLAG_INTERNAL_IS_ASH() (0)
#define BUILDFLAG_INTERNAL_IS_CHROMEOS_LACROS() (0)
#define BUILDFLAG_INTERNAL_IS_CHROMEOS_ASH() (0)

#endif  // BUILD_CHROMEOS_BUILDFLAGS_H_
