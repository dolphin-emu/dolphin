// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/UberShaderPixel.h"

#include "Common/Assert.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/UberShaderCommon.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace UberShader
{
namespace
{
void WriteCustomShaderStructImpl(ShaderCode* out, u32 num_texgen, bool per_pixel_lighting)
{
  out->Write("\tCustomShaderData custom_data;\n");
  if (per_pixel_lighting)
  {
    out->Write("\tcustom_data.position = WorldPos;\n");
    out->Write("\tcustom_data.normal = Normal;\n");
  }
  else
  {
    out->Write("\tcustom_data.position = float3(0, 0, 0);\n");
    out->Write("\tcustom_data.normal = float3(0, 0, 0);\n");
  }

  if (num_texgen == 0) [[unlikely]]
  {
    out->Write("\tcustom_data.texcoord[0] = float3(0, 0, 0);\n");
  }
  else
  {
    for (u32 i = 0; i < num_texgen; ++i)
    {
      out->Write("\tif (tex{0}.z == 0.0)\n", i);
      out->Write("\t{{\n");
      out->Write("\t\tcustom_data.texcoord[{0}] = tex{0};\n", i);
      out->Write("\t}}\n");
      out->Write("\telse {{\n");
      out->Write("\t\tcustom_data.texcoord[{0}] = float3(tex{0}.xy / tex{0}.z, 0);\n", i);
      out->Write("\t}}\n");
    }
  }

  out->Write("\tcustom_data.texcoord_count = {};\n", num_texgen);

  for (u32 i = 0; i < 8; i++)
  {
    // Shader compilation complains if every index isn't initialized
    out->Write("\tcustom_data.texmap_to_texcoord_index[{0}] = {0};\n", i);
  }

  for (u32 i = 0; i < NUM_XF_COLOR_CHANNELS; i++)
  {
    out->Write("\tcustom_data.base_material[{}] = vec4(0, 0, 0, 1);\n", i);
    out->Write("\tcustom_data.ambient_lighting[{}] = vec4(0, 0, 0, 1);\n", i);

    // Shader compilation errors can throw if not everything is initialized
    for (u32 light_count_index = 0; light_count_index < 8; light_count_index++)
    {
      // Color
      out->Write("\tcustom_data.lights_chan{}_color[{}].direction = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_color[{}].position = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_color[{}].color = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_color[{}].cosatt = float4(0, 0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_color[{}].distatt = float4(0, 0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_color[{}].attenuation_type = 0;\n", i,
                 light_count_index);

      // Alpha
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].direction = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].position = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].color = float3(0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].cosatt = float4(0, 0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].distatt = float4(0, 0, 0, 0);\n", i,
                 light_count_index);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].attenuation_type = 0;\n", i,
                 light_count_index);
    }

    out->Write("\tcustom_data.light_chan{}_color_count = 0;\n", i);
    out->Write("\tcustom_data.light_chan{}_alpha_count = 0;\n", i);
  }

  if (num_texgen > 0) [[likely]]
  {
    out->Write("\n");
    out->Write("\tfor(uint stage = 0u; stage <= num_stages; stage++)\n");
    out->Write("\t{{\n");
    out->Write("\t\tStageState ss;\n");
    out->Write("\t\tss.order = bpmem_tevorder(stage>>1);\n");
    out->Write("\t\tif ((stage & 1u) == 1u)\n");
    out->Write("\t\t\tss.order = ss.order >> {};\n\n",
               int(TwoTevStageOrders().enable_tex_odd.StartBit() -
                   TwoTevStageOrders().enable_tex_even.StartBit()));
    out->Write("\t\tuint texmap = {};\n",
               BitfieldExtract<&TwoTevStageOrders::texcoord_even>("ss.order"));
    // Shader compilation is weird, shader arrays can't use indexing by variable
    //  to set values unless the variable is an index in a for loop.
    // So instead we have to do this if check nonsense
    for (u32 i = 0; i < 8; i++)
    {
      out->Write("\t\tif (texmap == {})\n", i);
      out->Write("\t\t{{\n");
      out->Write("\t\t\tcustom_data.texmap_to_texcoord_index[{}] = selectTexCoordIndex(texmap);\n",
                 i);
      out->Write("\t\t}}\n");
    }
    out->Write("\t}}\n");
  }

  if (per_pixel_lighting)
  {
    out->Write("\tuint light_count = 0;\n");
    out->Write("\tfor (uint chan = 0u; chan < {}u; chan++)\n", NUM_XF_COLOR_CHANNELS);
    out->Write("\t{{\n");
    out->Write("\t\tuint colorreg = xfmem_color(chan);\n");
    out->Write("\t\tuint alphareg = xfmem_alpha(chan);\n");
    for (const auto& color_type : std::array<std::string_view, 2>{"colorreg", "alphareg"})
    {
      if (color_type == "colorreg")
      {
        out->Write("\t\tcustom_data.base_material[0] = " I_MATERIALS "[2u] / 255.0; \n");
        out->Write("\t\tif ({} != 0u)\n", BitfieldExtract<&LitChannel::enablelighting>(color_type));
        out->Write("\t\t\tcustom_data.base_material[0] = colors_0; \n");
      }
      else
      {
        out->Write("custom_data.base_material[1].w = " I_MATERIALS "[3u].w / 255.0; \n");
        out->Write("\t\tif ({} != 0u)\n", BitfieldExtract<&LitChannel::enablelighting>(color_type));
        out->Write("\t\t\tcustom_data.base_material[1].w = colors_1.w; \n");
      }
      out->Write("\t\tif ({} != 0u)\n", BitfieldExtract<&LitChannel::enablelighting>(color_type));
      out->Write("\t\t{{\n");
      out->Write("\t\t\tuint light_mask = {} | ({} << 4u);\n",
                 BitfieldExtract<&LitChannel::lightMask0_3>(color_type),
                 BitfieldExtract<&LitChannel::lightMask4_7>(color_type));
      out->Write("\t\t\tuint attnfunc = {};\n", BitfieldExtract<&LitChannel::attnfunc>(color_type));
      out->Write("\t\t\tfor (uint light_index = 0u; light_index < 8u; light_index++)\n");
      out->Write("\t\t\t{{\n");
      out->Write("\t\t\t\tif ((light_mask & (1u << light_index)) != 0u)\n");
      out->Write("\t\t\t\t{{\n");
      // Shader compilation is weird, shader arrays can't use indexing by variable
      //  to set values unless the variable is an index in a for loop.
      // So instead we have to do this if check nonsense
      for (u32 light_count_index = 0; light_count_index < 8; light_count_index++)
      {
        out->Write("\t\t\t\t\tif (light_index == {})\n", light_count_index);
        out->Write("\t\t\t\t\t{{\n");
        if (color_type == "colorreg")
        {
          for (u32 channel_index = 0; channel_index < NUM_XF_COLOR_CHANNELS; channel_index++)
          {
            out->Write("\t\t\t\t\t\tif (chan == {})\n", channel_index);
            out->Write("\t\t\t\t\t\t{{\n");
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].direction = " I_LIGHTS
                       "[light_index].dir.xyz;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].position = " I_LIGHTS
                       "[light_index].pos.xyz;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].cosatt = " I_LIGHTS
                       "[light_index].cosatt;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].distatt = " I_LIGHTS
                       "[light_index].distatt;\n",
                       channel_index, light_count_index);
            out->Write(
                "\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].attenuation_type = attnfunc;\n",
                channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_color[{}].color = " I_LIGHTS
                       "[light_index].color.rgb / float3(255.0, 255.0, 255.0);\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.light_chan{}_color_count += 1;\n", channel_index);
            out->Write("\t\t\t\t\t\t}}\n");
          }
        }
        else
        {
          for (u32 channel_index = 0; channel_index < NUM_XF_COLOR_CHANNELS; channel_index++)
          {
            out->Write("\t\t\t\t\t\tif (chan == {})\n", channel_index);
            out->Write("\t\t\t\t\t\t{{\n");
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].direction = " I_LIGHTS
                       "[light_index].dir.xyz;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].position = " I_LIGHTS
                       "[light_index].pos.xyz;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].cosatt = " I_LIGHTS
                       "[light_index].cosatt;\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].distatt = " I_LIGHTS
                       "[light_index].distatt;\n",
                       channel_index, light_count_index);
            out->Write(
                "\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].attenuation_type = attnfunc;\n",
                channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.lights_chan{}_alpha[{}].color = float3(" I_LIGHTS
                       "[light_index].color.a) / float3(255.0, 255.0, 255.0);\n",
                       channel_index, light_count_index);
            out->Write("\t\t\t\t\t\t\tcustom_data.light_chan{}_alpha_count += 1;\n", channel_index);
            out->Write("\t\t\t\t\t\t}}\n");
          }
        }

        out->Write("\t\t\t\t\t}}\n");
      }
      out->Write("\t\t\t\t}}\n");
      out->Write("\t\t\t}}\n");
      out->Write("\t\t}}\n");
    }
    out->Write("\t}}\n");
  }

  for (u32 i = 0; i < 16; i++)
  {
    // Shader compilation complains if every struct isn't initialized

    // Color Input
    for (u32 j = 0; j < 4; j++)
    {
      out->Write("\tcustom_data.tev_stages[{}].input_color[{}].input_type = "
                 "CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_UNUSED;\n",
                 i, j);
      out->Write("\tcustom_data.tev_stages[{}].input_color[{}].value = "
                 "float3(0, 0, 0);\n",
                 i, j);
    }

    // Alpha Input
    for (u32 j = 0; j < 4; j++)
    {
      out->Write("\tcustom_data.tev_stages[{}].input_alpha[{}].input_type = "
                 "CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_UNUSED;\n",
                 i, j);
      out->Write("\tcustom_data.tev_stages[{}].input_alpha[{}].value = "
                 "float(0);\n",
                 i, j);
    }

    // Texmap
    out->Write("\tcustom_data.tev_stages[{}].texmap = 0u;\n", i);

    // Output
    out->Write("\tcustom_data.tev_stages[{}].output_color = "
               "float4(0, 0, 0, 0);\n",
               i);
  }

  // Actual data will be filled out in the tev stage code, just set the
  // stage count for now
  out->Write("\tcustom_data.tev_stage_count = num_stages;\n");

  // Time
  out->Write("\tcustom_data.time_ms = time_ms;\n");
}
}  // namespace
PixelShaderUid GetPixelShaderUid()
{
  PixelShaderUid out;

  pixel_ubershader_uid_data* const uid_data = out.GetUidData();
  uid_data->num_texgens = xfmem.numTexGen.numTexGens;
  uid_data->early_depth = bpmem.GetEmulatedZ() == EmulatedZ::Early &&
                          (g_ActiveConfig.bFastDepthCalc ||
                           bpmem.alpha_test.TestResult() == AlphaTestResult::Undetermined) &&
                          !(bpmem.zmode.testenable && bpmem.genMode.zfreeze);
  uid_data->per_pixel_depth =
      (bpmem.ztex2.op != ZTexOp::Disabled && bpmem.GetEmulatedZ() == EmulatedZ::Late) ||
      (!g_ActiveConfig.bFastDepthCalc && bpmem.zmode.testenable && !uid_data->early_depth) ||
      (bpmem.zmode.testenable && bpmem.genMode.zfreeze);
  uid_data->uint_output = bpmem.blendmode.UseLogicOp();

  return out;
}

