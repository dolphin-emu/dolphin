// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidCommon/IDCache.h"
#include "UICommon/UICommon.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include <jni.h>

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

static JavaVM* s_java_vm;

static jclass s_native_library_class;
static jmethodID s_display_alert_msg;
static jmethodID s_rumble_output_method;

static jclass s_game_file_class;
static jfieldID s_game_file_pointer;
static jmethodID s_game_file_constructor;

static jclass s_game_file_cache_class;
static jfieldID s_game_file_cache_pointer;

namespace IDCache
{
JavaVM* GetJavaVM()
{
  return s_java_vm;
}

jclass GetNativeLibraryClass()
{
  return s_native_library_class;
}

jmethodID GetDisplayAlertMsg()
{
  return s_display_alert_msg;
}

jmethodID GetRumbleOutputMethod()
{
  return s_rumble_output_method;
}

jclass GetGameFileClass()
{
  return s_game_file_class;
}

jfieldID GetGameFilePointer()
{
  return s_game_file_pointer;
}

jmethodID GetGameFileConstructor()
{
  return s_game_file_constructor;
}

jclass GetGameFileCacheClass()
{
  return s_game_file_cache_class;
}

jfieldID GetGameFileCachePointer()
{
  return s_game_file_cache_pointer;
}

}  // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  s_java_vm = vm;

  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
    return JNI_ERR;

  const jclass native_library_class = env->FindClass("org/dolphinemu/dolphinemu/NativeLibrary");
  s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
  s_display_alert_msg = env->GetStaticMethodID(s_native_library_class, "displayAlertMsg",
                                               "(Ljava/lang/String;Ljava/lang/String;Z)Z");
  s_rumble_output_method = env->GetStaticMethodID(s_native_library_class, "rumble", "(ID)V");

  const jclass game_file_class = env->FindClass("org/dolphinemu/dolphinemu/model/GameFile");
  s_game_file_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_file_class));
  s_game_file_pointer = env->GetFieldID(game_file_class, "mPointer", "J");
  s_game_file_constructor = env->GetMethodID(game_file_class, "<init>", "(J)V");

  const jclass game_file_cache_class =
      env->FindClass("org/dolphinemu/dolphinemu/model/GameFileCache");
  s_game_file_cache_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_file_cache_class));
  s_game_file_cache_pointer = env->GetFieldID(game_file_cache_class, "mPointer", "J");

  WiimoteReal::InitAdapterClass();

  return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved)
{
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
    return;

  UICommon::Shutdown();

  env->DeleteGlobalRef(s_native_library_class);
  env->DeleteGlobalRef(s_game_file_class);
  env->DeleteGlobalRef(s_game_file_cache_class);
}

#ifdef __cplusplus
}
#endif
