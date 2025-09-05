// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/ShaderGenCommon.h"

struct CustomPipeline
{
  void UpdatePixelData(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                       std::span<const u32> texture_units,
                       const VideoCommon::CustomAssetLibrary::AssetID& material_to_load);
};
