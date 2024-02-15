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
#include "jni/Cheats/Cheats.h"

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

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_createNew(JNIEnv* env, jobject obj)
{
  auto* code = new Gecko::GeckoCode;
  code->user_defined = true;
  return reinterpret_cast<jlong>(code);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->name);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getCreator(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->creator);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getNotes(JNIEnv* env, jobject obj)
{
  return ToJString(env, JoinStrings(GetPointer(env, obj)->notes, "\n"));
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getCode(JNIEnv* env, jobject obj)
{
  Gecko::GeckoCode* code = GetPointer(env, obj);

  std::string code_string;

  for (size_t i = 0; i < code->codes.size(); ++i)
  {
    if (i != 0)
      code_string += '\n';

    code_string += code->codes[i].original_line;
  }

  return ToJString(env, code_string);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getUserDefined(JNIEnv* env,
                                                                               jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->user_defined);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_getEnabled(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->enabled);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_equalsImpl(JNIEnv* env, jobject obj,
                                                                           jobject other)
{
  return *GetPointer(env, obj) == *GetPointer(env, other);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_setCheatImpl(
    JNIEnv* env, jobject obj, jstring name, jstring creator, jstring notes, jstring code_string)
{
  Gecko::GeckoCode* code = GetPointer(env, obj);

  std::vector<Gecko::GeckoCode::Code> entries;

  std::vector<std::string> lines = SplitString(GetJString(env, code_string), '\n');

  for (size_t i = 0; i < lines.size(); i++)
  {
    const std::string& line = lines[i];

    if (line.empty())
      continue;

    if (std::optional<Gecko::GeckoCode::Code> c = Gecko::DeserializeLine(line))
      entries.emplace_back(*std::move(c));
    else
      return i + 1;  // Parse error on line i
  }

  if (entries.empty())
    return Cheats::TRY_SET_FAIL_NO_CODE_LINES;

  code->name = GetJString(env, name);
  code->creator = GetJString(env, creator);
  code->notes = SplitString(GetJString(env, notes), '\n');
  code->codes = std::move(entries);

  return Cheats::TRY_SET_SUCCESS;
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_setEnabledImpl(JNIEnv* env,
                                                                               jobject obj,
                                                                               jboolean enabled)
{
  GetPointer(env, obj)->enabled = static_cast<bool>(enabled);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_loadCodes(JNIEnv* env, jclass,
                                                                          jstring jGameID,
                                                                          jint revision)
{
  const std::string game_id = GetJString(env, jGameID);
  Common::IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
  const Common::IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, revision);

  const std::vector<Gecko::GeckoCode> codes = Gecko::LoadCodes(game_ini_default, game_ini_local);

  return VectorToJObjectArray(env, codes, IDCache::GetGeckoCheatClass(), GeckoCheatToJava);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_saveCodes(
    JNIEnv* env, jclass, jstring jGameID, jint revision, jobjectArray jCodes)
{
  const jsize size = env->GetArrayLength(jCodes);
  std::vector<Gecko::GeckoCode> vector;
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
  Gecko::SaveCodes(game_ini_local, vector);
  game_ini_local.Save(ini_path);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_GeckoCheat_downloadCodes(JNIEnv* env, jclass,
                                                                              jstring jGameTdbId)
{
  const std::string gametdb_id = GetJString(env, jGameTdbId);

  bool success = true;
  const std::vector<Gecko::GeckoCode> codes = Gecko::DownloadCodes(gametdb_id, &success, false);

  if (!success)
    return nullptr;

  const jobjectArray array =
      env->NewObjectArray(static_cast<jsize>(codes.size()), IDCache::GetGeckoCheatClass(), nullptr);

  jsize i = 0;
  for (const Gecko::GeckoCode& code : codes)
    env->SetObjectArrayElement(array, i++, GeckoCheatToJava(env, code));

  return array;
}
}
