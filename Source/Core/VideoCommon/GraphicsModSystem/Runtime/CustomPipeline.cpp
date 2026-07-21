// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

#include <span>

void CustomPipeline::UpdatePixelData(const VideoCommon::CustomAssetLibrary*, std::span<const u32>,
                                     const VideoCommon::CustomAssetLibrary::AssetID&)
{
}
