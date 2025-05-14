// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Common/Logging/Log.h"
#include "Core/AchievementManager.h"
#include "UICommon/UICommon.h"
#include "jni/Host.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_ActivityTracker_setBackgroundExecutionAllowedNative(
    JNIEnv*, jclass, jboolean allowed)
{
  // This is called with allowed == false when the app goes into the background.
  // We use this to stop continuously running background threads so we don't waste battery.

  INFO_LOG_FMT(CORE, "SetBackgroundExecutionAllowed {}", allowed);
  AchievementManager::GetInstance().SetBackgroundExecutionAllowed(allowed);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_utils_ActivityTracker_flushUnsavedData(JNIEnv*, jclass)
{
  HostThreadLock guard;
  UICommon::FlushUnsavedData();
}
}
