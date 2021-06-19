// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base\android\jni_generator\jni_generator.py
// For
//     org/chromium/net/NetworkChangeNotifier

#ifndef org_chromium_net_NetworkChangeNotifier_JNI
#define org_chromium_net_NetworkChangeNotifier_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.

JNI_REGISTRATION_EXPORT extern const char kClassPath_org_chromium_net_NetworkChangeNotifier[];
const char kClassPath_org_chromium_net_NetworkChangeNotifier[] =
    "org/chromium/net/NetworkChangeNotifier";
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass> g_org_chromium_net_NetworkChangeNotifier_clazz(nullptr);
#ifndef org_chromium_net_NetworkChangeNotifier_clazz_defined
#define org_chromium_net_NetworkChangeNotifier_clazz_defined
inline jclass org_chromium_net_NetworkChangeNotifier_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_org_chromium_net_NetworkChangeNotifier,
      &g_org_chromium_net_NetworkChangeNotifier_clazz);
}
#endif


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace net {

static void JNI_NetworkChangeNotifier_NotifyConnectionTypeChanged(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    jint newConnectionType,
    jlong defaultNetId);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyConnectionTypeChanged(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jint newConnectionType,
    jlong defaultNetId) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyConnectionTypeChanged(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller), newConnectionType, defaultNetId);
}

static void JNI_NetworkChangeNotifier_NotifyMaxBandwidthChanged(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    jint subType);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyMaxBandwidthChanged(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jint subType) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyMaxBandwidthChanged(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller), subType);
}

static void JNI_NetworkChangeNotifier_NotifyOfNetworkConnect(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    jlong netId,
    jint connectionType);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyOfNetworkConnect(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jlong netId,
    jint connectionType) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyOfNetworkConnect(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller), netId, connectionType);
}

static void JNI_NetworkChangeNotifier_NotifyOfNetworkSoonToDisconnect(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    jlong netId);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyOfNetworkSoonToDisconnect(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jlong netId) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyOfNetworkSoonToDisconnect(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller), netId);
}

static void JNI_NetworkChangeNotifier_NotifyOfNetworkDisconnect(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    jlong netId);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyOfNetworkDisconnect(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jlong netId) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyOfNetworkDisconnect(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller), netId);
}

static void JNI_NetworkChangeNotifier_NotifyPurgeActiveNetworkList(JNIEnv* env, jlong nativePtr,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jlongArray>& activeNetIds);

JNI_GENERATOR_EXPORT void
    Java_org_chromium_base_natives_GEN_1JNI_org_1chromium_1net_1NetworkChangeNotifier_1notifyPurgeActiveNetworkList(
    JNIEnv* env,
    jclass jcaller,
    jlong nativePtr,
    jobject caller,
    jlongArray activeNetIds) {
  // [rose fixing]
  // return JNI_NetworkChangeNotifier_NotifyPurgeActiveNetworkList(env, nativePtr,
  //    base::android::JavaParamRef<jobject>(env, caller),
  //    base::android::JavaParamRef<jlongArray>(env, activeNetIds));
}


static std::atomic<jmethodID> g_org_chromium_net_NetworkChangeNotifier_init(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_NetworkChangeNotifier_init(JNIEnv* env) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "init",
          "()Lorg/chromium/net/NetworkChangeNotifier;",
          &g_org_chromium_net_NetworkChangeNotifier_init);

  jobject ret =
      env->CallStaticObjectMethod(clazz,
          call_context.base.method_id);
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_getCurrentConnectionType(nullptr);
static jint Java_NetworkChangeNotifier_getCurrentConnectionType(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getCurrentConnectionType",
          "()I",
          &g_org_chromium_net_NetworkChangeNotifier_getCurrentConnectionType);

  jint ret =
      env->CallIntMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_getCurrentConnectionSubtype(nullptr);
static jint Java_NetworkChangeNotifier_getCurrentConnectionSubtype(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getCurrentConnectionSubtype",
          "()I",
          &g_org_chromium_net_NetworkChangeNotifier_getCurrentConnectionSubtype);

  jint ret =
      env->CallIntMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_getCurrentDefaultNetId(nullptr);
static jlong Java_NetworkChangeNotifier_getCurrentDefaultNetId(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getCurrentDefaultNetId",
          "()J",
          &g_org_chromium_net_NetworkChangeNotifier_getCurrentDefaultNetId);

  jlong ret =
      env->CallLongMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_getCurrentNetworksAndTypes(nullptr);
