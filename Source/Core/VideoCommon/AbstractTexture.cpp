// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/AbstractTexture.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/Image.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractStagingTexture.h"

AbstractTexture::AbstractTexture(const TextureConfig& c) : m_config(c)
{
}

void AbstractTexture::FinishedRendering()
{
}

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  // We can't dump compressed textures currently (it would mean drawing them to a RGBA8
  // framebuffer, and saving that). TextureCache does not call Save for custom textures
  // anyway, so this is fine for now.
  ASSERT(!IsCompressedFormat(m_config.format));
  ASSERT(level < m_config.levels);

  // Determine dimensions of image we want to save.
  u32 level_width = std::max(1u, m_config.width >> level);
  u32 level_height = std::max(1u, m_config.height >> level);

  // Use a temporary staging texture for the download. Certainly not optimal,
  // but this is not a frequently-executed code path..
  TextureConfig readback_texture_config(level_width, level_height, 1, 1, 1,
                                        AbstractTextureFormat::RGBA8, 0);
  auto readback_texture =
      g_gfx->CreateStagingTexture(StagingTextureType::Readback, readback_texture_config);
  if (!readback_texture)
    return false;

  // Copy to the readback texture's buffer.
  readback_texture->CopyFromTexture(this, 0, level);
  readback_texture->Flush();

  // Map it so we can encode it to the file.
  if (!readback_texture->Map())
    return false;

  return Common::SavePNG(filename,
                         reinterpret_cast<const u8*>(readback_texture->GetMappedPointer()),
                         Common::ImageByteFormat::RGBA, level_width, level_height,
                         static_cast<int>(readback_texture->GetMappedStride()));
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

bool AbstractTexture::IsDepthFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::D16:
  case AbstractTextureFormat::D24_S8:
  case AbstractTextureFormat::D32F:
  case AbstractTextureFormat::D32F_S8:
    return true;

  default:
    return false;
  }
}

bool AbstractTexture::IsStencilFormat(AbstractTextureFormat format)
{
  return format == AbstractTextureFormat::D24_S8 || format == AbstractTextureFormat::D32F_S8;
}

bool AbstractTexture::IsCompatibleDepthAndColorFormats(AbstractTextureFormat depth_format,
                                                       AbstractTextureFormat color_format)
{
  switch (depth_format)
  {
  case AbstractTextureFormat::D16:
    return color_format == AbstractTextureFormat::R16;

  case AbstractTextureFormat::D32F:
    return color_format == AbstractTextureFormat::R32F;

  default:
    return false;
  }
}

u32 AbstractTexture::CalculateStrideForFormat(AbstractTextureFormat format, u32 row_length)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 8;
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return static_cast<size_t>(std::max(1u, row_length / 4)) * 16;
  case AbstractTextureFormat::R16:
  case AbstractTextureFormat::D16:
    return static_cast<size_t>(row_length) * 2;
  case AbstractTextureFormat::RGBA8:
  case AbstractTextureFormat::BGRA8:
  case AbstractTextureFormat::R32F:
  case AbstractTextureFormat::D32F:
  case AbstractTextureFormat::D24_S8:
    return static_cast<size_t>(row_length) * 4;
  case AbstractTextureFormat::D32F_S8:
    return static_cast<size_t>(row_length) * 8;
  default:
    PanicAlertFmt("Unhandled texture format.");
    return 0;
  }
}

u32 AbstractTexture::GetTexelSizeForFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return 8;
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return 16;
  case AbstractTextureFormat::R16:
  case AbstractTextureFormat::D16:
    return 2;
  case AbstractTextureFormat::RGBA8:
  case AbstractTextureFormat::BGRA8:
  case AbstractTextureFormat::D24_S8:
  case AbstractTextureFormat::R32F:
  case AbstractTextureFormat::D32F:
    return 4;
  case AbstractTextureFormat::D32F_S8:
    return 8;
  default:
    PanicAlertFmt("Unhandled texture format.");
    return 0;
  }
}

u32 AbstractTexture::GetBlockSizeForFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
  case AbstractTextureFormat::DXT3:
  case AbstractTextureFormat::DXT5:
  case AbstractTextureFormat::BPTC:
    return 4;

  default:
    return 1;
  }
}

const TextureConfig& AbstractTexture::GetConfig() const
{
  return m_config;
}
