// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureConversionShader.h"

#include <map>
#include <sstream>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

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
    PanicAlertFmt("Invalid EFB Copy Format {}! (GetEncodedSampleCount)", format);
    return 1;
  }
}

static void WriteHeader(ShaderCode& code, APIType api_type)
{
  // left, top, of source rectangle within source texture
  // width of the destination rectangle, scale_factor (1 or 2)
  code.Write("UBO_BINDING(std140, 1) uniform PSBlock {{\n"
             "  int4 position;\n"
             "  float y_scale;\n"
             "  float gamma_rcp;\n"
             "  float2 clamp_tb;\n"
             "  uint3 filter_coefficients;\n"
             "}};\n");
  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    code.Write("VARYING_LOCATION(0) in VertexData {{\n"
               "  float3 v_tex0;\n"
               "}};\n");
  }
  else
  {
    code.Write("VARYING_LOCATION(0) in float3 v_tex0;\n");
  }
  code.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
             "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n");

  // Alpha channel in the copy is set to 1 the EFB format does not have an alpha channel.
  code.Write("float4 RGBA8ToRGB8(float4 src)\n"
             "{{\n"
             "  return float4(src.xyz, 1.0);\n"
             "}}\n"

             "float4 RGBA8ToRGBA6(float4 src)\n"
             "{{\n"
             "  int4 val = int4(roundEven(src * 255.0));\n"
             "  val = (val & 0xfc) | (val >> 6);\n"
             "  return float4(val) / 255.0;\n"
             "}}\n"

             "float4 RGBA8ToRGB565(float4 src)\n"
             "{{\n"
             "  int4 val = int4(roundEven(src * 255.0));\n"
             "  val.r = (val.r & 0xf8) | (val.r >> 5);\n"
             "  val.g = (val.g & 0xfc) | (val.g >> 6);\n"
             "  val.b = (val.b & 0xf8) | (val.b >> 5);\n"
             "  val.a = 255;\n"
             "  return float4(val) / 255.0;\n"
             "}}\n");
}

