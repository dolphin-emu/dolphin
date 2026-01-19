// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureInfo.h"

#include <span>

#include <fmt/format.h>
#include <xxhash.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/SpanUtils.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"

TextureInfo TextureInfo::FromStage(u32 stage)
{
  const auto tex = bpmem.tex.GetUnit(stage);

  const auto texture_format = tex.texImage0.format;
  const auto tlut_format = tex.texTlut.tlut_format;

  const auto width = tex.texImage0.width + 1;
  const auto height = tex.texImage0.height + 1;

  const u32 address = (tex.texImage3.image_base /* & 0x1FFFFF*/) << 5;

  const u32 tlutaddr = tex.texTlut.tmem_offset << 9;
  std::span<const u8> tlut_data = TexDecoder_GetTmemSpan(tlutaddr);

  std::optional<u32> mip_count;
  const bool has_mipmaps = tex.texMode0.mipmap_filter != MipMode::None;
  if (has_mipmaps)
  {
    mip_count = (tex.texMode1.max_lod + 0xf) / 0x10;
  }

  const bool from_tmem = tex.texImage1.cache_manually_managed != 0;
  const u32 tmem_address_even = from_tmem ? tex.texImage1.tmem_even * TMEM_LINE_SIZE : 0;
  const u32 tmem_address_odd = from_tmem ? tex.texImage2.tmem_odd * TMEM_LINE_SIZE : 0;

  if (from_tmem)
  {
    return TextureInfo(stage, TexDecoder_GetTmemSpan(tmem_address_even), tlut_data, address,
                       texture_format, tlut_format, width, height, true,
                       TexDecoder_GetTmemSpan(tmem_address_odd),
                       TexDecoder_GetTmemSpan(tmem_address_even), mip_count);
  }

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return TextureInfo(stage, memory.GetSpanForAddress(address), tlut_data, address, texture_format,
                     tlut_format, width, height, false, {}, {}, mip_count);
}

TextureInfo::TextureInfo(u32 stage, std::span<const u8> data, std::span<const u8> tlut_data,
                         u32 address, TextureFormat texture_format, TLUTFormat tlut_format,
                         u32 width, u32 height, bool from_tmem, std::span<const u8> tmem_odd,
                         std::span<const u8> tmem_even, std::optional<u32> mip_count)
    : m_data(data), m_tlut_data(tlut_data), m_address(address), m_from_tmem(from_tmem),
      m_tmem_even(tmem_even), m_tmem_odd(tmem_odd), m_texture_format(texture_format),
      m_tlut_format(tlut_format), m_raw_width(width), m_raw_height(height), m_stage(stage)
{
  const bool is_palette_texture = IsColorIndexed(m_texture_format);
  if (is_palette_texture)
    m_palette_size = TexDecoder_GetPaletteSize(m_texture_format);

  // TexelSizeInNibbles(format) * width * height / 16;
  m_block_width = TexDecoder_GetBlockWidthInTexels(m_texture_format);
  m_block_height = TexDecoder_GetBlockHeightInTexels(m_texture_format);

  m_expanded_width = Common::AlignUp(m_raw_width, m_block_width);
  m_expanded_height = Common::AlignUp(m_raw_height, m_block_height);

  m_texture_size =
      TexDecoder_GetTextureSizeInBytes(m_expanded_width, m_expanded_height, m_texture_format);

  if (data.size() < m_texture_size)
  {
    ERROR_LOG_FMT(VIDEO, "Trying to use an invalid texture address {:#010x}", GetRawAddress());
    m_data_valid = false;
  }
  else if (m_palette_size && tlut_data.size() < *m_palette_size)
  {
    ERROR_LOG_FMT(VIDEO, "Trying to use an invalid TLUT address {:#010x}", GetRawAddress());
    m_data_valid = false;
  }
  else
  {
    m_data_valid = true;
  }

  if (mip_count)
  {
    m_mipmaps_enabled = true;
    const u32 raw_mip_count = *mip_count;

    // GPUs don't like when the specified mipmap count would require more than one 1x1-sized LOD in
    // the mipmap chain
    // e.g. 64x64 with 7 LODs would have the mipmap chain 64x64,32x32,16x16,8x8,4x4,2x2,1x1,0x0, so
    // we limit the mipmap count to 6 there
    m_limited_mip_count =
        std::min<u32>(MathUtil::IntLog2(std::max(width, height)) + 1, raw_mip_count + 1) - 1;
  }
}

