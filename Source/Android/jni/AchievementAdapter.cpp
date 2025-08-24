// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "Common/Event.h"
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
  auto login_complete_event = std::make_shared<Common::Event>();
  instance.SetUpdateCallback(
      [&login_complete_event, &success](AchievementManager::UpdatedItems updated_items) {
        if (!updated_items.all)
        {
          success = (updated_items.failed_login_code == 0);
          login_complete_event->Set();
          AchievementManager::GetInstance().SetUpdateCallback({});
        }
      });
  instance.Login(GetJString(env, password));
  login_complete_event->Wait();
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