static void WriteSampleFunction(ShaderCode& code, const EFBCopyParams& params, APIType api_type)
{
  code.Write("uint4 SampleEFB0(float2 uv, float2 pixel_size, float x_offset, float y_offset) {{\n"
             "  float4 tex_sample = texture(samp0, float3(uv.x + x_offset * pixel_size.x, ");

  // Reverse the direction for OpenGL, since positive numbers are distance from the bottom row.
  // TODO: This isn't done on TextureConverterShaderGen - maybe it handles that via pixel_size?
  if (api_type == APIType::OpenGL)
    code.Write("clamp(uv.y - y_offset * pixel_size.y, clamp_tb.x, clamp_tb.y)");
  else
    code.Write("clamp(uv.y + y_offset * pixel_size.y, clamp_tb.x, clamp_tb.y)");

  code.Write(", 0.0));\n");

  // TODO: Is this really needed?  Doesn't the EFB only store appropriate values?  Or is this for
  // EFB2Ram having consistent output with force 32-bit color?
  if (params.efb_format == PixelFormat::RGB8_Z24)
    code.Write("  tex_sample = RGBA8ToRGB8(tex_sample);\n");
  else if (params.efb_format == PixelFormat::RGBA6_Z24)
    code.Write("  tex_sample = RGBA8ToRGBA6(tex_sample);\n");
  else if (params.efb_format == PixelFormat::RGB565_Z16)
    code.Write("  tex_sample = RGBA8ToRGB565(tex_sample);\n");

  if (params.depth)
  {
    if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      code.Write("  tex_sample.x = 1.0 - tex_sample.x;\n");

    code.Write("  uint depth = uint(tex_sample.x * 16777216.0);\n"
               "  return uint4((depth >> 16) & 255u, (depth >> 8) & 255u, depth & 255u, 255u);\n"
               "}}\n");
  }
  else
  {
    code.Write("  return uint4(tex_sample * 255.0);\n"
               "}}\n");
  }

  // The copy filter applies to both color and depth copies. This has been verified on hardware.
  // The filter is only applied to the RGB channels, the alpha channel is left intact.
  code.Write("float4 SampleEFB(float2 uv, float2 pixel_size, int x_offset)\n"
             "{{\n");
  if (params.all_copy_filter_coefs_needed)
  {
    code.Write("  uint4 prev_row = SampleEFB0(uv, pixel_size, float(x_offset), -1.0f);\n"
               "  uint4 current_row = SampleEFB0(uv, pixel_size, float(x_offset), 0.0f);\n"
               "  uint4 next_row = SampleEFB0(uv, pixel_size, float(x_offset), 1.0f);\n"
               "  uint3 combined_rows = prev_row.rgb * filter_coefficients[0] +\n"
               "                        current_row.rgb * filter_coefficients[1] +\n"
               "                        next_row.rgb * filter_coefficients[2];\n");
  }
  else
  {
    code.Write("  uint4 current_row = SampleEFB0(uv, pixel_size, float(x_offset), 0.0f);\n"
               "  uint3 combined_rows = current_row.rgb * filter_coefficients[1];\n");
  }
  code.Write("  // Shift right by 6 to divide by 64, as filter coefficients\n"
             "  // that sum to 64 result in no change in brightness\n"
             "  uint4 texcol_raw = uint4(combined_rows.rgb >> 6, current_row.a);\n");

  if (params.copy_filter_can_overflow)
    code.Write("  texcol_raw &= 0x1ffu;\n");
  // Note that overflow occurs when the sum of values is >= 128, but this max situation can be hit
  // on >= 64, so we always include it.
  code.Write("  texcol_raw = min(texcol_raw, uint4(255, 255, 255, 255));\n");

  if (params.apply_gamma)
  {
    code.Write("  texcol_raw = uint4(round(pow(float4(texcol_raw) / 255.0,\n"
               "                     float4(gamma_rcp, gamma_rcp, gamma_rcp, 1.0)) * 255.0));\n");
  }

  if (params.yuv)
  {
    code.Write("  // Intensity/YUV format conversion constants determined by hardware testing\n"
               "  const float4 y_const = float4( 66, 129,  25,  16);\n"
               "  const float4 u_const = float4(-38, -74, 112, 128);\n"
               "  const float4 v_const = float4(112, -94, -18, 128);\n"
               "  // Intensity/YUV format conversion\n"
               "  texcol_raw.rgb = uint3(dot(y_const, float4(texcol_raw.rgb, 256)),\n"
               "                         dot(u_const, float4(texcol_raw.rgb, 256)),\n"
               "                         dot(v_const, float4(texcol_raw.rgb, 256)));\n"
               "  // Divide by 256 and round .5 and higher up\n"
               "  texcol_raw.rgb = (texcol_raw.rgb >> 8) + ((texcol_raw.rgb >> 7) & 1u);\n");
  }

  code.Write("  return float4(texcol_raw) / 255.0;\n");
  code.Write("}}\n");
}

// Block dimensions   : widthStride, heightStride
// Texture dimensions : width, height, x offset, y offset
static void WriteSwizzler(ShaderCode& code, const EFBCopyParams& params, APIType api_type)
{
  code.Write("void main()\n"
             "{{\n"
             "  int2 sampleUv;\n"
             "  int2 uv1 = int2(gl_FragCoord.xy);\n");

  const int blkW = TexDecoder_GetEFBCopyBlockWidthInTexels(params.copy_format);
  const int blkH = TexDecoder_GetEFBCopyBlockHeightInTexels(params.copy_format);
  int samples = GetEncodedSampleCount(params.copy_format);

  code.Write("  int x_block_position = (uv1.x >> {}) << {};\n",
             MathUtil::IntLog2(blkH * blkW / samples), MathUtil::IntLog2(blkW));
  code.Write("  int y_block_position = uv1.y << {};\n", MathUtil::IntLog2(blkH));
  if (samples == 1)
  {
    // With samples == 1, we write out pairs of blocks; one A8R8, one G8B8.
    code.Write("  bool first = (uv1.x & {}) == 0;\n", blkH * blkW / 2);
    samples = 2;
  }
  code.Write("  int offset_in_block = uv1.x & {};\n", (blkH * blkW / samples) - 1);
  code.Write("  int y_offset_in_block = offset_in_block >> {};\n",
             MathUtil::IntLog2(blkW / samples));
  code.Write("  int x_offset_in_block = (offset_in_block & {}) << {};\n", (blkW / samples) - 1,
             MathUtil::IntLog2(samples));

  code.Write("  sampleUv.x = x_block_position + x_offset_in_block;\n"
             "  sampleUv.y = y_block_position + y_offset_in_block;\n");

  // sampleUv is the sample position in (int)gx_coords
  code.Write("  float2 uv0 = float2(sampleUv);\n");
  // Move to center of pixel
  code.Write("  uv0 += float2(0.5, 0.5);\n");
  // Scale by two if needed (also move to pixel borders
  // so that linear filtering will average adjacent
  // pixel)
  code.Write("  uv0 *= float(position.w);\n");

  // Move to copied rect
  code.Write("  uv0 += float2(position.xy);\n");
  // Normalize to [0:1]
  code.Write("  uv0 /= float2({}, {});\n", EFB_WIDTH, EFB_HEIGHT);
  // Apply the y scaling
  code.Write("  uv0 /= float2(1, y_scale);\n");
  // OGL has to flip up and down
  if (api_type == APIType::OpenGL)
  {
    code.Write("  uv0.y = 1.0-uv0.y;\n");
  }

  code.Write("  float2 pixel_size = float2(position.w, position.w) / float2({}, {});\n", EFB_WIDTH,
             EFB_HEIGHT);
}

