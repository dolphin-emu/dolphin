// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidAssets.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <string>
#include <vector>

#include "Common/Assert.h"

namespace AndroidAssets
{
static AAssetManager* s_asset_manager = nullptr;
static Asset s_assets_root;

Asset::Asset(const std::string& path_) : path(path_)
{
}

const Asset* Asset::Resolve(const std::string& path_to_resolve) const
{
  // Given a path like "directory1/directory2/fileA.bin", this function will find directory1
  // and then call Resolve on directory1 using the path "directory2/fileA.bin".

  if (path.empty() || path == "/")
    return this;

  const size_t first_dir_separator = path_to_resolve.find('/');
  const std::string searching_for_filename = path.substr(0, first_dir_separator);
  const std::string rest_of_path =
      (first_dir_separator != std::string::npos) ? path.substr(first_dir_separator + 1) : "";

  const std::string searching_for_path = path + "/" + searching_for_filename;

  for (const Asset& child : children)
    if (child.path == searching_for_path)
      return child.Resolve(rest_of_path);

  return nullptr;
}

static void BuildAssetDirectory(Asset* asset_directory, JNIEnv* env, jobject asset_manager,
                                jmethodID list_method)
{
  // We don't use AAssetManager_openDir, because it seems to ignore subdirectories.
  // Instead, we have to use the equivalent Java API through JNI.

  jstring path = env->NewStringUTF(asset_directory->path.c_str());
  jobjectArray children =
      static_cast<jobjectArray>(env->CallObjectMethod(asset_manager, list_method, path));

  const jsize size = env->GetArrayLength(children);
  for (jsize i = 0; i < size; ++i)
  {
    jstring child_j = static_cast<jstring>(env->GetObjectArrayElement(children, i));
    const char* child = env->GetStringUTFChars(child_j, nullptr);
    asset_directory->children.emplace_back(asset_directory->path + "/" + child);
    env->ReleaseStringUTFChars(child_j, child);
    env->DeleteLocalRef(child_j);
  }

  env->DeleteLocalRef(children);
  env->DeleteLocalRef(path);

  for (Asset& child : asset_directory->children)
    BuildAssetDirectory(&child, env, asset_manager, list_method);
}

static void BuildAssetDirectory(Asset* asset_directory, JNIEnv* env, jobject asset_manager)
{
  const jclass class_ = env->GetObjectClass(asset_manager);
  const jmethodID list_method =
      env->GetMethodID(class_, "list", "(Ljava/lang/String;)[Ljava/lang/String;");
  BuildAssetDirectory(asset_directory, env, asset_manager, list_method);
  env->DeleteLocalRef(class_);
}

void SetAssetManager(JNIEnv* env, jobject asset_manager)
{
  s_asset_manager = AAssetManager_fromJava(env, asset_manager);

  s_assets_root = Asset();
  BuildAssetDirectory(&s_assets_root, env, asset_manager);
}

AAsset* OpenFile(const char* path, int mode)
{
  _assert_(s_asset_manager);
  return AAssetManager_open(s_asset_manager, path, mode);
}

const Asset& GetAssetsRoot()
{
  return s_assets_root;
}

}  // namespace Common
