// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/FramebufferShaderGen.h"

#include <string_view>

#include "Common/Logging/Log.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace FramebufferShaderGen
{
namespace
{
APIType GetAPIType()
{
  return g_ActiveConfig.backend_info.api_type;
}

void EmitUniformBufferDeclaration(ShaderCode& code)
{
  code.Write("UBO_BINDING(std140, 1) uniform PSBlock\n");
}

void EmitSamplerDeclarations(ShaderCode& code, u32 start = 0, u32 end = 1,
                             bool multisampled = false)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  case APIType::Metal:
  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    const char* array_type = multisampled ? "sampler2DMSArray" : "sampler2DArray";

    for (u32 i = start; i < end; i++)
    {
      code.Write("SAMPLER_BINDING({}) uniform {} samp{};\n", i, array_type, i);
    }
  }
  break;
  default:
    break;
  }
}

void EmitSampleTexture(ShaderCode& code, u32 n, std::string_view coords)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  case APIType::Metal:
  case APIType::OpenGL:
  case APIType::Vulkan:
    code.Write("texture(samp{}, {})", n, coords);
    break;

  default:
    break;
  }
}

// Emits a texel fetch/load instruction. Assumes that "coords" is a 4-element vector, with z
// containing the layer, and w containing the mipmap level.
void EmitTextureLoad(ShaderCode& code, u32 n, std::string_view coords)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  case APIType::Metal:
  case APIType::OpenGL:
  case APIType::Vulkan:
    code.Write("texelFetch(samp{}, ({}).xyz, ({}).w)", n, coords, coords);
    break;

  default:
    break;
  }
}

void EmitVertexMainDeclaration(ShaderCode& code, u32 num_tex_inputs, u32 num_color_inputs,
                               bool position_input, u32 num_tex_outputs, u32 num_color_outputs,
                               std::string_view extra_inputs = {})
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  case APIType::Metal:
  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    for (u32 i = 0; i < num_tex_inputs; i++)
    {
      const auto attribute = ShaderAttrib::TexCoord0 + i;
      code.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawtex{};\n", attribute, i);
    }
    for (u32 i = 0; i < num_color_inputs; i++)
    {
      const auto attribute = ShaderAttrib::Color0 + i;
      code.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawcolor{};\n", attribute, i);
    }
    if (position_input)
      code.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawpos;\n", ShaderAttrib::Position);

    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      code.Write("VARYING_LOCATION(0) out VertexData {{\n");
      for (u32 i = 0; i < num_tex_outputs; i++)
        code.Write("  float3 v_tex{};\n", i);
      for (u32 i = 0; i < num_color_outputs; i++)
        code.Write("  float4 v_col{};\n", i);
      code.Write("}};\n");
    }
    else
    {
      for (u32 i = 0; i < num_tex_outputs; i++)
        code.Write("VARYING_LOCATION({}) out float3 v_tex{};\n", i, i);
      for (u32 i = 0; i < num_color_outputs; i++)
        code.Write("VARYING_LOCATION({}) out float4 v_col{};\n", num_tex_inputs + i, i);
    }
    code.Write("#define opos gl_Position\n");
    code.Write("{}\n", extra_inputs);
    code.Write("void main()\n");
  }
  break;
  default:
    break;
  }
}

void EmitPixelMainDeclaration(ShaderCode& code, u32 num_tex_inputs, u32 num_color_inputs,
                              std::string_view output_type = "float4",
                              std::string_view extra_vars = {}, bool emit_frag_coord = false)
{
  switch (GetAPIType())
  {
  case APIType::D3D:
  case APIType::Metal:
  case APIType::OpenGL:
  case APIType::Vulkan:
  {
    if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    {
      code.Write("VARYING_LOCATION(0) in VertexData {{\n");
      for (u32 i = 0; i < num_tex_inputs; i++)
        code.Write("  float3 v_tex{};\n", i);
      for (u32 i = 0; i < num_color_inputs; i++)
        code.Write("  float4 v_col{};\n", i);
      code.Write("}};\n");
    }
    else
    {
      for (u32 i = 0; i < num_tex_inputs; i++)
        code.Write("VARYING_LOCATION({}) in float3 v_tex{};\n", i, i);
      for (u32 i = 0; i < num_color_inputs; i++)
        code.Write("VARYING_LOCATION({}) in float4 v_col{};\n", num_tex_inputs + i, i);
    }

    code.Write("FRAGMENT_OUTPUT_LOCATION(0) out {} ocol0;\n", output_type);
    code.Write("{}\n", extra_vars);
    if (emit_frag_coord)
      code.Write("#define frag_coord gl_FragCoord\n");
    code.Write("void main()\n");
  }
  break;

  default:
    break;
  }
}
}  // Anonymous namespace

