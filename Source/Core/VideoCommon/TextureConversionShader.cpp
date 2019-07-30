// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

#define WRITE p += sprintf

static char text[16384];
static bool IntensityConstantAdded = false;

namespace TextureConversionShaderTiled
{
u16 GetEncodedSampleCount(EFBCopyFormat format)
{
  switch (format)
  {
  case EFBCopyFormat::R4:
    return 8;
  case EFBCopyFormat::RA4:
    return 4;
  case EFBCopyFormat::RA8:
    return 2;
  case EFBCopyFormat::RGB565:
    return 2;
  case EFBCopyFormat::RGB5A3:
    return 2;
  case EFBCopyFormat::RGBA8:
    return 1;
  case EFBCopyFormat::A8:
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
  case EFBCopyFormat::G8:
  case EFBCopyFormat::B8:
    return 4;
  case EFBCopyFormat::RG8:
  case EFBCopyFormat::GB8:
    return 2;
  case EFBCopyFormat::XFB:
    return 2;
  default:
    PanicAlert("Invalid EFB Copy Format (0x%X)! (GetEncodedSampleCount)", static_cast<int>(format));
    return 1;
  }
}

static void WriteHeader(char*& p, APIType ApiType)
{
  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    // left, top, of source rectangle within source texture
    // width of the destination rectangle, scale_factor (1 or 2)
    WRITE(p, "UBO_BINDING(std140, 1) uniform PSBlock {\n");
    WRITE(p, "  int4 position;\n");
    WRITE(p, "  float y_scale;\n");
    WRITE(p, "  float gamma_rcp;\n");
    WRITE(p, "  float2 clamp_tb;\n");
    WRITE(p, "  float3 filter_coefficients;\n");
    WRITE(p, "};\n");
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      WRITE(p, "VARYING_LOCATION(0) in VertexData {\n");
      WRITE(p, "  float3 v_tex0;\n");
      WRITE(p, "};\n");
    }
    else
    {
      WRITE(p, "VARYING_LOCATION(0) in float3 v_tex0;\n");
    }
    WRITE(p, "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n");
    WRITE(p, "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n");
  }
  else  // D3D
  {
    WRITE(p, "cbuffer PSBlock : register(b0) {\n");
    WRITE(p, "  int4 position;\n");
    WRITE(p, "  float y_scale;\n");
    WRITE(p, "  float gamma_rcp;\n");
    WRITE(p, "  float2 clamp_tb;\n");
    WRITE(p, "  float3 filter_coefficients;\n");
    WRITE(p, "};\n");
    WRITE(p, "sampler samp0 : register(s0);\n");
    WRITE(p, "Texture2DArray Tex0 : register(t0);\n");
  }

  // D3D does not have roundEven(), only round(), which is specified "to the nearest integer".
  // This differs from the roundEven() behavior, but to get consistency across drivers in OpenGL
  // we need to use roundEven().
  if (ApiType == APIType::D3D)
    WRITE(p, "#define roundEven(x) round(x)\n");

  // Alpha channel in the copy is set to 1 the EFB format does not have an alpha channel.
  WRITE(p, "float4 RGBA8ToRGB8(float4 src)\n");
  WRITE(p, "{\n");
  WRITE(p, "  return float4(src.xyz, 1.0);\n");
  WRITE(p, "}\n");

  WRITE(p, "float4 RGBA8ToRGBA6(float4 src)\n");
  WRITE(p, "{\n");
  WRITE(p, "  int4 val = int4(roundEven(src * 255.0)) >> 2;\n");
  WRITE(p, "  return float4(val) / 63.0;\n");
  WRITE(p, "}\n");

  WRITE(p, "float4 RGBA8ToRGB565(float4 src)\n");
  WRITE(p, "{\n");
  WRITE(p, "  int4 val = int4(roundEven(src * 255.0));\n");
  WRITE(p, "  val = int4(val.r >> 3, val.g >> 2, val.b >> 3, 1);\n");
  WRITE(p, "  return float4(val) / float4(31.0, 63.0, 31.0, 1.0);\n");
  WRITE(p, "}\n");
}

static void WriteSampleFunction(char*& p, const EFBCopyParams& params, APIType ApiType)
{
  auto WriteSampleOp = [&](int yoffset) {
    if (!params.depth)
    {
      switch (params.efb_format)
      {
      case PEControl::RGB8_Z24:
        WRITE(p, "RGBA8ToRGB8(");
        break;
      case PEControl::RGBA6_Z24:
        WRITE(p, "RGBA8ToRGBA6(");
        break;
      case PEControl::RGB565_Z16:
        WRITE(p, "RGBA8ToRGB565(");
        break;
      default:
        WRITE(p, "(");
        break;
      }
    }
    else
    {
      // Handle D3D depth inversion.
      if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
        WRITE(p, "1.0 - (");
      else
        WRITE(p, "(");
    }

    if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
      WRITE(p, "texture(samp0, float3(");
    else
      WRITE(p, "Tex0.Sample(samp0, float3(");

    WRITE(p, "uv.x + float(xoffset) * pixel_size.x, ");

    // Reverse the direction for OpenGL, since positive numbers are distance from the bottom row.
    if (yoffset != 0)
    {
      if (ApiType == APIType::OpenGL)
        WRITE(p, "clamp(uv.y - float(%d) * pixel_size.y, clamp_tb.x, clamp_tb.y)", yoffset);
      else
        WRITE(p, "clamp(uv.y + float(%d) * pixel_size.y, clamp_tb.x, clamp_tb.y)", yoffset);
    }
    else
    {
      WRITE(p, "uv.y");
    }

    WRITE(p, ", 0.0)))");
  };

  // The copy filter applies to both color and depth copies. This has been verified on hardware.
  // The filter is only applied to the RGB channels, the alpha channel is left intact.
  WRITE(p, "float4 SampleEFB(float2 uv, float2 pixel_size, int xoffset)\n");
  WRITE(p, "{\n");
  if (params.copy_filter)
  {
    WRITE(p, "  float4 prev_row = ");
    WriteSampleOp(-1);
    WRITE(p, ";\n");
    WRITE(p, "  float4 current_row = ");
    WriteSampleOp(0);
    WRITE(p, ";\n");
    WRITE(p, "  float4 next_row = ");
    WriteSampleOp(1);
    WRITE(p, ";\n");
    WRITE(p, "  return float4(min(prev_row.rgb * filter_coefficients[0] +\n"
             "                      current_row.rgb * filter_coefficients[1] +\n"
             "                      next_row.rgb * filter_coefficients[2], \n"
             "                    float3(1, 1, 1)), current_row.a);\n");
  }
  else
  {
    WRITE(p, "  float4 current_row = ");
    WriteSampleOp(0);
    WRITE(p, ";\n");
    WRITE(p, "return float4(min(current_row.rgb * filter_coefficients[1], float3(1, 1, 1)),\n"
             "              current_row.a);\n");
  }
  WRITE(p, "}\n");
}

