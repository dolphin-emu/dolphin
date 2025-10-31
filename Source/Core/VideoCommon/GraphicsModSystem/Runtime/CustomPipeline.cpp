// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"

#include <algorithm>
#include <array>
#include <variant>

#include "Common/Contains.h"
#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"

namespace
{
bool IsQualifier(std::string_view value)
{
  static constexpr std::array<std::string_view, 7> qualifiers = {
      "attribute", "const", "highp", "lowp", "mediump", "uniform", "varying",
  };
  return Common::Contains(qualifiers, value);
}

bool IsBuiltInMacro(std::string_view value)
{
  static constexpr std::array<std::string_view, 5> built_in = {
      "__LINE__", "__FILE__", "__VERSION__", "GL_core_profile", "GL_compatibility_profile",
  };
  return Common::Contains(built_in, value);
}

}  // namespace

void CustomPipeline::UpdatePixelData(std::shared_ptr<VideoCommon::CustomAssetLibrary>,
                                     std::span<const u32>,
                                     const VideoCommon::CustomAssetLibrary::AssetID&)
{
}
