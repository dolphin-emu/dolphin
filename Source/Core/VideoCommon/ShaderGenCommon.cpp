// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/ShaderGenCommon.h"

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

ShaderHostConfig ShaderHostConfig::GetCurrent()
{
  ShaderHostConfig bits = {};
  bits.msaa = g_ActiveConfig.iMultisamples > 1;
  bits.ssaa = g_ActiveConfig.iMultisamples > 1 && g_ActiveConfig.bSSAA &&
              g_ActiveConfig.backend_info.bSupportsSSAA;
  bits.stereo = g_ActiveConfig.stereo_mode != StereoMode::Off;
  bits.wireframe = g_ActiveConfig.bWireFrame;
  bits.per_pixel_lighting = g_ActiveConfig.bEnablePixelLighting;
  bits.vertex_rounding = g_ActiveConfig.UseVertexRounding();
  bits.fast_depth_calc = g_ActiveConfig.bFastDepthCalc;
  bits.bounding_box = g_ActiveConfig.bBBoxEnable;
  bits.backend_dual_source_blend = g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
  bits.backend_geometry_shaders = g_ActiveConfig.backend_info.bSupportsGeometryShaders;
  bits.backend_early_z = g_ActiveConfig.backend_info.bSupportsEarlyZ;
  bits.backend_bbox = g_ActiveConfig.backend_info.bSupportsBBox;
  bits.backend_gs_instancing = g_ActiveConfig.backend_info.bSupportsGSInstancing;
  bits.backend_clip_control = g_ActiveConfig.backend_info.bSupportsClipControl;
  bits.backend_ssaa = g_ActiveConfig.backend_info.bSupportsSSAA;
  bits.backend_atomics = g_ActiveConfig.backend_info.bSupportsFragmentStoresAndAtomics;
  bits.backend_depth_clamp = g_ActiveConfig.backend_info.bSupportsDepthClamp;
  bits.backend_reversed_depth_range = g_ActiveConfig.backend_info.bSupportsReversedDepthRange;
  bits.backend_bitfield = g_ActiveConfig.backend_info.bSupportsBitfield;
  bits.backend_dynamic_sampler_indexing =
      g_ActiveConfig.backend_info.bSupportsDynamicSamplerIndexing;
  bits.backend_shader_framebuffer_fetch = g_ActiveConfig.backend_info.bSupportsFramebufferFetch;
  bits.backend_logic_op = g_ActiveConfig.backend_info.bSupportsLogicOp;
  bits.backend_palette_conversion = g_ActiveConfig.backend_info.bSupportsPaletteConversion;
  bits.enable_validation_layer = g_ActiveConfig.bEnableValidationLayer;
  bits.manual_texture_sampling = !g_ActiveConfig.bFastTextureSampling;
  bits.manual_texture_sampling_custom_texture_sizes =
      g_ActiveConfig.ManualTextureSamplingWithCustomTextureSizes();
  bits.backend_sampler_lod_bias = g_ActiveConfig.backend_info.bSupportsLodBiasInSampler;
  bits.backend_dynamic_vertex_loader = g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader;
  bits.backend_vs_point_line_expand = g_ActiveConfig.UseVSForLinePointExpand();
  bits.backend_gl_layer_in_fs = g_ActiveConfig.backend_info.bSupportsGLLayerInFS;
  return bits;
}

std::string GetDiskShaderCacheFileName(APIType api_type, const char* type, bool include_gameid,
                                       bool include_host_config, bool include_api)
{
  if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
    File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

  std::string filename = File::GetUserPath(D_SHADERCACHE_IDX);
  if (include_api)
  {
    switch (api_type)
    {
    case APIType::D3D:
      filename += "D3D";
      break;
    case APIType::Metal:
      filename += "Metal";
      break;
    case APIType::OpenGL:
      filename += "OpenGL";
      break;
    case APIType::Vulkan:
      filename += "Vulkan";
      break;
    default:
      break;
    }
    filename += '-';
  }

  filename += type;

  if (include_gameid)
  {
    filename += '-';
    filename += SConfig::GetInstance().GetGameID();
  }

  if (include_host_config)
  {
    // We're using 21 bits, so 6 hex characters.
    ShaderHostConfig host_config = ShaderHostConfig::GetCurrent();
    filename += fmt::format("-{:06X}", host_config.bits);
  }

  filename += ".cache";
  return filename;
}

