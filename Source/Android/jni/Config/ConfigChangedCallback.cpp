// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Common/Config/Config.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

struct ConfigChangedCallbackContext
{
  jobject runnable;
  Config::ConfigChangedCallbackID callback_id;
};

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ConfigChangedCallback_initialize(
    JNIEnv* env, jclass, jobject runnable)
{
  auto* context = new ConfigChangedCallbackContext;

  jobject runnable_global = env->NewGlobalRef(runnable);
  context->runnable = runnable_global;
  context->callback_id = Config::AddConfigChangedCallback([runnable_global] {
    IDCache::GetEnvForThread()->CallVoidMethod(runnable_global, IDCache::GetRunnableRun());
  });

  return reinterpret_cast<jlong>(context);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ConfigChangedCallback_deinitialize(
    JNIEnv* env, jclass, jlong pointer)
{
  auto* context = reinterpret_cast<ConfigChangedCallbackContext*>(pointer);

  Config::RemoveConfigChangedCallback(context->callback_id);
  env->DeleteGlobalRef(context->runnable);

  delete context;
}
}
