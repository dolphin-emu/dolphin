// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

enum class TextureFormat;
enum class TLUTFormat;

class TextureInfo final
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

  bool IsDataValid() const { return m_data_valid; }

  const u8* GetData() const { return m_data.data(); }
  const u8* GetTlutAddress() const { return m_tlut_data.data(); }

  u32 GetRawAddress() const { return m_address; }

  bool IsFromTmem() const { return m_from_tmem; }
  const u8* GetTmemOddAddress() const { return m_tmem_odd.data(); }

  TextureFormat GetTextureFormat() const { return m_texture_format; }
  TLUTFormat GetTlutFormat() const { return m_tlut_format; }

  std::optional<u32> GetPaletteSize() const { return m_palette_size; }
  u32 GetTextureSize() const { return m_texture_size; }

  u32 GetBlockWidth() const { return m_block_width; }
  u32 GetBlockHeight() const { return m_block_height; }

  u32 GetExpandedWidth() const { return m_expanded_width; }
  u32 GetExpandedHeight() const { return m_expanded_height; }

  u32 GetRawWidth() const { return m_raw_width; }
  u32 GetRawHeight() const { return m_raw_height; }

  u32 GetStage() const { return m_stage; }

  class MipLevel final
  {
  public:
    MipLevel() = default;
    MipLevel(u32 level, const TextureInfo& parent, std::span<const u8>* data);

    bool IsDataValid() const { return m_data_valid; }

    const u8* GetData() const { return m_ptr; }
    u32 GetTextureSize() const { return m_texture_size; }

    u32 GetExpandedWidth() const { return m_expanded_width; }
    u32 GetExpandedHeight() const { return m_expanded_height; }

    u32 GetRawWidth() const { return m_raw_width; }
    u32 GetRawHeight() const { return m_raw_height; }

    u32 GetLevel() const { return m_level; }

  private:
    bool m_data_valid = false;

    const u8* m_ptr = nullptr;

    u32 m_texture_size = 0;

    u32 m_expanded_width = 0;
    u32 m_raw_width = 0;

    u32 m_expanded_height = 0;
    u32 m_raw_height = 0;

    u32 m_level = 0;
  };

  class MipLevelIterator final
  {
    friend TextureInfo;

  public:
    using difference_type = u32;
    using value_type = MipLevel;

    MipLevel& operator*() { return m_mip_level; }

    const MipLevel& operator*() const { return m_mip_level; }

    MipLevelIterator& operator++();

    MipLevelIterator operator++(int)
    {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    bool operator==(const MipLevelIterator& other) const
    {
      return m_level_index == other.m_level_index;
    }

  private:
    void CreateMipLevel();

    MipLevel m_mip_level;

    const TextureInfo* m_parent = nullptr;
    u32 m_level_index = 0;
    bool m_from_tmem = false;
    std::span<const u8> m_data;
    std::span<const u8> m_tmem_even;
    std::span<const u8> m_tmem_odd;
  };

  class MipLevels final
  {
  public:
    MipLevels(MipLevelIterator begin, MipLevelIterator end) : m_begin(begin), m_end(end) {}

    MipLevelIterator begin() const { return m_begin; }
    MipLevelIterator end() const { return m_end; }

  private:
    MipLevelIterator m_begin;
    MipLevelIterator m_end;
  };

  bool HasMipMaps() const { return m_limited_mip_count != 0; }
  u32 GetLevelCount() const { return m_limited_mip_count + 1; }
  MipLevels GetMipMapLevels() const;
  u32 GetFullLevelSize() const;

  static constexpr std::string_view format_prefix{"tex1_"};

private:
  std::span<const u8> m_data;
  std::span<const u8> m_tlut_data;

  u32 m_address;

  bool m_data_valid;

  bool m_from_tmem;
  std::span<const u8> m_tmem_even;
  std::span<const u8> m_tmem_odd;

  TextureFormat m_texture_format;
  TLUTFormat m_tlut_format;

  bool m_mipmaps_enabled = false;
  u32 m_limited_mip_count = 0;

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