void WriteIsNanHeader(ShaderCode& out, APIType api_type)
{
  if (api_type == APIType::D3D)
  {
    out.Write("bool dolphin_isnan(float f) {{\n"
              "  // Workaround for the HLSL compiler deciding that isnan can never be true and\n"
              "  // optimising away the call, even though the value can actually be NaN\n"
              "  // Just look for the bit pattern that indicates NaN instead\n"
              "  return (floatBitsToInt(f) & 0x7FFFFFFF) > 0x7F800000;\n"
              "}}\n\n");
    // If isfinite is needed, (floatBitsToInt(f) & 0x7F800000) != 0x7F800000 can be used
  }
  else
  {
    out.Write("#define dolphin_isnan(f) isnan(f)\n");
  }
}

void WriteBitfieldExtractHeader(ShaderCode& out, APIType api_type,
                                const ShaderHostConfig& host_config)
{
  // ==============================================
  //  BitfieldExtract for APIs which don't have it
  // ==============================================
  if (!host_config.backend_bitfield)
  {
    out.Write("uint bitfieldExtract(uint val, int off, int size) {{\n"
              "  // This built-in function is only supported in OpenGL 4.0+ and ES 3.1+\n"
              "  // Microsoft's HLSL compiler automatically optimises this to a bitfield extract "
              "instruction.\n"
              "  uint mask = uint((1 << size) - 1);\n"
              "  return uint(val >> off) & mask;\n"
              "}}\n\n");
    out.Write("int bitfieldExtract(int val, int off, int size) {{\n"
              "  // This built-in function is only supported in OpenGL 4.0+ and ES 3.1+\n"
              "  // Microsoft's HLSL compiler automatically optimises this to a bitfield extract "
              "instruction.\n"
              "  return ((val << (32 - size - off)) >> (32 - size));\n"
              "}}\n\n");
  }
}

static void DefineOutputMember(ShaderCode& object, APIType api_type, std::string_view qualifier,
                               std::string_view type, std::string_view name, int var_index,
                               ShaderStage stage, std::string_view semantic = {},
                               int semantic_index = -1)
{
  object.Write("\t{} {} {}", qualifier, type, name);

  if (var_index != -1)
    object.Write("{}", var_index);

  if (api_type == APIType::D3D && !semantic.empty() && stage == ShaderStage::Geometry)
  {
    if (semantic_index != -1)
      object.Write(" : {}{}", semantic, semantic_index);
    else
      object.Write(" : {}", semantic);
  }

  object.Write(";\n");
}