static void WriteSampleColor(ShaderCode& code, std::string_view color_comp, std::string_view dest,
                             int x_offset, APIType api_type, const EFBCopyParams& params)
{
  code.Write("  {} = SampleEFB(uv0, pixel_size, {}).{};\n", dest, x_offset, color_comp);
}

static void WriteToBitDepth(ShaderCode& code, u8 depth, std::string_view src, std::string_view dest)
{
  code.Write("  {} = floor({} * 255.0 / exp2(8.0 - {}.0));\n", dest, src, depth);
}

static void WriteRGB565Encoder(ShaderCode& code, APIType api_type, const EFBCopyParams& params)
{
  code.Write("  float3 texSample0;\n"
             "  float3 texSample1;\n");

  WriteSampleColor(code, "rgb", "texSample0", 0, api_type, params);
  WriteSampleColor(code, "rgb", "texSample1", 1, api_type, params);
  code.Write("  float2 texRs = float2(texSample0.r, texSample1.r);\n"
             "  float2 texGs = float2(texSample0.g, texSample1.g);\n"
             "  float2 texBs = float2(texSample0.b, texSample1.b);\n");

  WriteToBitDepth(code, 6, "texGs", "float2 gInt");
  code.Write("  float2 gUpper = floor(gInt / 8.0);\n"
             "  float2 gLower = gInt - gUpper * 8.0;\n");

  WriteToBitDepth(code, 5, "texRs", "ocol0.br");
  code.Write("  ocol0.br = ocol0.br * 8.0 + gUpper;\n");
  WriteToBitDepth(code, 5, "texBs", "ocol0.ga");
  code.Write("  ocol0.ga = ocol0.ga + gLower * 32.0;\n");

  code.Write("  ocol0 = ocol0 / 255.0;\n");
}