// block dimensions : widthStride, heightStride
// texture dims : width, height, x offset, y offset
static void WriteSwizzler(char*& p, const EFBCopyParams& params, EFBCopyFormat format,
                          APIType ApiType)
{
  WriteHeader(p, ApiType);
  WriteSampleFunction(p, params, ApiType);

  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    WRITE(p, "void main()\n");
    WRITE(p, "{\n"
             "  int2 sampleUv;\n"
             "  int2 uv1 = int2(gl_FragCoord.xy);\n");
  }
  else  // D3D
  {
    WRITE(p, "void main(\n");
    WRITE(p, "  in float3 v_tex0 : TEXCOORD0,\n");
    WRITE(p, "  in float4 rawpos : SV_Position,\n");
    WRITE(p, "  out float4 ocol0 : SV_Target)\n");
    WRITE(p, "{\n"
             "  int2 sampleUv;\n"
             "  int2 uv1 = int2(rawpos.xy);\n");
  }

  int blkW = TexDecoder_GetEFBCopyBlockWidthInTexels(format);
  int blkH = TexDecoder_GetEFBCopyBlockHeightInTexels(format);
  int samples = GetEncodedSampleCount(format);

  WRITE(p, "  int x_block_position = (uv1.x >> %d) << %d;\n", IntLog2(blkH * blkW / samples),
        IntLog2(blkW));
  WRITE(p, "  int y_block_position = uv1.y << %d;\n", IntLog2(blkH));
  if (samples == 1)
  {
    // With samples == 1, we write out pairs of blocks; one A8R8, one G8B8.
    WRITE(p, "  bool first = (uv1.x & %d) == 0;\n", blkH * blkW / 2);
    samples = 2;
  }
  WRITE(p, "  int offset_in_block = uv1.x & %d;\n", (blkH * blkW / samples) - 1);
  WRITE(p, "  int y_offset_in_block = offset_in_block >> %d;\n", IntLog2(blkW / samples));
  WRITE(p, "  int x_offset_in_block = (offset_in_block & %d) << %d;\n", (blkW / samples) - 1,
        IntLog2(samples));

  WRITE(p, "  sampleUv.x = x_block_position + x_offset_in_block;\n");
  WRITE(p, "  sampleUv.y = y_block_position + y_offset_in_block;\n");

  WRITE(p,
        "  float2 uv0 = float2(sampleUv);\n");  // sampleUv is the sample position in (int)gx_coords
  WRITE(p, "  uv0 += float2(0.5, 0.5);\n");     // move to center of pixel
  WRITE(p, "  uv0 *= float(position.w);\n");  // scale by two if needed (also move to pixel borders
                                              // so that linear filtering will average adjacent
                                              // pixel)
  WRITE(p, "  uv0 += float2(position.xy);\n");                    // move to copied rect
  WRITE(p, "  uv0 /= float2(%d, %d);\n", EFB_WIDTH, EFB_HEIGHT);  // normalize to [0:1]
  WRITE(p, "  uv0 /= float2(1, y_scale);\n");                     // apply the y scaling
  if (ApiType == APIType::OpenGL)                                 // ogl has to flip up and down
  {
    WRITE(p, "  uv0.y = 1.0-uv0.y;\n");
  }

  WRITE(p, "  float2 pixel_size = float2(position.w, position.w) / float2(%d, %d);\n", EFB_WIDTH,
        EFB_HEIGHT);
}

static void WriteSampleColor(char*& p, const char* colorComp, const char* dest, int xoffset,
                             APIType ApiType, const EFBCopyParams& params)
{
  WRITE(p, "  %s = SampleEFB(uv0, pixel_size, %d).%s;\n", dest, xoffset, colorComp);
}

static void WriteColorToIntensity(char*& p, const char* src, const char* dest)
{
  if (!IntensityConstantAdded)
  {
    WRITE(p, "  float4 IntensityConst = float4(0.257f,0.504f,0.098f,0.0625f);\n");
    IntensityConstantAdded = true;
  }
  WRITE(p, "  %s = dot(IntensityConst.rgb, %s.rgb);\n", dest, src);
  // don't add IntensityConst.a yet, because doing it later is faster and uses less instructions,
  // due to vectorization
}

static void WriteToBitDepth(char*& p, u8 depth, const char* src, const char* dest)
{
  WRITE(p, "  %s = floor(%s * 255.0 / exp2(8.0 - %d.0));\n", dest, src, depth);
}

static void WriteEncoderEnd(char*& p)
{
  WRITE(p, "}\n");
  IntensityConstantAdded = false;
}

static void WriteI8Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::R8, ApiType);
  WRITE(p, "  float3 texSample;\n");

  WriteSampleColor(p, "rgb", "texSample", 0, ApiType, params);
  WriteColorToIntensity(p, "texSample", "ocol0.b");

  WriteSampleColor(p, "rgb", "texSample", 1, ApiType, params);
  WriteColorToIntensity(p, "texSample", "ocol0.g");

  WriteSampleColor(p, "rgb", "texSample", 2, ApiType, params);
  WriteColorToIntensity(p, "texSample", "ocol0.r");

  WriteSampleColor(p, "rgb", "texSample", 3, ApiType, params);
  WriteColorToIntensity(p, "texSample", "ocol0.a");

  WRITE(p, "  ocol0.rgba += IntensityConst.aaaa;\n");  // see WriteColorToIntensity

  WriteEncoderEnd(p);
}