void ClearUnusedPixelShaderUidBits(APIType api_type, const ShaderHostConfig& host_config,
                                   PixelShaderUid* uid)
{
  pixel_ubershader_uid_data* const uid_data = uid->GetUidData();

  // With fbfetch, ubershaders always blend using that and don't use dual src
  if (host_config.backend_shader_framebuffer_fetch || !host_config.backend_dual_source_blend)
    uid_data->no_dual_src = 1;
  // Dual source is always enabled in the shader if this bug is not present
  else if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING))
    uid_data->no_dual_src = 0;

  // OpenGL and Vulkan convert implicitly normalized color outputs to their uint representation.
  // Therefore, it is not necessary to use a uint output on these backends. We also disable the
  // uint output when logic op is not supported (i.e. driver/device does not support D3D11.1).
  if (api_type != APIType::D3D || !host_config.backend_logic_op)
    uid_data->uint_output = 0;
}

ShaderCode GenPixelShader(APIType api_type, const ShaderHostConfig& host_config,
                          const pixel_ubershader_uid_data* uid_data,
                          const CustomPixelShaderContents& custom_details)
{
  const bool per_pixel_lighting = host_config.per_pixel_lighting;
  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool stereo = host_config.stereo;
  const bool use_framebuffer_fetch = host_config.backend_shader_framebuffer_fetch;
  const bool use_dual_source = host_config.backend_dual_source_blend && !uid_data->no_dual_src;
  const bool early_depth = uid_data->early_depth != 0;
  const bool per_pixel_depth = uid_data->per_pixel_depth != 0;
  const bool bounding_box = host_config.bounding_box;
  const u32 numTexgen = uid_data->num_texgens;
  ShaderCode out;

  ASSERT_MSG(VIDEO, !(use_dual_source && use_framebuffer_fetch),
             "If you're using framebuffer fetch, you shouldn't need dual source blend!");

  out.Write("// {}\n", *uid_data);
  WriteBitfieldExtractHeader(out, api_type, host_config);
  WritePixelShaderCommonHeader(out, api_type, host_config, bounding_box, custom_details);
  WriteCustomShaderStructDef(&out, numTexgen);
  for (std::size_t i = 0; i < custom_details.shaders.size(); i++)
  {
    const auto& shader_details = custom_details.shaders[i];
    out.Write(fmt::runtime(shader_details.custom_shader), i);
  }
  if (per_pixel_lighting)
    WriteLightingFunction(out);

#ifdef __APPLE__
  // Framebuffer fetch is only supported by Metal, so ensure that we're running Vulkan (MoltenVK)
  // if we want to use it.
  if (api_type == APIType::Vulkan || api_type == APIType::Metal)
  {
    if (use_dual_source)
    {
      out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 0) out vec4 ocol0;\n"
                "FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 1) out vec4 ocol1;\n");
    }
    else
    {
      // Metal doesn't support a single unified variable for both input and output,
      // so when using framebuffer fetch, we declare the input separately below.
      out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 {};\n",
                use_framebuffer_fetch ? "real_ocol0" : "ocol0");
    }

    if (use_framebuffer_fetch)
    {
      // Subpass inputs will be converted to framebuffer fetch by SPIRV-Cross.
      out.Write("INPUT_ATTACHMENT_BINDING(0, 0, 0) uniform subpassInput in_ocol0;\n");
    }
  }
  else
