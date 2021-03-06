// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/chromium/net/HttpNegotiateAuthenticator

#ifndef org_chromium_net_HttpNegotiateAuthenticator_JNI
#define org_chromium_net_HttpNegotiateAuthenticator_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.

JNI_REGISTRATION_EXPORT extern const char kClassPath_org_chromium_net_HttpNegotiateAuthenticator[];
const char kClassPath_org_chromium_net_HttpNegotiateAuthenticator[] =
    "org/chromium/net/HttpNegotiateAuthenticator";
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass>
    g_org_chromium_net_HttpNegotiateAuthenticator_clazz(nullptr);
#ifndef org_chromium_net_HttpNegotiateAuthenticator_clazz_defined
#define org_chromium_net_HttpNegotiateAuthenticator_clazz_defined
inline jclass org_chromium_net_HttpNegotiateAuthenticator_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_org_chromium_net_HttpNegotiateAuthenticator,
      &g_org_chromium_net_HttpNegotiateAuthenticator_clazz);
}
#endif


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace net {
namespace android {

static void JNI_HttpNegotiateAuthenticator_SetResult(JNIEnv* env, jlong
    nativeJavaNegotiateResultWrapper,
    const base::android::JavaParamRef<jobject>& caller,
    jint status,
    const base::android::JavaParamRef<jstring>& authToken);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1HttpNegotiateAuthenticator_1setResult(
    JNIEnv* env,
    jclass jcaller,
    jlong nativeJavaNegotiateResultWrapper,
    jobject caller,
    jint status,
    jstring authToken) {
  return JNI_HttpNegotiateAuthenticator_SetResult(env, nativeJavaNegotiateResultWrapper,
      base::android::JavaParamRef<jobject>(env, caller), status,
      base::android::JavaParamRef<jstring>(env, authToken));
}


static std::atomic<jmethodID> g_org_chromium_net_HttpNegotiateAuthenticator_create(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_HttpNegotiateAuthenticator_create(JNIEnv*
    env, const base::android::JavaRef<jstring>& accountType) {
  jclass clazz = org_chromium_net_HttpNegotiateAuthenticator_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_HttpNegotiateAuthenticator_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "create",
          "(Ljava/lang/String;)Lorg/chromium/net/HttpNegotiateAuthenticator;",
          &g_org_chromium_net_HttpNegotiateAuthenticator_create);

  jobject ret =
      env->CallStaticObjectMethod(clazz,
          call_context.base.method_id, accountType.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID>
    g_org_chromium_net_HttpNegotiateAuthenticator_getNextAuthToken(nullptr);
static void Java_HttpNegotiateAuthenticator_getNextAuthToken(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, jlong nativeResultObject,
    const base::android::JavaRef<jstring>& principal,
    const base::android::JavaRef<jstring>& authToken,
    jboolean canDelegate) {
  jclass clazz = org_chromium_net_HttpNegotiateAuthenticator_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_HttpNegotiateAuthenticator_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getNextAuthToken",
          "(JLjava/lang/String;Ljava/lang/String;Z)V",
          &g_org_chromium_net_HttpNegotiateAuthenticator_getNextAuthToken);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, nativeResultObject, principal.obj(), authToken.obj(),
              canDelegate);
}

}  // namespace android
}  // namespace net

#endif  // org_chromium_net_HttpNegotiateAuthenticator_JNI
