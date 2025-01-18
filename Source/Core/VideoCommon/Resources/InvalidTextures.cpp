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
  texture->Load(0, 1, 1, 0, pixel.data(), pixel.size());
  return texture;
}
std::unique_ptr<AbstractTexture> CreateInvalidColorTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2D};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Color Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  texture->Load(0, 1, 1, 0, pixel.data(), pixel.size());
  return texture;
}
std::unique_ptr<AbstractTexture> CreateInvalidCubemapTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 6, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_CubeMap};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Cubemap Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  texture->Load(0, 1, 1, 0, pixel.data(), pixel.size());
  texture->Load(1, 1, 1, 0, pixel.data(), pixel.size());
  texture->Load(3, 1, 1, 0, pixel.data(), pixel.size());
  texture->Load(4, 1, 1, 0, pixel.data(), pixel.size());
  texture->Load(5, 1, 1, 0, pixel.data(), pixel.size());
  return texture;
}
std::unique_ptr<AbstractTexture> CreateInvalidArrayTexture()
{
  const TextureConfig tex_config{
      1, 1, 1, 1, 1, AbstractTextureFormat::RGBA8, 0, AbstractTextureType::Texture_2DArray};
  auto texture = g_gfx->CreateTexture(tex_config, "Invalid Array Texture");
  const std::array<u8, 4> pixel{255, 0, 255, 255};
  texture->Load(0, 1, 1, 0, pixel.data(), pixel.size());
  return texture;
}
}  // namespace VideoCommon
