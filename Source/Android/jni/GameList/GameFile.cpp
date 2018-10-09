// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/GameList/GameFile.h"

#include <memory>
#include <utility>
#include <vector>

#include <jni.h>

#include "DiscIO/Enums.h"
#include "UICommon/GameFile.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

#include "Common/IniFile.h"
#include "Core/ActionReplay.h"
#include "Core/GeckoCodeConfig.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

static std::shared_ptr<const UICommon::GameFile>* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<std::shared_ptr<const UICommon::GameFile>*>(
      env->GetLongField(obj, IDCache::GetGameFilePointer()));
}

static std::shared_ptr<const UICommon::GameFile>& GetRef(JNIEnv* env, jobject obj)
{
  return *GetPointer(env, obj);
}

jobject GameFileToJava(JNIEnv* env, std::shared_ptr<const UICommon::GameFile> game_file)
{
  if (!game_file)
    return nullptr;

  return env->NewObject(
      IDCache::GetGameFileClass(), IDCache::GetGameFileConstructor(),
      reinterpret_cast<jlong>(new std::shared_ptr<const UICommon::GameFile>(std::move(game_file))));
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_finalize(JNIEnv* env,
                                                                              jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPlatform(JNIEnv* env,
                                                                                 jobject obj)
{
  return static_cast<jint>(GetRef(env, obj)->GetPlatform());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getTitle(JNIEnv* env,
                                                                                 jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetName());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDescription(JNIEnv* env,
                                                                                       jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetDescription());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCompany(JNIEnv* env,
                                                                                   jobject obj)
{
  return ToJString(env, DiscIO::GetCompanyFromID(GetRef(env, obj)->GetMakerID()));
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCountry(JNIEnv* env,
                                                                                jobject obj)
{
  return static_cast<jint>(GetRef(env, obj)->GetCountry());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRegion(JNIEnv* env,
                                                                               jobject obj)
{
  return static_cast<jint>(GetRef(env, obj)->GetRegion());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPath(JNIEnv* env,
                                                                                jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetFilePath());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getGameId(JNIEnv* env,
                                                                                  jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetGameID());
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBanner(JNIEnv* env,
                                                                                    jobject obj)
{
  const std::vector<u32>& buffer = GetRef(env, obj)->GetBannerImage().buffer;
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
  return static_cast<jint>(GetRef(env, obj)->GetBannerImage().width);
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBannerHeight(JNIEnv* env,
                                                                                     jobject obj)
{
  return static_cast<jint>(GetRef(env, obj)->GetBannerImage().height);
}

static struct {
    std::string game_id;
    u16 game_revision;
    std::vector<ActionReplay::ARCode> ar_codes;
    std::vector<Gecko::GeckoCode> gecko_codes;

    void LoadGameIni(const std::string& id, u16 revision)
    {
      if(game_id != id || game_revision != revision)
      {
        IniFile game_ini_default = SConfig::GetInstance().LoadDefaultGameIni(game_id, game_revision);
        IniFile game_ini_local = SConfig::GetInstance().LoadLocalGameIni(game_id, game_revision);
        game_id = id;
        game_revision = revision;
        ar_codes = ActionReplay::LoadCodes(game_ini_default, game_ini_local);
        gecko_codes = Gecko::LoadCodes(game_ini_default, game_ini_local);
      }
    }
} s_game_code_cache;

JNIEXPORT jobjectArray JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCodes(JNIEnv* env,
                                                                                        jobject obj)
{
  const std::shared_ptr<const UICommon::GameFile>& game_file = GetRef(env, obj);
  s_game_code_cache.LoadGameIni(game_file->GetGameID(), game_file->GetRevision());

  jsize ar_size = (jsize)s_game_code_cache.ar_codes.size();
  jsize gc_size = (jsize)s_game_code_cache.gecko_codes.size();

  jobjectArray list = (jobjectArray) env->NewObjectArray(
    ar_size + gc_size,
    env->FindClass("java/lang/String"),
    ToJString(env, ""));

  for(int i = 0; i < ar_size; ++i)
  {
    env->SetObjectArrayElement(list, i, ToJString(env, s_game_code_cache.ar_codes[i].name));
  }

  for(int i = 0; i < gc_size; ++i)
  {
    env->SetObjectArrayElement(list, i, ToJString(env, s_game_code_cache.gecko_codes[i].name));
  }

  return list;
}

#ifdef __cplusplus
}
#endif