static void WriteRGB5A3Encoder(ShaderCode& code, APIType api_type, const EFBCopyParams& params)
{
  code.Write("  float4 texSample;\n"
             "  float color0;\n"
             "  float gUpper;\n"
             "  float gLower;\n");

  WriteSampleColor(code, "rgba", "texSample", 0, api_type, params);

  // 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
  code.Write("if(texSample.a > 0.878f) {{\n");

  WriteToBitDepth(code, 5, "texSample.g", "color0");
  code.Write("  gUpper = floor(color0 / 8.0);\n"
             "  gLower = color0 - gUpper * 8.0;\n");

  WriteToBitDepth(code, 5, "texSample.r", "ocol0.b");
  code.Write("  ocol0.b = ocol0.b * 4.0 + gUpper + 128.0;\n");
  WriteToBitDepth(code, 5, "texSample.b", "ocol0.g");
  code.Write("  ocol0.g = ocol0.g + gLower * 32.0;\n");

  code.Write("}} else {{\n");

  WriteToBitDepth(code, 4, "texSample.r", "ocol0.b");
  WriteToBitDepth(code, 4, "texSample.b", "ocol0.g");

  WriteToBitDepth(code, 3, "texSample.a", "color0");
  code.Write("ocol0.b = ocol0.b + color0 * 16.0;\n");
  WriteToBitDepth(code, 4, "texSample.g", "color0");
  code.Write("ocol0.g = ocol0.g + color0 * 16.0;\n");

  code.Write("}}\n");

  WriteSampleColor(code, "rgba", "texSample", 1, api_type, params);

  code.Write("if(texSample.a > 0.878f) {{\n");

  WriteToBitDepth(code, 5, "texSample.g", "color0");
  code.Write("  gUpper = floor(color0 / 8.0);\n"
             "  gLower = color0 - gUpper * 8.0;\n");

  WriteToBitDepth(code, 5, "texSample.r", "ocol0.r");
  code.Write("  ocol0.r = ocol0.r * 4.0 + gUpper + 128.0;\n");
  WriteToBitDepth(code, 5, "texSample.b", "ocol0.a");
  code.Write("  ocol0.a = ocol0.a + gLower * 32.0;\n");

  code.Write("}} else {{\n");

  WriteToBitDepth(code, 4, "texSample.r", "ocol0.r");
  WriteToBitDepth(code, 4, "texSample.b", "ocol0.a");

  WriteToBitDepth(code, 3, "texSample.a", "color0");
  code.Write("ocol0.r = ocol0.r + color0 * 16.0;\n");
  WriteToBitDepth(code, 4, "texSample.g", "color0");
  code.Write("ocol0.a = ocol0.a + color0 * 16.0;\n");

  code.Write("}}\n");

  code.Write("  ocol0 = ocol0 / 255.0;\n");
}

static void WriteRGBA8Encoder(ShaderCode& code, APIType api_type, const EFBCopyParams& params)
{
  code.Write("  float4 texSample;\n"
             "  float4 color0;\n"
             "  float4 color1;\n");

  WriteSampleColor(code, "rgba", "texSample", 0, api_type, params);
  code.Write("  color0.b = texSample.a;\n"
             "  color0.g = texSample.r;\n"
             "  color1.b = texSample.g;\n"
             "  color1.g = texSample.b;\n");

  WriteSampleColor(code, "rgba", "texSample", 1, api_type, params);
  code.Write("  color0.r = texSample.a;\n"
             "  color0.a = texSample.r;\n"
             "  color1.r = texSample.g;\n"
             "  color1.a = texSample.b;\n");

  code.Write("  ocol0 = first ? color0 : color1;\n");
}

