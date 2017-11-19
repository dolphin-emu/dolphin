// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/RenderBase.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

AbstractTexture::~AbstractTexture() = default;

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  // We can't dump compressed textures currently (it would mean drawing them to a RGBA8
  // framebuffer, and saving that). TextureCache does not call Save for custom textures
  // anyway, so this is fine for now.
  _assert_(!IsCompressedFormat(m_config.format));
  _assert_(level < m_config.levels);

  // Determine dimensions of image we want to save.
  u32 level_width = std::max(1u, m_config.width >> level);
  u32 level_height = std::max(1u, m_config.height >> level);

  // Use a temporary staging texture for the download. Certainly not optimal,
  // but this is not a frequently-executed code path..
  TextureConfig readback_texture_config(level_width, level_height, 1, 1,
                                        AbstractTextureFormat::RGBA8, false);
  auto readback_texture =
      g_renderer->CreateStagingTexture(StagingTextureType::Readback, readback_texture_config);
  if (!readback_texture)
    return false;

  // Copy to the readback texture's buffer.
  readback_texture->CopyFromTexture(this, 0, level);
  readback_texture->Flush();

  // Map it so we can encode it to the file.
  if (!readback_texture->Map())
    return false;

  return TextureToPng(reinterpret_cast<const u8*>(readback_texture->GetMappedPointer()),
                      static_cast<int>(readback_texture->GetMappedStride()), filename, level_width,
                      level_height);
}

bool AbstractTexture::IsCompressedFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return true;

  default:
    return false;
  }
}

size_t AbstractTexture::CalculateStrideForFormat(AbstractTextureFormat format, u32 row_length)
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
  case AbstractTextureFormat::BGRA8:
    return static_cast<size_t>(row_length) * 4;
  default:
    PanicAlert("Unhandled texture format.");
    return 0;
  }
}

size_t AbstractTexture::GetTexelSizeForFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return 8;
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return 16;
  case AbstractTextureFormat::RGBA8:
  case AbstractTextureFormat::BGRA8:
    return 4;
  default:
    PanicAlert("Unhandled texture format.");
    return 0;
  }
}

const TextureConfig& AbstractTexture::GetConfig() const
{
  return m_config;
}
