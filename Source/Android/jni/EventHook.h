// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

#include "Common/HookableEvent.h"

// Takes ownership of an EventHook and makes sure it gets destroyed when the lifecycle enters the
// destroy state.
//
// global_reference is optional and can be set to any JNI global reference. If it's set,
// DeleteGlobalRef will be called for it when the lifecycle enters the destroy state.
void ConnectEventHookToLifecycle(JNIEnv* env, Common::EventHook&& event_hook,
                                 jobject lifecycle_owner, jobject global_reference = nullptr);
