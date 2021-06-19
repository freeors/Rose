// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/chromium/net/GURLUtils

#ifndef org_chromium_net_GURLUtils_JNI
#define org_chromium_net_GURLUtils_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace net {

static base::android::ScopedJavaLocalRef<jstring> JNI_GURLUtils_GetOrigin(JNIEnv* env, const
    base::android::JavaParamRef<jstring>& url);

JNI_GENERATOR_EXPORT jstring
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1GURLUtils_1getOrigin(
    JNIEnv* env,
    jclass jcaller,
    jstring url) {
  return JNI_GURLUtils_GetOrigin(env, base::android::JavaParamRef<jstring>(env, url)).Release();
}

static base::android::ScopedJavaLocalRef<jstring> JNI_GURLUtils_GetScheme(JNIEnv* env, const
    base::android::JavaParamRef<jstring>& url);

JNI_GENERATOR_EXPORT jstring
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1GURLUtils_1getScheme(
    JNIEnv* env,
    jclass jcaller,
    jstring url) {
  return JNI_GURLUtils_GetScheme(env, base::android::JavaParamRef<jstring>(env, url)).Release();
}


}  // namespace net

#endif  // org_chromium_net_GURLUtils_JNI