std::string GenerateScreenQuadVertexShader()
{
  ShaderCode code;
  EmitVertexMainDeclaration(code, 0, 0, false, 1, 0,

                            "#define id gl_VertexID\n");
  code.Write(
      "{{\n"
      "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n"
      "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n");

  // NDC space is flipped in Vulkan. We also flip in GL so that (0,0) is in the lower-left.
  if (GetAPIType() == APIType::Vulkan || GetAPIType() == APIType::OpenGL)
    code.Write("  opos.y = -opos.y;\n");

  code.Write("}}\n");

  return code.GetBuffer();
}

std::string GeneratePassthroughGeometryShader(u32 num_tex, u32 num_colors)
{
  ShaderCode code;
  if (GetAPIType() == APIType::D3D)
  {
    code.Write("struct VS_OUTPUT\n"
               "{{\n");
    for (u32 i = 0; i < num_tex; i++)
      code.Write("  float3 tex{} : TEXCOORD{};\n", i, i);
    for (u32 i = 0; i < num_colors; i++)
      code.Write("  float4 color{} : TEXCOORD{};\n", i, i + num_tex);
    code.Write("  float4 position : SV_Position;\n"
               "}};\n");

    code.Write("struct GS_OUTPUT\n"
               "{{");
    for (u32 i = 0; i < num_tex; i++)
      code.Write("  float3 tex{} : TEXCOORD{};\n", i, i);
    for (u32 i = 0; i < num_colors; i++)
      code.Write("  float4 color{} : TEXCOORD{};\n", i, i + num_tex);
    code.Write("  float4 position : SV_Position;\n"
               "  uint slice : SV_RenderTargetArrayIndex;\n"
               "}};\n\n");

    code.Write("[maxvertexcount(6)]\n"
               "void main(triangle VS_OUTPUT vso[3], inout TriangleStream<GS_OUTPUT> output)\n"
               "{{\n"
               "  for (uint slice = 0; slice < 2u; slice++)\n"
               "  {{\n"
               "    for (int i = 0; i < 3; i++)\n"
               "    {{\n"
               "      GS_OUTPUT gso;\n"
               "      gso.position = vso[i].position;\n");
    for (u32 i = 0; i < num_tex; i++)
      code.Write("      gso.tex{} = float3(vso[i].tex{}.xy, float(slice));\n", i, i);
    for (u32 i = 0; i < num_colors; i++)
      code.Write("      gso.color{} = vso[i].color{};\n", i, i);
    code.Write("      gso.slice = slice;\n"
               "      output.Append(gso);\n"
               "    }}\n"
               "    output.RestartStrip();\n"
               "  }}\n"
               "}}\n");
  }
  else if (GetAPIType() == APIType::OpenGL || GetAPIType() == APIType::Vulkan)
  {
    code.Write("layout(triangles) in;\n"
               "layout(triangle_strip, max_vertices = 6) out;\n");

    if (num_tex > 0 || num_colors > 0)
    {
      code.Write("VARYING_LOCATION(0) in VertexData {{\n");
      for (u32 i = 0; i < num_tex; i++)
        code.Write("  float3 v_tex{};\n", i);
      for (u32 i = 0; i < num_colors; i++)
        code.Write("  float4 v_col{};\n", i);
      code.Write("}} v_in[];\n");

      code.Write("VARYING_LOCATION(0) out VertexData {{\n");
      for (u32 i = 0; i < num_tex; i++)
        code.Write("  float3 v_tex{};\n", i);
      for (u32 i = 0; i < num_colors; i++)
        code.Write("  float4 v_col{};\n", i);
      code.Write("}} v_out;\n");
    }
    code.Write("\n"
               "void main()\n"
               "{{\n"
               "  for (int j = 0; j < 2; j++)\n"
               "  {{\n"
               "    gl_Layer = j;\n");

    // We have to explicitly unroll this loop otherwise the GL compiler gets cranky.
    for (u32 v = 0; v < 3; v++)
    {
      code.Write("    gl_Position = gl_in[{}].gl_Position;\n", v);
      for (u32 i = 0; i < num_tex; i++)
      {
        code.Write("    v_out.v_tex{} = float3(v_in[{}].v_tex{}.xy, float(j));\n", i, v, i);
      }
      for (u32 i = 0; i < num_colors; i++)
        code.Write("    v_out.v_col{} = v_in[{}].v_col{};\n", i, v, i);
      code.Write("    EmitVertex();\n\n");
    }
    code.Write("    EndPrimitive();\n"
               "  }}\n"
               "}}\n");
  }

  return code.GetBuffer();
}

