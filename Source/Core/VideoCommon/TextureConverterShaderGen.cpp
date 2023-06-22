// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/TextureConverterShaderGen.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace TextureConversionShaderGen
{
TCShaderUid GetShaderUid(EFBCopyFormat dst_format, bool is_depth_copy, bool is_intensity,
                         bool scale_by_half, float gamma_rcp,
                         const std::array<u32, 3>& filter_coefficients)
{
  TCShaderUid out;

  UidData* const uid_data = out.GetUidData();
  uid_data->dst_format = dst_format;
  uid_data->efb_has_alpha = bpmem.zcontrol.pixel_format == PixelFormat::RGBA6_Z24;
  uid_data->is_depth_copy = is_depth_copy;
  uid_data->is_intensity = is_intensity;
  uid_data->scale_by_half = scale_by_half;
  uid_data->all_copy_filter_coefs_needed =
      TextureCacheBase::AllCopyFilterCoefsNeeded(filter_coefficients);
  uid_data->copy_filter_can_overflow = TextureCacheBase::CopyFilterCanOverflow(filter_coefficients);
  // If the gamma is needed, then include that too.
  uid_data->apply_gamma = gamma_rcp != 1.0f;

  return out;
}

static void WriteHeader(APIType api_type, ShaderCode& out)
{
  out.Write("UBO_BINDING(std140, 1) uniform PSBlock {{\n"
            "  float2 src_offset, src_size;\n"
            "  uint3 filter_coefficients;\n"
            "  float gamma_rcp;\n"
            "  float2 clamp_tb;\n"
            "  float pixel_height;\n"
            "}};\n");
}

ShaderCode GenerateVertexShader(APIType api_type)
{
  ShaderCode out;
  WriteHeader(api_type, out);

  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    out.Write("VARYING_LOCATION(0) out VertexData {{\n"
              "  float3 v_tex0;\n"
              "}};\n");
  }
  else
  {
    out.Write("VARYING_LOCATION(0) out float3 v_tex0;\n");
  }
  out.Write("#define id gl_VertexID\n"
            "#define opos gl_Position\n"
            "void main() {{\n");
  out.Write("  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n");
  out.Write(
      "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n");
  out.Write("  v_tex0 = float3(src_offset + (src_size * v_tex0.xy), 0.0f);\n");

  // NDC space is flipped in Vulkan
  if (api_type == APIType::Vulkan)
    out.Write("  opos.y = -opos.y;\n");

  out.Write("}}\n");

  return out;
}

