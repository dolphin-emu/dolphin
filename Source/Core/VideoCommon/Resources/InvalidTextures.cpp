// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/InvalidTextures.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
std::unique_ptr<AbstractTexture> CreateInvalidTransparentTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2D};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Transparent Texture");
  const std::array<u8, 4> pixel{0, 0, 0, 0};
  texture->Load(0, 1, 1, 1, pixel.data(), pixel.size());
  return texture;
}

std::unique_ptr<AbstractTexture> CreateInvalidColorTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2D};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Color Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  texture->Load(0, 1, 1, 1, pixel.data(), pixel.size());
  return texture;
}

std::unique_ptr<AbstractTexture> CreateInvalidCubemapTexture()
{
#ifdef __APPLE__
  // TODO: figure out cube map oddness on Apple (specifically Metal)
  return nullptr;
#else
  const TextureConfig tex_config{
      1, 1, 1, 6, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_CubeMap};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Cubemap Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  for (u32 i = 0; i < tex_config.layers; i++)
  {
    texture->Load(0, tex_config.width, tex_config.height, 1, pixel.data(), pixel.size(), i);
  }
  return texture;
#endif
}

std::unique_ptr<AbstractTexture> CreateInvalidArrayTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2DArray};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Array Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  texture->Load(0, 1, 1, 1, pixel.data(), pixel.size());
  return texture;
}
}  // namespace VideoCommon
