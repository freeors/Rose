// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to
// debuggers.  You should use this to test if you're running under a debugger,
// and if you would like to yield (breakpoint) into the debugger.

#ifndef NET_NET_BUILDFLAGS_H_
#define NET_NET_BUILDFLAGS_H_

#include "build/buildflag.h"

#define BUILDFLAG_INTERNAL_POSIX_AVOID_MMAP() (0)
#define BUILDFLAG_INTERNAL_DISABLE_FILE_SUPPORT() (1)
#define BUILDFLAG_INTERNAL_DISABLE_FTP_SUPPORT() (1)
#define BUILDFLAG_INTERNAL_ENABLE_MDNS() (0)
#define BUILDFLAG_INTERNAL_ENABLE_REPORTING() (0)
#define BUILDFLAG_INTERNAL_ENABLE_WEBSOCKETS() (0)
#define BUILDFLAG_INTERNAL_INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST() (0)
#define BUILDFLAG_INTERNAL_USE_KERBEROS() (0)
#define BUILDFLAG_INTERNAL_USE_EXTERNAL_GSSAPI() (0)
#define BUILDFLAG_INTERNAL_TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED() (0)
#define BUILDFLAG_INTERNAL_BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED() (0)

#endif  // NET_NET_BUILDFLAGS_H_
