// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/AchievementManager.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Cheats/Cheats.h"

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_login(JNIEnv* env, jclass,
                                                                              jstring password)
{
  AchievementManager::GetInstance().Login(GetJString(env, password));
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_AchievementModel_logout(JNIEnv* env, jclass)
{
  AchievementManager::GetInstance().Logout();
}

}  // extern "C"
