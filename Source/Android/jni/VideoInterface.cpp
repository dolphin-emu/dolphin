// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>

#include <jni.h>

#include "Core/HW/VideoInterface.h"
#include "Core/System.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/EventHook.h"

extern "C" {

JNIEXPORT jdouble JNICALL
Java_org_dolphinemu_dolphinemu_model_VideoInterface_getTargetRefreshRate(JNIEnv* env, jclass)
{
  return Core::System::GetInstance().GetVideoInterface().GetTargetRefreshRate();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_VideoInterface_observeTargetRefreshRate(
    JNIEnv* env, jclass, jobject owner, jobject observer)
{
  jobject global_observer = env->NewGlobalRef(observer);

  auto& video_interface = Core::System::GetInstance().GetVideoInterface();
  Common::EventHook event_hook = video_interface.RegisterTargetRefreshRateChangedCallback(
      [global_observer](double refresh_rate) {
        JNIEnv* env = IDCache::GetEnvForThread();
        jobject boxed_refresh_rate = env->NewObject(IDCache::GetDoubleClass(),
                                                    IDCache::GetDoubleConstructor(), refresh_rate);
        env->CallVoidMethod(global_observer, IDCache::GetObserverOnChanged(), boxed_refresh_rate);
      });

  ConnectEventHookToLifecycle(env, std::move(event_hook), owner, global_observer);
}
}
