// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/GameList/GameFile.h"

#include <memory>
#include <utility>
#include <vector>

#include <jni.h>

#include "DiscIO/Blob.h"
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

extern "C" {

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
  return ToJString(env,
                   GetRef(env, obj)->GetName(UICommon::GameFile::Variant::LongAndPossiblyCustom));
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getDescription(JNIEnv* env,
                                                                                       jobject obj)
{
  return ToJString(
      env, GetRef(env, obj)->GetDescription(UICommon::GameFile::Variant::LongAndPossiblyCustom));
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
  return GetRef(env, obj)->GetDiscNumber();
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getRevision(JNIEnv* env,
                                                                                 jobject obj)
{
  return GetRef(env, obj)->GetRevision();
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBlobType(JNIEnv* env,
                                                                                 jobject obj)
{
  return static_cast<jint>(GetRef(env, obj)->GetBlobType());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFile_getFileFormatName(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetFileFormatName());
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBlockSize(JNIEnv* env,
                                                                                   jobject obj)
{
  return GetRef(env, obj)->GetBlockSize();
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFile_getCompressionMethod(JNIEnv* env, jobject obj)
{
  return ToJString(env, GetRef(env, obj)->GetCompressionMethod());
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFile_shouldShowFileFormatDetails(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetRef(env, obj)->ShouldShowFileFormatDetails());
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFile_shouldAllowConversion(JNIEnv* env, jobject obj)
{
  return static_cast<jboolean>(GetRef(env, obj)->ShouldAllowConversion());
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getFileSize(JNIEnv* env,
                                                                                  jobject obj)
{
  return GetRef(env, obj)->GetFileSize();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_isDatelDisc(JNIEnv* env,
                                                                                     jobject obj)
{
  return static_cast<jboolean>(GetRef(env, obj)->IsDatelDisc());
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_isNKit(JNIEnv* env,
                                                                                jobject obj)
{
  return static_cast<jboolean>(GetRef(env, obj)->IsNKit());
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_getBanner(JNIEnv* env,
                                                                                    jobject obj)
{
  const std::vector<u32>& buffer = GetRef(env, obj)->GetBannerImage().buffer;
  const auto size = static_cast<jsize>(buffer.size());
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

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_GameFile_parse(JNIEnv* env, jclass,
                                                                              jstring path)
{
  auto game_file = std::make_shared<UICommon::GameFile>(GetJString(env, path));
  if (!game_file->IsValid())
    game_file.reset();

  return GameFileToJava(env, game_file);
}
}