static base::android::ScopedJavaLocalRef<jlongArray>
    Java_NetworkChangeNotifier_getCurrentNetworksAndTypes(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getCurrentNetworksAndTypes",
          "()[J",
          &g_org_chromium_net_NetworkChangeNotifier_getCurrentNetworksAndTypes);

  jlongArray ret =
      static_cast<jlongArray>(env->CallObjectMethod(obj.obj(),
          call_context.base.method_id));
  return base::android::ScopedJavaLocalRef<jlongArray>(env, ret);
}

static std::atomic<jmethodID> g_org_chromium_net_NetworkChangeNotifier_addNativeObserver(nullptr);
static void Java_NetworkChangeNotifier_addNativeObserver(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, jlong nativeChangeNotifier) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "addNativeObserver",
          "(J)V",
          &g_org_chromium_net_NetworkChangeNotifier_addNativeObserver);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, nativeChangeNotifier);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_removeNativeObserver(nullptr);
static void Java_NetworkChangeNotifier_removeNativeObserver(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, jlong nativeChangeNotifier) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "removeNativeObserver",
          "(J)V",
          &g_org_chromium_net_NetworkChangeNotifier_removeNativeObserver);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, nativeChangeNotifier);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_registerNetworkCallbackFailed(nullptr);
static jboolean Java_NetworkChangeNotifier_registerNetworkCallbackFailed(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      org_chromium_net_NetworkChangeNotifier_clazz(env), false);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "registerNetworkCallbackFailed",
          "()Z",
          &g_org_chromium_net_NetworkChangeNotifier_registerNetworkCallbackFailed);

  jboolean ret =
      env->CallBooleanMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_forceConnectivityState(nullptr);
static void Java_NetworkChangeNotifier_forceConnectivityState(JNIEnv* env, jboolean
    networkAvailable) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "forceConnectivityState",
          "(Z)V",
          &g_org_chromium_net_NetworkChangeNotifier_forceConnectivityState);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, networkAvailable);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_fakeNetworkConnected(nullptr);
static void Java_NetworkChangeNotifier_fakeNetworkConnected(JNIEnv* env, jlong netId,
    JniIntWrapper connectionType) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakeNetworkConnected",
          "(JI)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakeNetworkConnected);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, netId, as_jint(connectionType));
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_fakeNetworkSoonToBeDisconnected(nullptr);
static void Java_NetworkChangeNotifier_fakeNetworkSoonToBeDisconnected(JNIEnv* env, jlong netId) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakeNetworkSoonToBeDisconnected",
          "(J)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakeNetworkSoonToBeDisconnected);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, netId);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_fakeNetworkDisconnected(nullptr);
static void Java_NetworkChangeNotifier_fakeNetworkDisconnected(JNIEnv* env, jlong netId) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakeNetworkDisconnected",
          "(J)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakeNetworkDisconnected);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, netId);
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_fakePurgeActiveNetworkList(nullptr);
static void Java_NetworkChangeNotifier_fakePurgeActiveNetworkList(JNIEnv* env, const
    base::android::JavaRef<jlongArray>& activeNetIds) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakePurgeActiveNetworkList",
          "([J)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakePurgeActiveNetworkList);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, activeNetIds.obj());
}

static std::atomic<jmethodID> g_org_chromium_net_NetworkChangeNotifier_fakeDefaultNetwork(nullptr);
static void Java_NetworkChangeNotifier_fakeDefaultNetwork(JNIEnv* env, jlong netId,
    JniIntWrapper connectionType) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakeDefaultNetwork",
          "(JI)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakeDefaultNetwork);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, netId, as_jint(connectionType));
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_fakeConnectionSubtypeChanged(nullptr);
static void Java_NetworkChangeNotifier_fakeConnectionSubtypeChanged(JNIEnv* env, JniIntWrapper
    connectionSubtype) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "fakeConnectionSubtypeChanged",
          "(I)V",
          &g_org_chromium_net_NetworkChangeNotifier_fakeConnectionSubtypeChanged);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, as_jint(connectionSubtype));
}

static std::atomic<jmethodID>
    g_org_chromium_net_NetworkChangeNotifier_isProcessBoundToNetwork(nullptr);
static jboolean Java_NetworkChangeNotifier_isProcessBoundToNetwork(JNIEnv* env) {
  jclass clazz = org_chromium_net_NetworkChangeNotifier_clazz(env);
  CHECK_CLAZZ(env, clazz,
      org_chromium_net_NetworkChangeNotifier_clazz(env), false);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "isProcessBoundToNetwork",
          "()Z",
          &g_org_chromium_net_NetworkChangeNotifier_isProcessBoundToNetwork);

  jboolean ret =
      env->CallStaticBooleanMethod(clazz,
          call_context.base.method_id);
  return ret;
}

}  // namespace net

#endif  // org_chromium_net_NetworkChangeNotifier_JNI