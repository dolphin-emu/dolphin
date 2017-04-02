// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct AAsset;

#include <jni.h>
#include <string>
#include <vector>

namespace AndroidAssets
{
struct Asset
{
  Asset(const std::string& path = "");

  std::string path;
  std::vector<Asset> children;

  // Returns nullptr if not found
  const Asset* Resolve(const std::string& path_to_resolve) const;
};

void SetAssetManager(JNIEnv* env, jobject asset_manager);

// Wrapper of AAssetManager_open. Uses the AAssetManager set by SetAssetManager.
AAsset* OpenFile(const char* path, int mode);

const Asset& GetAssetsRoot();

}  // namespace Common
