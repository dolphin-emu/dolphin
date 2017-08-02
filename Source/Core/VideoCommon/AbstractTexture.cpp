// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"
#include "VideoCommon/AbstractTexture.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

AbstractTexture::~AbstractTexture() = default;

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  return false;
}

bool AbstractTexture::IsCompressedHostTextureFormat(AbstractTextureFormat format)
{
  // This will need to be changed if we add any other compressed formats.
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return true;
  }

  return false;
}

size_t AbstractTexture::CalculateHostTextureLevelPitch(AbstractTextureFormat format, u32 row_length)
{
  switch (format)
  {
  case AbstractTextureFormat::RGBA8:
    return static_cast<size_t>(row_length) * 4;
  case AbstractTextureFormat::I8:
  case AbstractTextureFormat::AI4:
    return static_cast<size_t>(row_length);
  case AbstractTextureFormat::AI8:
  case AbstractTextureFormat::RGB565:
  case AbstractTextureFormat::ARGB4:
    return static_cast<size_t>(row_length) * 2;
  case AbstractTextureFormat::DXT1:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 8;
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 16;
  default:
    PanicAlert("Invalid host texture format 0x%X", static_cast<int>(format));
    return static_cast<size_t>(row_length) * 4;
  }
}

const TextureConfig& AbstractTexture::GetConfig() const
{
  return m_config;
}
