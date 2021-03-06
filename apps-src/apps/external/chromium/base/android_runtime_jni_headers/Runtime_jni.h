// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     java/lang/Runtime

#ifndef java_lang_Runtime_JNI
#define java_lang_Runtime_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"


// Step 1: Forward declarations.

JNI_REGISTRATION_EXPORT extern const char kClassPath_java_lang_Runtime[];
const char kClassPath_java_lang_Runtime[] = "java/lang/Runtime";
// Leaking this jclass as we cannot use LazyInstance from some threads.
JNI_REGISTRATION_EXPORT std::atomic<jclass> g_java_lang_Runtime_clazz(nullptr);
#ifndef java_lang_Runtime_clazz_defined
#define java_lang_Runtime_clazz_defined
inline jclass java_lang_Runtime_clazz(JNIEnv* env) {
  return base::android::LazyGetClass(env, kClassPath_java_lang_Runtime, &g_java_lang_Runtime_clazz);
}
#endif


// Step 2: Constants (optional).


// Step 3: Method stubs.
namespace JNI_Runtime {


static std::atomic<jmethodID> g_java_lang_Runtime_getRuntime(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getRuntime(JNIEnv* env) __attribute__
    ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getRuntime(JNIEnv* env) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, clazz,
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "getRuntime",
          "()Ljava/lang/Runtime;",
          &g_java_lang_Runtime_getRuntime);

  jobject ret =
      env->CallStaticObjectMethod(clazz,
          call_context.base.method_id);
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_exit(nullptr);
static void Java_Runtime_exit(JNIEnv* env, const base::android::JavaRef<jobject>& obj, JniIntWrapper
    p0) __attribute__ ((unused));
static void Java_Runtime_exit(JNIEnv* env, const base::android::JavaRef<jobject>& obj, JniIntWrapper
    p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exit",
          "(I)V",
          &g_java_lang_Runtime_exit);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, as_jint(p0));
}

static std::atomic<jmethodID> g_java_lang_Runtime_addShutdownHook(nullptr);
static void Java_Runtime_addShutdownHook(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& p0) __attribute__ ((unused));
static void Java_Runtime_addShutdownHook(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "addShutdownHook",
          "(Ljava/lang/Thread;)V",
          &g_java_lang_Runtime_addShutdownHook);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
}

static std::atomic<jmethodID> g_java_lang_Runtime_removeShutdownHook(nullptr);
static jboolean Java_Runtime_removeShutdownHook(JNIEnv* env, const base::android::JavaRef<jobject>&
    obj, const base::android::JavaRef<jobject>& p0) __attribute__ ((unused));
static jboolean Java_Runtime_removeShutdownHook(JNIEnv* env, const base::android::JavaRef<jobject>&
    obj, const base::android::JavaRef<jobject>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), false);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "removeShutdownHook",
          "(Ljava/lang/Thread;)Z",
          &g_java_lang_Runtime_removeShutdownHook);

  jboolean ret =
      env->CallBooleanMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
  return ret;
}

static std::atomic<jmethodID> g_java_lang_Runtime_halt(nullptr);
static void Java_Runtime_halt(JNIEnv* env, const base::android::JavaRef<jobject>& obj, JniIntWrapper
    p0) __attribute__ ((unused));
static void Java_Runtime_halt(JNIEnv* env, const base::android::JavaRef<jobject>& obj, JniIntWrapper
    p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "halt",
          "(I)V",
          &g_java_lang_Runtime_halt);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, as_jint(p0));
}

