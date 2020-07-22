// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include <jni.h>

#include "UICommon/GameFileCache.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/GameList/GameFile.h"

namespace UICommon
{
class GameFile;
}

static UICommon::GameFileCache* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<UICommon::GameFileCache*>(
      env->GetLongField(obj, IDCache::GetGameFileCachePointer()));
}

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_newGameFileCache(
    JNIEnv* env, jobject obj, jstring path);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_finalize(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_getAllGames(JNIEnv* env, jobject obj);
JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_addOrGet(JNIEnv* env,
                                                                                      jobject obj,
                                                                                      jstring path);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_update(
    JNIEnv* env, jobject obj, jobjectArray folder_paths, jboolean recursive_scan);
JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_updateAdditionalMetadata(JNIEnv* env,
                                                                            jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_load(JNIEnv* env,
                                                                                   jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_save(JNIEnv* env,
                                                                                   jobject obj);

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_newGameFileCache(
    JNIEnv* env, jobject obj, jstring path)
{
  return reinterpret_cast<jlong>(new UICommon::GameFileCache(GetJString(env, path)));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_finalize(JNIEnv* env,
                                                                                   jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_getAllGames(JNIEnv* env, jobject obj)
{
  const UICommon::GameFileCache* ptr = GetPointer(env, obj);
  const jobjectArray array =
      env->NewObjectArray(static_cast<jsize>(ptr->GetSize()), IDCache::GetGameFileClass(), nullptr);
  jsize i = 0;
  GetPointer(env, obj)->ForEach([env, array, &i](const auto& game_file) {
    env->SetObjectArrayElement(array, i++, GameFileToJava(env, game_file));
  });
  return array;
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_addOrGet(JNIEnv* env,
                                                                                      jobject obj,
                                                                                      jstring path)
{
  bool cache_changed = false;
  return GameFileToJava(env, GetPointer(env, obj)->AddOrGet(GetJString(env, path), &cache_changed));
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_update(
    JNIEnv* env, jobject obj, jobjectArray folder_paths, jboolean recursive_scan)
{
  jsize size = env->GetArrayLength(folder_paths);

  std::vector<std::string> folder_paths_vector;
  folder_paths_vector.reserve(size);

  for (jsize i = 0; i < size; ++i)
  {
    const auto path = reinterpret_cast<jstring>(env->GetObjectArrayElement(folder_paths, i));
    folder_paths_vector.push_back(GetJString(env, path));
    env->DeleteLocalRef(path);
  }

  return GetPointer(env, obj)->Update(
      UICommon::FindAllGamePaths(folder_paths_vector, recursive_scan));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_updateAdditionalMetadata(JNIEnv* env,
                                                                            jobject obj)
{
  return GetPointer(env, obj)->UpdateAdditionalMetadata();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_load(JNIEnv* env,
                                                                                   jobject obj)
{
  return GetPointer(env, obj)->Load();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_save(JNIEnv* env,
                                                                                   jobject obj)
{
  return GetPointer(env, obj)->Save();
}

#ifdef __cplusplus
}
#endif