std::string GenerateTextureCopyVertexShader()
{
  ShaderCode code;
  EmitUniformBufferDeclaration(code);
  code.Write("{{"
             "  float2 src_offset;\n"
             "  float2 src_size;\n"
             "}};\n\n");

  EmitVertexMainDeclaration(code, 0, 0, false, 1, 0,

                            "#define id gl_VertexID");
  code.Write("{{\n"
             "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n"
             "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n"
             "  v_tex0 = float3(src_offset + (src_size * v_tex0.xy), 0.0f);\n");

  // NDC space is flipped in Vulkan. We also flip in GL so that (0,0) is in the lower-left.
  if (GetAPIType() == APIType::Vulkan || GetAPIType() == APIType::OpenGL)
    code.Write("  opos.y = -opos.y;\n");

  code.Write("}}\n");

  return code.GetBuffer();
}

std::string GenerateTextureCopyPixelShader()
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, false);
  EmitPixelMainDeclaration(code, 1, 0);
  code.Write("{{\n"
             "  ocol0 = ");
  EmitSampleTexture(code, 0, "v_tex0");
  code.Write(";\n"
             "}}\n");
  return code.GetBuffer();
}

std::string GenerateColorPixelShader()
{
  ShaderCode code;
  EmitPixelMainDeclaration(code, 0, 1);
  code.Write("{{\n"
             "  ocol0 = v_col0;\n"
             "}}\n");
  return code.GetBuffer();
}

std::string GenerateResolveColorPixelShader(u32 samples)
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, true);
  EmitPixelMainDeclaration(code, 1, 0);
  code.Write("{{\n"
             "  int layer = int(v_tex0.z);\n"
             "  int3 coords = int3(int2(gl_FragCoord.xy), layer);\n"
             "  ocol0 = float4(0.0f);\n");
  code.Write("  for (int i = 0; i < {}; i++)\n", samples);
  code.Write("    ocol0 += texelFetch(samp0, coords, i);\n");
  code.Write("  ocol0 /= {}.0f;\n", samples);
  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateResolveDepthPixelShader(u32 samples)
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, true);
  EmitPixelMainDeclaration(code, 1, 0, "float", "");
  code.Write("{{\n"
             "  int layer = int(v_tex0.z);\n");
  code.Write("  int3 coords = int3(int2(gl_FragCoord.xy), layer);\n");

  // Take the minimum of all depth samples.
  code.Write("  ocol0 = texelFetch(samp0, coords, 0).r;\n");
  code.Write("  for (int i = 1; i < {}; i++)\n", samples);
  code.Write("    ocol0 = min(ocol0, texelFetch(samp0, coords, i).r);\n");

  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateClearVertexShader()
{
  ShaderCode code;
  EmitUniformBufferDeclaration(code);
  code.Write("{{\n"
             "  float4 clear_color;\n"
             "  float clear_depth;\n"
             "}};\n");

  EmitVertexMainDeclaration(code, 0, 0, false, 0, 1,

                            "#define id gl_VertexID\n");
  code.Write(
      "{{\n"
      "  float2 coord = float2(float((id << 1) & 2), float(id & 2));\n"
      "  opos = float4(coord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), clear_depth, 1.0f);\n"
      "  v_col0 = clear_color;\n");

  // NDC space is flipped in Vulkan
  if (GetAPIType() == APIType::Vulkan)
    code.Write("  opos.y = -opos.y;\n");

  code.Write("}}\n");

  return code.GetBuffer();
}

