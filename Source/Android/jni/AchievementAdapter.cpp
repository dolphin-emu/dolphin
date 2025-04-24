// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Core/AchievementManager.h"
#include "jni/AndroidCommon/AndroidCommon.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_init(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Init(nullptr);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_shutdown(JNIEnv* env,
                                                                                 jclass)
{
  AchievementManager::GetInstance().Shutdown();
}

}  // extern "C"
