// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

struct CustomPipeline
{
  void UpdatePixelData(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                       std::span<const u32> texture_units,
                       const VideoCommon::CustomAssetLibrary::AssetID& material_to_load);
};