static void WriteI4Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::R4, ApiType);
  WRITE(p, "  float3 texSample;\n");
  WRITE(p, "  float4 color0;\n");
  WRITE(p, "  float4 color1;\n");

  WriteSampleColor(p, "rgb", "texSample", 0, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color0.b");

  WriteSampleColor(p, "rgb", "texSample", 1, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color1.b");

  WriteSampleColor(p, "rgb", "texSample", 2, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color0.g");

  WriteSampleColor(p, "rgb", "texSample", 3, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color1.g");

  WriteSampleColor(p, "rgb", "texSample", 4, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color0.r");

  WriteSampleColor(p, "rgb", "texSample", 5, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color1.r");

  WriteSampleColor(p, "rgb", "texSample", 6, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color0.a");

  WriteSampleColor(p, "rgb", "texSample", 7, ApiType, params);
  WriteColorToIntensity(p, "texSample", "color1.a");

  WRITE(p, "  color0.rgba += IntensityConst.aaaa;\n");
  WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

  WriteToBitDepth(p, 4, "color0", "color0");
  WriteToBitDepth(p, 4, "color1", "color1");

  WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteIA8Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RA8, ApiType);
  WRITE(p, "  float4 texSample;\n");

  WriteSampleColor(p, "rgba", "texSample", 0, ApiType, params);
  WRITE(p, "  ocol0.b = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "ocol0.g");

  WriteSampleColor(p, "rgba", "texSample", 1, ApiType, params);
  WRITE(p, "  ocol0.r = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "ocol0.a");

  WRITE(p, "  ocol0.ga += IntensityConst.aa;\n");

  WriteEncoderEnd(p);
}

static void WriteIA4Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RA4, ApiType);
  WRITE(p, "  float4 texSample;\n");
  WRITE(p, "  float4 color0;\n");
  WRITE(p, "  float4 color1;\n");

  WriteSampleColor(p, "rgba", "texSample", 0, ApiType, params);
  WRITE(p, "  color0.b = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "color1.b");

  WriteSampleColor(p, "rgba", "texSample", 1, ApiType, params);
  WRITE(p, "  color0.g = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "color1.g");

  WriteSampleColor(p, "rgba", "texSample", 2, ApiType, params);
  WRITE(p, "  color0.r = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "color1.r");

  WriteSampleColor(p, "rgba", "texSample", 3, ApiType, params);
  WRITE(p, "  color0.a = texSample.a;\n");
  WriteColorToIntensity(p, "texSample", "color1.a");

  WRITE(p, "  color1.rgba += IntensityConst.aaaa;\n");

  WriteToBitDepth(p, 4, "color0", "color0");
  WriteToBitDepth(p, 4, "color1", "color1");

  WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteRGB565Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RGB565, ApiType);
  WRITE(p, "  float3 texSample0;\n");
  WRITE(p, "  float3 texSample1;\n");

  WriteSampleColor(p, "rgb", "texSample0", 0, ApiType, params);
  WriteSampleColor(p, "rgb", "texSample1", 1, ApiType, params);
  WRITE(p, "  float2 texRs = float2(texSample0.r, texSample1.r);\n");
  WRITE(p, "  float2 texGs = float2(texSample0.g, texSample1.g);\n");
  WRITE(p, "  float2 texBs = float2(texSample0.b, texSample1.b);\n");

  WriteToBitDepth(p, 6, "texGs", "float2 gInt");
  WRITE(p, "  float2 gUpper = floor(gInt / 8.0);\n");
  WRITE(p, "  float2 gLower = gInt - gUpper * 8.0;\n");

  WriteToBitDepth(p, 5, "texRs", "ocol0.br");
  WRITE(p, "  ocol0.br = ocol0.br * 8.0 + gUpper;\n");
  WriteToBitDepth(p, 5, "texBs", "ocol0.ga");
  WRITE(p, "  ocol0.ga = ocol0.ga + gLower * 32.0;\n");

  WRITE(p, "  ocol0 = ocol0 / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteRGB5A3Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RGB5A3, ApiType);

  WRITE(p, "  float4 texSample;\n");
  WRITE(p, "  float color0;\n");
  WRITE(p, "  float gUpper;\n");
  WRITE(p, "  float gLower;\n");

  WriteSampleColor(p, "rgba", "texSample", 0, ApiType, params);

  // 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
  WRITE(p, "if(texSample.a > 0.878f) {\n");

  WriteToBitDepth(p, 5, "texSample.g", "color0");
  WRITE(p, "  gUpper = floor(color0 / 8.0);\n");
  WRITE(p, "  gLower = color0 - gUpper * 8.0;\n");

  WriteToBitDepth(p, 5, "texSample.r", "ocol0.b");
  WRITE(p, "  ocol0.b = ocol0.b * 4.0 + gUpper + 128.0;\n");
  WriteToBitDepth(p, 5, "texSample.b", "ocol0.g");
  WRITE(p, "  ocol0.g = ocol0.g + gLower * 32.0;\n");

  WRITE(p, "} else {\n");

  WriteToBitDepth(p, 4, "texSample.r", "ocol0.b");
  WriteToBitDepth(p, 4, "texSample.b", "ocol0.g");

  WriteToBitDepth(p, 3, "texSample.a", "color0");
  WRITE(p, "ocol0.b = ocol0.b + color0 * 16.0;\n");
  WriteToBitDepth(p, 4, "texSample.g", "color0");
  WRITE(p, "ocol0.g = ocol0.g + color0 * 16.0;\n");

  WRITE(p, "}\n");

  WriteSampleColor(p, "rgba", "texSample", 1, ApiType, params);

  WRITE(p, "if(texSample.a > 0.878f) {\n");

  WriteToBitDepth(p, 5, "texSample.g", "color0");
  WRITE(p, "  gUpper = floor(color0 / 8.0);\n");
  WRITE(p, "  gLower = color0 - gUpper * 8.0;\n");

  WriteToBitDepth(p, 5, "texSample.r", "ocol0.r");
  WRITE(p, "  ocol0.r = ocol0.r * 4.0 + gUpper + 128.0;\n");
  WriteToBitDepth(p, 5, "texSample.b", "ocol0.a");
  WRITE(p, "  ocol0.a = ocol0.a + gLower * 32.0;\n");

  WRITE(p, "} else {\n");

  WriteToBitDepth(p, 4, "texSample.r", "ocol0.r");
  WriteToBitDepth(p, 4, "texSample.b", "ocol0.a");

  WriteToBitDepth(p, 3, "texSample.a", "color0");
  WRITE(p, "ocol0.r = ocol0.r + color0 * 16.0;\n");
  WriteToBitDepth(p, 4, "texSample.g", "color0");
  WRITE(p, "ocol0.a = ocol0.a + color0 * 16.0;\n");

  WRITE(p, "}\n");

  WRITE(p, "  ocol0 = ocol0 / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteRGBA8Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RGBA8, ApiType);

  WRITE(p, "  float4 texSample;\n");
  WRITE(p, "  float4 color0;\n");
  WRITE(p, "  float4 color1;\n");

  WriteSampleColor(p, "rgba", "texSample", 0, ApiType, params);
  WRITE(p, "  color0.b = texSample.a;\n");
  WRITE(p, "  color0.g = texSample.r;\n");
  WRITE(p, "  color1.b = texSample.g;\n");
  WRITE(p, "  color1.g = texSample.b;\n");

  WriteSampleColor(p, "rgba", "texSample", 1, ApiType, params);
  WRITE(p, "  color0.r = texSample.a;\n");
  WRITE(p, "  color0.a = texSample.r;\n");
  WRITE(p, "  color1.r = texSample.g;\n");
  WRITE(p, "  color1.a = texSample.b;\n");

  WRITE(p, "  ocol0 = first ? color0 : color1;\n");

  WriteEncoderEnd(p);
}

static void WriteC4Encoder(char*& p, const char* comp, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::R4, ApiType);
  WRITE(p, "  float4 color0;\n");
  WRITE(p, "  float4 color1;\n");

  WriteSampleColor(p, comp, "color0.b", 0, ApiType, params);
  WriteSampleColor(p, comp, "color1.b", 1, ApiType, params);
  WriteSampleColor(p, comp, "color0.g", 2, ApiType, params);
  WriteSampleColor(p, comp, "color1.g", 3, ApiType, params);
  WriteSampleColor(p, comp, "color0.r", 4, ApiType, params);
  WriteSampleColor(p, comp, "color1.r", 5, ApiType, params);
  WriteSampleColor(p, comp, "color0.a", 6, ApiType, params);
  WriteSampleColor(p, comp, "color1.a", 7, ApiType, params);

  WriteToBitDepth(p, 4, "color0", "color0");
  WriteToBitDepth(p, 4, "color1", "color1");

  WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteC8Encoder(char*& p, const char* comp, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::R8, ApiType);

  WriteSampleColor(p, comp, "ocol0.b", 0, ApiType, params);
  WriteSampleColor(p, comp, "ocol0.g", 1, ApiType, params);
  WriteSampleColor(p, comp, "ocol0.r", 2, ApiType, params);
  WriteSampleColor(p, comp, "ocol0.a", 3, ApiType, params);

  WriteEncoderEnd(p);
}

static void WriteCC4Encoder(char*& p, const char* comp, APIType ApiType,
                            const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RA4, ApiType);
  WRITE(p, "  float2 texSample;\n");
  WRITE(p, "  float4 color0;\n");
  WRITE(p, "  float4 color1;\n");

  WriteSampleColor(p, comp, "texSample", 0, ApiType, params);
  WRITE(p, "  color0.b = texSample.x;\n");
  WRITE(p, "  color1.b = texSample.y;\n");

  WriteSampleColor(p, comp, "texSample", 1, ApiType, params);
  WRITE(p, "  color0.g = texSample.x;\n");
  WRITE(p, "  color1.g = texSample.y;\n");

  WriteSampleColor(p, comp, "texSample", 2, ApiType, params);
  WRITE(p, "  color0.r = texSample.x;\n");
  WRITE(p, "  color1.r = texSample.y;\n");

  WriteSampleColor(p, comp, "texSample", 3, ApiType, params);
  WRITE(p, "  color0.a = texSample.x;\n");
  WRITE(p, "  color1.a = texSample.y;\n");

  WriteToBitDepth(p, 4, "color0", "color0");
  WriteToBitDepth(p, 4, "color1", "color1");

  WRITE(p, "  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
  WriteEncoderEnd(p);
}

static void WriteCC8Encoder(char*& p, const char* comp, APIType ApiType,
                            const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RA8, ApiType);

  WriteSampleColor(p, comp, "ocol0.bg", 0, ApiType, params);
  WriteSampleColor(p, comp, "ocol0.ra", 1, ApiType, params);

  WriteEncoderEnd(p);
}

static void WriteZ8Encoder(char*& p, const char* multiplier, APIType ApiType,
                           const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::G8, ApiType);

  WRITE(p, " float depth;\n");

  WriteSampleColor(p, "r", "depth", 0, ApiType, params);
  WRITE(p, "ocol0.b = frac(depth * %s);\n", multiplier);

  WriteSampleColor(p, "r", "depth", 1, ApiType, params);
  WRITE(p, "ocol0.g = frac(depth * %s);\n", multiplier);

  WriteSampleColor(p, "r", "depth", 2, ApiType, params);
  WRITE(p, "ocol0.r = frac(depth * %s);\n", multiplier);

  WriteSampleColor(p, "r", "depth", 3, ApiType, params);
  WRITE(p, "ocol0.a = frac(depth * %s);\n", multiplier);

  WriteEncoderEnd(p);
}

static void WriteZ16Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RA8, ApiType);

  WRITE(p, "  float depth;\n");
  WRITE(p, "  float3 expanded;\n");

  // byte order is reversed

  WriteSampleColor(p, "r", "depth", 0, ApiType, params);

  WRITE(p, "  depth *= 16777216.0;\n");
  WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
  WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
  WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

  WRITE(p, "  ocol0.b = expanded.g / 255.0;\n");
  WRITE(p, "  ocol0.g = expanded.r / 255.0;\n");

  WriteSampleColor(p, "r", "depth", 1, ApiType, params);

  WRITE(p, "  depth *= 16777216.0;\n");
  WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
  WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
  WRITE(p, "  expanded.g = floor(depth / 256.0);\n");

  WRITE(p, "  ocol0.r = expanded.g / 255.0;\n");
  WRITE(p, "  ocol0.a = expanded.r / 255.0;\n");

  WriteEncoderEnd(p);
}

static void WriteZ16LEncoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::GB8, ApiType);

  WRITE(p, "  float depth;\n");
  WRITE(p, "  float3 expanded;\n");

  // byte order is reversed

  WriteSampleColor(p, "r", "depth", 0, ApiType, params);

  WRITE(p, "  depth *= 16777216.0;\n");
  WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
  WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
  WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
  WRITE(p, "  depth -= expanded.g * 256.0;\n");
  WRITE(p, "  expanded.b = depth;\n");

  WRITE(p, "  ocol0.b = expanded.b / 255.0;\n");
  WRITE(p, "  ocol0.g = expanded.g / 255.0;\n");

  WriteSampleColor(p, "r", "depth", 1, ApiType, params);

  WRITE(p, "  depth *= 16777216.0;\n");
  WRITE(p, "  expanded.r = floor(depth / (256.0 * 256.0));\n");
  WRITE(p, "  depth -= expanded.r * 256.0 * 256.0;\n");
  WRITE(p, "  expanded.g = floor(depth / 256.0);\n");
  WRITE(p, "  depth -= expanded.g * 256.0;\n");
  WRITE(p, "  expanded.b = depth;\n");

  WRITE(p, "  ocol0.r = expanded.b / 255.0;\n");
  WRITE(p, "  ocol0.a = expanded.g / 255.0;\n");

  WriteEncoderEnd(p);
}

static void WriteZ24Encoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::RGBA8, ApiType);

  WRITE(p, "  float depth0;\n");
  WRITE(p, "  float depth1;\n");
  WRITE(p, "  float3 expanded0;\n");
  WRITE(p, "  float3 expanded1;\n");

  WriteSampleColor(p, "r", "depth0", 0, ApiType, params);
  WriteSampleColor(p, "r", "depth1", 1, ApiType, params);

  for (int i = 0; i < 2; i++)
  {
    WRITE(p, "  depth%i *= 16777216.0;\n", i);

    WRITE(p, "  expanded%i.r = floor(depth%i / (256.0 * 256.0));\n", i, i);
    WRITE(p, "  depth%i -= expanded%i.r * 256.0 * 256.0;\n", i, i);
    WRITE(p, "  expanded%i.g = floor(depth%i / 256.0);\n", i, i);
    WRITE(p, "  depth%i -= expanded%i.g * 256.0;\n", i, i);
    WRITE(p, "  expanded%i.b = depth%i;\n", i, i);
  }

  WRITE(p, "  if (!first) {\n");
  // upper 16
  WRITE(p, "     ocol0.b = expanded0.g / 255.0;\n");
  WRITE(p, "     ocol0.g = expanded0.b / 255.0;\n");
  WRITE(p, "     ocol0.r = expanded1.g / 255.0;\n");
  WRITE(p, "     ocol0.a = expanded1.b / 255.0;\n");
  WRITE(p, "  } else {\n");
  // lower 8
  WRITE(p, "     ocol0.b = 1.0;\n");
  WRITE(p, "     ocol0.g = expanded0.r / 255.0;\n");
  WRITE(p, "     ocol0.r = 1.0;\n");
  WRITE(p, "     ocol0.a = expanded1.r / 255.0;\n");
  WRITE(p, "  }\n");

  WriteEncoderEnd(p);
}