static void WriteC4Encoder(ShaderCode& code, std::string_view comp, APIType api_type,
                           const EFBCopyParams& params)
{
  code.Write("  float4 color0;\n"
             "  float4 color1;\n");

  WriteSampleColor(code, comp, "color0.b", 0, api_type, params);
  WriteSampleColor(code, comp, "color1.b", 1, api_type, params);
  WriteSampleColor(code, comp, "color0.g", 2, api_type, params);
  WriteSampleColor(code, comp, "color1.g", 3, api_type, params);
  WriteSampleColor(code, comp, "color0.r", 4, api_type, params);
  WriteSampleColor(code, comp, "color1.r", 5, api_type, params);
  WriteSampleColor(code, comp, "color0.a", 6, api_type, params);
  WriteSampleColor(code, comp, "color1.a", 7, api_type, params);

  WriteToBitDepth(code, 4, "color0", "color0");
  WriteToBitDepth(code, 4, "color1", "color1");

  code.Write("  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
}

static void WriteC8Encoder(ShaderCode& code, std::string_view comp, APIType api_type,
                           const EFBCopyParams& params)
{
  WriteSampleColor(code, comp, "ocol0.b", 0, api_type, params);
  WriteSampleColor(code, comp, "ocol0.g", 1, api_type, params);
  WriteSampleColor(code, comp, "ocol0.r", 2, api_type, params);
  WriteSampleColor(code, comp, "ocol0.a", 3, api_type, params);
}

static void WriteCC4Encoder(ShaderCode& code, std::string_view comp, APIType api_type,
                            const EFBCopyParams& params)
{
  code.Write("  float2 texSample;\n"
             "  float4 color0;\n"
             "  float4 color1;\n");

  WriteSampleColor(code, comp, "texSample", 0, api_type, params);
  code.Write("  color0.b = texSample.x;\n"
             "  color1.b = texSample.y;\n");

  WriteSampleColor(code, comp, "texSample", 1, api_type, params);
  code.Write("  color0.g = texSample.x;\n"
             "  color1.g = texSample.y;\n");

  WriteSampleColor(code, comp, "texSample", 2, api_type, params);
  code.Write("  color0.r = texSample.x;\n"
             "  color1.r = texSample.y;\n");

  WriteSampleColor(code, comp, "texSample", 3, api_type, params);
  code.Write("  color0.a = texSample.x;\n"
             "  color1.a = texSample.y;\n");

  WriteToBitDepth(code, 4, "color0", "color0");
  WriteToBitDepth(code, 4, "color1", "color1");

  code.Write("  ocol0 = (color0 * 16.0 + color1) / 255.0;\n");
}

static void WriteCC8Encoder(ShaderCode& code, std::string_view comp, APIType api_type,
                            const EFBCopyParams& params)
{
  WriteSampleColor(code, comp, "ocol0.bg", 0, api_type, params);
  WriteSampleColor(code, comp, "ocol0.ra", 1, api_type, params);
}

static void WriteXFBEncoder(ShaderCode& code, APIType api_type, const EFBCopyParams& params)
{
  code.Write("float4 color0 = float4(0, 0, 0, 1), color1 = float4(0, 0, 0, 1);\n");
  WriteSampleColor(code, "rgb", "color0.rgb", 0, api_type, params);
  WriteSampleColor(code, "rgb", "color1.rgb", 1, api_type, params);

  // Convert to YUV.
  code.Write("  // Intensity/YUV format conversion constants determined by hardware testing\n"
             "  const float4 y_const = float4( 66, 129,  25,  16);\n"
             "  const float4 u_const = float4(-38, -74, 112, 128);\n"
             "  const float4 v_const = float4(112, -94, -18, 128);\n"
             "  float4 average = (color0 + color1) * 0.5;\n"
             "  // TODO: check rounding\n"
             "  ocol0.b = round(dot(color0,  y_const)) / 256.0;\n"
             "  ocol0.g = round(dot(average, u_const)) / 256.0;\n"
             "  ocol0.r = round(dot(color1,  y_const)) / 256.0;\n"
             "  ocol0.a = round(dot(average, v_const)) / 256.0;\n");
}

std::string GenerateEncodingShader(const EFBCopyParams& params, APIType api_type)
{
  ShaderCode code;

  WriteHeader(code, api_type);
  WriteSampleFunction(code, params, api_type);
  WriteSwizzler(code, params, api_type);

  switch (params.copy_format)
  {
  case EFBCopyFormat::R4:
    WriteC4Encoder(code, "r", api_type, params);
    break;
  case EFBCopyFormat::RA4:
    WriteCC4Encoder(code, "ar", api_type, params);
    break;
  case EFBCopyFormat::RA8:
    WriteCC8Encoder(code, "ar", api_type, params);
    break;
  case EFBCopyFormat::RGB565:
    WriteRGB565Encoder(code, api_type, params);
    break;
  case EFBCopyFormat::RGB5A3:
    WriteRGB5A3Encoder(code, api_type, params);
    break;
  case EFBCopyFormat::RGBA8:
    WriteRGBA8Encoder(code, api_type, params);
    break;
  case EFBCopyFormat::A8:
    WriteC8Encoder(code, "a", api_type, params);
    break;
  case EFBCopyFormat::R8_0x1:
  case EFBCopyFormat::R8:
    WriteC8Encoder(code, "r", api_type, params);
    break;
  case EFBCopyFormat::G8:
    WriteC8Encoder(code, "g", api_type, params);
    break;
  case EFBCopyFormat::B8:
    WriteC8Encoder(code, "b", api_type, params);
    break;
  case EFBCopyFormat::RG8:
    WriteCC8Encoder(code, "gr", api_type, params);
    break;
  case EFBCopyFormat::GB8:
    WriteCC8Encoder(code, "bg", api_type, params);
    break;
  case EFBCopyFormat::XFB:
    WriteXFBEncoder(code, api_type, params);
    break;
  default:
    PanicAlertFmt("Invalid EFB Copy Format {}! (GenerateEncodingShader)", params.copy_format);
    break;
  }

  code.Write("}}\n");

  return code.GetBuffer();
}

// NOTE: In these uniforms, a row refers to a row of blocks, not texels.
static constexpr char decoding_shader_header[] = R"(
#if defined(PALETTE_FORMAT_IA8) || defined(PALETTE_FORMAT_RGB565) || defined(PALETTE_FORMAT_RGB5A3)
#define HAS_PALETTE 1
#endif

UBO_BINDING(std140, 1) uniform UBO {
  uint2 u_dst_size;
  uint2 u_src_size;
  uint u_src_offset;
  uint u_src_row_stride;
  uint u_palette_offset;
};

#if defined(API_METAL)

#if defined(TEXEL_BUFFER_FORMAT_R8)
  SSBO_BINDING(0) readonly buffer Input { uint8_t s_input_buffer[]; };
  #define FETCH(offset) uint(s_input_buffer[offset])
#elif defined(TEXEL_BUFFER_FORMAT_R16)
  SSBO_BINDING(0) readonly buffer Input { uint16_t s_input_buffer[]; };
  #define FETCH(offset) uint(s_input_buffer[offset])
#elif defined(TEXEL_BUFFER_FORMAT_RGBA8)
  SSBO_BINDING(0) readonly buffer Input { u8vec4 s_input_buffer[]; };
  #define FETCH(offset) uvec4(s_input_buffer[offset])
#elif defined(TEXEL_BUFFER_FORMAT_R32G32)
  SSBO_BINDING(0) readonly buffer Input { uvec2 s_input_buffer[]; };
  #define FETCH(offset) s_input_buffer[offset]
#else
  #error No texel buffer?
#endif

#ifdef HAS_PALETTE
  SSBO_BINDING(1) readonly buffer Palette { uint16_t s_palette_buffer[]; };
  #define FETCH_PALETTE(offset) uint(s_palette_buffer[offset])
#endif

#else

TEXEL_BUFFER_BINDING(0) uniform usamplerBuffer s_input_buffer;

#if defined(TEXEL_BUFFER_FORMAT_R8) || defined(TEXEL_BUFFER_FORMAT_R16)
  #define FETCH(offset) texelFetch(s_input_buffer, int((offset) + u_src_offset)).r
#elif defined(TEXEL_BUFFER_FORMAT_RGBA8)
  #define FETCH(offset) texelFetch(s_input_buffer, int((offset) + u_src_offset))
#elif defined(TEXEL_BUFFER_FORMAT_R32G32)
  #define FETCH(offset) texelFetch(s_input_buffer, int((offset) + u_src_offset)).rg
#else
  #error No texel buffer?
#endif

#ifdef HAS_PALETTE
  TEXEL_BUFFER_BINDING(1) uniform usamplerBuffer s_palette_buffer;
  #define FETCH_PALETTE(offset) texelFetch(s_palette_buffer, int((offset) + u_palette_offset)).r
#endif

#endif // defined(API_METAL)
IMAGE_BINDING(rgba8, 0) uniform writeonly image2DArray output_image;

#define GROUP_MEMORY_BARRIER_WITH_SYNC memoryBarrierShared(); barrier();
#define GROUP_SHARED shared

#define DEFINE_MAIN(lx, ly) \
  layout(local_size_x = lx, local_size_y = ly) in; \
  void main()

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
  uint buffer_pos = 0u;
  buffer_pos += block.y * u_src_row_stride;
  buffer_pos += block.x * (block_size.x * block_size.y);
  buffer_pos += offset.y * block_size.x;
  buffer_pos += offset.x;
  return buffer_pos;
}

