// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <span>

struct CustomPipeline
{
  void UpdatePixelData(const VideoCommon::CustomAssetLibrary* library,
                       std::span<const u32> texture_units,
                       const VideoCommon::CustomAssetLibrary::AssetID& material_to_load);
};
