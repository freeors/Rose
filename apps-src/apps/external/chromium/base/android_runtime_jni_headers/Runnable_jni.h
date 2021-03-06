// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     java/lang/Runnable

#ifndef java_lang_Runnable_JNI
#define java_lang_Runnable_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.

JNI_REGISTRATION_EXPORT extern const char kClassPath_java_lang_Runnable[];
const char kClassPath_java_lang_Runnable[] = "java/lang/Runnable";
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass> g_java_lang_Runnable_clazz(nullptr);
#ifndef java_lang_Runnable_clazz_defined
#define java_lang_Runnable_clazz_defined
inline jclass java_lang_Runnable_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_java_lang_Runnable,
      &g_java_lang_Runnable_clazz);
}
#endif


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace JNI_Runnable {


static std::atomic<jmethodID> g_java_lang_Runnable_run(nullptr);
static void Java_Runnable_run(JNIEnv* env, const base::android::JavaRef<jobject>& obj) __attribute__
    ((unused));
static void Java_Runnable_run(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runnable_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runnable_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "run",
          "()V",
          &g_java_lang_Runnable_run);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id);
}

}  // namespace JNI_Runnable

#endif  // java_lang_Runnable_JNI
