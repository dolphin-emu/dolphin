// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "VideoCommon/AbstractRawTexture.h"
#include "VideoCommon/AbstractTexture.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

AbstractTexture::~AbstractTexture() = default;

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  auto raw_texture = GetRawData(level);

  if (raw_texture == nullptr)
  {
    return false;
  }

  return raw_texture->Save(filename);
}

std::unique_ptr<AbstractRawTexture> AbstractTexture::GetRawData(unsigned int level)
{
  return nullptr;
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
