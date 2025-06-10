// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include <latch>
#include "Common/Event.h"
#include "Common/HookableEvent.h"
#include "Core/AchievementManager.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_init(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Init(nullptr);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_login(JNIEnv* env, jclass,
                                                                              jstring password)
{
  auto& instance = AchievementManager::GetInstance();
  bool success;
  std::latch login_complete_event{1};
  Common::EventHook login_hook =
      instance.login_event.Register([&login_complete_event, &success](int result) {
        success = (result == RC_OK);
        login_complete_event.count_down();
      });
  instance.Login(GetJString(env, password));
  login_complete_event.wait();
  return success;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_logout(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Logout();
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_isHardcoreModeActive(
    JNIEnv* env, jclass)
{
  return AchievementManager::GetInstance().IsHardcoreModeActive();
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_shutdown(JNIEnv* env,
                                                                                 jclass)
{
  AchievementManager::GetInstance().Shutdown();
}

}  // extern "C"
