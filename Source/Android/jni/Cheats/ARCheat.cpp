// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ARDecrypt.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/Cheats/Cheats.h"

static ActionReplay::ARCode* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ActionReplay::ARCode*>(
      env->GetLongField(obj, IDCache::GetARCheatPointer()));
}

jobject ARCheatToJava(JNIEnv* env, const ActionReplay::ARCode& code)
{
  return env->NewObject(IDCache::GetARCheatClass(), IDCache::GetARCheatConstructor(),
                        reinterpret_cast<jlong>(new ActionReplay::ARCode(code)));
}

extern "C" {

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_finalize(JNIEnv* env, jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_createNew(JNIEnv* env, jobject obj)
{
  auto* code = new ActionReplay::ARCode;
  code->user_defined = true;
  return reinterpret_cast<jlong>(code);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_getName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->name);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_getCode(JNIEnv* env, jobject obj)
{
  ActionReplay::ARCode* code = GetPointer(env, obj);

  std::string code_string;

  for (size_t i = 0; i < code->ops.size(); ++i)
  {
    if (i != 0)
      code_string += '\n';

    code_string += ActionReplay::SerializeLine(code->ops[i]);
  }

  return ToJString(env, code_string);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_getUserDefined(JNIEnv* env,
                                                                            jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->user_defined);
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_getEnabled(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetPointer(env, obj)->enabled);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_setCheatImpl(
    JNIEnv* env, jobject obj, jstring name, jstring creator, jstring notes, jstring code_string)
{
  ActionReplay::ARCode* code = GetPointer(env, obj);

  std::vector<ActionReplay::AREntry> entries;
  std::vector<std::string> encrypted_lines;

  std::vector<std::string> lines = SplitString(GetJString(env, code_string), '\n');

  for (size_t i = 0; i < lines.size(); i++)
  {
    const std::string& line = lines[i];

    if (line.empty())
      continue;

    auto parse_result = ActionReplay::DeserializeLine(line);

    if (std::holds_alternative<ActionReplay::AREntry>(parse_result))
      entries.emplace_back(std::get<ActionReplay::AREntry>(std::move(parse_result)));
    else if (std::holds_alternative<ActionReplay::EncryptedLine>(parse_result))
      encrypted_lines.emplace_back(std::get<ActionReplay::EncryptedLine>(std::move(parse_result)));
    else
      return i + 1;  // Parse error on line i
  }

  if (!encrypted_lines.empty())
  {
    if (!entries.empty())
      return Cheats::TRY_SET_FAIL_CODE_MIXED_ENCRYPTION;

    ActionReplay::DecryptARCode(encrypted_lines, &entries);
  }

  if (entries.empty())
    return Cheats::TRY_SET_FAIL_NO_CODE_LINES;

  code->name = GetJString(env, name);
  code->ops = std::move(entries);

  return Cheats::TRY_SET_SUCCESS;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_setEnabledImpl(
    JNIEnv* env, jobject obj, jboolean enabled)
{
  GetPointer(env, obj)->enabled = static_cast<bool>(enabled);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_loadCodes(JNIEnv* env, jclass,
                                                                       jstring jGameID,
                                                                       jint revision)
{
  const std::string game_id = GetJString(env, jGameID);
  Common::IniFile game_ini_local;

  // We don't use LoadLocalGameIni() here because user cheat codes that are installed via the UI
  // will always be stored in GS/${GAMEID}.ini
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
  const Common::IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, revision);

  const std::vector<ActionReplay::ARCode> codes =
      ActionReplay::LoadCodes(game_ini_default, game_ini_local);

  return VectorToJObjectArray(env, codes, IDCache::GetARCheatClass(), ARCheatToJava);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_features_cheats_model_ARCheat_saveCodes(
    JNIEnv* env, jclass, jstring jGameID, jint revision, jobjectArray jCodes)
{
  const jsize size = env->GetArrayLength(jCodes);
  std::vector<ActionReplay::ARCode> vector;
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
  ActionReplay::SaveCodes(&game_ini_local, vector);
  game_ini_local.Save(ini_path);
}
}