static void WriteXFBEncoder(char*& p, APIType ApiType, const EFBCopyParams& params)
{
  WriteSwizzler(p, params, EFBCopyFormat::XFB, ApiType);

  WRITE(p, "float3 color0, color1;\n");
  WriteSampleColor(p, "rgb", "color0", 0, ApiType, params);
  WriteSampleColor(p, "rgb", "color1", 1, ApiType, params);

  // Gamma is only applied to XFB copies.
  WRITE(p, "  color0 = pow(color0, float3(gamma_rcp, gamma_rcp, gamma_rcp));\n");
  WRITE(p, "  color1 = pow(color1, float3(gamma_rcp, gamma_rcp, gamma_rcp));\n");

  // Convert to YUV.
  WRITE(p, "  const float3 y_const = float3(0.257, 0.504, 0.098);\n");
  WRITE(p, "  const float3 u_const = float3(-0.148, -0.291, 0.439);\n");
  WRITE(p, "  const float3 v_const = float3(0.439, -0.368, -0.071);\n");
  WRITE(p, "  float3 average = (color0 + color1) * 0.5;\n");
  WRITE(p, "  ocol0.b = dot(color0,  y_const) + 0.0625;\n");
  WRITE(p, "  ocol0.g = dot(average, u_const) + 0.5;\n");
  WRITE(p, "  ocol0.r = dot(color1,  y_const) + 0.0625;\n");
  WRITE(p, "  ocol0.a = dot(average, v_const) + 0.5;\n");

  WriteEncoderEnd(p);
}

