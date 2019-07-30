// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include <jni.h>

#include "UICommon/GameFile.h"
#include "UICommon/GameFileCache.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"
#include "jni/GameList/GameFile.h"

static std::shared_ptr<UICommon::GameFileCache> g_game_file_cache;

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_init(JNIEnv* env,
                                                                               jobject obj)
{
  g_game_file_cache = std::make_shared<UICommon::GameFileCache>();
}

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_getAllGames(JNIEnv* env, jobject obj)
{
  const jobjectArray array = env->NewObjectArray(static_cast<jsize>(g_game_file_cache->GetSize()),
                                                 IDCache::sGameFile.Clazz, nullptr);
  jsize i = 0;
  g_game_file_cache->ForEach([env, array, &i](const auto& game_file) {
    env->SetObjectArrayElement(array, i++, GameFileToJava(env, game_file.get()));
  });
  return array;
}

JNIEXPORT jobject JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_addOrGet(JNIEnv* env,
                                                                                      jobject obj,
                                                                                      jstring path)
{
  bool cache_changed = false;
  return GameFileToJava(env,
                        g_game_file_cache->AddOrGet(GetJString(env, path), &cache_changed).get());
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_update(
    JNIEnv* env, jobject obj, jobjectArray folder_paths)
{
  jsize size = env->GetArrayLength(folder_paths);

  std::vector<std::string> folder_paths_vector;
  folder_paths_vector.reserve(size);

  for (jsize i = 0; i < size; ++i)
  {
    const jstring path = reinterpret_cast<jstring>(env->GetObjectArrayElement(folder_paths, i));
    folder_paths_vector.push_back(GetJString(env, path));
    env->DeleteLocalRef(path);
  }

  return g_game_file_cache->Update(UICommon::FindAllGamePaths(folder_paths_vector, false));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_model_GameFileCache_updateAdditionalMetadata(JNIEnv* env,
                                                                            jobject obj)
{
  return g_game_file_cache->UpdateAdditionalMetadata();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_load(JNIEnv* env,
                                                                                   jobject obj)
{
  return g_game_file_cache->Load();
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_model_GameFileCache_save(JNIEnv* env,
                                                                                   jobject obj)
{
  return g_game_file_cache->Save();
}

#ifdef __cplusplus
}
#endif
