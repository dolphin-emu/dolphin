// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
bool LoadTextureDataFromFile(const CustomAssetLibrary::AssetID& asset_id,
                             const std::filesystem::path& asset_path, AbstractTextureType type,
                             CustomTextureData* data);

bool ValidateTextureData(const CustomAssetLibrary::AssetID& asset_id, const CustomTextureData& data,
                         u32 native_width, u32 native_height);

bool PurgeInvalidMipsFromTextureData(const CustomAssetLibrary::AssetID& asset_id,
                                     CustomTextureData* data);
}  // namespace VideoCommon