std::string GenerateEFBPokeVertexShader()
{
  ShaderCode code;
  EmitVertexMainDeclaration(code, 0, 1, true, 0, 1);
  code.Write("{{\n"
             "  v_col0 = rawcolor0;\n"
             "  opos = float4(rawpos.xyz, 1.0f);\n");
  if (g_ActiveConfig.backend_info.bSupportsLargePoints)
    code.Write("  gl_PointSize = rawpos.w;\n");

  // NDC space is flipped in Vulkan.
  if (GetAPIType() == APIType::Vulkan)
    code.Write("  opos.y = -opos.y;\n");

  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateFormatConversionShader(EFBReinterpretType convtype, u32 samples)
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, samples > 1);
  EmitPixelMainDeclaration(code, 1, 0, "float4",

                           "");
  code.Write("{{\n"
             "  int layer = int(v_tex0.z);\n");
  code.Write("  int3 coords = int3(int2(gl_FragCoord.xy), layer);\n");

  if (samples == 1)
  {
    // No MSAA at all.
    code.Write("  float4 val = texelFetch(samp0, coords, 0);\n");
  }
  else if (g_ActiveConfig.bSSAA)
  {
    // Sample shading, shader runs once per sample
    code.Write("  float4 val = texelFetch(samp0, coords, gl_SampleID);");
  }
  else
  {
    // MSAA without sample shading, average out all samples.
    code.Write("  float4 val = float4(0.0f, 0.0f, 0.0f, 0.0f);\n");
    code.Write("  for (int i = 0; i < {}; i++)\n", samples);
    code.Write("    val += texelFetch(samp0, coords, i);\n");
    code.Write("  val /= float({});\n", samples);
  }

  switch (convtype)
  {
  case EFBReinterpretType::RGB8ToRGBA6:
    code.Write("  int4 src8 = int4(round(val * 255.f));\n"
               "  int4 dst6;\n"
               "  dst6.r = src8.r >> 2;\n"
               "  dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
               "  dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
               "  dst6.a = src8.b & 0x3F;\n"
               "  ocol0 = float4(dst6) / 63.f;\n");
    break;

  case EFBReinterpretType::RGB8ToRGB565:
    code.Write("  ocol0 = val;\n");
    break;

  case EFBReinterpretType::RGBA6ToRGB8:
    code.Write("  int4 src6 = int4(round(val * 63.f));\n"
               "  int4 dst8;\n"
               "  dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
               "  dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
               "  dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
               "  dst8.a = 255;\n"
               "  ocol0 = float4(dst8) / 255.f;\n");
    break;

  case EFBReinterpretType::RGBA6ToRGB565:
    code.Write("  ocol0 = val;\n");
    break;

  case EFBReinterpretType::RGB565ToRGB8:
    code.Write("  ocol0 = val;\n");
    break;

  case EFBReinterpretType::RGB565ToRGBA6:
    //
    code.Write("  ocol0 = val;\n");
    break;
  }

  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateTextureReinterpretShader(TextureFormat from_format, TextureFormat to_format)
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, false);
  EmitPixelMainDeclaration(code, 1, 0, "float4", "", true);
  code.Write("{{\n"
             "  int layer = int(v_tex0.z);\n"
             "  int4 coords = int4(int2(frag_coord.xy), layer, 0);\n");

  // Convert to a 32-bit value encompassing all channels, filling the most significant bits with
  // zeroes.
  code.Write("  uint raw_value;\n");
  switch (from_format)
  {
  case TextureFormat::I8:
  case TextureFormat::C8:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n"
               "  raw_value = uint(temp_value.r * 255.0);\n");
  }
  break;

  case TextureFormat::IA8:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n"
               "  raw_value = uint(temp_value.r * 255.0) | (uint(temp_value.a * 255.0) << 8);\n");
  }
  break;

  case TextureFormat::I4:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n"
               "  raw_value = uint(temp_value.r * 15.0);\n");
  }
  break;

  case TextureFormat::IA4:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n"
               "  raw_value = uint(temp_value.r * 15.0) | (uint(temp_value.a * 15.0) << 4);\n");
  }
  break;

  case TextureFormat::RGB565:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n"
               "  raw_value = uint(temp_value.b * 31.0) | (uint(temp_value.g * 63.0) << 5) |\n"
               "              (uint(temp_value.r * 31.0) << 11);\n");
  }
  break;

  case TextureFormat::RGB5A3:
  {
    code.Write("  float4 temp_value = ");
    EmitTextureLoad(code, 0, "coords");
    code.Write(";\n");

    // 0.8784 = 224 / 255 which is the maximum alpha value that can be represented in 3 bits
    code.Write(
        "  if (temp_value.a > 0.878f) {{\n"
        "    raw_value = (uint(temp_value.b * 31.0)) | (uint(temp_value.g * 31.0) << 5) |\n"
        "                (uint(temp_value.r * 31.0) << 10) | 0x8000u;\n"
        "  }} else {{\n"
        "     raw_value = (uint(temp_value.b * 15.0)) | (uint(temp_value.g * 15.0) << 4) |\n"
        "                 (uint(temp_value.r * 15.0) << 8) | (uint(temp_value.a * 7.0) << 12);\n"
        "  }}\n");
  }
  break;

  default:
    WARN_LOG_FMT(VIDEO, "From format {} is not supported", from_format);
    return "{}\n";
  }

  // Now convert it to its new representation.
  switch (to_format)
  {
  case TextureFormat::I8:
  case TextureFormat::C8:
  {
    code.Write("  float orgba = float(raw_value & 0xFFu) / 255.0;\n"
               "  ocol0 = float4(orgba, orgba, orgba, orgba);\n");
  }
  break;

  case TextureFormat::IA8:
  {
    code.Write("  float orgb = float(raw_value & 0xFFu) / 255.0;\n"
               "  ocol0 = float4(orgb, orgb, orgb, float((raw_value >> 8) & 0xFFu) / 255.0);\n");
  }
  break;

  case TextureFormat::IA4:
  {
    code.Write("  float orgb = float(raw_value & 0xFu) / 15.0;\n"
               "  ocol0 = float4(orgb, orgb, orgb, float((raw_value >> 4) & 0xFu) / 15.0);\n");
  }
  break;

  case TextureFormat::RGB565:
  {
    code.Write("  ocol0 = float4(float((raw_value >> 10) & 0x1Fu) / 31.0,\n"
               "                 float((raw_value >> 5) & 0x1Fu) / 31.0,\n"
               "                 float(raw_value & 0x1Fu) / 31.0, 1.0);\n");
  }
  break;

  case TextureFormat::RGB5A3:
  {
    code.Write("  if ((raw_value & 0x8000u) != 0u) {{\n"
               "    ocol0 = float4(float((raw_value >> 10) & 0x1Fu) / 31.0,\n"
               "                   float((raw_value >> 5) & 0x1Fu) / 31.0,\n"
               "                   float(raw_value & 0x1Fu) / 31.0, 1.0);\n"
               "  }} else {{\n"
               "    ocol0 = float4(float((raw_value >> 8) & 0x0Fu) / 15.0,\n"
               "                   float((raw_value >> 4) & 0x0Fu) / 15.0,\n"
               "                   float(raw_value & 0x0Fu) / 15.0,\n"
               "                   float((raw_value >> 12) & 0x07u) / 7.0);\n"
               "  }}\n");
  }
  break;
  default:
    WARN_LOG_FMT(VIDEO, "To format {} is not supported", to_format);
    return "{}\n";
  }

  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateEFBRestorePixelShader()
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 2, false);
  EmitPixelMainDeclaration(code, 1, 0, "float4", "");
  code.Write("{{\n"
             "  ocol0 = ");
  EmitSampleTexture(code, 0, "v_tex0");
  code.Write(";\n");
  code.Write("  gl_FragDepth = ");
  EmitSampleTexture(code, 1, "v_tex0");
  code.Write(".r;\n"
             "}}\n");
  return code.GetBuffer();
}

