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
                                                                              jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPlatform(JNIEnv* env,
                                                                                 jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getTitle(JNIEnv* env,
                                                                                 jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDescription(JNIEnv* env,
                                                                                       jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCompany(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getCountry(JNIEnv* env,
                                                                                jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRegion(JNIEnv* env,
                                                                               jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getPath(JNIEnv* env,
                                                                                jobject obj);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getGameId(JNIEnv* env,
                                                                                  jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDiscNumber(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRevision(JNIEnv* env,
                                                                                 jobject obj);
JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBanner(JNIEnv* env,
                                                                                    jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBannerWidth(JNIEnv* env,
                                                                                    jobject obj);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBannerHeight(JNIEnv* env,
                                                                                     jobject obj);

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

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getGameTdbId(JNIEnv* env,
                                                                                     jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetGameTDBID());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDiscNumber(JNIEnv* env,
                                                                                   jobject obj)
{
  return env, GetRef(env, obj)->GetDiscNumber();
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRevision(JNIEnv* env,
                                                                                 jobject obj)
{
  return env, GetRef(env, obj)->GetRevision();
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

#ifdef __cplusplus
}
#endif
