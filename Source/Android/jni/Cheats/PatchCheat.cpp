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
#include "jni/Cheats/Cheats.h"

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

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_createNew(JNIEnv* env, jobject obj)
{
  auto* patch = new PatchEngine::Patch;
  patch->user_defined = true;
  return reinterpret_cast<jlong>(patch);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->name);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getCode(JNIEnv* env, jobject obj)
{
  PatchEngine::Patch* patch = GetPointer(env, obj);

  std::string code_string;

  for (size_t i = 0; i < patch->entries.size(); ++i)
  {
    if (i != 0)
      code_string += '\n';

    code_string += PatchEngine::SerializeLine(patch->entries[i]);
  }

  return ToJString(env, code_string);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getUserDefined(JNIEnv* env,
                                                                               jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->user_defined);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_getEnabled(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->enabled);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_PatchCheat_setCheatImpl(
    JNIEnv* env, jobject obj, jstring name, jstring creator, jstring notes, jstring code_string)
{
  PatchEngine::Patch* patch = GetPointer(env, obj);

  std::vector<PatchEngine::PatchEntry> entries;

  std::vector<std::string> lines = SplitString(GetJString(env, code_string), '\n');

  for (size_t i = 0; i < lines.size(); i++)
  {
    const std::string& line = lines[i];

    if (line.empty())
      continue;

    if (std::optional<PatchEngine::PatchEntry> entry = PatchEngine::DeserializeLine(line))
      entries.emplace_back(*std::move(entry));
    else
      return i + 1;  // Parse error on line i
  }

  if (entries.empty())
    return Cheats::TRY_SET_FAIL_NO_CODE_LINES;

  patch->name = GetJString(env, name);
  patch->entries = std::move(entries);

  return Cheats::TRY_SET_SUCCESS;
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
  Common::IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
  const Common::IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, revision);

  std::vector<PatchEngine::Patch> patches;
  PatchEngine::LoadPatchSection("OnFrame", &patches, game_ini_default, game_ini_local);

  return VectorToJObjectArray(env, patches, IDCache::GetPatchCheatClass(), PatchCheatToJava);
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

  Common::IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  PatchEngine::SavePatchSection(&game_ini_local, vector);
  game_ini_local.Save(ini_path);
}
}
