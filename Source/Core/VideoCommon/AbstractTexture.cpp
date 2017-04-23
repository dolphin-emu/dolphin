// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "VideoCommon/AbstractTexture.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

AbstractTexture::~AbstractTexture() = default;

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  return false;
}

bool AbstractTexture::IsCompressedHostTextureFormat(HostTextureFormat format)
{
  // This will need to be changed if we add any other uncompressed formats.
  return format != HostTextureFormat::RGBA8;
}

size_t AbstractTexture::CalculateHostTextureLevelPitch(HostTextureFormat format, u32 row_length)
{
  switch (format)
  {
  case HostTextureFormat::DXT1:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 8;
  case HostTextureFormat::DXT3:
  case HostTextureFormat::DXT5:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 16;
  case HostTextureFormat::RGBA8:
  default:
    return static_cast<size_t>(row_length) * 4;
  }
}

const TextureConfig AbstractTexture::GetConfig() const
{
  return m_config;
}
