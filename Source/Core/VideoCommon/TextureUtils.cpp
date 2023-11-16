// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureUtils.h"

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"

#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon::TextureUtils
{
void DumpTexture(const ::AbstractTexture& texture, std::string basename, u32 level,
                 bool is_arbitrary)
{
  std::string szDir = File::GetUserPath(D_DUMPTEXTURES_IDX) + SConfig::GetInstance().GetGameID();

  // make sure that the directory exists
  if (!File::IsDirectory(szDir))
    File::CreateDir(szDir);

  if (is_arbitrary)
  {
    basename += "_arb";
  }

  if (level > 0)
  {
    basename += fmt::format("_mip{}", level);
  }

  const std::string filename = fmt::format("{}/{}.png", szDir, basename);
  if (File::Exists(filename))
    return;

  texture.Save(filename, level, Config::Get(Config::GFX_TEXTURE_PNG_COMPRESSION_LEVEL));
}
}  // namespace VideoCommon::TextureUtils
