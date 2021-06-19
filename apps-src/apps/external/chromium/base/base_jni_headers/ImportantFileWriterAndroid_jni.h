// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/chromium/base/ImportantFileWriterAndroid

#ifndef org_chromium_base_ImportantFileWriterAndroid_JNI
#define org_chromium_base_ImportantFileWriterAndroid_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace base {
namespace android {

static jboolean JNI_ImportantFileWriterAndroid_WriteFileAtomically(JNIEnv* env, const
    base::android::JavaParamRef<jstring>& fileName,
    const base::android::JavaParamRef<jbyteArray>& data);

JNI_GENERATOR_EXPORT jboolean
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1base_1ImportantFileWriterAndroid_1writeFileAtomically(
    JNIEnv* env,
    jclass jcaller,
    jstring fileName,
    jbyteArray data) {
  return JNI_ImportantFileWriterAndroid_WriteFileAtomically(env,
      base::android::JavaParamRef<jstring>(env, fileName),
      base::android::JavaParamRef<jbyteArray>(env, data));
}


}  // namespace android
}  // namespace base

#endif  // org_chromium_base_ImportantFileWriterAndroid_JNI