#endif
  {
    if (use_framebuffer_fetch)
    {
      out.Write("FRAGMENT_OUTPUT_LOCATION(0) FRAGMENT_INOUT vec4 real_ocol0;\n");
    }
    else
    {
      out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 0) out {} ocol0;\n",
                uid_data->uint_output ? "uvec4" : "vec4");
    }

    if (use_dual_source)
    {
      out.Write("{} out {} ocol1;\n", "FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 1)",
                uid_data->uint_output ? "uvec4" : "vec4");
    }
  }

  if (per_pixel_depth)
    out.Write("#define depth gl_FragDepth\n");

  if (host_config.backend_geometry_shaders)
  {
    out.Write("VARYING_LOCATION(0) in VertexData {{\n");
    GenerateVSOutputMembers(out, api_type, numTexgen, host_config,
                            GetInterpolationQualifier(msaa, ssaa, true, true), ShaderStage::Pixel);

    out.Write("}};\n\n");
    if (stereo && !host_config.backend_gl_layer_in_fs)
      out.Write("flat in int layer;");
  }
  else
  {
    // Let's set up attributes
    u32 counter = 0;
    out.Write("VARYING_LOCATION({}) {} in float4 colors_0;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    out.Write("VARYING_LOCATION({}) {} in float4 colors_1;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    for (u32 i = 0; i < numTexgen; ++i)
    {
      out.Write("VARYING_LOCATION({}) {} in float3 tex{};\n", counter++,
                GetInterpolationQualifier(msaa, ssaa), i);
    }
    if (!host_config.fast_depth_calc)
    {
      out.Write("VARYING_LOCATION({}) {} in float4 clipPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
    if (per_pixel_lighting)
    {
      out.Write("VARYING_LOCATION({}) {} in float3 Normal;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
      out.Write("VARYING_LOCATION({}) {} in float3 WorldPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
  }

  // Uniform index -> texture coordinates
  // Quirk: when the tex coord is not less than the number of tex gens (i.e. the tex coord does
  // not exist), then tex coord 0 is used (though sometimes glitchy effects happen on console).
  // This affects the Mario portrait in Luigi's Mansion, where the developers forgot to set
  // the number of tex gens to 2 (bug 11462).
  if (numTexgen > 0)
  {
    out.Write("int2 selectTexCoord(uint index");
    for (u32 i = 0; i < numTexgen; i++)
      out.Write(", int2 fixpoint_uv{}", i);
    out.Write(") {{\n");

    if (api_type == APIType::D3D)
    {
      out.Write("  switch (index) {{\n");
      for (u32 i = 0; i < numTexgen; i++)
      {
        out.Write("  case {}u:\n"
                  "    return fixpoint_uv{};\n",
                  i, i);
      }
      out.Write("  default:\n"
                "    return fixpoint_uv0;\n"
                "  }}\n");
    }
    else
    {
      out.Write("  if (index >= {}u) {{\n", numTexgen);
      out.Write("    return fixpoint_uv0;\n"
                "  }}\n");
      if (numTexgen > 4)
        out.Write("  if (index < 4u) {{\n");
      if (numTexgen > 2)
        out.Write("    if (index < 2u) {{\n");
      if (numTexgen > 1)
        out.Write("      return (index == 0u) ? fixpoint_uv0 : fixpoint_uv1;\n");
      else
        out.Write("      return fixpoint_uv0;\n");
      if (numTexgen > 2)
      {
        out.Write("    }} else {{\n");  // >= 2 < min(4, numTexgen)
        if (numTexgen > 3)
          out.Write("      return (index == 2u) ? fixpoint_uv2 : fixpoint_uv3;\n");
        else
          out.Write("      return fixpoint_uv2;\n");
        out.Write("    }}\n");
      }
      if (numTexgen > 4)
      {
        out.Write("  }} else {{\n");  // >= 4 < min(8, numTexgen)
        if (numTexgen > 6)
          out.Write("    if (index < 6u) {{\n");
        if (numTexgen > 5)
          out.Write("      return (index == 4u) ? fixpoint_uv4 : fixpoint_uv5;\n");
        else
          out.Write("      return fixpoint_uv4;\n");
        if (numTexgen > 6)
        {
          out.Write("    }} else {{\n");  // >= 6 < min(8, numTexgen)
          if (numTexgen > 7)
            out.Write("      return (index == 6u) ? fixpoint_uv6 : fixpoint_uv7;\n");
          else
            out.Write("      return fixpoint_uv6;\n");
          out.Write("    }}\n");
        }
        out.Write("  }}\n");
      }
    }

    out.Write("}}\n\n");

    out.Write("uint selectTexCoordIndex(uint texmap)");
    out.Write("{{\n");

    if (api_type == APIType::D3D)
    {
      out.Write("  switch (texmap) {{\n");
      for (u32 i = 0; i < numTexgen; i++)
      {
        out.Write("  case {}u:\n"
                  "    return {}u;\n",
                  i, i);
      }
      out.Write("  default:\n"
                "    return 0u;\n"
                "  }}\n");
    }
    else
    {
      out.Write("  if (texmap >= {}u) {{\n", numTexgen);
      out.Write("    return 0u;\n"
                "  }}\n");
      if (numTexgen > 4)
        out.Write("  if (texmap < 4u) {{\n");
      if (numTexgen > 2)
        out.Write("    if (texmap < 2u) {{\n");
      if (numTexgen > 1)
        out.Write("      return (texmap == 0u) ? 0u : 1u;\n");
      else
        out.Write("      return 0u;\n");
      if (numTexgen > 2)
      {
        out.Write("    }} else {{\n");  // >= 2 < min(4, numTexgen)
        if (numTexgen > 3)
          out.Write("      return (texmap == 2u) ? 2u : 3u;\n");
        else
          out.Write("      return 2u;\n");
        out.Write("    }}\n");
      }
      if (numTexgen > 4)
      {
        out.Write("  }} else {{\n");  // >= 4 < min(8, numTexgen)
        if (numTexgen > 6)
          out.Write("    if (texmap < 6u) {{\n");
        if (numTexgen > 5)
          out.Write("      return (texmap == 4u) ? 4u : 5u;\n");
        else
          out.Write("      return 4u;\n");
        if (numTexgen > 6)
        {
          out.Write("    }} else {{\n");  // >= 6 < min(8, numTexgen)
          if (numTexgen > 7)
            out.Write("      return (texmap == 6u) ? 6u : 7u;\n");
          else
            out.Write("      return 6u;\n");
          out.Write("    }}\n");
        }
        out.Write("  }}\n");
      }
    }

    out.Write("}}\n\n");
  }

  // =====================
  //   Texture Sampling
  // =====================

  if (host_config.backend_dynamic_sampler_indexing)
  {
    // Doesn't look like DirectX supports this. Oh well the code path is here just in case it
    // supports this in the future.
    out.Write("int4 sampleTextureWrapper(uint texmap, int2 uv, int layer) {{\n");
    out.Write("  return sampleTexture(texmap, samp[texmap], uv, layer);\n");
    out.Write("}}\n\n");
  }
  else
  {
    out.Write("int4 sampleTextureWrapper(uint sampler_num, int2 uv, int layer) {{\n"
              "  // This is messy, but DirectX, OpenGL 3.3, and OpenGL ES 3.0 don't support "
              "dynamic indexing of the sampler array\n"
              "  // With any luck the shader compiler will optimise this if the hardware supports "
              "dynamic indexing.\n"
              "  switch(sampler_num) {{\n");
    for (int i = 0; i < 8; i++)
    {
      out.Write("  case {0}u: return sampleTexture({0}u, samp[{0}u], uv, layer);\n", i);
    }
    out.Write("  }}\n"
              "}}\n\n");
  }

  // ======================
  //   Arbitrary Swizzling
  // ======================

  out.Write("int4 Swizzle(uint s, int4 color) {{\n"
            "  // AKA: Color Channel Swapping\n"
            "\n"
            "  int4 ret;\n");
  out.Write("  ret.r = color[{}];\n", BitfieldExtract<&TevKSel::swap_rb>("bpmem_tevksel(s * 2u)"));
  out.Write("  ret.g = color[{}];\n", BitfieldExtract<&TevKSel::swap_ga>("bpmem_tevksel(s * 2u)"));
  out.Write("  ret.b = color[{}];\n",
            BitfieldExtract<&TevKSel::swap_rb>("bpmem_tevksel(s * 2u + 1u)"));
  out.Write("  ret.a = color[{}];\n",
            BitfieldExtract<&TevKSel::swap_ga>("bpmem_tevksel(s * 2u + 1u)"));
  out.Write("  return ret;\n"
            "}}\n\n");

  // ======================
  //   Indirect Wrapping
  // ======================
  out.Write("int Wrap(int coord, uint mode) {{\n"
            "  if (mode == 0u) // ITW_OFF\n"
            "    return coord;\n"
            "  else if (mode < 6u) // ITW_256 to ITW_16\n"
            "    return coord & (0xfffe >> mode);\n"
            "  else // ITW_0\n"
            "    return 0;\n"
            "}}\n\n");

  // ======================
  //    Indirect Lookup
  // ======================
  const auto LookupIndirectTexture = [&out](std::string_view out_var_name,
                                            std::string_view in_index_name) {
    // in_index_name is the indirect stage, not the tev stage
    // bpmem_iref is packed differently from RAS1_IREF
    // This function assumes bpmem_iref is nonzero (i.e. matrix is not off, and the
    // indirect texture stage is enabled).
    out.Write("{{\n"
              "  uint iref = bpmem_iref({});\n"
              "  uint texcoord = bitfieldExtract(iref, 0, 3);\n"
              "  uint texmap = bitfieldExtract(iref, 8, 3);\n"
              "  int2 fixedPoint_uv = getTexCoord(texcoord);\n"
              "\n"
              "  if (({} & 1u) == 0u)\n"
              "    fixedPoint_uv = fixedPoint_uv >> " I_INDTEXSCALE "[{} >> 1].xy;\n"
              "  else\n"
              "    fixedPoint_uv = fixedPoint_uv >> " I_INDTEXSCALE "[{} >> 1].zw;\n"
              "\n"
              "  {} = sampleTextureWrapper(texmap, fixedPoint_uv, layer).abg;\n"
              "}}\n",
              in_index_name, in_index_name, in_index_name, in_index_name, out_var_name);
  };

  // ======================
  //   TEV's Special Lerp
  // ======================
  const auto WriteTevLerp = [&out](std::string_view components) {
    out.Write("// TEV's Linear Interpolate, plus bias, add/subtract and scale\n"
              "int{0} tevLerp{0}(int{0} A, int{0} B, int{0} C, int{0} D, uint bias, bool op, "
              "uint scale) {{\n"
              " // Scale C from 0..255 to 0..256\n"
              "  C += C >> 7;\n"
              "\n"
              " // Add bias to D\n"
              "  if (bias == 1u) D += 128;\n"
              "  else if (bias == 2u) D -= 128;\n"
              "\n"
              "  int{0} lerp = (A << 8) + (B - A)*C;\n"
              "  if (scale != 3u) {{\n"
              "    lerp = lerp << scale;\n"
              "    D = D << scale;\n"
              "  }}\n"
              "\n"
              "  // TODO: Is this rounding bias still added when the scale is divide by 2?  "
              "Currently we "
              "do not apply it.\n"
              "  if (scale != 3u)\n"
              "    lerp = lerp + (op ? 127 : 128);\n"
              "\n"
              "  int{0} result = lerp >> 8;\n"
              "\n"
              "  // Add/Subtract D\n"
              "  if (op) // Subtract\n"
              "    result = D - result;\n"
              "  else // Add\n"
              "    result = D + result;\n"
              "\n"
              "  // Most of the Scale was moved inside the lerp for improved precision\n"
              "  // But we still do the divide by 2 here\n"
              "  if (scale == 3u)\n"
              "    result = result >> 1;\n"
              "  return result;\n"
              "}}\n\n",
              components);
  };
  WriteTevLerp("");   // int
  WriteTevLerp("3");  // int3

  // =======================
  //   TEV's Color Compare
  // =======================

  out.Write(
      "// Implements operations 0-5 of TEV's compare mode,\n"
      "// which are common to both color and alpha channels\n"
      "bool tevCompare(uint op, int3 color_A, int3 color_B) {{\n"
      "  switch (op) {{\n"
      "  case 0u: // TevCompareMode::R8, TevComparison::GT\n"
      "    return (color_A.r > color_B.r);\n"
      "  case 1u: // TevCompareMode::R8, TevComparison::EQ\n"
      "    return (color_A.r == color_B.r);\n"
      "  case 2u: // TevCompareMode::GR16, TevComparison::GT\n"
      "    int A_16 = (color_A.r | (color_A.g << 8));\n"
      "    int B_16 = (color_B.r | (color_B.g << 8));\n"
      "    return A_16 > B_16;\n"
      "  case 3u: // TevCompareMode::GR16, TevComparison::EQ\n"
      "    return (color_A.r == color_B.r && color_A.g == color_B.g);\n"
      "  case 4u: // TevCompareMode::BGR24, TevComparison::GT\n"
      "    int A_24 = (color_A.r | (color_A.g << 8) | (color_A.b << 16));\n"
      "    int B_24 = (color_B.r | (color_B.g << 8) | (color_B.b << 16));\n"
      "    return A_24 > B_24;\n"
      "  case 5u: // TevCompareMode::BGR24, TevComparison::EQ\n"
      "    return (color_A.r == color_B.r && color_A.g == color_B.g && color_A.b == color_B.b);\n"
      "  default:\n"
      "    return false;\n"
      "  }}\n"
      "}}\n\n");

  // =================
  //   Input Selects
  // =================

  out.Write("struct State {{\n"
            "  int4 Reg[4];\n"
            "  int4 TexColor;\n"
            "  int AlphaBump;\n"
            "}};\n"
            "struct StageState {{\n"
            "  uint stage;\n"
            "  uint order;\n"
            "  uint cc;\n"
            "  uint ac;\n"
            "}};\n"
            "\n"
            "int4 getRasColor(State s, StageState ss, float4 colors_0, float4 colors_1);\n"
            "int4 getKonstColor(State s, StageState ss);\n"
            "\n");

  static constexpr Common::EnumMap<std::string_view, CompareMode::Always> tev_alpha_funcs_table{
      "return false;",   // CompareMode::Never
      "return a <  b;",  // CompareMode::Less
      "return a == b;",  // CompareMode::Equal
      "return a <= b;",  // CompareMode::LEqual
      "return a >  b;",  // CompareMode::Greater
      "return a != b;",  // CompareMode::NEqual
      "return a >= b;",  // CompareMode::GEqual
      "return true;"     // CompareMode::Always
  };

  static constexpr Common::EnumMap<std::string_view, TevColorArg::Zero> tev_c_input_table{
      "return s.Reg[0].rgb;",                                // CPREV,
      "return s.Reg[0].aaa;",                                // APREV,
      "return s.Reg[1].rgb;",                                // C0,
      "return s.Reg[1].aaa;",                                // A0,
      "return s.Reg[2].rgb;",                                // C1,
      "return s.Reg[2].aaa;",                                // A1,
      "return s.Reg[3].rgb;",                                // C2,
      "return s.Reg[3].aaa;",                                // A2,
      "return s.TexColor.rgb;",                              // TEXC,
      "return s.TexColor.aaa;",                              // TEXA,
      "return getRasColor(s, ss, colors_0, colors_1).rgb;",  // RASC,
      "return getRasColor(s, ss, colors_0, colors_1).aaa;",  // RASA,
      "return int3(255, 255, 255);",                         // ONE
      "return int3(128, 128, 128);",                         // HALF
      "return getKonstColor(s, ss).rgb;",                    // KONST
      "return int3(0, 0, 0);",                               // ZERO
  };

  static constexpr Common::EnumMap<std::string_view, TevColorArg::Zero> tev_c_input_type{
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_PREV;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_PREV;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_RAS;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_RAS;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_KONST;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC;",
  };

  static constexpr Common::EnumMap<std::string_view, TevAlphaArg::Zero> tev_a_input_table{
      "return s.Reg[0].a;",                                // APREV,
      "return s.Reg[1].a;",                                // A0,
      "return s.Reg[2].a;",                                // A1,
      "return s.Reg[3].a;",                                // A2,
      "return s.TexColor.a;",                              // TEXA,
      "return getRasColor(s, ss, colors_0, colors_1).a;",  // RASA,
      "return getKonstColor(s, ss).a;",                    // KONST,  (hw1 had quarter)
      "return 0;",                                         // ZERO
  };

  static constexpr Common::EnumMap<std::string_view, TevAlphaArg::Zero> tev_a_input_type{
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_PREV;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_COLOR;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_TEX;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_RAS;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_KONST;",
      "return CUSTOM_SHADER_TEV_STAGE_INPUT_TYPE_NUMERIC;",
  };

  static constexpr Common::EnumMap<std::string_view, TevOutput::Color2> tev_regs_lookup_table{
      "return s.Reg[0];",
      "return s.Reg[1];",
      "return s.Reg[2];",
      "return s.Reg[3];",
  };

  static constexpr Common::EnumMap<std::string_view, TevOutput::Color2> tev_c_set_table{
      "s.Reg[0].rgb = color;",
      "s.Reg[1].rgb = color;",
      "s.Reg[2].rgb = color;",
      "s.Reg[3].rgb = color;",
  };

  static constexpr Common::EnumMap<std::string_view, TevOutput::Color2> tev_a_set_table{
      "s.Reg[0].a = alpha;",
      "s.Reg[1].a = alpha;",
      "s.Reg[2].a = alpha;",
      "s.Reg[3].a = alpha;",
  };

  out.Write("// Helper function for Alpha Test\n"
            "bool alphaCompare(int a, int b, uint compare) {{\n");
  WriteSwitch(out, api_type, "compare", tev_alpha_funcs_table, 2, false);
  out.Write("}}\n"
            "\n"
            "int3 selectColorInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
            "uint index) {{\n");
  WriteSwitch(out, api_type, "index", tev_c_input_table, 2, false);
  out.Write("}}\n"
            "\n"
            "int selectAlphaInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
            "uint index) {{\n");
  WriteSwitch(out, api_type, "index", tev_a_input_table, 2, false);
  out.Write("}}\n"
            "\n"
            "int4 getTevReg(in State s, uint index) {{\n");
  WriteSwitch(out, api_type, "index", tev_regs_lookup_table, 2, false);
  out.Write("}}\n"
            "\n");

  out.Write("// Helper function for Custom Shader Input Type\n"
            "uint getColorInputType(uint index) {{\n");
  WriteSwitch(out, api_type, "index", tev_c_input_type, 2, false);
  out.Write("}}\n"
            "\n"
            "uint getAlphaInputType(uint index) {{\n");
  WriteSwitch(out, api_type, "index", tev_a_input_type, 2, false);
  out.Write("}}\n"
            "\n");

  // Since the fixed-point texture coodinate variables aren't global, we need to pass
  // them to the select function.  This applies to all backends.
  if (numTexgen > 0)
  {
    out.Write("#define getTexCoord(index) selectTexCoord((index)");
    for (u32 i = 0; i < numTexgen; i++)
      out.Write(", fixpoint_uv{}", i);
    out.Write(")\n\n");
  }

  if (early_depth && host_config.backend_early_z)
    out.Write("FORCE_EARLY_Z;\n");

  out.Write("void main()\n{{\n");
  out.Write("  float4 rawpos = gl_FragCoord;\n");

  out.Write("  uint num_stages = {};\n\n",
            BitfieldExtract<&GenMode::numtevstages>("bpmem_genmode"));

  bool has_custom_shader_details = false;
  if (std::ranges::any_of(custom_details.shaders, [](const std::optional<CustomPixelShader>& ps) {
        return ps.has_value();
      }))
  {
    WriteCustomShaderStructImpl(&out, numTexgen, per_pixel_lighting);
    has_custom_shader_details = true;
  }

  if (use_framebuffer_fetch)
  {
    // Store off a copy of the initial framebuffer value.
    //
    // If FB_FETCH_VALUE isn't defined (i.e. no special keyword for fetching from the
    // framebuffer), we read from real_ocol0.
    out.Write("#ifdef FB_FETCH_VALUE\n"
              "  float4 initial_ocol0 = FB_FETCH_VALUE;\n"
              "#else\n"
              "  float4 initial_ocol0 = real_ocol0;\n"
              "#endif\n");

    // QComm's Adreno driver doesn't seem to like using the framebuffer_fetch value as an
    // intermediate value with multiple reads & modifications, so we pull out the "real" output
    // value above and use a temporary for calculations, then set the output value once at the
    // end of the shader.
    out.Write("  float4 ocol0;\n"
              "  float4 ocol1;\n");
  }

  if (host_config.backend_geometry_shaders && stereo)
  {
    if (host_config.backend_gl_layer_in_fs)
      out.Write("\tint layer = gl_Layer;\n");
  }
  else
  {
    out.Write("\tint layer = 0;\n");
  }

  out.Write("  int3 tevcoord = int3(0, 0, 0);\n"
            "  State s;\n"
            "  s.TexColor = int4(0, 0, 0, 0);\n"
            "  s.AlphaBump = 0;\n"
            "\n");
  for (int i = 0; i < 4; i++)
    out.Write("  s.Reg[{}] = " I_COLORS "[{}];\n", i, i);

  const char* color_input_prefix = "";
  if (per_pixel_lighting)
  {
    out.Write("  float4 lit_colors_0 = colors_0;\n"
              "  float4 lit_colors_1 = colors_1;\n"
              "  float3 lit_normal = normalize(Normal.xyz);\n"
              "  float3 lit_pos = WorldPos.xyz;\n");
    WriteVertexLighting(out, api_type, "lit_pos", "lit_normal", "colors_0", "colors_1",
                        "lit_colors_0", "lit_colors_1");
    color_input_prefix = "lit_";
    out.Write("  // The number of colors available to TEV is determined by numColorChans.\n"
              "  // Normally this is performed in the vertex shader after lighting,\n"
              "  // but with per-pixel lighting, we need to perform it here.\n"
              "  // TODO: Actually implement this for ubershaders\n"
              "  // if (xfmem_numColorChans == 0u)\n"
              "  //   o.colors_0 = float4(0.0, 0.0, 0.0, 0.0);\n"
              "  // if (xfmem_numColorChans <= 1u)\n"
              "  //   o.colors_1 = float4(0.0, 0.0, 0.0, 0.0);\n");
  }

  out.Write("  // Main tev loop\n");

  out.Write("  for(uint stage = 0u; stage <= num_stages; stage++)\n"
            "  {{\n"
            "    StageState ss;\n"
            "    ss.stage = stage;\n"
            "    ss.cc = bpmem_combiners(stage).x;\n"
            "    ss.ac = bpmem_combiners(stage).y;\n"
            "    ss.order = bpmem_tevorder(stage>>1);\n"
            "    if ((stage & 1u) == 1u)\n"
            "      ss.order = ss.order >> {};\n\n",
            int(TwoTevStageOrders().enable_tex_odd.StartBit() -
                TwoTevStageOrders().enable_tex_even.StartBit()));

  // Disable texturing when there are no texgens (for now)
  if (numTexgen != 0)
  {
    for (u32 i = 0; i < numTexgen; i++)
    {
      out.Write("    int2 fixpoint_uv{} = int2(", i);
      out.Write("(tex{}.z == 0.0 ? tex{}.xy : tex{}.xy / tex{}.z)", i, i, i, i);
      out.Write(" * float2(" I_TEXDIMS "[{}].zw * 128));\n", i);
      // TODO: S24 overflows here?
    }

    out.Write("\n"
              "    uint tex_coord = {};\n",
              BitfieldExtract<&TwoTevStageOrders::texcoord_even>("ss.order"));
    out.Write("    int2 fixedPoint_uv = getTexCoord(tex_coord);\n"
              "\n"
              "    bool texture_enabled = (ss.order & {}u) != 0u;\n",
              1 << TwoTevStageOrders().enable_tex_even.StartBit());
    out.Write("\n"
              "    // Indirect textures\n"
              "    uint tevind = bpmem_tevind(stage);\n"
              "    if (tevind != 0u)\n"
              "    {{\n"
              "      uint bs = {};\n",
              BitfieldExtract<&TevStageIndirect::bs>("tevind"));
    out.Write("      uint fmt = {};\n", BitfieldExtract<&TevStageIndirect::fmt>("tevind"));
    out.Write("      uint bias = {};\n", BitfieldExtract<&TevStageIndirect::bias>("tevind"));
    out.Write("      uint bt = {};\n", BitfieldExtract<&TevStageIndirect::bt>("tevind"));
    out.Write("      uint matrix_index = {};\n",
              BitfieldExtract<&TevStageIndirect::matrix_index>("tevind"));
    out.Write("      uint matrix_id = {};\n",
              BitfieldExtract<&TevStageIndirect::matrix_id>("tevind"));
    out.Write("      int2 indtevtrans = int2(0, 0);\n"
              "\n");
    // There is always a bit set in bpmem_iref if the data is valid (matrix is not off, and the
    // indirect texture stage is enabled). If the matrix is off, the result doesn't matter; if the
    // indirect texture stage is disabled, the result is undefined (and produces a glitchy pattern
    // on hardware, different from this).
    // For the undefined case, we just skip applying the indirect operation, which is close
    // enough. Viewtiful Joe hits the undefined case (bug 12525). Wrapping and add to previous
    // still apply in this case (and when the stage is disabled).
    out.Write("      if (bpmem_iref(bt) != 0u) {{\n");
    out.Write("        int3 indcoord;\n");
    LookupIndirectTexture("indcoord", "bt");
    out.Write("        if (bs != 0u)\n"
              "          s.AlphaBump = indcoord[bs - 1u];\n"
              "        switch(fmt)\n"
              "        {{\n"
              "        case {:s}:\n",
              IndTexFormat::ITF_8);
    out.Write("          indcoord.x = indcoord.x + ((bias & 1u) != 0u ? -128 : 0);\n"
              "          indcoord.y = indcoord.y + ((bias & 2u) != 0u ? -128 : 0);\n"
              "          indcoord.z = indcoord.z + ((bias & 4u) != 0u ? -128 : 0);\n"
              "          s.AlphaBump = s.AlphaBump & 0xf8;\n"
              "          break;\n"
              "        case {:s}:\n",
              IndTexFormat::ITF_5);
    out.Write("          indcoord.x = (indcoord.x >> 3) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "          indcoord.y = (indcoord.y >> 3) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "          indcoord.z = (indcoord.z >> 3) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "          s.AlphaBump = s.AlphaBump << 5;\n"
              "          break;\n"
              "        case {:s}:\n",
              IndTexFormat::ITF_4);
    out.Write("          indcoord.x = (indcoord.x >> 4) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "          indcoord.y = (indcoord.y >> 4) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "          indcoord.z = (indcoord.z >> 4) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "          s.AlphaBump = s.AlphaBump << 4;\n"
              "          break;\n"
              "        case {:s}:\n",
              IndTexFormat::ITF_3);
    out.Write("          indcoord.x = (indcoord.x >> 5) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "          indcoord.y = (indcoord.y >> 5) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "          indcoord.z = (indcoord.z >> 5) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "          s.AlphaBump = s.AlphaBump << 3;\n"
              "          break;\n"
              "        }}\n"
              "\n"
              "        // Matrix multiply\n"
              "        if (matrix_index != 0u)\n"
              "        {{\n"
              "          uint mtxidx = 2u * (matrix_index - 1u);\n"
              "          int shift = " I_INDTEXMTX "[mtxidx].w;\n"
              "\n"
              "          switch (matrix_id)\n"
              "          {{\n"
              "          case 0u: // 3x2 S0.10 matrix\n"
              "            indtevtrans = int2(idot(" I_INDTEXMTX
              "[mtxidx].xyz, indcoord), idot(" I_INDTEXMTX "[mtxidx + 1u].xyz, indcoord)) >> 3;\n"
              "            break;\n"
              "          case 1u: // S matrix, S17.7 format\n"
              "            indtevtrans = (fixedPoint_uv * indcoord.xx) >> 8;\n"
              "            break;\n"
              "          case 2u: // T matrix, S17.7 format\n"
              "            indtevtrans = (fixedPoint_uv * indcoord.yy) >> 8;\n"
              "            break;\n"
              "          }}\n"
              "\n"
              "          if (shift >= 0)\n"
              "            indtevtrans = indtevtrans >> shift;\n"
              "          else\n"
              "            indtevtrans = indtevtrans << ((-shift) & 31);\n"
              "        }}\n"
              "      }}\n"
              "\n"
              "      // Wrapping\n"
              "      uint sw = {};\n",
              BitfieldExtract<&TevStageIndirect::sw>("tevind"));
    out.Write("      uint tw = {}; \n", BitfieldExtract<&TevStageIndirect::tw>("tevind"));
    out.Write(
        "      int2 wrapped_coord = int2(Wrap(fixedPoint_uv.x, sw), Wrap(fixedPoint_uv.y, tw));\n"
        "\n"
        "      if ((tevind & {}u) != 0u) // add previous tevcoord\n",
        1 << TevStageIndirect().fb_addprev.StartBit());
    out.Write("        tevcoord.xy += wrapped_coord + indtevtrans;\n"
              "      else\n"
              "        tevcoord.xy = wrapped_coord + indtevtrans;\n"
              "\n"
              "      // Emulate s24 overflows\n"
              "      tevcoord.xy = (tevcoord.xy << 8) >> 8;\n"
              "    }}\n"
              "    else\n"
              "    {{\n"
              "      tevcoord.xy = fixedPoint_uv;\n"
              "    }}\n"
              "\n"
              "    // Sample texture for stage\n"
              "    if (texture_enabled) {{\n"
              "      uint sampler_num = {};\n",
              BitfieldExtract<&TwoTevStageOrders::texmap_even>("ss.order"));
    out.Write("\n"
              "      int4 color = sampleTextureWrapper(sampler_num, tevcoord.xy, layer);\n"
              "      uint swap = {};\n",
              BitfieldExtract<&TevStageCombiner::AlphaCombiner::tswap>("ss.ac"));
    out.Write("      s.TexColor = Swizzle(swap, color);\n");
    out.Write("    }} else {{\n"
              "      // Texture is disabled\n"
              "      s.TexColor = int4(255, 255, 255, 255);\n"
              "    }}\n"
              "\n");
  }

  out.Write("    // This is the Meat of TEV\n"
            "    {{\n"
            "      // Color Combiner\n");
  out.Write("      uint color_a = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::a>("ss.cc"));
  out.Write("      uint color_b = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::b>("ss.cc"));
  out.Write("      uint color_c = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::c>("ss.cc"));
  out.Write("      uint color_d = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::d>("ss.cc"));

  out.Write("      uint color_bias = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::bias>("ss.cc"));
  out.Write("      bool color_op = bool({});\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::op>("ss.cc"));
  out.Write("      bool color_clamp = bool({});\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::clamp>("ss.cc"));
  out.Write("      uint color_scale = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::scale>("ss.cc"));
  out.Write("      uint color_dest = {};\n",
            BitfieldExtract<&TevStageCombiner::ColorCombiner::dest>("ss.cc"));

  out.Write(
      "      uint color_compare_op = color_scale << 1 | uint(color_op);\n"
      "\n"
      "      int3 color_A = selectColorInput(s, ss, {0}colors_0, {0}colors_1, color_a) & "
      "int3(255, 255, 255);\n"
      "      int3 color_B = selectColorInput(s, ss, {0}colors_0, {0}colors_1, color_b) & "
      "int3(255, 255, 255);\n"
      "      int3 color_C = selectColorInput(s, ss, {0}colors_0, {0}colors_1, color_c) & "
      "int3(255, 255, 255);\n"
      "      int3 color_D = selectColorInput(s, ss, {0}colors_0, {0}colors_1, color_d);  // 10 "
      "bits + sign\n"
      "\n",  // TODO: do we need to sign extend?
      color_input_prefix);
  out.Write(
      "      int3 color;\n"
      "      if (color_bias != 3u) {{ // Normal mode\n"
      "        color = tevLerp3(color_A, color_B, color_C, color_D, color_bias, color_op, "
      "color_scale);\n"
      "      }} else {{ // Compare mode\n"
      "        // op 6 and 7 do a select per color channel\n"
      "        if (color_compare_op == 6u) {{\n"
      "          // TevCompareMode::RGB8, TevComparison::GT\n"
      "          color.r = (color_A.r > color_B.r) ? color_C.r : 0;\n"
      "          color.g = (color_A.g > color_B.g) ? color_C.g : 0;\n"
      "          color.b = (color_A.b > color_B.b) ? color_C.b : 0;\n"
      "        }} else if (color_compare_op == 7u) {{\n"
      "          // TevCompareMode::RGB8, TevComparison::EQ\n"
      "          color.r = (color_A.r == color_B.r) ? color_C.r : 0;\n"
      "          color.g = (color_A.g == color_B.g) ? color_C.g : 0;\n"
      "          color.b = (color_A.b == color_B.b) ? color_C.b : 0;\n"
      "        }} else {{\n"
      "          // The remaining ops do one compare which selects all 3 channels\n"
      "          color = tevCompare(color_compare_op, color_A, color_B) ? color_C : int3(0, 0, "
      "0);\n"
      "        }}\n"
      "        color = color_D + color;\n"
      "      }}\n"
      "\n"
      "      // Clamp result\n"
      "      if (color_clamp)\n"
      "        color = clamp(color, 0, 255);\n"
      "      else\n"
      "        color = clamp(color, -1024, 1023);\n"
      "\n"
      "      // Write result to the correct input register of the next stage\n");
  WriteSwitch(out, api_type, "color_dest", tev_c_set_table, 6, true);
  out.Write("\n");

  // Alpha combiner
  out.Write("      // Alpha Combiner\n");
  out.Write("      uint alpha_a = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::a>("ss.ac"));
  out.Write("      uint alpha_b = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::b>("ss.ac"));
  out.Write("      uint alpha_c = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::c>("ss.ac"));
  out.Write("      uint alpha_d = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::d>("ss.ac"));

  out.Write("      uint alpha_bias = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::bias>("ss.ac"));
  out.Write("      bool alpha_op = bool({});\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::op>("ss.ac"));
  out.Write("      bool alpha_clamp = bool({});\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::clamp>("ss.ac"));
  out.Write("      uint alpha_scale = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::scale>("ss.ac"));
  out.Write("      uint alpha_dest = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::dest>("ss.ac"));

  out.Write(
      "      uint alpha_compare_op = alpha_scale << 1 | uint(alpha_op);\n"
      "\n"
      "      int alpha_A = 0;\n"
      "      int alpha_B = 0;\n"
      "      if (alpha_bias != 3u || alpha_compare_op > 5u) {{\n"
      "        // Small optimisation here: alpha_A and alpha_B are unused by compare ops 0-5\n"
      "        alpha_A = selectAlphaInput(s, ss, {0}colors_0, {0}colors_1, alpha_a) & 255;\n"
      "        alpha_B = selectAlphaInput(s, ss, {0}colors_0, {0}colors_1, alpha_b) & 255;\n"
      "      }};\n"
      "      int alpha_C = selectAlphaInput(s, ss, {0}colors_0, {0}colors_1, alpha_c) & 255;\n"
      "      int alpha_D = selectAlphaInput(s, ss, {0}colors_0, {0}colors_1, alpha_d); // 10 "
      "bits "
      "+ sign\n"
      "\n",  // TODO: do we need to sign extend?
      color_input_prefix);
  out.Write("\n"
            "      int alpha;\n"
            "      if (alpha_bias != 3u) {{ // Normal mode\n"
            "        alpha = tevLerp(alpha_A, alpha_B, alpha_C, alpha_D, alpha_bias, alpha_op, "
            "alpha_scale);\n"
            "      }} else {{ // Compare mode\n"
            "        if (alpha_compare_op == 6u) {{\n"
            "          // TevCompareMode::A8, TevComparison::GT\n"
            "          alpha = (alpha_A > alpha_B) ? alpha_C : 0;\n"
            "        }} else if (alpha_compare_op == 7u) {{\n"
            "          // TevCompareMode::A8, TevComparison::EQ\n"
            "          alpha = (alpha_A == alpha_B) ? alpha_C : 0;\n"
            "        }} else {{\n"
            "          // All remaining alpha compare ops actually compare the color channels\n"
            "          alpha = tevCompare(alpha_compare_op, color_A, color_B) ? alpha_C : 0;\n"
            "        }}\n"
            "        alpha = alpha_D + alpha;\n"
            "      }}\n"
            "\n"
            "      // Clamp result\n"
            "      if (alpha_clamp)\n"
            "        alpha = clamp(alpha, 0, 255);\n"
            "      else\n"
            "        alpha = clamp(alpha, -1024, 1023);\n"
            "\n"
            "      // Write result to the correct input register of the next stage\n");
  WriteSwitch(out, api_type, "alpha_dest", tev_a_set_table, 6, true);
  if (has_custom_shader_details)
  {
    for (u32 stage_index = 0; stage_index < 16; stage_index++)
    {
      out.Write("\tif (stage == {}u) {{\n", stage_index);
      // Color input
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[0].value = color_A / float3(255.0, "
                "255.0, 255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[0].input_type = "
                "getColorInputType(color_a);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[1].value = color_B / float3(255.0, "
                "255.0, 255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[1].input_type = "
                "getColorInputType(color_b);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[2].value = color_C / float3(255.0, "
                "255.0, 255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[2].input_type = "
                "getColorInputType(color_c);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[3].value = color_D / float3(255.0, "
                "255.0, 255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_color[3].input_type = "
                "getColorInputType(color_c);\n",
                stage_index);

      // Alpha input
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[0].value = alpha_A / float(255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[0].input_type = "
                "getAlphaInputType(alpha_a);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[1].value = alpha_B / float(255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[1].input_type = "
                "getAlphaInputType(alpha_b);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[2].value = alpha_C / float(255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[2].input_type = "
                "getAlphaInputType(alpha_c);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[3].value = alpha_D / float(255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].input_alpha[3].input_type = "
                "getAlphaInputType(alpha_d);\n",
                stage_index);

      if (numTexgen != 0)
      {
        // Texmap
        out.Write("\t\tif (texture_enabled) {{\n");
        out.Write("\t\t\tuint sampler_num = {};\n",
                  BitfieldExtract<&TwoTevStageOrders::texmap_even>("ss.order"));
        out.Write("\t\tcustom_data.tev_stages[{}].texmap = sampler_num;\n", stage_index);
        out.Write("\t\t}}\n");
      }

      // Output
      out.Write("\t\tcustom_data.tev_stages[{}].output_color.rgb = color / float3(255.0, 255.0, "
                "255.0);\n",
                stage_index);
      out.Write("\t\tcustom_data.tev_stages[{}].output_color.a = alpha / float(255.0);\n",
                stage_index);
      out.Write("\t}}\n");
    }
  }
  out.Write("    }}\n");
  out.Write("    }} // Main TEV loop\n");
  out.Write("\n");

  // Select the output color and alpha registers from the last stage.
  out.Write("  int4 TevResult;\n");
  out.Write(
      "  TevResult.xyz = getTevReg(s, {}).xyz;\n",
      BitfieldExtract<&TevStageCombiner::ColorCombiner::dest>("bpmem_combiners(num_stages).x"));
  out.Write(
      "  TevResult.w = getTevReg(s, {}).w;\n",
      BitfieldExtract<&TevStageCombiner::AlphaCombiner::dest>("bpmem_combiners(num_stages).y"));

  out.Write("  TevResult &= 255;\n\n");

  if (host_config.fast_depth_calc)
  {
    if (!host_config.backend_reversed_depth_range)
      out.Write("  int zCoord = int((1.0 - rawpos.z) * 16777216.0);\n");
    else
      out.Write("  int zCoord = int(rawpos.z * 16777216.0);\n");
    out.Write("  zCoord = clamp(zCoord, 0, 0xFFFFFF);\n"
              "\n");
  }
  else
  {
    out.Write("\tint zCoord = " I_ZBIAS "[1].x + int((clipPos.z / clipPos.w) * float(" I_ZBIAS
              "[1].y));\n");
  }

  // ===========
  //   ZFreeze
  // ===========

  if (per_pixel_depth)
  {
    // Zfreeze forces early depth off
    out.Write("  // ZFreeze\n"
              "  if ((bpmem_genmode & {}u) != 0u) {{\n",
              1 << GenMode().zfreeze.StartBit());
    out.Write("    float2 screenpos = rawpos.xy * " I_EFBSCALE ".xy;\n");
    if (api_type == APIType::OpenGL)
    {
      out.Write("    // OpenGL has reversed vertical screenspace coordinates\n"
                "    screenpos.y = 528.0 - screenpos.y;\n");
    }
    out.Write("    zCoord = int(" I_ZSLOPE ".z + " I_ZSLOPE ".x * screenpos.x + " I_ZSLOPE
              ".y * screenpos.y);\n"
              " }}\n"
              "\n");
  }

  // =================
  //   Depth Texture
  // =================

  out.Write("  // Depth Texture\n"
            "  int early_zCoord = zCoord;\n"
            "  if (bpmem_ztex_op != 0u) {{\n"
            "    int ztex = int(" I_ZBIAS "[1].w); // fixed bias\n"
            "\n"
            "    // Whatever texture was in our last stage, it's now our depth texture\n"
            "    ztex += idot(s.TexColor.xyzw, " I_ZBIAS "[0].xyzw);\n"
            "    ztex += (bpmem_ztex_op == 1u) ? zCoord : 0;\n"
            "    zCoord = ztex & 0xFFFFFF;\n"
            "  }}\n"
            "\n");

  if (per_pixel_depth)
  {
    out.Write("  // If early depth is enabled, write to zbuffer before depth textures\n"
              "  // If early depth isn't enabled, we write to the zbuffer here\n"
              "  int zbuffer_zCoord = bpmem_late_ztest ? zCoord : early_zCoord;\n");
    if (!host_config.backend_reversed_depth_range)
      out.Write("  depth = 1.0 - float(zbuffer_zCoord) / 16777216.0;\n");
    else
      out.Write("  depth = float(zbuffer_zCoord) / 16777216.0;\n");
  }

  out.Write("  // Alpha Test\n");

  if (early_depth && DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DISCARD_WITH_EARLY_Z))
  {
    // Instead of using discard, fetch the framebuffer's color value and use it as the output
    // for this fragment.
    out.Write("  #define discard_fragment {{ real_ocol0 = float4(initial_ocol0.xyz, 1.0); "
              "return; }}\n");
  }
  else
  {
    out.Write("  #define discard_fragment discard\n");
  }

  out.Write("  if (bpmem_alphaTest != 0u) {{\n"
            "    bool comp0 = alphaCompare(TevResult.a, " I_ALPHA ".r, {});\n",
            BitfieldExtract<&AlphaTest::comp0>("bpmem_alphaTest"));
  out.Write("    bool comp1 = alphaCompare(TevResult.a, " I_ALPHA ".g, {});\n",
            BitfieldExtract<&AlphaTest::comp1>("bpmem_alphaTest"));
  out.Write("\n"
            "    // These if statements are written weirdly to work around intel and Qualcomm "
            "bugs with handling booleans.\n"
            "    switch ({}) {{\n",
            BitfieldExtract<&AlphaTest::logic>("bpmem_alphaTest"));
  out.Write("    case 0u: // AND\n"
            "      if (comp0 && comp1) break; else discard_fragment; break;\n"
            "    case 1u: // OR\n"
            "      if (comp0 || comp1) break; else discard_fragment; break;\n"
            "    case 2u: // XOR\n"
            "      if (comp0 != comp1) break; else discard_fragment; break;\n"
            "    case 3u: // XNOR\n"
            "      if (comp0 == comp1) break; else discard_fragment; break;\n"
            "    }}\n"
            "  }}\n"
            "\n");

  out.Write("  // Hardware testing indicates that an alpha of 1 can pass an alpha test,\n"
            "  // but doesn't do anything in blending\n"
            "  if (TevResult.a == 1) TevResult.a = 0;\n");
  // =========
  // Dithering
  // =========
  out.Write("  if (bpmem_dither) {{\n"
            "    // Flipper uses a standard 2x2 Bayer Matrix for 6 bit dithering\n"
            "    // Here the matrix is encoded into the two factor constants\n"
            "    int2 dither = int2(rawpos.xy) & 1;\n"
            "    TevResult.rgb = (TevResult.rgb - (TevResult.rgb >> 6)) + abs(dither.y * 3 - "
            "dither.x * 2);\n"
            "  }}\n\n");

  // =========
  //    Fog
  // =========

  // FIXME: Fog is implemented the same as ShaderGen, but ShaderGen's fog is all hacks.
  //        Should be fixed point, and should not make guesses about Range-Based adjustments.
  out.Write("  // Fog\n"
            "  uint fog_function = {};\n",
            BitfieldExtract<&FogParam3::fsel>("bpmem_fogParam3"));
  out.Write("  if (fog_function != {:s}) {{\n", FogType::Off);
  out.Write("    // TODO: This all needs to be converted from float to fixed point\n"
            "    float ze;\n"
            "    if ({} == 0u) {{\n",
            BitfieldExtract<&FogParam3::proj>("bpmem_fogParam3"));
  out.Write("      // perspective\n"
            "      // ze = A/(B - (Zs >> B_SHF)\n"
            "      ze = (" I_FOGF ".x * 16777216.0) / float(" I_FOGI ".y - (zCoord >> " I_FOGI
            ".w));\n"
            "    }} else {{\n"
            "      // orthographic\n"
            "      // ze = a*Zs    (here, no B_SHF)\n"
            "      ze = " I_FOGF ".x * float(zCoord) / 16777216.0;\n"
            "    }}\n"
            "\n"
            "    if (bool({})) {{\n",
            BitfieldExtract<&FogRangeParams::RangeBase::Enabled>("bpmem_fogRangeBase"));
  out.Write("      // x_adjust = sqrt((x-center)^2 + k^2)/k\n"
            "      // ze *= x_adjust\n"
            "      float offset = (2.0 * (rawpos.x / " I_FOGF ".w)) - 1.0 - " I_FOGF ".z;\n"
            "      float floatindex = clamp(9.0 - abs(offset) * 9.0, 0.0, 9.0);\n"
            "      uint indexlower = uint(floatindex);\n"
            "      uint indexupper = indexlower + 1u;\n"
            "      float klower = " I_FOGRANGE "[indexlower >> 2u][indexlower & 3u];\n"
            "      float kupper = " I_FOGRANGE "[indexupper >> 2u][indexupper & 3u];\n"
            "      float k = lerp(klower, kupper, frac(floatindex));\n"
            "      float x_adjust = sqrt(offset * offset + k * k) / k;\n"
            "      ze *= x_adjust;\n"
            "    }}\n"
            "\n"
            "    float fog = clamp(ze - " I_FOGF ".y, 0.0, 1.0);\n"
            "\n");
  out.Write("    if (fog_function >= {:s}) {{\n", FogType::Exp);
  out.Write("      switch (fog_function) {{\n"
            "      case {:s}:\n"
            "        fog = 1.0 - exp2(-8.0 * fog);\n"
            "        break;\n",
            FogType::Exp);
  out.Write("      case {:s}:\n"
            "        fog = 1.0 - exp2(-8.0 * fog * fog);\n"
            "        break;\n",
            FogType::ExpSq);
  out.Write("      case {:s}:\n"
            "        fog = exp2(-8.0 * (1.0 - fog));\n"
            "        break;\n",
            FogType::BackwardsExp);
  out.Write("      case {:s}:\n"
            "        fog = 1.0 - fog;\n"
            "        fog = exp2(-8.0 * fog * fog);\n"
            "        break;\n",
            FogType::BackwardsExpSq);
  out.Write("      }}\n"
            "    }}\n"
            "\n"
            "    int ifog = iround(fog * 256.0);\n"
            "    TevResult.rgb = (TevResult.rgb * (256 - ifog) + " I_FOGCOLOR ".rgb * ifog) >> 8;\n"
            "  }}\n"
            "\n");

  for (std::size_t i = 0; i < custom_details.shaders.size(); i++)
  {
    const auto& shader_details = custom_details.shaders[i];

    if (!shader_details.custom_shader.empty())
    {
      out.Write("\t{{\n");
      out.Write("\t\tcustom_data.final_color = float4(TevResult.r / 255.0, TevResult.g / 255.0, "
                "TevResult.b / 255.0, TevResult.a / 255.0);\n");
      out.Write("\t\tCustomShaderOutput custom_output = {}_{}(custom_data);\n",
                CUSTOM_PIXELSHADER_COLOR_FUNC, i);
      out.Write(
          "\t\tTevResult = int4(custom_output.main_rt.r * 255, custom_output.main_rt.g * 255, "
          "custom_output.main_rt.b * 255, custom_output.main_rt.a * 255);\n");
      out.Write("\t}}\n\n");
    }
  }

  if (use_framebuffer_fetch)
  {
    static constexpr std::array<const char*, 16> logic_op_mode{
        "int4(0, 0, 0, 0)",          // CLEAR
        "TevResult & fb_value",      // AND
        "TevResult & ~fb_value",     // AND_REVERSE
        "TevResult",                 // COPY
        "~TevResult & fb_value",     // AND_INVERTED
        "fb_value",                  // NOOP
        "TevResult ^ fb_value",      // XOR
        "TevResult | fb_value",      // OR
        "~(TevResult | fb_value)",   // NOR
        "~(TevResult ^ fb_value)",   // EQUIV
        "~fb_value",                 // INVERT
        "TevResult | ~fb_value",     // OR_REVERSE
        "~TevResult",                // COPY_INVERTED
        "~TevResult | fb_value",     // OR_INVERTED
        "~(TevResult & fb_value)",   // NAND
        "int4(255, 255, 255, 255)",  // SET
    };

    out.Write("  // Logic Ops\n"
              "  if (logic_op_enable) {{\n"
              "    int4 fb_value = iround(initial_ocol0 * 255.0);"
              "    switch (logic_op_mode) {{\n");
    for (size_t i = 0; i < logic_op_mode.size(); i++)
    {
      out.Write("      case {}u: TevResult = {}; break;\n", i, logic_op_mode[i]);
    }

    out.Write("    }}\n"
              "    TevResult &= 0xff;\n"
              "  }}\n");
  }
  else if (!host_config.backend_logic_op)
  {
    out.Write("  // Helpers for logic op blending approximations\n"
              "  if (logic_op_enable) {{\n"
              "    switch (logic_op_mode) {{\n");
    out.Write("      case {}: // Clear\n", static_cast<u32>(LogicOp::Clear));
    out.Write("        TevResult = int4(0, 0, 0, 0);\n"
              "        break;\n");
    out.Write("      case {}: // Copy Inverted\n", static_cast<u32>(LogicOp::CopyInverted));
    out.Write("        TevResult ^= 0xff;\n"
              "        break;\n");
    out.Write("      case {}: // Set\n", static_cast<u32>(LogicOp::Set));
    out.Write("      case {}: // Invert\n", static_cast<u32>(LogicOp::Invert));
    out.Write("        TevResult = int4(255, 255, 255, 255);\n"
              "        break;\n");
    out.Write("      default:\n"
              "        break;\n"
              "    }}\n"
              "  }}\n");
  }

  // Some backends require that the shader outputs be uint when writing to a uint render target
  // for logic op.
  if (uid_data->uint_output)
  {
    out.Write("  if (bpmem_rgba6_format)\n"
              "    ocol0 = uint4(TevResult & 0xFC);\n"
              "  else\n"
              "    ocol0 = uint4(TevResult);\n"
              "\n");
  }
  else
  {
    out.Write("  if (bpmem_rgba6_format)\n"
              "    ocol0.rgb = float3(TevResult.rgb >> 2) / 63.0;\n"
              "  else\n"
              "    ocol0.rgb = float3(TevResult.rgb) / 255.0;\n"
              "\n"
              "  if (bpmem_dstalpha != 0u)\n");
    out.Write("    ocol0.a = float({} >> 2) / 63.0;\n",
              BitfieldExtract<&ConstantAlpha::alpha>("bpmem_dstalpha"));
    out.Write("  else\n"
              "    ocol0.a = float(TevResult.a >> 2) / 63.0;\n"
              "  \n");

    if (use_dual_source || use_framebuffer_fetch)
    {
      out.Write("  // Dest alpha override (dual source blending)\n"
                "  // Colors will be blended against the alpha from ocol1 and\n"
                "  // the alpha from ocol0 will be written to the framebuffer.\n"
                "  ocol1 = float4(0.0, 0.0, 0.0, float(TevResult.a) / 255.0);\n");
    }
  }

  if (bounding_box)
  {
    out.Write("  if (bpmem_bounding_box) {{\n"
              "    UpdateBoundingBox(rawpos.xy);\n"
              "  }}\n");
  }

  if (use_framebuffer_fetch)
  {
    using Common::EnumMap;

    static constexpr EnumMap<std::string_view, SrcBlendFactor::InvDstAlpha> blendSrcFactor{
        "blend_src.rgb = float3(0,0,0);",                      // ZERO
        "blend_src.rgb = float3(1,1,1);",                      // ONE
        "blend_src.rgb = initial_ocol0.rgb;",                  // DSTCLR
        "blend_src.rgb = float3(1,1,1) - initial_ocol0.rgb;",  // INVDSTCLR
        "blend_src.rgb = src_color.aaa;",                      // SRCALPHA
        "blend_src.rgb = float3(1,1,1) - src_color.aaa;",      // INVSRCALPHA
        "blend_src.rgb = initial_ocol0.aaa;",                  // DSTALPHA
        "blend_src.rgb = float3(1,1,1) - initial_ocol0.aaa;",  // INVDSTALPHA
    };
    static constexpr EnumMap<std::string_view, SrcBlendFactor::InvDstAlpha> blendSrcFactorAlpha{
        "blend_src.a = 0.0;",                    // ZERO
        "blend_src.a = 1.0;",                    // ONE
        "blend_src.a = initial_ocol0.a;",        // DSTCLR
        "blend_src.a = 1.0 - initial_ocol0.a;",  // INVDSTCLR
        "blend_src.a = src_color.a;",            // SRCALPHA
        "blend_src.a = 1.0 - src_color.a;",      // INVSRCALPHA
        "blend_src.a = initial_ocol0.a;",        // DSTALPHA
        "blend_src.a = 1.0 - initial_ocol0.a;",  // INVDSTALPHA
    };
    static constexpr EnumMap<std::string_view, DstBlendFactor::InvDstAlpha> blendDstFactor{
        "blend_dst.rgb = float3(0,0,0);",                      // ZERO
        "blend_dst.rgb = float3(1,1,1);",                      // ONE
        "blend_dst.rgb = ocol0.rgb;",                          // SRCCLR
        "blend_dst.rgb = float3(1,1,1) - ocol0.rgb;",          // INVSRCCLR
        "blend_dst.rgb = src_color.aaa;",                      // SRCALHA
        "blend_dst.rgb = float3(1,1,1) - src_color.aaa;",      // INVSRCALPHA
        "blend_dst.rgb = initial_ocol0.aaa;",                  // DSTALPHA
        "blend_dst.rgb = float3(1,1,1) - initial_ocol0.aaa;",  // INVDSTALPHA
    };
    static constexpr EnumMap<std::string_view, DstBlendFactor::InvDstAlpha> blendDstFactorAlpha{
        "blend_dst.a = 0.0;",                    // ZERO
        "blend_dst.a = 1.0;",                    // ONE
        "blend_dst.a = ocol0.a;",                // SRCCLR
        "blend_dst.a = 1.0 - ocol0.a;",          // INVSRCCLR
        "blend_dst.a = src_color.a;",            // SRCALPHA
        "blend_dst.a = 1.0 - src_color.a;",      // INVSRCALPHA
        "blend_dst.a = initial_ocol0.a;",        // DSTALPHA
        "blend_dst.a = 1.0 - initial_ocol0.a;",  // INVDSTALPHA
    };

    out.Write("  if (blend_enable) {{\n"
              "    float4 src_color;\n"
              "    if (bpmem_dstalpha != 0u) {{\n"
              "      src_color = ocol1;\n"
              "    }} else {{\n"
              "      src_color = ocol0;\n"
              "    }}"
              "    float4 blend_src;\n");
    WriteSwitch(out, api_type, "blend_src_factor", blendSrcFactor, 4, true);
    WriteSwitch(out, api_type, "blend_src_factor_alpha", blendSrcFactorAlpha, 4, true);

    out.Write("    float4 blend_dst;\n");
    WriteSwitch(out, api_type, "blend_dst_factor", blendDstFactor, 4, true);
    WriteSwitch(out, api_type, "blend_dst_factor_alpha", blendDstFactorAlpha, 4, true);

    out.Write("    float4 blend_result;\n"
              "    if (blend_subtract)\n"
              "      blend_result.rgb = initial_ocol0.rgb * blend_dst.rgb - ocol0.rgb * "
              "blend_src.rgb;\n"
              "    else\n"
              "      blend_result.rgb = initial_ocol0.rgb * blend_dst.rgb + ocol0.rgb * "
              "blend_src.rgb;\n");

    out.Write("    if (blend_subtract_alpha)\n"
              "      blend_result.a = initial_ocol0.a * blend_dst.a - ocol0.a * blend_src.a;\n"
              "    else\n"
              "      blend_result.a = initial_ocol0.a * blend_dst.a + ocol0.a * blend_src.a;\n");

    out.Write("    real_ocol0 = blend_result;\n");

    out.Write("  }} else {{\n"
              "    real_ocol0 = ocol0;\n"
              "  }}\n");
  }

  out.Write("}}\n"
            "\n"
            "int4 getRasColor(State s, StageState ss, float4 colors_0, float4 colors_1) {{\n"
            "  // Select Ras for stage\n"
            "  uint ras = {};\n",
            BitfieldExtract<&TwoTevStageOrders::colorchan_even>("ss.order"));
  out.Write("  if (ras < 2u) {{ // Lighting Channel 0 or 1\n"
            "    int4 color = iround(((ras == 0u) ? colors_0 : colors_1) * 255.0);\n"
            "    uint swap = {};\n",
            BitfieldExtract<&TevStageCombiner::AlphaCombiner::rswap>("ss.ac"));
  out.Write("    return Swizzle(swap, color);\n");
  out.Write("  }} else if (ras == 5u) {{ // Alpha Bump\n"
            "    return int4(s.AlphaBump, s.AlphaBump, s.AlphaBump, s.AlphaBump);\n"
            "  }} else if (ras == 6u) {{ // Normalzied Alpha Bump\n"
            "    int normalized = s.AlphaBump | s.AlphaBump >> 5;\n"
            "    return int4(normalized, normalized, normalized, normalized);\n"
            "  }} else {{\n"
            "    return int4(0, 0, 0, 0);\n"
            "  }}\n"
            "}}\n"
            "\n"
            "int4 getKonstColor(State s, StageState ss) {{\n"
            "  // Select Konst for stage\n"
            "  // TODO: a switch case might be better here than an dynamically"
            "  // indexed uniform lookup\n"
            "  uint tevksel = bpmem_tevksel(ss.stage>>1);\n"
            "  if ((ss.stage & 1u) == 0u)\n"
            "    return int4(konstLookup[{}].rgb, konstLookup[{}].a);\n",
            BitfieldExtract<&TevKSel::kcsel_even>("tevksel"),
            BitfieldExtract<&TevKSel::kasel_even>("tevksel"));
  out.Write("  else\n"
            "    return int4(konstLookup[{}].rgb, konstLookup[{}].a);\n",
            BitfieldExtract<&TevKSel::kcsel_odd>("tevksel"),
            BitfieldExtract<&TevKSel::kasel_odd>("tevksel"));
  out.Write("}}\n");

  return out;
}

void EnumeratePixelShaderUids(const std::function<void(const PixelShaderUid&)>& callback)
{
  PixelShaderUid uid;

  for (u32 texgens = 0; texgens <= 8; texgens++)
  {
    pixel_ubershader_uid_data* const puid = uid.GetUidData();
    puid->num_texgens = texgens;

    for (u32 early_depth = 0; early_depth < 2; early_depth++)
    {
      puid->early_depth = early_depth != 0;
      for (u32 per_pixel_depth = 0; per_pixel_depth < 2; per_pixel_depth++)
      {
        // Don't generate shaders where we have early depth tests enabled, and write gl_FragDepth.
        if (early_depth && per_pixel_depth)
          continue;

        puid->per_pixel_depth = per_pixel_depth != 0;
        for (u32 uint_output = 0; uint_output < 2; uint_output++)
        {
          puid->uint_output = uint_output;
          for (u32 no_dual_src = 0; no_dual_src < 2; no_dual_src++)
          {
            puid->no_dual_src = no_dual_src;
            callback(uid);
          }
        }
      }
    }
  }
}
}  // namespace UberShader
