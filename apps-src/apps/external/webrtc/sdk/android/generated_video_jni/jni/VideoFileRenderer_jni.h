// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/webrtc/VideoFileRenderer

#ifndef org_webrtc_VideoFileRenderer_JNI
#define org_webrtc_VideoFileRenderer_JNI

#include "sdk/android/src/jni/jni_generator_helper.h"

// Step 1: forward declarations.
JNI_REGISTRATION_EXPORT extern const char
    kClassPath_org_webrtc_VideoFileRenderer[];
const char kClassPath_org_webrtc_VideoFileRenderer[] =
    "org/webrtc/VideoFileRenderer";

// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT base::subtle::AtomicWord
    g_org_webrtc_VideoFileRenderer_clazz = 0;
#ifndef org_webrtc_VideoFileRenderer_clazz_defined
#define org_webrtc_VideoFileRenderer_clazz_defined
inline jclass org_webrtc_VideoFileRenderer_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env,
      kClassPath_org_webrtc_VideoFileRenderer,
      &g_org_webrtc_VideoFileRenderer_clazz);
}
#endif

namespace webrtc {
namespace jni {

// Step 2: method stubs.

static void JNI_VideoFileRenderer_I420Scale(JNIEnv* env, const
    base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& srcY,
    jint strideY,
    const base::android::JavaParamRef<jobject>& srcU,
    jint strideU,
    const base::android::JavaParamRef<jobject>& srcV,
    jint strideV,
    jint width,
    jint height,
    const base::android::JavaParamRef<jobject>& dst,
    jint dstWidth,
    jint dstHeight);

JNI_GENERATOR_EXPORT void
    Java_org_webrtc_VideoFileRenderer_nativeI420Scale(JNIEnv* env, jclass
    jcaller,
    jobject srcY,
    jint strideY,
    jobject srcU,
    jint strideU,
    jobject srcV,
    jint strideV,
    jint width,
    jint height,
    jobject dst,
    jint dstWidth,
    jint dstHeight) {
  return JNI_VideoFileRenderer_I420Scale(env,
      base::android::JavaParamRef<jclass>(env, jcaller),
      base::android::JavaParamRef<jobject>(env, srcY), strideY,
      base::android::JavaParamRef<jobject>(env, srcU), strideU,
      base::android::JavaParamRef<jobject>(env, srcV), strideV, width, height,
      base::android::JavaParamRef<jobject>(env, dst), dstWidth, dstHeight);
}

}  // namespace jni
}  // namespace webrtc

#endif  // org_webrtc_VideoFileRenderer_JNI