std::string TextureInfo::NameDetails::GetFullName() const
{
  return fmt::format("{}_{}{}_{}", base_name, texture_name, tlut_name, format_name);
}

TextureInfo::NameDetails TextureInfo::CalculateTextureName() const
{
  if (!IsDataValid())
    return NameDetails{};

  const u8* tlut = m_tlut_data.data();
  size_t tlut_size = m_palette_size ? *m_palette_size : 0;

  // checking for min/max on paletted textures
  u32 min = 0xffff;
  u32 max = 0;
  switch (tlut_size)
  {
  case 0:
    break;
  case 16 * 2:
    for (size_t i = 0; i < m_texture_size; i++)
    {
      const u32 low_nibble = m_data[i] & 0xf;
      const u32 high_nibble = m_data[i] >> 4;

      min = std::min({min, low_nibble, high_nibble});
      max = std::max({max, low_nibble, high_nibble});
    }
    break;
  case 256 * 2:
    for (size_t i = 0; i < m_texture_size; i++)
    {
      const u32 texture_byte = m_data[i];

      min = std::min(min, texture_byte);
      max = std::max(max, texture_byte);
    }
    break;
  case 16384 * 2:
    for (size_t i = 0; i < m_texture_size; i += sizeof(u16))
    {
      const u32 texture_halfword = Common::swap16(m_data[i]) & 0x3fff;

      min = std::min(min, texture_halfword);
      max = std::max(max, texture_halfword);
    }
    break;
  }
  if (tlut_size > 0)
  {
    tlut_size = 2 * (max + 1 - min);
    tlut += 2 * min;
  }

  DEBUG_ASSERT(tlut_size <= m_palette_size.value_or(0));

  const u64 tex_hash = XXH64(m_data.data(), m_texture_size, 0);
  const u64 tlut_hash = tlut_size ? XXH64(tlut, tlut_size, 0) : 0;

  return {.base_name = fmt::format("{}{}x{}{}", format_prefix, m_raw_width, m_raw_height,
                                   m_mipmaps_enabled ? "_m" : ""),
          .texture_name = fmt::format("{:016x}", tex_hash),
          .tlut_name = tlut_size ? fmt::format("_{:016x}", tlut_hash) : "",
          .format_name = fmt::to_string(static_cast<int>(m_texture_format))};
}

TextureInfo::MipLevels TextureInfo::GetMipMapLevels() const
{
  MipLevelIterator begin;
  begin.m_parent = this;
  begin.m_from_tmem = m_from_tmem;
  begin.m_data = Common::SafeSubspan(m_data, GetTextureSize());
  begin.m_tmem_even = Common::SafeSubspan(m_tmem_even, GetTextureSize());
  begin.m_tmem_odd = m_tmem_odd;
  begin.CreateMipLevel();

  MipLevelIterator end;
  end.m_level_index = m_limited_mip_count;

  return MipLevels(begin, end);
}

u32 TextureInfo::GetFullLevelSize() const
{
  u32 all_mips_size = 0;
  for (const auto& mip_map : GetMipMapLevels())
  {
    if (mip_map.IsDataValid())
      all_mips_size += mip_map.GetTextureSize();
  }
  return m_texture_size + all_mips_size;
}

TextureInfo::MipLevel::MipLevel(u32 level, const TextureInfo& parent, std::span<const u8>* data)
    : m_level(level)
{
  m_raw_width = std::max(parent.GetRawWidth() >> level, 1u);
  m_raw_height = std::max(parent.GetRawHeight() >> level, 1u);
  m_expanded_width = Common::AlignUp(m_raw_width, parent.GetBlockWidth());
  m_expanded_height = Common::AlignUp(m_raw_height, parent.GetBlockHeight());

  m_texture_size = TexDecoder_GetTextureSizeInBytes(m_expanded_width, m_expanded_height,
                                                    parent.GetTextureFormat());

  m_ptr = data->data();
  m_data_valid = data->size() >= m_texture_size;

  *data = Common::SafeSubspan(*data, m_texture_size);
}

TextureInfo::MipLevelIterator& TextureInfo::MipLevelIterator::operator++()
{
  ++m_level_index;
  CreateMipLevel();
  return *this;
}

void TextureInfo::MipLevelIterator::CreateMipLevel()
{
  const u32 level = m_level_index + 1;
  std::span<const u8>* data = m_from_tmem ? ((level % 2) ? &m_tmem_odd : &m_tmem_even) : &m_data;

  // The MipLevel constructor mutates the data argument so the next MipLevel gets the right data
  m_mip_level = MipLevel(level, *m_parent, data);
}
