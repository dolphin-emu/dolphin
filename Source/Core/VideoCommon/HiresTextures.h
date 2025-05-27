// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomResourceManager.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureInfo.h"

enum class TextureFormat;

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id);

class HiresTexture
{
public:
  static void Update();
  static void Clear();
  static void Shutdown();
  static std::shared_ptr<HiresTexture> Search(const TextureInfo& texture_info);

  HiresTexture(bool has_arbitrary_mipmaps, std::string id);

  bool HasArbitraryMipmaps() const { return m_has_arbitrary_mipmaps; }
  VideoCommon::CustomResourceManager::TextureTimePair LoadTexture() const;
  const std::string& GetId() const { return m_id; }

private:
  bool m_has_arbitrary_mipmaps = false;
  std::string m_id;
};