const char* GenerateEncodingShader(const EFBCopyParams& params, APIType api_type)
{
  text[sizeof(text) - 1] = 0x7C;  // canary

  char* p = text;

  switch (params.copy_format)
  {
  case EFBCopyFormat::R4:
    if (params.yuv)
      WriteI4Encoder(p, api_type, params);
    else
      WriteC4Encoder(p, "r", api_type, params);
    break;
  case EFBCopyFormat::RA4:
    if (params.yuv)
      WriteIA4Encoder(p, api_type, params);
    else
      WriteCC4Encoder(p, "ar", api_type, params);
    break;
  case EFBCopyFormat::RA8:
    if (params.yuv)
      WriteIA8Encoder(p, api_type, params);
    else
      WriteCC8Encoder(p, "ar", api_type, params);
    break;
  case EFBCopyFormat::RGB565:
    WriteRGB565Encoder(p, api_type, params);
    break;
  case EFBCopyFormat::RGB5A3:
    WriteRGB5A3Encoder(p, api_type, params);
    break;
  case EFBCopyFormat::RGBA8:
    if (params.depth)
      WriteZ24Encoder(p, api_type, params);
    else
      WriteRGBA8Encoder(p, api_type, params);
    break;
  case EFBCopyFormat::A8:
    WriteC8Encoder(p, "a", api_type, params);
    break;
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
    if (params.yuv)
      WriteI8Encoder(p, api_type, params);
    else
      WriteC8Encoder(p, "r", api_type, params);
    break;
  case EFBCopyFormat::G8:
    if (params.depth)
      WriteZ8Encoder(p, "256.0", api_type, params);  // Z8M
    else
      WriteC8Encoder(p, "g", api_type, params);
    break;
  case EFBCopyFormat::B8:
    if (params.depth)
      WriteZ8Encoder(p, "65536.0", api_type, params);  // Z8L
    else
      WriteC8Encoder(p, "b", api_type, params);
    break;
  case EFBCopyFormat::RG8:
    if (params.depth)
      WriteZ16Encoder(p, api_type, params);  // Z16H
    else
      WriteCC8Encoder(p, "gr", api_type, params);
    break;
  case EFBCopyFormat::GB8:
    if (params.depth)
      WriteZ16LEncoder(p, api_type, params);  // Z16L
    else
      WriteCC8Encoder(p, "bg", api_type, params);
    break;
  case EFBCopyFormat::XFB:
    WriteXFBEncoder(p, api_type, params);
    break;
  default:
    PanicAlert("Invalid EFB Copy Format (0x%X)! (GenerateEncodingShader)",
               static_cast<int>(params.copy_format));
    break;
  }

  if (text[sizeof(text) - 1] != 0x7C)
    PanicAlert("TextureConversionShader generator - buffer too small, canary has been eaten!");

  return text;
}

// NOTE: In these uniforms, a row refers to a row of blocks, not texels.
static const char decoding_shader_header[] = R"(
#if defined(PALETTE_FORMAT_IA8) || defined(PALETTE_FORMAT_RGB565) || defined(PALETTE_FORMAT_RGB5A3)
#define HAS_PALETTE 1
#endif

#ifdef API_D3D
cbuffer UBO : register(b0) {
#else
UBO_BINDING(std140, 1) uniform UBO {
#endif
  uint2 u_dst_size;
  uint2 u_src_size;
  uint u_src_offset;
  uint u_src_row_stride;
  uint u_palette_offset;
};

#ifdef API_D3D

Buffer<uint4> s_input_buffer : register(t0);
#ifdef HAS_PALETTE
Buffer<uint4> s_palette_buffer : register(t1);
#endif

RWTexture2DArray<unorm float4> output_image : register(u0);

// Helpers for reading/writing.
#define texelFetch(buffer, pos) buffer.Load(pos)
#define imageStore(image, coords, value) image[coords] = value
#define GROUP_MEMORY_BARRIER_WITH_SYNC GroupMemoryBarrierWithGroupSync();
#define GROUP_SHARED groupshared

#define DEFINE_MAIN(lx, ly) \
  [numthreads(lx, ly, 1)] \
  void main(uint3 gl_WorkGroupID : SV_GroupId, \
            uint3 gl_LocalInvocationID : SV_GroupThreadID, \
            uint3 gl_GlobalInvocationID : SV_DispatchThreadID)

uint bitfieldExtract(uint val, int off, int size)
{
  // This built-in function is only support in OpenGL 4.0+ and ES 3.1+\n"
  // Microsoft's HLSL compiler automatically optimises this to a bitfield extract instruction.
  uint mask = uint((1 << size) - 1);
  return uint(val >> off) & mask;
}

#else

TEXEL_BUFFER_BINDING(0) uniform usamplerBuffer s_input_buffer;
#ifdef HAS_PALETTE
TEXEL_BUFFER_BINDING(1) uniform usamplerBuffer s_palette_buffer;
#endif
IMAGE_BINDING(rgba8, 0) uniform writeonly image2DArray output_image;

#define GROUP_MEMORY_BARRIER_WITH_SYNC memoryBarrierShared(); barrier();
#define GROUP_SHARED shared

#define DEFINE_MAIN(lx, ly) \
  layout(local_size_x = lx, local_size_y = ly) in; \
  void main()

#endif

uint Swap16(uint v)
{
  // Convert BE to LE.
  return ((v >> 8) | (v << 8)) & 0xFFFFu;
}

uint Convert3To8(uint v)
{
  // Swizzle bits: 00000123 -> 12312312
  return (v << 5) | (v << 2) | (v >> 1);
}
uint Convert4To8(uint v)
{
  // Swizzle bits: 00001234 -> 12341234
  return (v << 4) | v;
}
uint Convert5To8(uint v)
{
  // Swizzle bits: 00012345 -> 12345123
  return (v << 3) | (v >> 2);
}
uint Convert6To8(uint v)
{
  // Swizzle bits: 00123456 -> 12345612
  return (v << 2) | (v >> 4);
}

uint GetTiledTexelOffset(uint2 block_size, uint2 coords)
{
  uint2 block = coords / block_size;
  uint2 offset = coords % block_size;
  uint buffer_pos = u_src_offset;
  buffer_pos += block.y * u_src_row_stride;
  buffer_pos += block.x * (block_size.x * block_size.y);
  buffer_pos += offset.y * block_size.x;
  buffer_pos += offset.x;
  return buffer_pos;
}

uint4 GetPaletteColor(uint index)
{
  // Fetch and swap BE to LE.
  uint val = Swap16(texelFetch(s_palette_buffer, int(u_palette_offset + index)).x);

  uint4 color;
#if defined(PALETTE_FORMAT_IA8)
  uint a = bitfieldExtract(val, 8, 8);
  uint i = bitfieldExtract(val, 0, 8);
  color = uint4(i, i, i, a);
#elif defined(PALETTE_FORMAT_RGB565)
  color.x = Convert5To8(bitfieldExtract(val, 11, 5));
  color.y = Convert6To8(bitfieldExtract(val, 5, 6));
  color.z = Convert5To8(bitfieldExtract(val, 0, 5));
  color.a = 255u;

#elif defined(PALETTE_FORMAT_RGB5A3)
  if ((val & 0x8000u) != 0u)
  {
    color.x = Convert5To8(bitfieldExtract(val, 10, 5));
    color.y = Convert5To8(bitfieldExtract(val, 5, 5));
    color.z = Convert5To8(bitfieldExtract(val, 0, 5));
    color.a = 255u;
  }
  else
  {
    color.a = Convert3To8(bitfieldExtract(val, 12, 3));
    color.r = Convert4To8(bitfieldExtract(val, 8, 4));
    color.g = Convert4To8(bitfieldExtract(val, 4, 4));
    color.b = Convert4To8(bitfieldExtract(val, 0, 4));
  }
#else
  // Not used.
  color = uint4(0, 0, 0, 0);
#endif

  return color;
}

float4 GetPaletteColorNormalized(uint index)
{
  uint4 color = GetPaletteColor(index);
  return float4(color) / 255.0;
}

)";

