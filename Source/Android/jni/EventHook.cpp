// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/EventHook.h"

#include <jni.h>

#include "Common/HookableEvent.h"
#include "jni/AndroidCommon/IDCache.h"

void ConnectEventHookToLifecycle(JNIEnv* env, Common::EventHook&& event_hook,
                                 jobject lifecycle_owner, jobject global_reference)
{
  env->NewObject(IDCache::GetEventHookLifecycleObserverClass(),
                 IDCache::GetEventHookLifecycleObserverConstructor(), lifecycle_owner,
                 reinterpret_cast<jlong>(event_hook.release()),
                 reinterpret_cast<jlong>(global_reference));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_EventHookLifecycleObserver_deleteEventHook(JNIEnv* env,
                                                                                jobject,
                                                                                jlong pointer)
{
  delete reinterpret_cast<Common::HookBase*>(pointer);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_EventHookLifecycleObserver_deleteGlobalReference(JNIEnv* env,
                                                                                      jobject,
                                                                                      jlong pointer)
{
  env->DeleteGlobalRef(reinterpret_cast<jobject>(pointer));
}
}
