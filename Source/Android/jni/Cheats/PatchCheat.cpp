// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "Core/PatchEngine.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

static PatchEngine::Patch* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<PatchEngine::Patch*>(
      env->GetLongField(obj, IDCache::GetPatchCheatPointer()));
}

jobject PatchCheatToJava(JNIEnv* env, const PatchEngine::Patch& patch)
{
  return env->NewObject(IDCache::GetPatchCheatClass(), IDCache::GetPatchCheatConstructor(),
                        reinterpret_cast<jlong>(new PatchEngine::Patch(patch)));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_finalize(JNIEnv* env, jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->name);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getEnabled(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->enabled);
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_setEnabledImpl(JNIEnv* env,
                                                                               jobject obj,
                                                                               jboolean enabled)
{
  GetPointer(env, obj)->enabled = static_cast<bool>(enabled);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_loadCodes(JNIEnv* env, jclass,
                                                                          jstring jGameID,
                                                                          jint revision)
{
  const std::string game_id = GetJString(env, jGameID);
  IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
  const IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, revision);

  std::vector<PatchEngine::Patch> patches;
  PatchEngine::LoadPatchSection("OnFrame", &patches, game_ini_default, game_ini_local);

  const jobjectArray array = env->NewObjectArray(static_cast<jsize>(patches.size()),
                                                 IDCache::GetPatchCheatClass(), nullptr);

  jsize i = 0;
  for (const PatchEngine::Patch& patch : patches)
    env->SetObjectArrayElement(array, i++, PatchCheatToJava(env, patch));

  return array;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_saveCodes(
    JNIEnv* env, jclass, jstring jGameID, jint revision, jobjectArray jCodes)
{
  const jsize size = env->GetArrayLength(jCodes);
  std::vector<PatchEngine::Patch> vector;
  vector.reserve(size);

  for (jsize i = 0; i < size; ++i)
  {
    jobject code = reinterpret_cast<jstring>(env->GetObjectArrayElement(jCodes, i));
    vector.emplace_back(*GetPointer(env, code));
    env->DeleteLocalRef(code);
  }

  const std::string game_id = GetJString(env, jGameID);
  const std::string ini_path = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini";

  IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  PatchEngine::SavePatchSection(&game_ini_local, vector);
  game_ini_local.Save(ini_path);
}
}
