// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/chromium/base/SysUtils

#ifndef org_chromium_base_SysUtils_JNI
#define org_chromium_base_SysUtils_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.

JNI_REGISTRATION_EXPORT extern const char kClassPath_org_chromium_base_SysUtils[];
const char kClassPath_org_chromium_base_SysUtils[] = "org/chromium/base/SysUtils";
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass> g_org_chromium_base_SysUtils_clazz(nullptr);
#ifndef org_chromium_base_SysUtils_clazz_defined
#define org_chromium_base_SysUtils_clazz_defined
inline jclass org_chromium_base_SysUtils_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_org_chromium_base_SysUtils,
      &g_org_chromium_base_SysUtils_clazz);
}
#endif


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace base {
namespace android {

static void JNI_SysUtils_LogPageFaultCountToTracing(JNIEnv* env);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1base_1SysUtils_1logPageFaultCountToTracing(
    JNIEnv* env,
    jclass jcaller) {
  return JNI_SysUtils_LogPageFaultCountToTracing(env);
}


static std::atomic<jmethodID> g_org_chromium_base_SysUtils_isLowEndDevice(nullptr);
static jboolean Java_SysUtils_isLowEndDevice(JNIEnv* env) {
  jclass clazz = org_chromium_base_SysUtils_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_base_SysUtils_clazz(env), false);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "isLowEndDevice",
          "()Z",
          &g_org_chromium_base_SysUtils_isLowEndDevice);

  jboolean ret =
      env->CallStaticBooleanMethod(clazz,
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID> g_org_chromium_base_SysUtils_isCurrentlyLowMemory(nullptr);
static jboolean Java_SysUtils_isCurrentlyLowMemory(JNIEnv* env) {
  jclass clazz = org_chromium_base_SysUtils_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_base_SysUtils_clazz(env), false);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "isCurrentlyLowMemory",
          "()Z",
          &g_org_chromium_base_SysUtils_isCurrentlyLowMemory);

  jboolean ret =
      env->CallStaticBooleanMethod(clazz,
          call_context.base.method_id);
  return ret;
}

}  // namespace android
}  // namespace base

#endif  // org_chromium_base_SysUtils_JNI
