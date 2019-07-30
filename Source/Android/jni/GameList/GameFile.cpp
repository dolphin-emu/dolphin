// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/GameList/GameFile.h"

#include <vector>

#include <jni.h>

#include "DiscIO/Enums.h"
#include "UICommon/GameFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCodeConfig.h"

const Core::TitleDatabase* Host_GetTitleDatabase();

static const UICommon::GameFile* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<const UICommon::GameFile*>(
      env->GetLongField(obj, IDCache::sGameFile.Pointer));
}

jobject GameFileToJava(JNIEnv* env, const UICommon::GameFile* game_file)
{
  if (!game_file)
    return nullptr;

  return env->NewObject(IDCache::sGameFile.Clazz, IDCache::sGameFile.Constructor,
                        reinterpret_cast<jlong>(game_file));
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPlatform(JNIEnv* env,
                                                                                 jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetPlatform());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getName(JNIEnv* env,
                                                                                jobject obj)
{
  const Core::TitleDatabase* db = Host_GetTitleDatabase();
  if (db)
    return ToJString(env, GetPointer(env, obj)->GetName(*db));
  else
    return ToJString(env, GetPointer(env, obj)->GetName());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDescription(JNIEnv* env,
                                                                                       jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->GetDescription());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCompany(JNIEnv* env,
                                                                                   jobject obj)
{
  return ToJString(env, DiscIO::GetCompanyFromID(GetPointer(env, obj)->GetMakerID()));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCountry(JNIEnv* env,
                                                                                jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetCountry());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRegion(JNIEnv* env,
                                                                               jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetRegion());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPath(JNIEnv* env,
                                                                                jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->GetFilePath());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getTitlePath(JNIEnv* env,
                                                                                     jobject obj)
{
  u64 titleID = GetPointer(env, obj)->GetTitleID();
  std::string path = StringFromFormat("Wii/title/%08x/%08x", (u32)(titleID >> 32), (u32)titleID);
  return ToJString(env, path);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getGameId(JNIEnv* env,
                                                                                  jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->GetGameID());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getGameTdbId(JNIEnv* env,
                                                                                     jobject obj)
{
  return ToJString(env, GetPointer(env, obj)->GetGameTDBID());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDiscNumber(JNIEnv* env,
                                                                                   jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetDiscNumber());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRevision(JNIEnv* env,
                                                                                 jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetRevision());
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBanner(JNIEnv* env,
                                                                                    jobject obj)
{
  const std::vector<u32>& buffer = GetPointer(env, obj)->GetBannerImage().buffer;
  const jsize size = static_cast<jsize>(buffer.size());
  const jintArray out_array = env->NewIntArray(size);
  if (!out_array)
    return nullptr;
  env->SetIntArrayRegion(out_array, 0, size, reinterpret_cast<const jint*>(buffer.data()));
  return out_array;
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBannerWidth(JNIEnv* env,
                                                                                    jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetBannerImage().width);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBannerHeight(JNIEnv* env,
                                                                                     jobject obj)
{
  return static_cast<jint>(GetPointer(env, obj)->GetBannerImage().height);
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_parse(JNIEnv* env,
                                                                              jobject obj,
                                                                              jstring path)
{
  static std::map<std::string, std::shared_ptr<UICommon::GameFile>> file_dict;
  std::string file = GetJString(env, path);
  std::shared_ptr<UICommon::GameFile> game_file;
  auto iter = file_dict.find(file);
  if (iter != file_dict.end())
  {
    game_file = iter->second;
  }
  else
  {
    auto game_file = std::make_shared<UICommon::GameFile>(file);
    if (game_file->IsValid())
    {
      file_dict[file] = game_file;
    }
    else
    {
      game_file.reset();
    }
  }
  return GameFileToJava(env, game_file.get());
}

#ifdef __cplusplus
}
#endif