std::string GenerateImGuiVertexShader()
{
  ShaderCode code;

  // Uniform buffer contains the viewport size, and we transform in the vertex shader.
  EmitUniformBufferDeclaration(code);
  code.Write("{{\n"
             "float2 u_rcp_viewport_size_mul2;\n"
             "}};\n\n");

  EmitVertexMainDeclaration(code, 1, 1, true, 1, 1);
  code.Write("{{\n"
             "  v_tex0 = float3(rawtex0.xy, 0.0);\n"
             "  v_col0 = rawcolor0;\n"
             "  opos = float4(rawpos.x * u_rcp_viewport_size_mul2.x - 1.0,"
             "                1.0 - rawpos.y * u_rcp_viewport_size_mul2.y, 0.0, 1.0);\n");

  // NDC space is flipped in Vulkan.
  if (GetAPIType() == APIType::Vulkan)
    code.Write("  opos.y = -opos.y;\n");

  code.Write("}}\n");
  return code.GetBuffer();
}

std::string GenerateImGuiPixelShader()
{
  ShaderCode code;
  EmitSamplerDeclarations(code, 0, 1, false);
  EmitPixelMainDeclaration(code, 1, 1);
  code.Write("{{\n"
             "  ocol0 = ");
  EmitSampleTexture(code, 0, "float3(v_tex0.xy, 0.0)");
  code.Write(" * v_col0;\n"
             "}}\n");

  return code.GetBuffer();
}

}  // namespace FramebufferShaderGen
