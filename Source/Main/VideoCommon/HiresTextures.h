// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"

enum class TextureFormat;

class HiresTexture
{
public:
  static void Init();
  static void Update();
  static void Shutdown();

  static std::shared_ptr<HiresTexture> Search(const u8* texture, size_t texture_size,
                                              const u8* tlut, size_t tlut_size, u32 width,
                                              u32 height, TextureFormat format, bool has_mipmaps);

  static std::string GenBaseName(const u8* texture, size_t texture_size, const u8* tlut,
                                 size_t tlut_size, u32 width, u32 height, TextureFormat format,
                                 bool has_mipmaps, bool dump = false);

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

  static std::string GetTextureDirectory(const std::string& game_id);

  HiresTexture() {}
  bool m_has_arbitrary_mipmaps;
};