ShaderCode GeneratePixelShader(APIType api_type, const UidData* uid_data)
{
  const bool mono_depth = uid_data->is_depth_copy && g_ActiveConfig.bStereoEFBMonoDepth;

  ShaderCode out;
  WriteHeader(api_type, out);

  out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n");
  out.Write("uint4 SampleEFB(float3 uv, float y_offset) {{\n"
            "  float4 tex_sample = texture(samp0, float3(uv.x, clamp(uv.y + (y_offset * "
            "pixel_height), clamp_tb.x, clamp_tb.y), {}));\n",
            mono_depth ? "0.0" : "uv.z");
  if (uid_data->is_depth_copy)
  {
    if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      out.Write("  tex_sample.x = 1.0 - tex_sample.x;\n");

    out.Write("  uint depth = uint(tex_sample.x * 16777216.0);\n"
              "  return uint4((depth >> 16) & 255u, (depth >> 8) & 255u, depth & 255u, 255u);\n"
              "}}\n");
  }
  else
  {
    out.Write("  return uint4(tex_sample * 255.0);\n"
              "}}\n");
  }

  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    out.Write("VARYING_LOCATION(0) in VertexData {{\n"
              "  float3 v_tex0;\n"
              "}};\n");
  }
  else
  {
    out.Write("VARYING_LOCATION(0) in vec3 v_tex0;\n");
  }

  out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n"
            "void main()\n{{\n");

  // The copy filter applies to both color and depth copies. This has been verified on hardware.
  // The filter is only applied to the RGB channels, the alpha channel is left intact.
  if (uid_data->all_copy_filter_coefs_needed)
  {
    out.Write("  uint4 prev_row = SampleEFB(v_tex0, -1.0f);\n"
              "  uint4 current_row = SampleEFB(v_tex0, 0.0f);\n"
              "  uint4 next_row = SampleEFB(v_tex0, 1.0f);\n"
              "  uint3 combined_rows = prev_row.rgb * filter_coefficients[0] +\n"
              "                        current_row.rgb * filter_coefficients[1] +\n"
              "                        next_row.rgb * filter_coefficients[2];\n");
  }
  else
  {
    out.Write("  uint4 current_row = SampleEFB(v_tex0, 0.0f);\n"
              "  uint3 combined_rows = current_row.rgb * filter_coefficients[1];\n");
  }
  out.Write("  // Shift right by 6 to divide by 64, as filter coefficients\n"
            "  // that sum to 64 result in no change in brightness\n"
            "  uint4 texcol_raw = uint4(combined_rows.rgb >> 6, {});\n",
            uid_data->efb_has_alpha ? "current_row.a" : "255");

  if (uid_data->copy_filter_can_overflow)
    out.Write("  texcol_raw &= 0x1ffu;\n");
  // Note that overflow occurs when the sum of values is >= 128, but this max situation can be hit
  // on >= 64, so we always include it.
  out.Write("  texcol_raw = min(texcol_raw, uint4(255, 255, 255, 255));\n");

  if (uid_data->apply_gamma)
  {
    out.Write("  texcol_raw = uint4(round(pow(abs(float4(texcol_raw) / 255.0),\n"
              "                     float4(gamma_rcp, gamma_rcp, gamma_rcp, 1.0)) * 255.0));\n");
  }

  if (uid_data->is_intensity)
  {
    out.Write("  // Intensity/YUV format conversion constants determined by hardware testing\n"
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

  switch (uid_data->dst_format)
  {
  case EFBCopyFormat::R4:  // R4
    out.Write("  float red = float(texcol_raw.r & 0xF0u) / 240.0;\n"
              "  ocol0 = float4(red, red, red, red);\n");
    break;

  case EFBCopyFormat::R8_0x1:  // R8
  case EFBCopyFormat::R8:      // R8
    out.Write("  ocol0 = float4(texcol_raw).rrrr / 255.0;\n");
    break;

  case EFBCopyFormat::RA4:  // RA4
    out.Write("  float2 red_alpha = float2(texcol_raw.ra & 0xF0u) / 240.0;\n"
              "  ocol0 = red_alpha.rrrg;\n");
    break;

  case EFBCopyFormat::RA8:  // RA8
    out.Write("  ocol0 = float4(texcol_raw).rrra / 255.0;\n");
    break;

  case EFBCopyFormat::A8:  // A8
    out.Write("  ocol0 = float4(texcol_raw).aaaa / 255.0;\n");
    break;

  case EFBCopyFormat::G8:  // G8
    out.Write("  ocol0 = float4(texcol_raw).gggg / 255.0;\n");
    break;

  case EFBCopyFormat::B8:  // B8
    out.Write("  ocol0 = float4(texcol_raw).bbbb / 255.0;\n");
    break;

  case EFBCopyFormat::RG8:  // RG8
    out.Write("  ocol0 = float4(texcol_raw).rrrg / 255.0;\n");
    break;

  case EFBCopyFormat::GB8:  // GB8
    out.Write("  ocol0 = float4(texcol_raw).gggb / 255.0;\n");
    break;

  case EFBCopyFormat::RGB565:  // RGB565
    out.Write("  float2 red_blue = float2(texcol_raw.rb & 0xF8u) / 248.0;\n"
              "  float green = float(texcol_raw.g & 0xFCu) / 252.0;\n"
              "  ocol0 = float4(red_blue.r, green, red_blue.g, 1.0);\n");
    break;

  case EFBCopyFormat::RGB5A3:  // RGB5A3
    // TODO: The MSB controls whether we have RGB5 or RGB4A3, this selection
    // will need to be implemented once we move away from floats.
    out.Write("  float3 color = float3(texcol_raw.rgb & 0xF8u) / 248.0;\n"
              "  float alpha = float(texcol_raw.a & 0xE0u) / 224.0;\n"
              "  ocol0 = float4(color, alpha);\n");
    break;

  case EFBCopyFormat::RGBA8:  // RGBA8
    out.Write("  ocol0 = float4(texcol_raw.rgba) / 255.0;\n");
    break;

  case EFBCopyFormat::XFB:
    out.Write("  ocol0 = float4(float3(texcol_raw.rgb) / 255.0, 1.0);\n");
    break;

  default:
    ERROR_LOG_FMT(VIDEO, "Unknown copy/intensity color format: {} {}", uid_data->dst_format,
                  uid_data->is_intensity);
    out.Write("  ocol0 = float4(texcol_raw.rgba) / 255.0;\n");
    break;
  }

  out.Write("}}\n");

  return out;
}

}  // namespace TextureConversionShaderGen
