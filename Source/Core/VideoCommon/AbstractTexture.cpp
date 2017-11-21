// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/ImageWrite.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

AbstractTexture::~AbstractTexture() = default;

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  // We can't dump compressed textures currently (it would mean drawing them to a RGBA8
  // framebuffer, and saving that). TextureCache does not call Save for custom textures
  // anyway, so this is fine for now.
  _assert_(m_config.format == AbstractTextureFormat::RGBA8);

  auto result = level == 0 ? Map() : Map(level);

  if (!result.has_value())
  {
    return false;
  }

  auto raw_data = result.value();
  return TextureToPng(raw_data.data, raw_data.stride, filename, raw_data.width, raw_data.height);
}

std::optional<AbstractTexture::RawTextureInfo> AbstractTexture::Map()
{
  if (m_currently_mapped)
  {
    Unmap();
    m_currently_mapped = false;
  }
  auto result = MapFullImpl();

  if (!result.has_value())
  {
    m_currently_mapped = false;
    return {};
  }

  m_currently_mapped = true;
  return result;
}

std::optional<AbstractTexture::RawTextureInfo> AbstractTexture::Map(u32 level, u32 x, u32 y,
                                                                    u32 width, u32 height)
{
  _assert_(level < m_config.levels);

  u32 max_level_width = std::max(m_config.width >> level, 1u);
  u32 max_level_height = std::max(m_config.height >> level, 1u);

  _assert_(width < max_level_width);
  _assert_(height < max_level_height);

  auto result = MapRegionImpl(level, x, y, width, height);

  if (!result.has_value())
  {
    m_currently_mapped = false;
    return {};
  }

  m_currently_mapped = true;
  return result;
}

std::optional<AbstractTexture::RawTextureInfo> AbstractTexture::Map(u32 level)
{
  _assert_(level < m_config.levels);

  u32 level_width = std::max(m_config.width >> level, 1u);
  u32 level_height = std::max(m_config.height >> level, 1u);

  return Map(level, 0, 0, level_width, level_height);
}

void AbstractTexture::Unmap()
{
}

std::optional<AbstractTexture::RawTextureInfo> AbstractTexture::MapFullImpl()
{
  return {};
}

std::optional<AbstractTexture::RawTextureInfo>
AbstractTexture::MapRegionImpl(u32 level, u32 x, u32 y, u32 width, u32 height)
{
  return {};
}

bool AbstractTexture::IsCompressedHostTextureFormat(AbstractTextureFormat format)
{
  // This will need to be changed if we add any other uncompressed formats.
  return format != AbstractTextureFormat::RGBA8;
}

size_t AbstractTexture::CalculateHostTextureLevelPitch(AbstractTextureFormat format, u32 row_length)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 8;
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 16;
  case AbstractTextureFormat::RGBA8:
  default:
    return static_cast<size_t>(row_length) * 4;
  }
}

const TextureConfig& AbstractTexture::GetConfig() const
{
  return m_config;
}
