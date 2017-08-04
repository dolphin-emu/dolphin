// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

TextureConverterShaderUid GetTextureConverterShaderUid(EFBCopyFormat dst_format, bool is_depth_copy,
                                                       bool is_intensity, bool scale_by_half)
{
  TextureConverterShaderUid out;
  convertion_shader_uid_data* uid_data = out.GetUidData<convertion_shader_uid_data>();
  memset(uid_data, 0, sizeof(*uid_data));

  uid_data->dst_format = dst_format;
  uid_data->efb_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
  uid_data->is_depth_copy = is_depth_copy;
  uid_data->is_intensity = is_intensity;
  uid_data->scale_by_half = scale_by_half;

  return out;
}

ShaderCode GenerateTextureConverterShaderCode(APIType api_type,
                                              const convertion_shader_uid_data* uid_data)
{
  ShaderCode out;

  std::array<float, 28> colmat = {};
  float* const const_add = &colmat[16];
  float* const color_mask = &colmat[20];
  color_mask[0] = color_mask[1] = color_mask[2] = color_mask[3] = 255.0f;
  color_mask[4] = color_mask[5] = color_mask[6] = color_mask[7] = 1.0f / 255.0f;

  if (api_type == APIType::OpenGL)
    out.Write("SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
              "#define samp0 samp9\n"
              "#define uv0 f_uv0\n"
              "in vec3 uv0;\n"
              "out vec4 ocol0;\n");

  else if (api_type == APIType::Vulkan)
    out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n"
              "layout(location = 0) in vec3 uv0;\n"
              "layout(location = 1) in vec4 col0;\n"
              "layout(location = 0) out vec4 ocol0;");

  bool mono_depth = uid_data->is_depth_copy && g_ActiveConfig.bStereoEFBMonoDepth;
  out.Write("void main(){\n"
            "  vec4 texcol = texture(samp0, %s);\n",
            mono_depth ? "vec3(uv0.xy, 0.0)" : "uv0");

  if (uid_data->is_depth_copy)
  {
    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:  // Z4
      colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1.0f;
      break;
    case EFBCopyFormat::R8_0x1:  // Z8
    case EFBCopyFormat::R8:      // Z8H
      colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1.0f;
      break;

    case EFBCopyFormat::RA8:  // Z16
      colmat[1] = colmat[5] = colmat[9] = colmat[12] = 1.0f;
      break;

    case EFBCopyFormat::RG8:  // Z16 (reverse order)
      colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
      break;

    case EFBCopyFormat::RGBA8:  // Z24X8
      colmat[0] = colmat[5] = colmat[10] = 1.0f;
      break;

    case EFBCopyFormat::G8:  // Z8M
      colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
      break;

    case EFBCopyFormat::B8:  // Z8L
      colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
      break;

    case EFBCopyFormat::GB8:  // Z16L - copy lower 16 depth bits
      // expected to be used as an IA8 texture (upper 8 bits stored as intensity, lower 8 bits
      // stored as alpha)
      // Used e.g. in Zelda: Skyward Sword
      colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%X", static_cast<int>(uid_data->dst_format));
      colmat[2] = colmat[5] = colmat[8] = 1.0f;
      break;
    }
  }
  else if (uid_data->is_intensity)
  {
    const_add[0] = const_add[1] = const_add[2] = 16.0f / 255.0f;
    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:      // I4
    case EFBCopyFormat::R8_0x1:  // I8
    case EFBCopyFormat::R8:      // I8
    case EFBCopyFormat::RA4:     // IA4
    case EFBCopyFormat::RA8:     // IA8
      // TODO - verify these coefficients
      colmat[0] = 0.257f;
      colmat[1] = 0.504f;
      colmat[2] = 0.098f;
      colmat[4] = 0.257f;
      colmat[5] = 0.504f;
      colmat[6] = 0.098f;
      colmat[8] = 0.257f;
      colmat[9] = 0.504f;
      colmat[10] = 0.098f;

      if (uid_data->dst_format == EFBCopyFormat::R4 ||
          uid_data->dst_format == EFBCopyFormat::R8_0x1 ||
          uid_data->dst_format == EFBCopyFormat::R8)
      {
        colmat[12] = 0.257f;
        colmat[13] = 0.504f;
        colmat[14] = 0.098f;
        const_add[3] = 16.0f / 255.0f;
        if (uid_data->dst_format == EFBCopyFormat::R4)
        {
          color_mask[0] = color_mask[1] = color_mask[2] = 255.0f / 16.0f;
          color_mask[4] = color_mask[5] = color_mask[6] = 1.0f / 15.0f;
        }
      }
      else  // alpha
      {
        colmat[15] = 1;
        if (uid_data->dst_format == EFBCopyFormat::RA4)
        {
          color_mask[0] = color_mask[1] = color_mask[2] = color_mask[3] = 255.0f / 16.0f;
          color_mask[4] = color_mask[5] = color_mask[6] = color_mask[7] = 1.0f / 15.0f;
        }
      }
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%X",
                static_cast<int>(uid_data->dst_format));
      colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
      break;
    }
  }
  else
  {
    switch (uid_data->dst_format)
    {
    case EFBCopyFormat::R4:  // R4
      colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
      color_mask[0] = 255.0f / 16.0f;
      color_mask[4] = 1.0f / 15.0f;
      break;
    case EFBCopyFormat::R8_0x1:  // R8
    case EFBCopyFormat::R8:      // R8
      colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
      break;

    case EFBCopyFormat::RA4:  // RA4
      colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;
      color_mask[0] = color_mask[3] = 255.0f / 16.0f;
      color_mask[4] = color_mask[7] = 1.0f / 15.0f;

      if (!uid_data->efb_has_alpha)
      {
        color_mask[3] = 0.0f;
        const_add[3] = 1.0f;
      }
      break;
    case EFBCopyFormat::RA8:  // RA8
      colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;

      if (!uid_data->efb_has_alpha)
      {
        color_mask[3] = 0.0f;
        const_add[3] = 1.0f;
      }
      break;

    case EFBCopyFormat::A8:  // A8
      colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1.0f;

      if (!uid_data->efb_has_alpha)
      {
        color_mask[3] = 0.0f;
        const_add[0] = 1.0f;
        const_add[1] = 1.0f;
        const_add[2] = 1.0f;
        const_add[3] = 1.0f;
      }
      break;

    case EFBCopyFormat::G8:  // G8
      colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
      break;
    case EFBCopyFormat::B8:  // B8
      colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
      break;

    case EFBCopyFormat::RG8:  // RG8
      colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
      break;

    case EFBCopyFormat::GB8:  // GB8
      colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
      break;

    case EFBCopyFormat::RGB565:  // RGB565
      colmat[0] = colmat[5] = colmat[10] = 1.0f;
      color_mask[0] = color_mask[2] = 255.0f / 8.0f;
      color_mask[4] = color_mask[6] = 1.0f / 31.0f;
      color_mask[1] = 255.0f / 4.0f;
      color_mask[5] = 1.0f / 63.0f;
      const_add[3] = 1.0f;  // set alpha to 1
      break;

    case EFBCopyFormat::RGB5A3:  // RGB5A3
      colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
      color_mask[0] = color_mask[1] = color_mask[2] = 255.0f / 8.0f;
      color_mask[4] = color_mask[5] = color_mask[6] = 1.0f / 31.0f;
      color_mask[3] = 255.0f / 32.0f;
      color_mask[7] = 1.0f / 7.0f;

      if (!uid_data->efb_has_alpha)
      {
        color_mask[3] = 0.0f;
        const_add[3] = 1.0f;
      }
      break;
    case EFBCopyFormat::RGBA8:  // RGBA8
      colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;

      if (!uid_data->efb_has_alpha)
      {
        color_mask[3] = 0.0f;
        const_add[3] = 1.0f;
      }
      break;

    case EFBCopyFormat::XFB:  // XFB copy, we just pretend it's an RGBX copy
      colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
      color_mask[3] = 0.0f;
      const_add[3] = 1.0f;
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown copy color format: 0x%X", static_cast<int>(uid_data->dst_format));
      colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
      break;
    }
  }

  out.Write("  const vec4 colmat[7] = {\n");
  for (size_t i = 0; i < colmat.size() / 4; i++)
  {
    out.Write("    vec4(%f, %f, %f, %f)%s\n", colmat[i * 4 + 0], colmat[i * 4 + 1],
              colmat[i * 4 + 2], colmat[i * 4 + 3], i < 7 ? "," : "");
  }
  out.Write("  };\n");

  if (uid_data->is_depth_copy)
  {
    if (api_type == APIType::Vulkan)
      out.Write("texcol.x = 1.0 - texcol.x;\n");

    out.Write("  int depth = int(texcol.x * 16777216.0);\n"

              // Convert to Z24 format
              "  ivec4 workspace;\n"
              "  workspace.r = (depth >> 16) & 255;\n"
              "  workspace.g = (depth >> 8) & 255;\n"
              "  workspace.b = depth & 255;\n"

              // Convert to Z4 format
              "  workspace.a = (depth >> 16) & 0xF0;\n"

              // Normalize components to [0.0..1.0]
              "  texcol = vec4(workspace) / 255.0;\n");
  }
  else
  {
    out.Write("  texcol = floor(texcol * colmat[5]) * colmat[6];\n");
  }
  out.Write("  ocol0 = texcol * mat4(colmat[0], colmat[1], colmat[2], colmat[3]) + colmat[4];\n"
            "}\n");

  return out;
}