void GenerateVSOutputMembers(ShaderCode& object, APIType api_type, u32 texgens,
                             const ShaderHostConfig& host_config, std::string_view qualifier,
                             ShaderStage stage)
{
  // SPIRV-Cross names all semantics as "TEXCOORD"
  // Unfortunately Geometry shaders (which also uses this function)
  // aren't supported.  The output semantic name needs to match
  // up with the input semantic name for both the next stage (pixel shader)
  // and the previous stage (vertex shader), so
  // we need to handle geometry in a special way...
  if (api_type == APIType::D3D && stage == ShaderStage::Geometry)
  {
    DefineOutputMember(object, api_type, qualifier, "float4", "pos", -1, stage, "TEXCOORD", 0);
    DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 0, stage, "TEXCOORD", 1);
    DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 1, stage, "TEXCOORD", 2);

    constexpr unsigned int index_base = 3;
    unsigned int index_offset = 0;
    if (host_config.backend_geometry_shaders)
    {
      DefineOutputMember(object, api_type, qualifier, "float", "clipDist", 0, stage, "TEXCOORD",
                         index_base + index_offset);
      DefineOutputMember(object, api_type, qualifier, "float", "clipDist", 1, stage, "TEXCOORD",
                         index_base + index_offset + 1);
      index_offset += 2;
    }

    for (unsigned int i = 0; i < texgens; ++i)
    {
      DefineOutputMember(object, api_type, qualifier, "float3", "tex", i, stage, "TEXCOORD",
                         index_base + index_offset + i);
    }
    index_offset += texgens;

    if (!host_config.fast_depth_calc)
    {
      DefineOutputMember(object, api_type, qualifier, "float4", "clipPos", -1, stage, "TEXCOORD",
                         index_base + index_offset);
      index_offset++;
    }

    if (host_config.per_pixel_lighting)
    {
      DefineOutputMember(object, api_type, qualifier, "float3", "Normal", -1, stage, "TEXCOORD",
                         index_base + index_offset);
      DefineOutputMember(object, api_type, qualifier, "float3", "WorldPos", -1, stage, "TEXCOORD",
                         index_base + index_offset + 1);
      index_offset += 2;
    }
  }
  else
  {
    DefineOutputMember(object, api_type, qualifier, "float4", "pos", -1, stage, "SV_Position");
    DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 0, stage, "COLOR", 0);
    DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 1, stage, "COLOR", 1);

    if (host_config.backend_geometry_shaders)
    {
      DefineOutputMember(object, api_type, qualifier, "float", "clipDist", 0, stage,
                         "SV_ClipDistance", 0);
      DefineOutputMember(object, api_type, qualifier, "float", "clipDist", 1, stage,
                         "SV_ClipDistance", 1);
    }

    for (unsigned int i = 0; i < texgens; ++i)
      DefineOutputMember(object, api_type, qualifier, "float3", "tex", i, stage, "TEXCOORD", i);

    if (!host_config.fast_depth_calc)
      DefineOutputMember(object, api_type, qualifier, "float4", "clipPos", -1, stage, "TEXCOORD",
                         texgens);

    if (host_config.per_pixel_lighting)
    {
      DefineOutputMember(object, api_type, qualifier, "float3", "Normal", -1, stage, "TEXCOORD",
                         texgens + 1);
      DefineOutputMember(object, api_type, qualifier, "float3", "WorldPos", -1, stage, "TEXCOORD",
                         texgens + 2);
    }
  }
}

void AssignVSOutputMembers(ShaderCode& object, std::string_view a, std::string_view b, u32 texgens,
                           const ShaderHostConfig& host_config)
{
  object.Write("\t{}.pos = {}.pos;\n", a, b);
  object.Write("\t{}.colors_0 = {}.colors_0;\n", a, b);
  object.Write("\t{}.colors_1 = {}.colors_1;\n", a, b);

  for (unsigned int i = 0; i < texgens; ++i)
    object.Write("\t{}.tex{} = {}.tex{};\n", a, i, b, i);

  if (!host_config.fast_depth_calc)
    object.Write("\t{}.clipPos = {}.clipPos;\n", a, b);

  if (host_config.per_pixel_lighting)
  {
    object.Write("\t{}.Normal = {}.Normal;\n", a, b);
    object.Write("\t{}.WorldPos = {}.WorldPos;\n", a, b);
  }

  if (host_config.backend_geometry_shaders)
  {
    object.Write("\t{}.clipDist0 = {}.clipDist0;\n", a, b);
    object.Write("\t{}.clipDist1 = {}.clipDist1;\n", a, b);
  }
}

