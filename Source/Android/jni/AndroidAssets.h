// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

struct AAsset;
struct AAssetDir;
struct AAssetManager;

namespace AndroidAssets
{
void SetAssetManager(AAssetManager* asset_manager);

// Wrapper of AAssetManager_open. Uses the AAssetManager set by SetAssetManager.
AAsset* OpenFile(const char* path, int mode);

// Wrapper of AAssetManager_openDir. Uses the AAssetManager set by SetAssetManager.
AAssetDir* OpenDirectory(const char* path);

}  // namespace Common
