// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static Gecko::GeckoCode* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<Gecko::GeckoCode*>(
      env->GetLongField(obj, IDCache::GetGeckoCheatPointer()));
}

jobject GeckoCheatToJava(JNIEnv* env, const Gecko::GeckoCode& code)
{
  return env->NewObject(IDCache::GetGeckoCheatClass(), IDCache::GetGeckoCheatConstructor(),
                        reinterpret_cast<jlong>(new Gecko::GeckoCode(code)));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_finalize(JNIEnv* env, jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->name);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_loadCodes(JNIEnv* env, jclass,
                                                                          jstring jGameID,
                                                                          jint revision)
{
  const std::string game_id = GetJString(env, jGameID);
  IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
  const IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, revision);

  const std::vector<Gecko::GeckoCode> codes = Gecko::LoadCodes(game_ini_default, game_ini_local);

  const jobjectArray array =
      env->NewObjectArray(static_cast<jsize>(codes.size()), IDCache::GetGeckoCheatClass(), nullptr);

  jsize i = 0;
  for (const Gecko::GeckoCode& code : codes)
    env->SetObjectArrayElement(array, i++, GeckoCheatToJava(env, code));

  return array;
}
}