void GenerateLineOffset(ShaderCode& object, std::string_view indent0, std::string_view indent1,
                        std::string_view pos_a, std::string_view pos_b, std::string_view sign)
{
  // GameCube/Wii's line drawing algorithm is a little quirky. It does not
  // use the correct line caps. Instead, the line caps are vertical or
  // horizontal depending the slope of the line.
  object.Write("{indent0}float2 offset;\n"
               "{indent0}float2 to = abs({pos_a}.xy / {pos_a}.w - {pos_b}.xy / {pos_b}.w);\n"
               // FIXME: What does real hardware do when line is at a 45-degree angle?
               // FIXME: Lines aren't drawn at the correct width. See Twilight Princess map.
               "{indent0}if (" I_LINEPTPARAMS ".y * to.y > " I_LINEPTPARAMS ".x * to.x) {{\n"
               // Line is more tall. Extend geometry left and right.
               // Lerp LineWidth/2 from [0..VpWidth] to [-1..1]
               "{indent1}offset = float2({sign}" I_LINEPTPARAMS ".z / " I_LINEPTPARAMS ".x, 0);\n"
               "{indent0}}} else {{\n"
               // Line is more wide. Extend geometry up and down.
               // Lerp LineWidth/2 from [0..VpHeight] to [1..-1]
               "{indent1}offset = float2(0, {sign}-" I_LINEPTPARAMS ".z / " I_LINEPTPARAMS ".y);\n"
               "{indent0}}}\n",
               fmt::arg("indent0", indent0), fmt::arg("indent1", indent1),  //
               fmt::arg("pos_a", pos_a), fmt::arg("pos_b", pos_b), fmt::arg("sign", sign));
}

void GenerateVSLineExpansion(ShaderCode& object, std::string_view indent, u32 texgens)
{
  std::string indent1 = std::string(indent) + "  ";
  object.Write("{0}other_pos = float4(dot(" I_PROJECTION "[0], other_pos), dot(" I_PROJECTION
               "[1], other_pos), dot(" I_PROJECTION "[2], other_pos), dot(" I_PROJECTION
               "[3], other_pos));\n"
               "\n"
               "{0}float expand_sign = is_right ? 1.0f : -1.0f;\n",
               indent);
  GenerateLineOffset(object, indent, indent1, "o.pos", "other_pos", "expand_sign * ");
  object.Write("\n"
               "{}o.pos.xy += offset * o.pos.w;\n",
               indent);
  if (texgens > 0)
  {
    object.Write("{}if ((" I_TEXOFFSET "[2] != 0) && is_right) {{\n", indent);
    object.Write("{}  float texOffset = 1.0 / float(" I_TEXOFFSET "[2]);\n", indent);
    for (u32 i = 0; i < texgens; i++)
    {
      object.Write("{}  if (((" I_TEXOFFSET "[0] >> {}) & 0x1) != 0)\n", indent, i);
      object.Write("{}    o.tex{}.x += texOffset;\n", indent, i);
    }
    object.Write("{}}}\n", indent);
  }
}

void GenerateVSPointExpansion(ShaderCode& object, std::string_view indent, u32 texgens)
{
  object.Write(
      "{0}float2 expand_sign = float2(is_right ? 1.0f : -1.0f, is_bottom ? -1.0f : 1.0f);\n"
      "{0}float2 offset = expand_sign * " I_LINEPTPARAMS ".ww / " I_LINEPTPARAMS ".xy;\n"
      "{0}o.pos.xy += offset * o.pos.w;\n",
      indent);
  if (texgens > 0)
  {
    object.Write("{0}if (" I_TEXOFFSET "[3] != 0) {{\n"
                 "{0}  float texOffsetMagnitude = 1.0f / float(" I_TEXOFFSET "[3]);\n"
                 "{0}  float2 texOffset = float2(is_right ? texOffsetMagnitude : 0.0f, "
                 "is_bottom ? texOffsetMagnitude : 0.0f);",
                 indent);
    for (u32 i = 0; i < texgens; i++)
    {
      object.Write("{}  if (((" I_TEXOFFSET "[1] >> {}) & 0x1) != 0)\n", indent, i);
      object.Write("{}    o.tex{}.xy += texOffset;\n", indent, i);
    }
    object.Write("{}}}\n", indent);
  }
}

const char* GetInterpolationQualifier(bool msaa, bool ssaa, bool in_glsl_interface_block, bool in)
{
  if (!msaa)
    return "";

  // Without GL_ARB_shading_language_420pack support, the interpolation qualifier must be
  // "centroid in" and not "centroid", even within an interface block.
  if (in_glsl_interface_block && !g_ActiveConfig.backend_info.bSupportsBindingLayout)
  {
    if (!ssaa)
      return in ? "centroid in" : "centroid out";
    else
      return in ? "sample in" : "sample out";
  }
  else
  {
    if (!ssaa)
      return "centroid";
    else
      return "sample";
  }
}