static std::atomic<jmethodID> g_java_lang_Runtime_runFinalizersOnExit(nullptr);
static void Java_Runtime_runFinalizersOnExit(JNIEnv* env, jboolean p0) __attribute__ ((unused));
static void Java_Runtime_runFinalizersOnExit(JNIEnv* env, jboolean p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, clazz,
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_STATIC>(
          env,
          clazz,
          "runFinalizersOnExit",
          "(Z)V",
          &g_java_lang_Runtime_runFinalizersOnExit);

     env->CallStaticVoidMethod(clazz,
          call_context.base.method_id, p0);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_JLS(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0) __attribute__
    ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "(Ljava/lang/String;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_JLS);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_JLS_LJLS(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0,
    const base::android::JavaRef<jobjectArray>& p1) __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0,
    const base::android::JavaRef<jobjectArray>& p1) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "(Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_JLS_LJLS);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_JLS_LJLS_JIF(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS_LJLS_JIF(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0,
    const base::android::JavaRef<jobjectArray>& p1,
    const base::android::JavaRef<jobject>& p2) __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_JLS_LJLS_JIF(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jstring>& p0,
    const base::android::JavaRef<jobjectArray>& p1,
    const base::android::JavaRef<jobject>& p2) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "(Ljava/lang/String;[Ljava/lang/String;Ljava/io/File;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_JLS_LJLS_JIF);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj(), p2.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_LJLS(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0)
    __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "([Ljava/lang/String;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_LJLS);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_LJLS_LJLS(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0,
    const base::android::JavaRef<jobjectArray>& p1) __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS_LJLS(JNIEnv* env, const
    base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0,
    const base::android::JavaRef<jobjectArray>& p1) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "([Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_LJLS_LJLS);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_execJLP_LJLS_LJLS_JIF(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS_LJLS_JIF(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0,
    const base::android::JavaRef<jobjectArray>& p1,
    const base::android::JavaRef<jobject>& p2) __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_execJLP_LJLS_LJLS_JIF(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobjectArray>& p0,
    const base::android::JavaRef<jobjectArray>& p1,
    const base::android::JavaRef<jobject>& p2) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "exec",
          "([Ljava/lang/String;[Ljava/lang/String;Ljava/io/File;)Ljava/lang/Process;",
          &g_java_lang_Runtime_execJLP_LJLS_LJLS_JIF);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj(), p2.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_availableProcessors(nullptr);
static jint Java_Runtime_availableProcessors(JNIEnv* env, const base::android::JavaRef<jobject>&
    obj) __attribute__ ((unused));
static jint Java_Runtime_availableProcessors(JNIEnv* env, const base::android::JavaRef<jobject>&
    obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "availableProcessors",
          "()I",
          &g_java_lang_Runtime_availableProcessors);

  jint ret =
      env->CallIntMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID> g_java_lang_Runtime_freeMemory(nullptr);
static jlong Java_Runtime_freeMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj)
    __attribute__ ((unused));
static jlong Java_Runtime_freeMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "freeMemory",
          "()J",
          &g_java_lang_Runtime_freeMemory);

  jlong ret =
      env->CallLongMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID> g_java_lang_Runtime_totalMemory(nullptr);
static jlong Java_Runtime_totalMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj)
    __attribute__ ((unused));
static jlong Java_Runtime_totalMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "totalMemory",
          "()J",
          &g_java_lang_Runtime_totalMemory);

  jlong ret =
      env->CallLongMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID> g_java_lang_Runtime_maxMemory(nullptr);
static jlong Java_Runtime_maxMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj)
    __attribute__ ((unused));
static jlong Java_Runtime_maxMemory(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), 0);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "maxMemory",
          "()J",
          &g_java_lang_Runtime_maxMemory);

  jlong ret =
      env->CallLongMethod(obj.obj(),
          call_context.base.method_id);
  return ret;
}

static std::atomic<jmethodID> g_java_lang_Runtime_gc(nullptr);
static void Java_Runtime_gc(JNIEnv* env, const base::android::JavaRef<jobject>& obj) __attribute__
    ((unused));
static void Java_Runtime_gc(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "gc",
          "()V",
          &g_java_lang_Runtime_gc);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id);
}

static std::atomic<jmethodID> g_java_lang_Runtime_runFinalization(nullptr);
static void Java_Runtime_runFinalization(JNIEnv* env, const base::android::JavaRef<jobject>& obj)
    __attribute__ ((unused));
static void Java_Runtime_runFinalization(JNIEnv* env, const base::android::JavaRef<jobject>& obj) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "runFinalization",
          "()V",
          &g_java_lang_Runtime_runFinalization);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id);
}

