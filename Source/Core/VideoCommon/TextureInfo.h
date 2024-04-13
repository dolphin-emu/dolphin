// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

enum class TextureFormat;
enum class TLUTFormat;

class TextureInfo
{
public:
  static TextureInfo FromStage(u32 stage);
  TextureInfo(u32 stage, std::span<const u8> data, std::span<const u8> tlut_data, u32 address,
              TextureFormat texture_format, TLUTFormat tlut_format, u32 width, u32 height,
              bool from_tmem, std::span<const u8> tmem_odd, std::span<const u8> tmem_even,
              std::optional<u32> mip_count);

  struct NameDetails
  {
    std::string base_name;
    std::string texture_name;
    std::string tlut_name;
    std::string format_name;

    std::string GetFullName() const;
  };
  NameDetails CalculateTextureName() const;

  bool IsDataValid() const;

  const u8* GetData() const;
  const u8* GetTlutAddress() const;

  u32 GetRawAddress() const;

  bool IsFromTmem() const;
  const u8* GetTmemOddAddress() const;

  TextureFormat GetTextureFormat() const;
  TLUTFormat GetTlutFormat() const;

  std::optional<u32> GetPaletteSize() const;
  u32 GetTextureSize() const;

  u32 GetBlockWidth() const;
  u32 GetBlockHeight() const;

  u32 GetExpandedWidth() const;
  u32 GetExpandedHeight() const;

  u32 GetRawWidth() const;
  u32 GetRawHeight() const;

  u32 GetStage() const;

  class MipLevel
  {
  public:
    MipLevel(u32 level, const TextureInfo& parent, bool from_tmem, std::span<const u8>* src_data,
             std::span<const u8>* tmem_even, std::span<const u8>* tmem_odd);

    bool IsDataValid() const;

    const u8* GetData() const;
    u32 GetTextureSize() const;

    u32 GetExpandedWidth() const;
    u32 GetExpandedHeight() const;

    u32 GetRawWidth() const;
    u32 GetRawHeight() const;

  private:
    bool m_data_valid;

    const u8* m_ptr;

    u32 m_texture_size = 0;

    u32 m_expanded_width;
    u32 m_raw_width;

    u32 m_expanded_height;
    u32 m_raw_height;
  };

  bool HasMipMaps() const;
  u32 GetLevelCount() const;
  const MipLevel* GetMipMapLevel(u32 level) const;
  u32 GetFullLevelSize() const;

  static constexpr std::string_view format_prefix{"tex1_"};

private:
  const u8* m_ptr;
  const u8* m_tlut_ptr;

  u32 m_address;

  bool m_data_valid;

  bool m_from_tmem;
  const u8* m_tmem_odd;

  TextureFormat m_texture_format;
  TLUTFormat m_tlut_format;

  bool m_mipmaps_enabled = false;
  std::vector<MipLevel> m_mip_levels;

  u32 m_texture_size = 0;
  std::optional<u32> m_palette_size;

  u32 m_block_width;
  u32 m_expanded_width;
  u32 m_raw_width;

  u32 m_block_height;
  u32 m_expanded_height;
  u32 m_raw_height;

  u32 m_stage;
};