#if defined(HAS_PALETTE)
uint4 GetPaletteColor(uint index)
{
  // Fetch and swap BE to LE.
  uint val = Swap16(FETCH_PALETTE(index));

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
#endif // defined(HAS_PALETTE)

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
        uint buffer_pos = 0u;
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x / 2u;

        // Select high nibble for odd texels, low for even.
        uint val = FETCH(buffer_pos);
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
        uint val = FETCH(buffer_pos);
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
        uint i = FETCH(buffer_pos);
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
        uint val = FETCH(buffer_pos);
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
        uint val = Swap16(FETCH(buffer_pos));

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
        uint val = Swap16(FETCH(buffer_pos));

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
        uint buffer_pos = 0u;

        // Our buffer has 16-bit elements, so the offsets here are half what they would be in bytes.
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x;

        // The two GB channels follow after the block's AR channels.
        uint val1 = FETCH(buffer_pos +  0u);
        uint val2 = FETCH(buffer_pos + 16u);

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

      DEFINE_MAIN(GROUP_SIZE, 1)
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
          uint buffer_pos = 0u;
          buffer_pos += tile_block_coords.y * u_src_row_stride;
          buffer_pos += tile_block_coords.x * 4u;
          buffer_pos += subtile_block_coords.y * 2u;
          buffer_pos += subtile_block_coords.x;

          // Read the entire DXT block to shared memory.
          uint2 raw_data = FETCH(buffer_pos);
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
        uint buffer_pos = 0u;
        buffer_pos += block.y * u_src_row_stride;
        buffer_pos += block.x * 32u;
        buffer_pos += offset.y * 4u;
        buffer_pos += offset.x / 2u;

        // Select high nibble for odd texels, low for even.
        uint val = FETCH(buffer_pos);
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
        uint index = FETCH(buffer_pos);
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
        uint index = Swap16(FETCH(buffer_pos)) & 0x3FFFu;
        float4 norm_color = GetPaletteColorNormalized(index);
        imageStore(output_image, int3(int2(coords), 0), norm_color);
      }
      )"}},

    // We do the inverse BT.601 conversion for YCbCr to RGB
    // http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
    // TODO: Use more precise numbers for this conversion (although on real hardware, the XFB isn't
    // in a real texture format, so does this conversion actually ever happen?)
    {TextureFormat::XFB,
     {TEXEL_BUFFER_FORMAT_RGBA8_UINT, 0, 8, 8, false,
      R"(
      DEFINE_MAIN(8, 8)
      {
        uint2 uv = gl_GlobalInvocationID.xy;
        uint buffer_pos = (uv.y * u_src_row_stride) + (uv.x / 2u);
        float4 yuyv = float4(FETCH(buffer_pos));

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

std::string GenerateDecodingShader(TextureFormat format, std::optional<TLUTFormat> palette_format,
                                   APIType api_type)
{
  const DecodingShaderInfo* info = GetDecodingShaderInfo(format);
  if (!info)
    return "";

  std::ostringstream ss;
  if (palette_format.has_value())
  {
    switch (*palette_format)
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
  }

  switch (info->buffer_format)
  {
  case TEXEL_BUFFER_FORMAT_R8_UINT:
    ss << "#define TEXEL_BUFFER_FORMAT_R8 1\n";
    break;
  case TEXEL_BUFFER_FORMAT_R16_UINT:
    ss << "#define TEXEL_BUFFER_FORMAT_R16 1\n";
    break;
  case TEXEL_BUFFER_FORMAT_RGBA8_UINT:
    ss << "#define TEXEL_BUFFER_FORMAT_RGBA8 1\n";
    break;
  case TEXEL_BUFFER_FORMAT_R32G32_UINT:
    ss << "#define TEXEL_BUFFER_FORMAT_R32G32 1\n";
    break;
  case NUM_TEXEL_BUFFER_FORMATS:
    ASSERT(false);
    break;
  }

  ss << decoding_shader_header;
  ss << info->shader_body;

  return ss.str();
}

std::string GeneratePaletteConversionShader(TLUTFormat palette_format, APIType api_type)
{
  std::ostringstream ss;

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
    PanicAlertFmt("Unknown format");
    break;
  }

  ss << "\n";

  if (api_type == APIType::Metal)
    ss << "SSBO_BINDING(0) readonly buffer Palette { uint16_t palette[]; };\n";
  else
    ss << "TEXEL_BUFFER_BINDING(0) uniform usamplerBuffer samp0;\n";
  ss << "SAMPLER_BINDING(1) uniform sampler2DArray samp1;\n";
  ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";

  ss << "  float multiplier;\n";
  ss << "  int texel_buffer_offset;\n";
  ss << "};\n";

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
  if (api_type == APIType::Metal)
    ss << "  src = int(palette[uint(src)]);\n";
  else
    ss << "  src = int(texelFetch(samp0, src + texel_buffer_offset).r);\n";

  ss << "  src = ((src << 8) | (src >> 8)) & 0xFFFF;\n";
  ss << "  ocol0 = DecodePixel(src);\n";
  ss << "}\n";

  return ss.str();
}

}  // namespace TextureConversionShaderTiled