static std::atomic<jmethodID> g_java_lang_Runtime_traceInstructions(nullptr);
static void Java_Runtime_traceInstructions(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    jboolean p0) __attribute__ ((unused));
static void Java_Runtime_traceInstructions(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    jboolean p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "traceInstructions",
          "(Z)V",
          &g_java_lang_Runtime_traceInstructions);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0);
}

static std::atomic<jmethodID> g_java_lang_Runtime_traceMethodCalls(nullptr);
static void Java_Runtime_traceMethodCalls(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    jboolean p0) __attribute__ ((unused));
static void Java_Runtime_traceMethodCalls(JNIEnv* env, const base::android::JavaRef<jobject>& obj,
    jboolean p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "traceMethodCalls",
          "(Z)V",
          &g_java_lang_Runtime_traceMethodCalls);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0);
}

static std::atomic<jmethodID> g_java_lang_Runtime_load(nullptr);
static void Java_Runtime_load(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jstring>& p0) __attribute__ ((unused));
static void Java_Runtime_load(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jstring>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "load",
          "(Ljava/lang/String;)V",
          &g_java_lang_Runtime_load);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
}

static std::atomic<jmethodID> g_java_lang_Runtime_load0(nullptr);
static void Java_Runtime_load0(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jclass>& p0,
    const base::android::JavaRef<jstring>& p1) __attribute__ ((unused));
static void Java_Runtime_load0(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jclass>& p0,
    const base::android::JavaRef<jstring>& p1) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "load0",
          "(Ljava/lang/Class;Ljava/lang/String;)V",
          &g_java_lang_Runtime_load0);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj());
}

static std::atomic<jmethodID> g_java_lang_Runtime_loadLibrary(nullptr);
static void Java_Runtime_loadLibrary(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jstring>& p0) __attribute__ ((unused));
static void Java_Runtime_loadLibrary(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jstring>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "loadLibrary",
          "(Ljava/lang/String;)V",
          &g_java_lang_Runtime_loadLibrary);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
}

static std::atomic<jmethodID> g_java_lang_Runtime_loadLibrary0(nullptr);
static void Java_Runtime_loadLibrary0(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jclass>& p0,
    const base::android::JavaRef<jstring>& p1) __attribute__ ((unused));
static void Java_Runtime_loadLibrary0(JNIEnv* env, const base::android::JavaRef<jobject>& obj, const
    base::android::JavaRef<jclass>& p0,
    const base::android::JavaRef<jstring>& p1) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env));

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "loadLibrary0",
          "(Ljava/lang/Class;Ljava/lang/String;)V",
          &g_java_lang_Runtime_loadLibrary0);

     env->CallVoidMethod(obj.obj(),
          call_context.base.method_id, p0.obj(), p1.obj());
}

static std::atomic<jmethodID> g_java_lang_Runtime_getLocalizedInputStream(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getLocalizedInputStream(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& p0)
    __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getLocalizedInputStream(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getLocalizedInputStream",
          "(Ljava/io/InputStream;)Ljava/io/InputStream;",
          &g_java_lang_Runtime_getLocalizedInputStream);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

static std::atomic<jmethodID> g_java_lang_Runtime_getLocalizedOutputStream(nullptr);
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getLocalizedOutputStream(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& p0)
    __attribute__ ((unused));
static base::android::ScopedJavaLocalRef<jobject> Java_Runtime_getLocalizedOutputStream(JNIEnv* env,
    const base::android::JavaRef<jobject>& obj, const base::android::JavaRef<jobject>& p0) {
  jclass clazz = java_lang_Runtime_clazz(env);
  CHECK_CLAZZ(env, obj.obj(),
      java_lang_Runtime_clazz(env), NULL);

  jni_generator::JniJavaCallContextChecked call_context;
  call_context.Init<
      base::android::MethodID::TYPE_INSTANCE>(
          env,
          clazz,
          "getLocalizedOutputStream",
          "(Ljava/io/OutputStream;)Ljava/io/OutputStream;",
          &g_java_lang_Runtime_getLocalizedOutputStream);

  jobject ret =
      env->CallObjectMethod(obj.obj(),
          call_context.base.method_id, p0.obj());
  return base::android::ScopedJavaLocalRef<jobject>(env, ret);
}

}  // namespace JNI_Runtime

#endif  // java_lang_Runtime_JNI
