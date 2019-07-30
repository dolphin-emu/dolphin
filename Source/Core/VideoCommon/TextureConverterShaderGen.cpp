// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/TextureConverterShaderGen.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace TextureConversionShaderGen
{
TCShaderUid GetShaderUid(EFBCopyFormat dst_format, bool is_depth_copy, bool is_intensity,
                         bool scale_by_half, bool copy_filter)
{
  TCShaderUid out;

  UidData* const uid_data = out.GetUidData();
  uid_data->dst_format = dst_format;
  uid_data->efb_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  uid_data->is_depth_copy = is_depth_copy;
  uid_data->is_intensity = is_intensity;
  uid_data->scale_by_half = scale_by_half;
  uid_data->copy_filter = copy_filter;

  return out;
}

static void WriteHeader(APIType api_type, ShaderCode& out)
{
  if (api_type == APIType::D3D)
  {
    out.Write("cbuffer PSBlock : register(b0) {\n"
              "  float2 src_offset, src_size;\n"
              "  float3 filter_coefficients;\n"
              "  float gamma_rcp;\n"
              "  float2 clamp_tb;\n"
              "  float pixel_height;\n"
              "};\n\n");
  }
  else if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
  {
    out.Write("UBO_BINDING(std140, 1) uniform PSBlock {\n"
              "  float2 src_offset, src_size;\n"
              "  float3 filter_coefficients;\n"
              "  float gamma_rcp;\n"
              "  float2 clamp_tb;\n"
              "  float pixel_height;\n"
              "};\n");
  }
}

ShaderCode GenerateVertexShader(APIType api_type)
{
  ShaderCode out;
  WriteHeader(api_type, out);

  if (api_type == APIType::D3D)
  {
    out.Write("void main(in uint id : SV_VertexID, out float3 v_tex0 : TEXCOORD0,\n"
              "          out float4 opos : SV_Position) {\n");
  }
  else if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
  {
    out.Write("VARYING_LOCATION(0) out float3 v_tex0;\n");
    out.Write("#define id gl_VertexID\n"
              "#define opos gl_Position\n"
              "void main() {\n");
  }
  out.Write("  float2 rawpos = float2(float((id << 1) & 2), float(id & 2));\n");
  if (api_type == APIType::Vulkan)
  {
    // NDC space is flipped in Vulkan
    out.Write("  opos = float4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);\n");
  }
  else
  {
    out.Write(
      "  opos = float4(rawpos * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n");
  }
  out.Write("  v_tex0 = float3(src_offset + (src_size * rawpos), 0.0f);\n");
  out.Write("}\n");
  return out;
}

ShaderCode GeneratePixelShader(APIType api_type, const UidData* uid_data)
{
  const bool mono_depth = uid_data->is_depth_copy && false;

  ShaderCode out;
  WriteHeader(api_type, out);

  if (api_type == APIType::D3D)
  {
    out.Write("Texture2DArray tex0 : register(t0);\n"
              "SamplerState samp0 : register(s0);\n"
              "float4 SampleEFB(float3 uv, float y_offset) {\n"
              "  return tex0.Sample(samp0, float3(uv.x, clamp(uv.y + (y_offset * pixel_height), "
              "clamp_tb.x, clamp_tb.y), %s));\n"
              "}\n\n",
              mono_depth ? "0.0" : "uv.z");
    out.Write("void main(in float3 v_tex0 : TEXCOORD0, out float4 ocol0 : SV_Target)\n{\n");
  }
  else if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
  {
    out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n");
    out.Write("float4 SampleEFB(float3 uv, float y_offset) {\n"
              "  return texture(samp0, float3(uv.x, clamp(uv.y + (y_offset * pixel_height), "
              "clamp_tb.x, clamp_tb.y), %s));\n"
              "}\n",
              mono_depth ? "0.0" : "uv.z");
    out.Write("VARYING_LOCATION(0) in float3 v_tex0;\n");
    out.Write("FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;"
              "void main()\n{\n");
  }

  // The copy filter applies to both color and depth copies. This has been verified on hardware.
  // The filter is only applied to the RGB channels, the alpha channel is left intact.
  if (uid_data->copy_filter)
  {
    out.Write("  float4 prev_row = SampleEFB(v_tex0, -1.0f);\n"
              "  float4 current_row = SampleEFB(v_tex0, 0.0f);\n"
              "  float4 next_row = SampleEFB(v_tex0, 1.0f);\n"
              "  float4 texcol = float4(min(prev_row.rgb * filter_coefficients[0] +\n"
              "                               current_row.rgb * filter_coefficients[1] +\n"
              "                               next_row.rgb * filter_coefficients[2], \n"
              "                             float3(1, 1, 1)), current_row.a);\n");
  }
  else
  {
    out.Write(
        "  float4 current_row = SampleEFB(v_tex0, 0.0f);\n"
        "  float4 texcol = float4(min(current_row.rgb * filter_coefficients[1], float3(1, 1, 1)),\n"
        "                         current_row.a);\n");
  }

  if (uid_data->is_depth_copy)
  {
    if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      out.Write("texcol.x = 1.0 - texcol.x;\n");

    out.Write("  int depth = int(texcol.x * 16777216.0);\n"

              // Convert to Z24 format
              "  int4 workspace;\n"
              "  workspace.r = (depth >> 16) & 255;\n"
              "  workspace.g = (depth >> 8) & 255;\n"
              "  workspace.b = depth & 255;\n"

              // Convert to Z4 format
              "  workspace.a = (depth >> 16) & 0xF0;\n"

              // Normalize components to [0.0..1.0]
              "  texcol = float4(workspace) / 255.0;\n");
    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:  // Z4
      out.Write("  ocol0 = texcol.aaaa;\n");
      break;

    case EFBCopyFormat::R8_0x1:  // Z8
    case EFBCopyFormat::R8:      // Z8H
      out.Write("  ocol0 = texcol.rrrr;\n");
      break;

    case EFBCopyFormat::RA8:  // Z16
      out.Write("  ocol0 = texcol.gggr;\n");
      break;

    case EFBCopyFormat::RG8:  // Z16 (reverse order)
      out.Write("  ocol0 = texcol.rrrg;\n");
      break;

    case EFBCopyFormat::RGBA8:  // Z24X8
      out.Write("  ocol0 = float4(texcol.rgb, 1.0);\n");
      break;

    case EFBCopyFormat::G8:  // Z8M
      out.Write("  ocol0 = texcol.gggg;\n");
      break;

    case EFBCopyFormat::B8:  // Z8L
      out.Write("  ocol0 = texcol.bbbb;\n");
      break;

    case EFBCopyFormat::GB8:  // Z16L - copy lower 16 depth bits
      // expected to be used as an IA8 texture (upper 8 bits stored as intensity, lower 8 bits
      // stored as alpha)
      // Used e.g. in Zelda: Skyward Sword
      out.Write("  ocol0 = texcol.gggb;\n");
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%X", static_cast<int>(uid_data->dst_format));
      out.Write("  ocol0 = float4(texcol.bgr, 0.0);\n");
      break;
    }
  }
  else if (uid_data->is_intensity)
  {
    bool has_four_bits =
        (uid_data->dst_format == EFBCopyFormat::R4 || uid_data->dst_format == EFBCopyFormat::RA4);
    bool has_alpha =
        (uid_data->dst_format == EFBCopyFormat::RA4 || uid_data->dst_format == EFBCopyFormat::RA8);

    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:      // I4
    case EFBCopyFormat::R8_0x1:  // I8
    case EFBCopyFormat::R8:      // I8
    case EFBCopyFormat::RA4:     // IA4
    case EFBCopyFormat::RA8:     // IA8
      if (has_four_bits)
        out.Write("  texcol = float4(int4(texcol * 255.0) & 0xF0) * (1.0 / 240.0);\n");

      // TODO - verify these coefficients
      out.Write("  const float3 coefficients = float3(0.257, 0.504, 0.098);\n"
                "  float intensity = dot(texcol.rgb, coefficients) + 16.0 / 255.0;\n"
                "  ocol0 = float4(intensity, intensity, intensity, %s);\n",
                has_alpha ? "texcol.a" : "intensity");
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%X",
                static_cast<int>(uid_data->dst_format));
      out.Write("  ocol0 = texcol;\n");
      break;
    }
  }
  else
  {
    if (!uid_data->efb_has_alpha)
      out.Write("  texcol.a = 1.0;\n");

    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:  // R4
      out.Write("  float red = float(int(texcol.r * 255.0) & 0xF0) * (1.0 / 240.0);\n"
                "  ocol0 = float4(red, red, red, red);\n");
      break;

    case EFBCopyFormat::R8_0x1:  // R8
    case EFBCopyFormat::R8:      // R8
      out.Write("  ocol0 = texcol.rrrr;\n");
      break;

    case EFBCopyFormat::RA4:  // RA4
      out.Write("  float2 red_alpha = float2(int2(texcol.ra * 255.0) & 0xF0) * (1.0 / 240.0);\n"
                "  ocol0 = red_alpha.rrrg;\n");
      break;

    case EFBCopyFormat::RA8:  // RA8
      out.Write("  ocol0 = texcol.rrra;\n");
      break;

    case EFBCopyFormat::A8:  // A8
      out.Write("  ocol0 = texcol.aaaa;\n");
      break;

    case EFBCopyFormat::G8:  // G8
      out.Write("  ocol0 = texcol.gggg;\n");
      break;

    case EFBCopyFormat::B8:  // B8
      out.Write("  ocol0 = texcol.bbbb;\n");
      break;

    case EFBCopyFormat::RG8:  // RG8
      out.Write("  ocol0 = texcol.rrrg;\n");
      break;

    case EFBCopyFormat::GB8:  // GB8
      out.Write("  ocol0 = texcol.gggb;\n");
      break;

    case EFBCopyFormat::RGB565:  // RGB565
      out.Write("  float2 red_blue = float2(int2(texcol.rb * 255.0) & 0xF8) * (1.0 / 248.0);\n"
                "  float green = float(int(texcol.g * 255.0) & 0xFC) * (1.0 / 252.0);\n"
                "  ocol0 = float4(red_blue.r, green, red_blue.g, 1.0);\n");
      break;

    case EFBCopyFormat::RGB5A3:  // RGB5A3
      // TODO: The MSB controls whether we have RGB5 or RGB4A3, this selection
      // will need to be implemented once we move away from floats.
      out.Write("  float3 color = float3(int3(texcol.rgb * 255.0) & 0xF8) * (1.0 / 248.0);\n"
                "  float alpha = float(int(texcol.a * 255.0) & 0xE0) * (1.0 / 224.0);\n"
                "  ocol0 = float4(color, alpha);\n");
      break;

    case EFBCopyFormat::RGBA8:  // RGBA8
      out.Write("  ocol0 = texcol;\n");
      break;

    case EFBCopyFormat::XFB:
      out.Write(
          "  ocol0 = float4(pow(texcol.rgb, float3(gamma_rcp, gamma_rcp, gamma_rcp)), 1.0f);\n");
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy color format: 0x%X", static_cast<int>(uid_data->dst_format));
      out.Write("  ocol0 = texcol;\n");
      break;
    }
  }

  out.Write("}\n");

  return out;
}

}  // namespace TextureConversionShaderGen