void WriteCustomShaderStructDef(ShaderCode* out, u32 numtexgens)
{
  // Bump this when there are breaking changes to the API
  out->Write("#define CUSTOM_SHADER_API_VERSION 1;\n");

  // CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE "enum" values
  out->Write("const uint CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_NONE = {}u;\n",
             static_cast<u32>(AttenuationFunc::None));
  out->Write("const uint CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_POINT = {}u;\n",
             static_cast<u32>(AttenuationFunc::Spec));
  out->Write("const uint CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_DIR = {}u;\n",
             static_cast<u32>(AttenuationFunc::Dir));
  out->Write("const uint CUSTOM_SHADER_LIGHTING_ATTENUATION_TYPE_SPOT = {}u;\n",
             static_cast<u32>(AttenuationFunc::Spot));

  out->Write("struct CustomShaderOutput\n");
  out->Write("{{\n");
  out->Write("\tfloat4 main_rt;\n");
  out->Write("}};\n\n");

  out->Write("struct CustomShaderLightData\n");
  out->Write("{{\n");
  out->Write("\tfloat3 position;\n");
  out->Write("\tfloat3 direction;\n");
  out->Write("\tfloat3 color;\n");
  out->Write("\tuint attenuation_type;\n");
  out->Write("\tfloat4 cosatt;\n");
  out->Write("\tfloat4 distatt;\n");
  out->Write("}};\n\n");

  // CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE "enum" values
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_PREV = 0u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR = 1u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX = 2u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_RAS = 3u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_KONST = 4u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC = 5u;\n");
  out->Write("const uint CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_UNUSED = 6u;\n");

  out->Write("struct CustomShaderTevStageInputColor\n");
  out->Write("{{\n");
  out->Write("\tuint input_type;\n");
  out->Write("\tfloat3 value;\n");
  out->Write("}};\n\n");

  out->Write("struct CustomShaderTevStageInputAlpha\n");
  out->Write("{{\n");
  out->Write("\tuint input_type;\n");
  out->Write("\tfloat value;\n");
  out->Write("}};\n\n");

  out->Write("struct CustomShaderTevStage\n");
  out->Write("{{\n");
  out->Write("\tCustomShaderTevStageInputColor[4] input_color;\n");
  out->Write("\tCustomShaderTevStageInputAlpha[4] input_alpha;\n");
  out->Write("\tuint texmap;\n");
  out->Write("\tfloat4 output_color;\n");
  out->Write("}};\n\n");

  // Custom structure for data we pass to custom shader hooks
  out->Write("struct CustomShaderData\n");
  out->Write("{{\n");
  out->Write("\tfloat3 position;\n");
  out->Write("\tfloat3 normal;\n");
  if (numtexgens == 0)
  {
    // Cheat so shaders compile
    out->Write("\tfloat3[1] texcoord;\n");
  }
  else
  {
    out->Write("\tfloat3[{}] texcoord;\n", numtexgens);
  }
  out->Write("\tuint texcoord_count;\n");
  out->Write("\tuint[8] texmap_to_texcoord_index;\n");
  out->Write("\tCustomShaderLightData[8] lights_chan0_color;\n");
  out->Write("\tCustomShaderLightData[8] lights_chan0_alpha;\n");
  out->Write("\tCustomShaderLightData[8] lights_chan1_color;\n");
  out->Write("\tCustomShaderLightData[8] lights_chan1_alpha;\n");
  out->Write("\tfloat4[2] ambient_lighting;\n");
  out->Write("\tfloat4[2] base_material;\n");
  out->Write("\tuint light_chan0_color_count;\n");
  out->Write("\tuint light_chan0_alpha_count;\n");
  out->Write("\tuint light_chan1_color_count;\n");
  out->Write("\tuint light_chan1_alpha_count;\n");
  out->Write("\tCustomShaderTevStage[16] tev_stages;\n");
  out->Write("\tuint tev_stage_count;\n");
  out->Write("\tfloat4 final_color;\n");
  out->Write("\tuint time_ms;\n");
  out->Write("}};\n\n");
}