static const std::map<TextureFormat, DecodingShaderInfo> s_decoding_shader_info{
    {TextureFormat::I4,
     {TEXEL_BUFFER_FORMAT_R8_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 8x8 blocks, 4 bits per pixel
        // We need to do the tiling manually here because the texel size is smaller than
        // the size of the buffer elements.
        uint2 block = coords.xy / 8u;
        uint2 offset = coords.xy % 8u;
        uint buffer_pos = u_src_offset;
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x / 2u;

        // Select high nibble for odd texels, low for even.
        uint val = texelFetch(s_input_buffer, int(buffer_pos)).x;
        uint i;
        if ((coords.x & 1u) == 0u)
          i = Convert4To8((val >> 4));
        else
          i = Convert4To8((val & 0x0Fu));

        uint4 color = uint4(i, i, i, i);
        float4 norm_color = float4(color) / 255.0;

        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }

      )"}},
    {TextureFormat::IA4,
     {TEXEL_BUFFER_FORMAT_R8_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 8x4 blocks, 8 bits per pixel
        uint buffer_pos = GetTiledTexelOffset(uint2(8u, 4u), coords);
        uint val = texelFetch(s_input_buffer, int(buffer_pos)).x;
        uint i = Convert4To8((val & 0x0Fu));
        uint a = Convert4To8((val >> 4));
        uint4 color = uint4(i, i, i, a);
        float4 norm_color = float4(color) / 255.0;

        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},
    {TextureFormat::I8,
     {TEXEL_BUFFER_FORMAT_R8_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 8x4 blocks, 8 bits per pixel
        uint buffer_pos = GetTiledTexelOffset(uint2(8u, 4u), coords);
        uint i = texelFetch(s_input_buffer, int(buffer_pos)).x;
        uint4 color = uint4(i, i, i, i);
        float4 norm_color = float4(color) / 255.0;

        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},
    {TextureFormat::IA8,
     {TEXEL_BUFFER_FORMAT_R16_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 4x4 blocks, 16 bits per pixel
        uint buffer_pos = GetTiledTexelOffset(uint2(4u, 4u), coords);
        uint val = texelFetch(s_input_buffer, int(buffer_pos)).x;
        uint a = (val & 0xFFu);
        uint i = (val >> 8);
        uint4 color = uint4(i, i, i, a);
        float4 norm_color = float4(color) / 255.0;
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},
    {TextureFormat::RGB565,
     {TEXEL_BUFFER_FORMAT_R16_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 4x4 blocks
        uint buffer_pos = GetTiledTexelOffset(uint2(4u, 4u), coords);
        uint val = Swap16(texelFetch(s_input_buffer, int(buffer_pos)).x);

        uint4 color;
        color.x = Convert5To8(bitfieldExtract(val, 11, 5));
        color.y = Convert6To8(bitfieldExtract(val, 5, 6));
        color.z = Convert5To8(bitfieldExtract(val, 0, 5));
        color.a = 255u;

        float4 norm_color = float4(color) / 255.0;
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }

      )"}},
    {TextureFormat::RGB5A3,
     {TEXEL_BUFFER_FORMAT_R16_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 4x4 blocks
        uint buffer_pos = GetTiledTexelOffset(uint2(4u, 4u), coords);
        uint val = Swap16(texelFetch(s_input_buffer, int(buffer_pos)).x);

        uint4 color;
        if ((val & 0x8000u) != 0u)
        {
          color.x = Convert5To8(bitfieldExtract(val, 10, 5));
          color.y = Convert5To8(bitfieldExtract(val, 5, 5));
          color.z = Convert5To8(bitfieldExtract(val, 0, 5));
          color.a = 255u;
        }
        else
        {
          color.a = Convert3To8(bitfieldExtract(val, 12, 3));
          color.r = Convert4To8(bitfieldExtract(val, 8, 4));
          color.g = Convert4To8(bitfieldExtract(val, 4, 4));
          color.b = Convert4To8(bitfieldExtract(val, 0, 4));
        }

        float4 norm_color = float4(color) / 255.0;
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }

      )"}},
    {TextureFormat::RGBA8,
     {TEXEL_BUFFER_FORMAT_R16_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 4x4 blocks
        // We can't use the normal calculation function, as these are packed as the AR channels
        // for the entire block, then the GB channels afterwards.
        uint2 block = coords.xy / 4u;
        uint2 offset = coords.xy % 4u;
        uint buffer_pos = u_src_offset;

        // Our buffer has 16-bit elements, so the offsets here are half what they would be in bytes.
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x;

        // The two GB channels follow after the block's AR channels.
        uint val1 = texelFetch(s_input_buffer, int(buffer_pos + 0u)).x;
        uint val2 = texelFetch(s_input_buffer, int(buffer_pos + 16u)).x;

        uint4 color;
        color.a = (val1 & 0xFFu);
        color.r = (val1 >> 8);
        color.g = (val2 & 0xFFu);
        color.b = (val2 >> 8);

        float4 norm_color = float4(color) / 255.0;
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},
    {TextureFormat::CMPR,
     {TEXEL_BUFFER_FORMAT_R32G32_UINT, 0, 64, 1, true,
      R"(
      // In the compute version of this decoder, we flatten the blocks to a one-dimension array.
      // Each group is subdivided into 16, and the first thread in each group fetches the DXT data.
      // All threads then calculate the possible colors for the block and write to the output image.

      #define GROUP_SIZE 64u
      #define BLOCK_SIZE_X 4u
      #define BLOCK_SIZE_Y 4u
      #define BLOCK_SIZE (BLOCK_SIZE_X * BLOCK_SIZE_Y)
      #define BLOCKS_PER_GROUP (GROUP_SIZE / BLOCK_SIZE)

      uint DXTBlend(uint v1, uint v2)
      {
        // 3/8 blend, which is close to 1/3
        return ((v1 * 3u + v2 * 5u) >> 3);
      }

      GROUP_SHARED uint2 shared_temp[BLOCKS_PER_GROUP];

      DEFINE_MAIN(GROUP_SIZE, 8)
      {
        uint local_thread_id = gl_LocalInvocationID.x;
        uint block_in_group = local_thread_id / BLOCK_SIZE;
        uint thread_in_block = local_thread_id % BLOCK_SIZE;
        uint block_index = gl_WorkGroupID.x * BLOCKS_PER_GROUP + block_in_group;

        // Annoyingly, we can't precalculate this as a uniform because the DXT block size differs
        // from the block size of the overall texture (4 vs 8). We can however use a multiply and
        // subtraction to avoid the modulo for calculating the block's X coordinate.
        uint blocks_wide = u_src_size.x / BLOCK_SIZE_X;
        uint2 block_coords;
        block_coords.y = block_index / blocks_wide;
        block_coords.x = block_index - (block_coords.y * blocks_wide);

        // Only the first thread for each block reads from the texel buffer.
        if (thread_in_block == 0u)
        {
          // Calculate tiled block coordinates.
          uint2 tile_block_coords = block_coords / 2u;
          uint2 subtile_block_coords = block_coords % 2u;
          uint buffer_pos = u_src_offset;
          buffer_pos += tile_block_coords.y * u_src_row_stride;
          buffer_pos += tile_block_coords.x * 4u;
          buffer_pos += subtile_block_coords.y * 2u;
          buffer_pos += subtile_block_coords.x;

          // Read the entire DXT block to shared memory.
          uint2 raw_data = texelFetch(s_input_buffer, int(buffer_pos)).xy;
          shared_temp[block_in_group] = raw_data;
        }

        // Ensure store is completed before the remaining threads in the block continue.
        GROUP_MEMORY_BARRIER_WITH_SYNC;

        // Unpack colors and swap BE to LE.
        uint2 raw_data = shared_temp[block_in_group];
        uint swapped = ((raw_data.x & 0xFF00FF00u) >> 8) | ((raw_data.x & 0x00FF00FFu) << 8);
        uint c1 = swapped & 0xFFFFu;
        uint c2 = swapped >> 16;

        // Expand 5/6 bit channels to 8-bits per channel.
        uint blue1 = Convert5To8(bitfieldExtract(c1, 0, 5));
        uint blue2 = Convert5To8(bitfieldExtract(c2, 0, 5));
        uint green1 = Convert6To8(bitfieldExtract(c1, 5, 6));
        uint green2 = Convert6To8(bitfieldExtract(c2, 5, 6));
        uint red1 = Convert5To8(bitfieldExtract(c1, 11, 5));
        uint red2 = Convert5To8(bitfieldExtract(c2, 11, 5));

        // Determine the four colors the block can use.
        // It's quicker to just precalculate all four colors rather than branching on the index.
        // NOTE: These must be masked with 0xFF. This is done at the normalization stage below.
        uint4 color0, color1, color2, color3;
        color0 = uint4(red1, green1, blue1, 255u);
        color1 = uint4(red2, green2, blue2, 255u);
        if (c1 > c2)
        {
          color2 = uint4(DXTBlend(red2, red1), DXTBlend(green2, green1), DXTBlend(blue2, blue1), 255u);
          color3 = uint4(DXTBlend(red1, red2), DXTBlend(green1, green2), DXTBlend(blue1, blue2), 255u);
        }
        else
        {
          color2 = uint4((red1 + red2) / 2u, (green1 + green2) / 2u, (blue1 + blue2) / 2u, 255u);
          color3 = uint4((red1 + red2) / 2u, (green1 + green2) / 2u, (blue1 + blue2) / 2u, 0u);
        }

        // Calculate the texel coordinates that we will write to.
        // The divides/modulo here should be turned into a shift/binary AND.
        uint local_y = thread_in_block / BLOCK_SIZE_X;
        uint local_x = thread_in_block % BLOCK_SIZE_X;
        uint global_x = block_coords.x * BLOCK_SIZE_X + local_x;
        uint global_y = block_coords.y * BLOCK_SIZE_Y + local_y;

        // Use the coordinates within the block to shift the 32-bit value containing
        // all 16 indices to a single 2-bit index.
        uint index = bitfieldExtract(raw_data.y, int((local_y * 8u) + (6u - local_x * 2u)), 2);

        // Select the un-normalized color from the precalculated color array.
        // Using a switch statement here removes the need for dynamic indexing of an array.
        uint4 color;
        switch (index)
        {
        case 0u:  color = color0;   break;
        case 1u:  color = color1;   break;
        case 2u:  color = color2;   break;
        case 3u:  color = color3;   break;
        default:  color = color0;   break;
        }

        // Normalize and write to the output image.
        float4 norm_color = float4(color & 0xFFu) / 255.0;
        imageStore(output_image, int3(int2(uint2(global_x, global_y)), 0), norm_color);
      }
      )"}},
    {TextureFormat::C4,
     {TEXEL_BUFFER_FORMAT_R8_UINT, static_cast<u32>(TexDecoder_GetPaletteSize(TextureFormat::C4)),
      8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 8x8 blocks, 4 bits per pixel
        // We need to do the tiling manually here because the texel size is smaller than
        // the size of the buffer elements.
        uint2 block = coords.xy / 8u;
        uint2 offset = coords.xy % 8u;
        uint buffer_pos = u_src_offset;
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x / 2u;

        // Select high nibble for odd texels, low for even.
        uint val = texelFetch(s_input_buffer, int(buffer_pos)).x;
        uint index = ((coords.x & 1u) == 0u) ? (val >> 4) : (val & 0x0Fu);
        float4 norm_color = GetPaletteColorNormalized(index);
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }

      )"}},
    {TextureFormat::C8,
     {TEXEL_BUFFER_FORMAT_R8_UINT, static_cast<u32>(TexDecoder_GetPaletteSize(TextureFormat::C8)),
      8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 8x4 blocks, 8 bits per pixel
        uint buffer_pos = GetTiledTexelOffset(uint2(8u, 4u), coords);
        uint index = texelFetch(s_input_buffer, int(buffer_pos)).x;
        float4 norm_color = GetPaletteColorNormalized(index);
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},
    {TextureFormat::C14X2,
     {TEXEL_BUFFER_FORMAT_R16_UINT,
      static_cast<u32>(TexDecoder_GetPaletteSize(TextureFormat::C14X2)), 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 coords = gl_GlobalInvocationID.xy;

        // Tiled in 4x4 blocks, 16 bits per pixel
        uint buffer_pos = GetTiledTexelOffset(uint2(4u, 4u), coords);
        uint index = Swap16(texelFetch(s_input_buffer, int(buffer_pos)).x) & 0x3FFFu;
        float4 norm_color = GetPaletteColorNormalized(index);
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},

    // We do the inverse BT.601 conversion for YCbCr to RGB
    // http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
    {TextureFormat::XFB,
     {TEXEL_BUFFER_FORMAT_RGBA8_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 uv = gl_GlobalInvocationID.xy;
        int buffer_pos = int(u_src_offset + (uv.y * u_src_row_stride) + (uv.x / 2u));
        float4 yuyv = float4(texelFetch(s_input_buffer, buffer_pos));

        float y = (uv.x & 1u) != 0u ? yuyv.b : yuyv.r;

        float yComp = 1.164 * (y - 16.0);
        float uComp = yuyv.g - 128.0;
        float vComp = yuyv.a - 128.0;

        float4 rgb = float4(yComp + (1.596 * vComp),
                        yComp - (0.813 * vComp) - (0.391 * uComp),
                        yComp + (2.018 * uComp),
                        255.0);
        float4 rgba_norm = rgb / 255.0;
        imageStore(output_image, int3(int2(uv), 0), rgba_norm);
      }
      )"}}};

const DecodingShaderInfo* GetDecodingShaderInfo(TextureFormat format)
{
  auto iter = s_decoding_shader_info.find(format);
  return iter != s_decoding_shader_info.end() ? &iter->second : nullptr;
}

std::pair<u32, u32> GetDispatchCount(const DecodingShaderInfo* info, u32 width, u32 height)
{
  // Flatten to a single dimension?
  if (info->group_flatten)
    return {(width * height + (info->group_size_x - 1)) / info->group_size_x, 1};

  return {(width + (info->group_size_x - 1)) / info->group_size_x,
          (height + (info->group_size_y - 1)) / info->group_size_y};
}

std::string GenerateDecodingShader(TextureFormat format, TLUTFormat palette_format,
                                   APIType api_type)
{
  const DecodingShaderInfo* info = GetDecodingShaderInfo(format);
  if (!info)
    return "";

  std::stringstream ss;
  switch (palette_format)
  {
  case TLUTFormat::IA8:
    ss << "#define PALETTE_FORMAT_IA8 1\n";
    break;
  case TLUTFormat::RGB565:
    ss << "#define PALETTE_FORMAT_RGB565 1\n";
    break;
  case TLUTFormat::RGB5A3:
    ss << "#define PALETTE_FORMAT_RGB5A3 1\n";
    break;
  }

  ss << decoding_shader_header;
  ss << info->shader_body;

  return ss.str();
}

std::string GeneratePaletteConversionShader(TLUTFormat palette_format, APIType api_type)
{
  std::stringstream ss;

  ss << R"(
int Convert3To8(int v)
{
  // Swizzle bits: 00000123 -> 12312312
  return (v << 5) | (v << 2) | (v >> 1);
}
int Convert4To8(int v)
{
  // Swizzle bits: 00001234 -> 12341234
  return (v << 4) | v;
}
int Convert5To8(int v)
{
  // Swizzle bits: 00012345 -> 12345123
  return (v << 3) | (v >> 2);
}
int Convert6To8(int v)
{
  // Swizzle bits: 00123456 -> 12345612
  return (v << 2) | (v >> 4);
})";

  switch (palette_format)
  {
  case TLUTFormat::IA8:
    ss << R"(
float4 DecodePixel(int val)
{
  int i = val & 0xFF;
  int a = val >> 8;
  return float4(i, i, i, a) / 255.0;
})";
    break;

  case TLUTFormat::RGB565:
    ss << R"(
float4 DecodePixel(int val)
{
  int r, g, b, a;
  r = Convert5To8((val >> 11) & 0x1f);
  g = Convert6To8((val >> 5) & 0x3f);
  b = Convert5To8((val) & 0x1f);
  a = 0xFF;
  return float4(r, g, b, a) / 255.0;
})";
    break;

  case TLUTFormat::RGB5A3:
    ss << R"(
float4 DecodePixel(int val)
{
  int r,g,b,a;
  if ((val&0x8000) > 0)
  {
    r=Convert5To8((val>>10) & 0x1f);
    g=Convert5To8((val>>5 ) & 0x1f);
    b=Convert5To8((val    ) & 0x1f);
    a=0xFF;
  }
  else
  {
    a=Convert3To8((val>>12) & 0x7);
    r=Convert4To8((val>>8 ) & 0xf);
    g=Convert4To8((val>>4 ) & 0xf);
    b=Convert4To8((val    ) & 0xf);
  }
  return float4(r, g, b, a) / 255.0;
})";
    break;

  default:
    PanicAlert("Unknown format");
    break;
  }

  ss << "\n";

  if (api_type == APIType::D3D)
  {
    ss << "Buffer<uint> tex0 : register(t0);\n";
    ss << "Texture2DArray tex1 : register(t1);\n";
    ss << "SamplerState samp1 : register(s1);\n";
    ss << "cbuffer PSBlock : register(b0) {\n";
  }
  else
  {
    ss << "TEXEL_BUFFER_BINDING(0) uniform usamplerBuffer samp0;\n";
    ss << "SAMPLER_BINDING(1) uniform sampler2DArray samp1;\n";
    ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";
  }

  ss << "  float multiplier;\n";
  ss << "  int texel_buffer_offset;\n";
  ss << "};\n";

  if (api_type == APIType::D3D)
  {
    ss << "void main(in float3 v_tex0 : TEXCOORD0, out float4 ocol0 : SV_Target) {\n";
    ss << "  int src = int(round(tex1.Sample(samp1, v_tex0).r * multiplier));\n";
    ss << "  src = int(tex0.Load(src + texel_buffer_offset).r);\n";
  }
  else
  {
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      ss << "VARYING_LOCATION(0) in VertexData {\n";
      ss << "  float3 v_tex0;\n";
      ss << "};\n";
    }
    else
    {
      ss << "VARYING_LOCATION(0) in float3 v_tex0;\n";
    }
    ss << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n";
    ss << "void main() {\n";
    ss << "  float3 coords = v_tex0;\n";
    ss << "  int src = int(round(texture(samp1, coords).r * multiplier));\n";
    ss << "  src = int(texelFetch(samp0, src + texel_buffer_offset).r);\n";
  }

  ss << "  src = ((src << 8) & 0xFF00) | (src >> 8);\n";
  ss << "  ocol0 = DecodePixel(src);\n";
  ss << "}\n";

  return ss.str();
}

}  // namespace TextureConversionShaderTiled
