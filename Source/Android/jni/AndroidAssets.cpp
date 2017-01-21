// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "jni/AndroidAssets.h"

#include <android/asset_manager.h>

#include "Common/Assert.h"

namespace AndroidAssets
{
static AAssetManager* s_asset_manager = nullptr;

void SetAssetManager(AAssetManager* asset_manager)
{
  s_asset_manager = asset_manager;
}

AAsset* OpenFile(const char* path, int mode)
{
  _assert_(s_asset_manager);
  return AAssetManager_open(s_asset_manager, path, mode);
}

AAssetDir* OpenDirectory(const char* path)
{
  _assert_(s_asset_manager);
  return AAssetManager_openDir(s_asset_manager, path);
}

}  // namespace Common
