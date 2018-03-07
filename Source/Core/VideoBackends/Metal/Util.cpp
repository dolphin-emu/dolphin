// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/Util.h"

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/MathUtil.h"

#include "VideoBackends/Metal/MetalContext.h"

namespace Metal
{
namespace Util
{
size_t AlignBufferOffset(size_t offset, size_t alignment)
{
  // Assume an offset of zero is already aligned to a value larger than alignment.
  if (offset == 0)
    return 0;

  return Common::AlignUp(offset, alignment);
}

bool IsDepthFormat(mtlpp::PixelFormat format)
{
  switch (format)
  {
  case mtlpp::PixelFormat::Depth16Unorm:
  case mtlpp::PixelFormat::Depth24Unorm_Stencil8:
  case mtlpp::PixelFormat::Depth32Float:
  case mtlpp::PixelFormat::Depth32Float_Stencil8:
    return true;
  default:
    return false;
  }
}

bool IsCompressedFormat(mtlpp::PixelFormat format)
{
  switch (format)
  {
  case mtlpp::PixelFormat::BC1_RGBA:
  case mtlpp::PixelFormat::BC2_RGBA:
  case mtlpp::PixelFormat::BC3_RGBA:
  case mtlpp::PixelFormat::BC7_RGBAUnorm:
    return true;

  default:
    return false;
  }
}

mtlpp::PixelFormat GetMetalPixelFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return mtlpp::PixelFormat::BC1_RGBA;

  case AbstractTextureFormat::DXT3:
    return mtlpp::PixelFormat::BC2_RGBA;

  case AbstractTextureFormat::DXT5:
    return mtlpp::PixelFormat::BC3_RGBA;

  case AbstractTextureFormat::BPTC:
    return mtlpp::PixelFormat::BC7_RGBAUnorm;

  case AbstractTextureFormat::RGBA8:
    return mtlpp::PixelFormat::RGBA8Unorm;

  case AbstractTextureFormat::BGRA8:
    return mtlpp::PixelFormat::BGRA8Unorm;

  case AbstractTextureFormat::R16:
    return mtlpp::PixelFormat::R16Unorm;

  case AbstractTextureFormat::D16:
    return mtlpp::PixelFormat::Depth16Unorm;

  case AbstractTextureFormat::R32F:
    return mtlpp::PixelFormat::R32Float;

  case AbstractTextureFormat::D32F:
    return mtlpp::PixelFormat::Depth32Float;

  case AbstractTextureFormat::D32F_S8:
    return mtlpp::PixelFormat::Depth32Float_Stencil8;

  default:
    PanicAlert("Unhandled texture format.");
    return mtlpp::PixelFormat::RGBA8Unorm;
  }
}

AbstractTextureFormat GetAbstractTextureFormat(mtlpp::PixelFormat format)
{
  switch (format)
  {
  case mtlpp::PixelFormat::BC1_RGBA:
    return AbstractTextureFormat::DXT1;
  case mtlpp::PixelFormat::BC2_RGBA:
    return AbstractTextureFormat::DXT3;
  case mtlpp::PixelFormat::BC3_RGBA:
    return AbstractTextureFormat::DXT5;
  case mtlpp::PixelFormat::BC7_RGBAUnorm:
    return AbstractTextureFormat::BPTC;
  case mtlpp::PixelFormat::RGBA8Unorm:
    return AbstractTextureFormat::RGBA8;
  case mtlpp::PixelFormat::BGRA8Unorm:
    return AbstractTextureFormat::BGRA8;
  case mtlpp::PixelFormat::R16Unorm:
    return AbstractTextureFormat::R16;
  case mtlpp::PixelFormat::Depth16Unorm:
    return AbstractTextureFormat::D16;
  case mtlpp::PixelFormat::R32Float:
    return AbstractTextureFormat::R32F;
  case mtlpp::PixelFormat::Depth32Float:
    return AbstractTextureFormat::D32F;
  case mtlpp::PixelFormat::Depth32Float_Stencil8:
    return AbstractTextureFormat::D32F_S8;
  default:
    PanicAlert("Unknown pixel format");
    return AbstractTextureFormat::Undefined;
  }
}

u32 GetTexelSize(mtlpp::PixelFormat format)
{
  // Only contains pixel formats we use.
  switch (format)
  {
  case mtlpp::PixelFormat::R32Float:
    return 4;

  case mtlpp::PixelFormat::Depth32Float:
    return 4;

  case mtlpp::PixelFormat::RGBA8Unorm:
  case mtlpp::PixelFormat::BGRA8Unorm:
    return 4;

  case mtlpp::PixelFormat::BC1_RGBA:
    return 8;

  case mtlpp::PixelFormat::BC2_RGBA:
  case mtlpp::PixelFormat::BC3_RGBA:
  case mtlpp::PixelFormat::BC7_RGBAUnorm:
    return 16;

  default:
    PanicAlert("Unhandled pixel format");
    return 1;
  }
}

u32 GetBlockSize(mtlpp::PixelFormat format)
{
  switch (format)
  {
  case mtlpp::PixelFormat::BC1_RGBA:
  case mtlpp::PixelFormat::BC2_RGBA:
  case mtlpp::PixelFormat::BC3_RGBA:
  case mtlpp::PixelFormat::BC7_RGBAUnorm:
    return 4;

  default:
    return 1;
  }
}

mtlpp::BlendFactor GetAlphaBlendFactor(mtlpp::BlendFactor factor)
{
  switch (factor)
  {
  case mtlpp::BlendFactor::SourceColor:
    return mtlpp::BlendFactor::SourceAlpha;
  case mtlpp::BlendFactor::OneMinusSourceColor:
    return mtlpp::BlendFactor::OneMinusSourceAlpha;
  case mtlpp::BlendFactor::DestinationColor:
    return mtlpp::BlendFactor::DestinationAlpha;
  case mtlpp::BlendFactor::OneMinusDestinationColor:
    return mtlpp::BlendFactor::OneMinusDestinationAlpha;
  default:
    return factor;
  }
}

std::array<VideoCommon::UtilityVertex, 4> GetQuadVertices(float z, u32 color, float u0, float u1,
                                                          float v0, float v1, float uv_layer)
{
  std::array<VideoCommon::UtilityVertex, 4> vertices;
  vertices[0].SetPosition(-1.0f, 1.0f, z);
  vertices[0].SetTextureCoordinates(u0, v0, uv_layer);
  vertices[0].SetColor(color);
  vertices[1].SetPosition(1.0f, 1.0f, z);
  vertices[1].SetTextureCoordinates(u1, v0, uv_layer);
  vertices[1].SetColor(color);
  vertices[2].SetPosition(-1.0f, -1.0f, z);
  vertices[2].SetTextureCoordinates(u0, v1, uv_layer);
  vertices[2].SetColor(color);
  vertices[3].SetPosition(1.0f, -1.0f, z);
  vertices[3].SetTextureCoordinates(u1, v1, uv_layer);
  vertices[3].SetColor(color);
  return vertices;
}

std::array<VideoCommon::UtilityVertex, 4> GetQuadVertices(const MathUtil::Rectangle<int>& uv_rect,
                                                          u32 tex_width, u32 tex_height, float z,
                                                          u32 color)
{
  const float rcp_width = 1.0f / tex_width;
  const float rcp_height = 1.0f / tex_height;
  return GetQuadVertices(z, color, uv_rect.left * rcp_width, uv_rect.right * rcp_width,
                         uv_rect.top * rcp_height, uv_rect.bottom * rcp_height);
}
}  // namespace Util
}  // namespace Metal
