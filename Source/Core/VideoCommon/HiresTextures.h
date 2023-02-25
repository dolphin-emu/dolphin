// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/TextureInfo.h"

enum class TextureFormat;

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id);

class HiresTexture
{
public:
  static void Init();
  static void Update();
  static void Clear();
  static void Shutdown();

  static std::shared_ptr<HiresTexture> Search(const TextureInfo& texture_info);

  static std::string GenBaseName(const TextureInfo& texture_info, bool dump = false);

  static u32 CalculateMipCount(u32 width, u32 height);

  ~HiresTexture();

  AbstractTextureFormat GetFormat() const;
  bool HasArbitraryMipmaps() const;

  struct Level
  {
    std::vector<u8> data;
    AbstractTextureFormat format = AbstractTextureFormat::RGBA8;
    u32 width = 0;
    u32 height = 0;
    u32 row_length = 0;
  };
  std::vector<Level> m_levels;

private:
  static std::unique_ptr<HiresTexture> Load(const std::string& base_filename, u32 width,
                                            u32 height);
  static bool LoadDDSTexture(HiresTexture* tex, const std::string& filename);
  static bool LoadDDSTexture(Level& level, const std::string& filename, u32 mip_level);
  static bool LoadTexture(Level& level, const std::vector<u8>& buffer);
  static void Prefetch();

  HiresTexture() = default;
  bool m_has_arbitrary_mipmaps = false;
};
