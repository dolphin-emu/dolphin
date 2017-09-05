// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/UberShaderCommon.h"
#include "VideoCommon/XFMemory.h"

namespace UberShader
{
PixelShaderUid GetPixelShaderUid()
{
  PixelShaderUid out;
  pixel_ubershader_uid_data* uid_data = out.GetUidData<pixel_ubershader_uid_data>();
  memset(uid_data, 0, sizeof(*uid_data));
  uid_data->num_texgens = xfmem.numTexGen.numTexGens;
  uid_data->early_depth =
      bpmem.UseEarlyDepthTest() &&
      (g_ActiveConfig.bFastDepthCalc || bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED) &&
      !(bpmem.zmode.testenable && bpmem.genMode.zfreeze);
  uid_data->per_pixel_depth =
      (bpmem.ztex2.op != ZTEXTURE_DISABLE && bpmem.UseLateDepthTest()) ||
      (!g_ActiveConfig.bFastDepthCalc && bpmem.zmode.testenable && !uid_data->early_depth) ||
      (bpmem.zmode.testenable && bpmem.genMode.zfreeze);
  uid_data->uint_output = bpmem.blendmode.UseLogicOp();
  return out;
}

void ClearUnusedPixelShaderUidBits(APIType ApiType, PixelShaderUid* uid)
{
  pixel_ubershader_uid_data* uid_data = uid->GetUidData<pixel_ubershader_uid_data>();

  // OpenGL and Vulkan convert implicitly normalized color outputs to their uint representation.
  // Therefore, it is not necessary to use a uint output on these backends.
  if (ApiType != APIType::D3D)
    uid_data->uint_output = 0;
}

ShaderCode GenPixelShader(APIType ApiType, const ShaderHostConfig& host_config,
                          const pixel_ubershader_uid_data* uid_data)
{
  const bool per_pixel_lighting = host_config.per_pixel_lighting;
  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool stereo = host_config.stereo;
  const bool use_dual_source = host_config.backend_dual_source_blend;
  const bool early_depth = uid_data->early_depth != 0;
  const bool per_pixel_depth = uid_data->per_pixel_depth != 0;
  const bool bounding_box =
      host_config.bounding_box && g_ActiveConfig.BBoxUseFragmentShaderImplementation();
  const u32 numTexgen = uid_data->num_texgens;
  ShaderCode out;

  out.Write("// Pixel UberShader for %u texgens%s%s\n", numTexgen,
            early_depth ? ", early-depth" : "", per_pixel_depth ? ", per-pixel depth" : "");
  WritePixelShaderCommonHeader(out, ApiType, numTexgen, per_pixel_lighting, bounding_box);
  WriteUberShaderCommonHeader(out, ApiType, host_config);
  if (per_pixel_lighting)
    WriteLightingFunction(out);

  // Shader inputs/outputs in GLSL (HLSL is in main).
  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    if (use_dual_source)
    {
      if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_FRAGMENT_SHADER_INDEX_DECORATION))
      {
        out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n");
        out.Write("FRAGMENT_OUTPUT_LOCATION(1) out vec4 ocol1;\n");
      }
      else
      {
        out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 0) out vec4 ocol0;\n");
        out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 1) out vec4 ocol1;\n");
      }
    }
    else
    {
      out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n");
    }

    if (per_pixel_depth)
      out.Write("#define depth gl_FragDepth\n");

    if (host_config.backend_geometry_shaders || ApiType == APIType::Vulkan)
    {
      out.Write("VARYING_LOCATION(0) in VertexData {\n");
      GenerateVSOutputMembers(out, ApiType, numTexgen, per_pixel_lighting,
                              GetInterpolationQualifier(msaa, ssaa));

      if (stereo)
        out.Write("  flat int layer;\n");

      out.Write("};\n\n");
    }
    else
    {
      out.Write("%s in float4 colors_0;\n", GetInterpolationQualifier(msaa, ssaa));
      out.Write("%s in float4 colors_1;\n", GetInterpolationQualifier(msaa, ssaa));
      // compute window position if needed because binding semantic WPOS is not widely supported
      // Let's set up attributes
      for (u32 i = 0; i < numTexgen; ++i)
        out.Write("%s in float3 tex%d;\n", GetInterpolationQualifier(msaa, ssaa), i);
      out.Write("%s in float4 clipPos;\n", GetInterpolationQualifier(msaa, ssaa));
      if (per_pixel_lighting)
      {
        out.Write("%s in float3 Normal;\n", GetInterpolationQualifier(msaa, ssaa));
        out.Write("%s in float3 WorldPos;\n", GetInterpolationQualifier(msaa, ssaa));
      }
    }
  }

  // Uniform index -> texture coordinates
  if (numTexgen > 0)
  {
    if (ApiType != APIType::D3D)
    {
      out.Write("float3 selectTexCoord(uint index) {\n");
    }
    else
    {
      out.Write("float3 selectTexCoord(uint index");
      for (u32 i = 0; i < numTexgen; i++)
        out.Write(", float3 tex%u", i);
      out.Write(") {\n");
    }

    if (ApiType == APIType::D3D)
    {
      out.Write("  switch (index) {\n");
      for (u32 i = 0; i < numTexgen; i++)
      {
        out.Write("  case %uu:\n"
                  "    return tex%u;\n",
                  i, i);
      }
      out.Write("  default:\n"
                "    return float3(0.0, 0.0, 0.0);\n"
                "  }\n");
    }
    else
    {
      if (numTexgen > 4)
        out.Write("  if (index < 4u) {\n");
      if (numTexgen > 2)
        out.Write("    if (index < 2u) {\n");
      if (numTexgen > 1)
        out.Write("      return (index == 0u) ? tex0 : tex1;\n");
      else
        out.Write("      return (index == 0u) ? tex0 : float3(0.0, 0.0, 0.0);\n");
      if (numTexgen > 2)
      {
        out.Write("    } else {\n");  // >= 2
        if (numTexgen > 3)
          out.Write("      return (index == 2u) ? tex2 : tex3;\n");
        else
          out.Write("      return (index == 2u) ? tex2 : float3(0.0, 0.0, 0.0);\n");
        out.Write("    }\n");
      }
      if (numTexgen > 4)
      {
        out.Write("  } else {\n");  // >= 4 <= 8
        if (numTexgen > 6)
          out.Write("    if (index < 6u) {\n");
        if (numTexgen > 5)
          out.Write("      return (index == 4u) ? tex4 : tex5;\n");
        else
          out.Write("      return (index == 4u) ? tex4 : float3(0.0, 0.0, 0.0);\n");
        if (numTexgen > 6)
        {
          out.Write("    } else {\n");  // >= 6 <= 8
          if (numTexgen > 7)
            out.Write("      return (index == 6u) ? tex6 : tex7;\n");
          else
            out.Write("      return (index == 6u) ? tex6 : float3(0.0, 0.0, 0.0);\n");
          out.Write("    }\n");
        }
        out.Write("  }\n");
      }
    }

    out.Write("}\n\n");
  }

  // =====================
  //   Texture Sampling
  // =====================

  if (host_config.backend_dynamic_sampler_indexing)
  {
    // Doesn't look like directx supports this. Oh well the code path is here just incase it
    // supports this in the future.
    out.Write("int4 sampleTexture(uint sampler_num, float3 uv) {\n");
    if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
      out.Write("  return iround(texture(samp[sampler_num], uv) * 255.0);\n");
    else if (ApiType == APIType::D3D)
      out.Write("  return iround(Tex[sampler_num].Sample(samp[sampler_num], uv) * 255.0);\n");
    out.Write("}\n\n");
  }
  else
  {
    out.Write("int4 sampleTexture(uint sampler_num, float3 uv) {\n"
              "  // This is messy, but DirectX, OpenGl 3.3 and Opengl ES 3.0 doesn't support "
              "dynamic indexing of the sampler array\n"
              "  // With any luck the shader compiler will optimise this if the hardware supports "
              "dynamic indexing.\n"
              "  switch(sampler_num) {\n");
    for (int i = 0; i < 8; i++)
    {
      if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
        out.Write("  case %du: return iround(texture(samp[%d], uv) * 255.0);\n", i, i);
      else if (ApiType == APIType::D3D)
        out.Write("  case %du: return iround(Tex[%d].Sample(samp[%d], uv) * 255.0);\n", i, i, i);
    }
    out.Write("  }\n"
              "}\n\n");
  }

  // ======================
  //   Arbatary Swizzling
  // ======================

  out.Write("int4 Swizzle(uint s, int4 color) {\n"
            "  // AKA: Color Channel Swapping\n"
            "\n"
            "  int4 ret;\n");
  out.Write("  ret.r = color[%s];\n",
            BitfieldExtract("bpmem_tevksel(s * 2u)", TevKSel().swap1).c_str());
  out.Write("  ret.g = color[%s];\n",
            BitfieldExtract("bpmem_tevksel(s * 2u)", TevKSel().swap2).c_str());
  out.Write("  ret.b = color[%s];\n",
            BitfieldExtract("bpmem_tevksel(s * 2u + 1u)", TevKSel().swap1).c_str());
  out.Write("  ret.a = color[%s];\n",
            BitfieldExtract("bpmem_tevksel(s * 2u + 1u)", TevKSel().swap2).c_str());
  out.Write("  return ret;\n"
            "}\n\n");

  // ======================
  //   Indirect Wrappping
  // ======================
  out.Write("int Wrap(int coord, uint mode) {\n"
            "  if (mode == 0u) // ITW_OFF\n"
            "    return coord;\n"
            "  else if (mode < 6u) // ITW_256 to ITW_16\n"
            "    return coord & (0xfffe >> mode);\n"
            "  else // ITW_0\n"
            "    return 0;\n"
            "}\n\n");

  // ======================
  //    Indirect Lookup
  // ======================
  auto LookupIndirectTexture = [&out, stereo](const char* out_var_name, const char* in_index_name) {
    out.Write("{\n"
              "  uint iref = bpmem_iref(%s);\n"
              "  if ( iref != 0u)\n"
              "  {\n"
              "    uint texcoord = bitfieldExtract(iref, 0, 3);\n"
              "    uint texmap = bitfieldExtract(iref, 8, 3);\n"
              "    float3 uv = getTexCoord(texcoord);\n"
              "    int2 fixedPoint_uv = int2((uv.z == 0.0 ? uv.xy : (uv.xy / uv.z)) * " I_TEXDIMS
              "[texcoord].zw);\n"
              "\n"
              "    if ((%s & 1u) == 0u)\n"
              "      fixedPoint_uv = fixedPoint_uv >> " I_INDTEXSCALE "[%s >> 1].xy;\n"
              "    else\n"
              "      fixedPoint_uv = fixedPoint_uv >> " I_INDTEXSCALE "[%s >> 1].zw;\n"
              "\n"
              "    %s = sampleTexture(texmap, float3(float2(fixedPoint_uv) * " I_TEXDIMS
              "[texmap].xy, %s)).abg;\n",
              in_index_name, in_index_name, in_index_name, in_index_name, out_var_name,
              stereo ? "float(layer)" : "0.0");
    out.Write("  }\n"
              "  else\n"
              "  {\n"
              "    %s = int3(0, 0, 0);\n"
              "  }\n"
              "}\n",
              out_var_name);
  };

  // ======================
  //   TEV's Special Lerp
  // ======================
  auto WriteTevLerp = [&out](const char* components) {
    out.Write("// TEV's Linear Interpolate, plus bias, add/subtract and scale\n"
              "int%s tevLerp%s(int%s A, int%s B, int%s C, int%s D, uint bias, bool op, bool alpha, "
              "uint shift) {\n"
              " // Scale C from 0..255 to 0..256\n"
              "  C += C >> 7;\n"
              "\n"
              " // Add bias to D\n"
              "  if (bias == 1u) D += 128;\n"
              "  else if (bias == 2u) D -= 128;\n"
              "\n"
              "  int%s lerp = (A << 8) + (B - A)*C;\n"
              "  if (shift != 3u) {\n"
              "    lerp = lerp << shift;\n"
              "    D = D << shift;\n"
              "  }\n"
              "\n"
              "  if ((shift == 3u) == alpha)\n"
              "    lerp = lerp + (op ? 127 : 128);\n"
              "\n"
              "  int%s result = lerp >> 8;\n"
              "\n"
              "  // Add/Subtract D\n"
              "  if(op) // Subtract\n"
              "    result = D - result;\n"
              "  else // Add\n"
              "    result = D + result;\n"
              "\n"
              "  // Most of the Shift was moved inside the lerp for improved percision\n"
              "  // But we still do the divide by 2 here\n"
              "  if (shift == 3u)\n"
              "    result = result >> 1;\n"
              "  return result;\n"
              "}\n\n",
              components, components, components, components, components, components, components,
              components);
  };
  WriteTevLerp("");   // int
  WriteTevLerp("3");  // int3

  // =======================
  //   TEV's Color Compare
  // =======================

  out.Write(
      "// Implements operations 0-5 of tev's compare mode,\n"
      "// which are common to both color and alpha channels\n"
      "bool tevCompare(uint op, int3 color_A, int3 color_B) {\n"
      "  switch (op) {\n"
      "  case 0u: // TEVCMP_R8_GT\n"
      "    return (color_A.r > color_B.r);\n"
      "  case 1u: // TEVCMP_R8_EQ\n"
      "    return (color_A.r == color_B.r);\n"
      "  case 2u: // TEVCMP_GR16_GT\n"
      "    int A_16 = (color_A.r | (color_A.g << 8));\n"
      "    int B_16 = (color_B.r | (color_B.g << 8));\n"
      "    return A_16 > B_16;\n"
      "  case 3u: // TEVCMP_GR16_EQ\n"
      "    return (color_A.r == color_B.r && color_A.g == color_B.g);\n"
      "  case 4u: // TEVCMP_BGR24_GT\n"
      "    int A_24 = (color_A.r | (color_A.g << 8) | (color_A.b << 16));\n"
      "    int B_24 = (color_B.r | (color_B.g << 8) | (color_B.b << 16));\n"
      "    return A_24 > B_24;\n"
      "  case 5u: // TEVCMP_BGR24_EQ\n"
      "    return (color_A.r == color_B.r && color_A.g == color_B.g && color_A.b == color_B.b);\n"
      "  default:\n"
      "    return false;\n"
      "  }\n"
      "}\n\n");

  // =================
  //   Input Selects
  // =================

  out.Write("struct State {\n"
            "  int4 Reg[4];\n"
            "  int4 TexColor;\n"
            "  int AlphaBump;\n"
            "};\n"
            "struct StageState {\n"
            "  uint stage;\n"
            "  uint order;\n"
            "  uint cc;\n"
            "  uint ac;\n");

  out.Write("};\n"
            "\n"
            "int4 getRasColor(State s, StageState ss, float4 colors_0, float4 colors_1);\n"
            "int4 getKonstColor(State s, StageState ss);\n"
            "\n");

  // The switch statements in these functions appear to get transformed into an if..else chain
  // on NVIDIA's OpenGL/Vulkan drivers, resulting in lower performance than the D3D counterparts.
  // Transforming the switch into a binary tree of ifs can increase performance by up to 20%.
  if (ApiType == APIType::D3D)
  {
    out.Write("// Helper function for Alpha Test\n"
              "bool alphaCompare(int a, int b, uint compare) {\n"
              "  switch (compare) {\n"
              "  case 0u: // NEVER\n"
              "    return false;\n"
              "  case 1u: // LESS\n"
              "    return a < b;\n"
              "  case 2u: // EQUAL\n"
              "    return a == b;\n"
              "  case 3u: // LEQUAL\n"
              "    return a <= b;\n"
              "  case 4u: // GREATER\n"
              "    return a > b;\n"
              "  case 5u: // NEQUAL;\n"
              "    return a != b;\n"
              "  case 6u: // GEQUAL\n"
              "    return a >= b;\n"
              "  case 7u: // ALWAYS\n"
              "    return true;\n"
              "  }\n"
              "}\n"
              "\n"
              "int3 selectColorInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
              "uint index) {\n"
              "  switch (index) {\n"
              "  case 0u: // prev.rgb\n"
              "    return s.Reg[0].rgb;\n"
              "  case 1u: // prev.aaa\n"
              "    return s.Reg[0].aaa;\n"
              "  case 2u: // c0.rgb\n"
              "    return s.Reg[1].rgb;\n"
              "  case 3u: // c0.aaa\n"
              "    return s.Reg[1].aaa;\n"
              "  case 4u: // c1.rgb\n"
              "    return s.Reg[2].rgb;\n"
              "  case 5u: // c1.aaa\n"
              "    return s.Reg[2].aaa;\n"
              "  case 6u: // c2.rgb\n"
              "    return s.Reg[3].rgb;\n"
              "  case 7u: // c2.aaa\n"
              "    return s.Reg[3].aaa;\n"
              "  case 8u:\n"
              "    return s.TexColor.rgb;\n"
              "  case 9u:\n"
              "    return s.TexColor.aaa;\n"
              "  case 10u:\n"
              "    return getRasColor(s, ss, colors_0, colors_1).rgb;\n"
              "  case 11u:\n"
              "    return getRasColor(s, ss, colors_0, colors_1).aaa;\n"
              "  case 12u: // One\n"
              "    return int3(255, 255, 255);\n"
              "  case 13u: // Half\n"
              "    return int3(128, 128, 128);\n"
              "  case 14u:\n"
              "    return getKonstColor(s, ss).rgb;\n"
              "  case 15u: // Zero\n"
              "    return int3(0, 0, 0);\n"
              "  }\n"
              "}\n"
              "\n"
              "int selectAlphaInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
              "uint index) {\n"
              "  switch (index) {\n"
              "  case 0u: // prev.a\n"
              "    return s.Reg[0].a;\n"
              "  case 1u: // c0.a\n"
              "    return s.Reg[1].a;\n"
              "  case 2u: // c1.a\n"
              "    return s.Reg[2].a;\n"
              "  case 3u: // c2.a\n"
              "    return s.Reg[3].a;\n"
              "  case 4u:\n"
              "    return s.TexColor.a;\n"
              "  case 5u:\n"
              "    return getRasColor(s, ss, colors_0, colors_1).a;\n"
              "  case 6u:\n"
              "    return getKonstColor(s, ss).a;\n"
              "  case 7u: // Zero\n"
              "    return 0;\n"
              "  }\n"
              "}\n"
              "\n"
              "int4 getTevReg(in State s, uint index) {\n"
              "  switch (index) {\n"
              "  case 0u: // prev\n"
              "    return s.Reg[0];\n"
              "  case 1u: // c0\n"
              "    return s.Reg[1];\n"
              "  case 2u: // c1\n"
              "    return s.Reg[2];\n"
              "  case 3u: // c2\n"
              "    return s.Reg[3];\n"
              "  default: // prev\n"
              "    return s.Reg[0];\n"
              "  }\n"
              "}\n"
              "\n"
              "void setRegColor(inout State s, uint index, int3 color) {\n"
              "  switch (index) {\n"
              "  case 0u: // prev\n"
              "    s.Reg[0].rgb = color;\n"
              "    break;\n"
              "  case 1u: // c0\n"
              "    s.Reg[1].rgb = color;\n"
              "    break;\n"
              "  case 2u: // c1\n"
              "    s.Reg[2].rgb = color;\n"
              "    break;\n"
              "  case 3u: // c2\n"
              "    s.Reg[3].rgb = color;\n"
              "    break;\n"
              "  }\n"
              "}\n"
              "\n"
              "void setRegAlpha(inout State s, uint index, int alpha) {\n"
              "  switch (index) {\n"
              "  case 0u: // prev\n"
              "    s.Reg[0].a = alpha;\n"
              "    break;\n"
              "  case 1u: // c0\n"
              "    s.Reg[1].a = alpha;\n"
              "    break;\n"
              "  case 2u: // c1\n"
              "    s.Reg[2].a = alpha;\n"
              "    break;\n"
              "  case 3u: // c2\n"
              "    s.Reg[3].a = alpha;\n"
              "    break;\n"
              "  }\n"
              "}\n"
              "\n");
  }
  else
  {
    out.Write(
        "// Helper function for Alpha Test\n"
        "bool alphaCompare(int a, int b, uint compare) {\n"
        "  if (compare < 4u) {\n"
        "    if (compare < 2u) {\n"
        "      return (compare == 0u) ? (false) : (a < b);\n"
        "    } else {\n"
        "      return (compare == 2u) ? (a == b) : (a <= b);\n"
        "    }\n"
        "  } else {\n"
        "    if (compare < 6u) {\n"
        "      return (compare == 4u) ? (a > b) : (a != b);\n"
        "    } else {\n"
        "      return (compare == 6u) ? (a >= b) : (true);\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n"
        "int3 selectColorInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
        "uint index) {\n"
        "  if (index < 8u) {\n"
        "    if (index < 4u) {\n"
        "      if (index < 2u) {\n"
        "        return (index == 0u) ? s.Reg[0].rgb : s.Reg[0].aaa;\n"
        "      } else {\n"
        "        return (index == 2u) ? s.Reg[1].rgb : s.Reg[1].aaa;\n"
        "      }\n"
        "    } else {\n"
        "      if (index < 6u) {\n"
        "        return (index == 4u) ? s.Reg[2].rgb : s.Reg[2].aaa;\n"
        "      } else {\n"
        "        return (index == 6u) ? s.Reg[3].rgb : s.Reg[3].aaa;\n"
        "      }\n"
        "    }\n"
        "  } else {\n"
        "    if (index < 12u) {\n"
        "      if (index < 10u) {\n"
        "        return (index == 8u) ? s.TexColor.rgb : s.TexColor.aaa;\n"
        "      } else {\n"
        "        int4 ras = getRasColor(s, ss, colors_0, colors_1);\n"
        "        return (index == 10u) ? ras.rgb : ras.aaa;\n"
        "      }\n"
        "    } else {\n"
        "      if (index < 14u) {\n"
        "        return (index == 12u) ? int3(255, 255, 255) : int3(128, 128, 128);\n"
        "      } else {\n"
        "        return (index == 14u) ? getKonstColor(s, ss).rgb : int3(0, 0, 0);\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n"
        "int selectAlphaInput(State s, StageState ss, float4 colors_0, float4 colors_1, "
        "uint index) {\n"
        "  if (index < 4u) {\n"
        "    if (index < 2u) {\n"
        "      return (index == 0u) ? s.Reg[0].a : s.Reg[1].a;\n"
        "    } else {\n"
        "      return (index == 2u) ? s.Reg[2].a : s.Reg[3].a;\n"
        "    }\n"
        "  } else {\n"
        "    if (index < 6u) {\n"
        "      return (index == 4u) ? s.TexColor.a : getRasColor(s, ss, colors_0, colors_1).a;\n"
        "    } else {\n"
        "      return (index == 6u) ? getKonstColor(s, ss).a : 0;\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n"
        "int4 getTevReg(in State s, uint index) {\n"
        "  if (index < 2u) {\n"
        "    if (index == 0u) {\n"
        "      return s.Reg[0];\n"
        "    } else {\n"
        "      return s.Reg[1];\n"
        "    }\n"
        "  } else {\n"
        "    if (index == 2u) {\n"
        "      return s.Reg[2];\n"
        "    } else {\n"
        "      return s.Reg[3];\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n"
        "void setRegColor(inout State s, uint index, int3 color) {\n"
        "  if (index < 2u) {\n"
        "    if (index == 0u) {\n"
        "      s.Reg[0].rgb = color;\n"
        "    } else {\n"
        "      s.Reg[1].rgb = color;\n"
        "    }\n"
        "  } else {\n"
        "    if (index == 2u) {\n"
        "      s.Reg[2].rgb = color;\n"
        "    } else {\n"
        "      s.Reg[3].rgb = color;\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n"
        "void setRegAlpha(inout State s, uint index, int alpha) {\n"
        "  if (index < 2u) {\n"
        "    if (index == 0u) {\n"
        "      s.Reg[0].a = alpha;\n"
        "    } else {\n"
        "      s.Reg[1].a = alpha;\n"
        "    }\n"
        "  } else {\n"
        "    if (index == 2u) {\n"
        "      s.Reg[2].a = alpha;\n"
        "    } else {\n"
        "      s.Reg[3].a = alpha;\n"
        "    }\n"
        "  }\n"
        "}\n"
        "\n");
  }

  // Since the texture coodinate variables aren't global, we need to pass
  // them to the select function in D3D.
  if (numTexgen > 0)
  {
    if (ApiType != APIType::D3D)
    {
      out.Write("#define getTexCoord(index) selectTexCoord((index))\n\n");
    }
    else
    {
      out.Write("#define getTexCoord(index) selectTexCoord((index)");
      for (u32 i = 0; i < numTexgen; i++)
        out.Write(", tex%u", i);
      out.Write(")\n\n");
    }
  }

  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    if (early_depth && host_config.backend_early_z)
      out.Write("FORCE_EARLY_Z;\n");

    out.Write("void main()\n{\n");
    out.Write("  float4 rawpos = gl_FragCoord;\n");
  }
  else  // D3D
  {
    if (early_depth && host_config.backend_early_z)
      out.Write("[earlydepthstencil]\n");

    out.Write("void main(\n");
    if (uid_data->uint_output)
    {
      out.Write("  out uint4 ocol0 : SV_Target,\n");
    }
    else
    {
      out.Write("  out float4 ocol0 : SV_Target0,\n"
                "  out float4 ocol1 : SV_Target1,\n");
    }
    if (per_pixel_depth)
      out.Write("  out float depth : SV_Depth,\n");
    out.Write("  in float4 rawpos : SV_Position,\n");
    out.Write("  in %s float4 colors_0 : COLOR0,\n", GetInterpolationQualifier(msaa, ssaa));
    out.Write("  in %s float4 colors_1 : COLOR1", GetInterpolationQualifier(msaa, ssaa));

    // compute window position if needed because binding semantic WPOS is not widely supported
    for (u32 i = 0; i < numTexgen; ++i)
      out.Write(",\n  in %s float3 tex%u : TEXCOORD%u", GetInterpolationQualifier(msaa, ssaa), i,
                i);
    out.Write("\n,\n  in %s float4 clipPos : TEXCOORD%u", GetInterpolationQualifier(msaa, ssaa),
              numTexgen);
    if (per_pixel_lighting)
    {
      out.Write(",\n  in %s float3 Normal : TEXCOORD%u", GetInterpolationQualifier(msaa, ssaa),
                numTexgen + 1);
      out.Write(",\n  in %s float3 WorldPos : TEXCOORD%u", GetInterpolationQualifier(msaa, ssaa),
                numTexgen + 2);
    }
    out.Write(",\n  in float clipDist0 : SV_ClipDistance0\n");
    out.Write(",\n  in float clipDist1 : SV_ClipDistance1\n");
    if (stereo)
      out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
    out.Write("\n        ) {\n");
  }

  out.Write("  int3 tevcoord = int3(0, 0, 0);\n"
            "  State s;\n"
            "  s.TexColor = int4(0, 0, 0, 0);\n"
            "  s.AlphaBump = 0;\n"
            "\n");
  for (int i = 0; i < 4; i++)
    out.Write("  s.Reg[%d] = " I_COLORS "[%d];\n", i, i);

  const char* color_input_prefix = "";
  if (per_pixel_lighting)
  {
    out.Write("  float4 lit_colors_0 = colors_0;\n");
    out.Write("  float4 lit_colors_1 = colors_1;\n");
    out.Write("  float3 lit_normal = normalize(Normal.xyz);\n");
    out.Write("  float3 lit_pos = WorldPos.xyz;\n");
    WriteVertexLighting(out, ApiType, "lit_pos", "lit_normal", "colors_0", "colors_1",
                        "lit_colors_0", "lit_colors_1");
    color_input_prefix = "lit_";
  }

  out.Write("  uint num_stages = %s;\n\n",
            BitfieldExtract("bpmem_genmode", bpmem.genMode.numtevstages).c_str());

  out.Write("  // Main tev loop\n");
  if (ApiType == APIType::D3D)
  {
    // Tell DirectX we don't want this loop unrolled (it crashes if it tries to)
    out.Write("  [loop]\n");
  }

  out.Write("  for(uint stage = 0u; stage <= num_stages; stage++)\n"
            "  {\n"
            "    StageState ss;\n"
            "    ss.stage = stage;\n"
            "    ss.cc = bpmem_combiners(stage).x;\n"
            "    ss.ac = bpmem_combiners(stage).y;\n"
            "    ss.order = bpmem_tevorder(stage>>1);\n"
            "    if ((stage & 1u) == 1u)\n"
            "      ss.order = ss.order >> %d;\n\n",
            int(TwoTevStageOrders().enable1.StartBit() - TwoTevStageOrders().enable0.StartBit()));

  // Disable texturing when there are no texgens (for now)
  if (numTexgen != 0)
  {
    out.Write("    uint tex_coord = %s;\n",
              BitfieldExtract("ss.order", TwoTevStageOrders().texcoord0).c_str());
    out.Write("    float3 uv = getTexCoord(tex_coord);\n"
              "    int2 fixedPoint_uv = int2((uv.z == 0.0 ? uv.xy : (uv.xy / uv.z)) * " I_TEXDIMS
              "[tex_coord].zw);\n"
              "\n"
              "    bool texture_enabled = (ss.order & %du) != 0u;\n",
              1 << TwoTevStageOrders().enable0.StartBit());
    out.Write("\n"
              "    // Indirect textures\n"
              "    uint tevind = bpmem_tevind(stage);\n"
              "    if (tevind != 0u)\n"
              "    {\n"
              "      uint bs = %s;\n",
              BitfieldExtract("tevind", TevStageIndirect().bs).c_str());
    out.Write("      uint fmt = %s;\n", BitfieldExtract("tevind", TevStageIndirect().fmt).c_str());
    out.Write("      uint bias = %s;\n",
              BitfieldExtract("tevind", TevStageIndirect().bias).c_str());
    out.Write("      uint bt = %s;\n", BitfieldExtract("tevind", TevStageIndirect().bt).c_str());
    out.Write("      uint mid = %s;\n", BitfieldExtract("tevind", TevStageIndirect().mid).c_str());
    out.Write("\n");
    out.Write("      int3 indcoord;\n");
    LookupIndirectTexture("indcoord", "bt");
    out.Write("      if (bs != 0u)\n"
              "        s.AlphaBump = indcoord[bs - 1u];\n"
              "      switch(fmt)\n"
              "      {\n"
              "      case %iu:\n",
              ITF_8);
    out.Write("        indcoord.x = indcoord.x + ((bias & 1u) != 0u ? -128 : 0);\n"
              "        indcoord.y = indcoord.y + ((bias & 2u) != 0u ? -128 : 0);\n"
              "        indcoord.z = indcoord.z + ((bias & 4u) != 0u ? -128 : 0);\n"
              "        s.AlphaBump = s.AlphaBump & 0xf8;\n"
              "        break;\n"
              "      case %iu:\n",
              ITF_5);
    out.Write("        indcoord.x = (indcoord.x & 0x1f) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "        indcoord.y = (indcoord.y & 0x1f) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "        indcoord.z = (indcoord.z & 0x1f) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "        s.AlphaBump = s.AlphaBump & 0xe0;\n"
              "        break;\n"
              "      case %iu:\n",
              ITF_4);
    out.Write("        indcoord.x = (indcoord.x & 0x0f) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "        indcoord.y = (indcoord.y & 0x0f) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "        indcoord.z = (indcoord.z & 0x0f) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "        s.AlphaBump = s.AlphaBump & 0xf0;\n"
              "        break;\n"
              "      case %iu:\n",
              ITF_3);
    out.Write("        indcoord.x = (indcoord.x & 0x07) + ((bias & 1u) != 0u ? 1 : 0);\n"
              "        indcoord.y = (indcoord.y & 0x07) + ((bias & 2u) != 0u ? 1 : 0);\n"
              "        indcoord.z = (indcoord.z & 0x07) + ((bias & 4u) != 0u ? 1 : 0);\n"
              "        s.AlphaBump = s.AlphaBump & 0xf8;\n"
              "        break;\n"
              "      }\n"
              "\n"
              "      // Matrix multiply\n"
              "      int2 indtevtrans = int2(0, 0);\n"
              "      if ((mid & 3u) != 0u)\n"
              "      {\n"
              "        uint mtxidx = 2u * ((mid & 3u) - 1u);\n"
              "        int shift = " I_INDTEXMTX "[mtxidx].w;\n"
              "\n"
              "        switch (mid >> 2)\n"
              "        {\n"
              "        case 0u: // 3x2 S0.10 matrix\n"
              "          indtevtrans = int2(idot(" I_INDTEXMTX
              "[mtxidx].xyz, indcoord), idot(" I_INDTEXMTX "[mtxidx + 1u].xyz, indcoord)) >> 3;\n"
              "          break;\n"
              "        case 1u: // S matrix, S17.7 format\n"
              "          indtevtrans = (fixedPoint_uv * indcoord.xx) >> 8;\n"
              "          break;\n"
              "        case 2u: // T matrix, S17.7 format\n"
              "          indtevtrans = (fixedPoint_uv * indcoord.yy) >> 8;\n"
              "          break;\n"
              "        }\n"
              "\n"
              "        if (shift >= 0)\n"
              "          indtevtrans = indtevtrans >> shift;\n"
              "        else\n"
              "          indtevtrans = indtevtrans << ((-shift) & 31);\n"
              "      }\n"
              "\n"
              "      // Wrapping\n"
              "      uint sw = %s;\n",
              BitfieldExtract("tevind", TevStageIndirect().sw).c_str());
    out.Write("      uint tw = %s; \n", BitfieldExtract("tevind", TevStageIndirect().tw).c_str());
    out.Write(
        "      int2 wrapped_coord = int2(Wrap(fixedPoint_uv.x, sw), Wrap(fixedPoint_uv.y, tw));\n"
        "\n"
        "      if ((tevind & %du) != 0u) // add previous tevcoord\n",
        1 << TevStageIndirect().fb_addprev.StartBit());
    out.Write("        tevcoord.xy += wrapped_coord + indtevtrans;\n"
              "      else\n"
              "        tevcoord.xy = wrapped_coord + indtevtrans;\n"
              "\n"
              "      // Emulate s24 overflows\n"
              "      tevcoord.xy = (tevcoord.xy << 8) >> 8;\n"
              "    }\n"
              "    else if (texture_enabled)\n"
              "    {\n"
              "      tevcoord.xy = fixedPoint_uv;\n"
              "    }\n"
              "\n"
              "    // Sample texture for stage\n"
              "    if(texture_enabled) {\n"
              "      uint sampler_num = %s;\n",
              BitfieldExtract("ss.order", TwoTevStageOrders().texmap0).c_str());
    out.Write("\n"
              "      float2 uv = (float2(tevcoord.xy)) * " I_TEXDIMS "[sampler_num].xy;\n");
    out.Write("      int4 color = sampleTexture(sampler_num, float3(uv, %s));\n",
              stereo ? "float(layer)" : "0.0");
    out.Write("      uint swap = %s;\n",
              BitfieldExtract("ss.ac", TevStageCombiner().alphaC.tswap).c_str());
    out.Write("      s.TexColor = Swizzle(swap, color);\n");
    out.Write("    } else {\n"
              "      // Texture is disabled\n"
              "      s.TexColor = int4(255, 255, 255, 255);\n"
              "    }\n"
              "\n");
  }

  out.Write("    // This is the Meat of TEV\n"
            "    {\n"
            "      // Color Combiner\n");
  out.Write("      uint color_a = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.a).c_str());
  out.Write("      uint color_b = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.b).c_str());
  out.Write("      uint color_c = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.c).c_str());
  out.Write("      uint color_d = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.d).c_str());

  out.Write("      uint color_bias = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.bias).c_str());
  out.Write("      bool color_op = bool(%s);\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.op).c_str());
  out.Write("      bool color_clamp = bool(%s);\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.clamp).c_str());
  out.Write("      uint color_shift = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.shift).c_str());
  out.Write("      uint color_dest = %s;\n",
            BitfieldExtract("ss.cc", TevStageCombiner().colorC.dest).c_str());

  out.Write("      uint color_compare_op = color_shift << 1 | uint(color_op);\n"
            "\n"
            "      int3 color_A = selectColorInput(s, ss, %scolors_0, %scolors_1, color_a) & "
            "int3(255, 255, 255);\n"
            "      int3 color_B = selectColorInput(s, ss, %scolors_0, %scolors_1, color_b) & "
            "int3(255, 255, 255);\n"
            "      int3 color_C = selectColorInput(s, ss, %scolors_0, %scolors_1, color_c) & "
            "int3(255, 255, 255);\n"
            "      int3 color_D = selectColorInput(s, ss, %scolors_0, %scolors_1, color_d);  // 10 "
            "bits + sign\n"
            "\n",  // TODO: do we need to sign extend?
            color_input_prefix,
            color_input_prefix, color_input_prefix, color_input_prefix, color_input_prefix,
            color_input_prefix, color_input_prefix, color_input_prefix);
  out.Write(
      "      int3 color;\n"
      "      if(color_bias != 3u) { // Normal mode\n"
      "        color = tevLerp3(color_A, color_B, color_C, color_D, color_bias, color_op, false, "
      "color_shift);\n"
      "      } else { // Compare mode\n"
      "        // op 6 and 7 do a select per color channel\n"
      "        if (color_compare_op == 6u) {\n"
      "          // TEVCMP_RGB8_GT\n"
      "          color.r = (color_A.r > color_B.r) ? color_C.r : 0;\n"
      "          color.g = (color_A.g > color_B.g) ? color_C.g : 0;\n"
      "          color.b = (color_A.b > color_B.b) ? color_C.b : 0;\n"
      "        } else if (color_compare_op == 7u) {\n"
      "          // TEVCMP_RGB8_EQ\n"
      "          color.r = (color_A.r == color_B.r) ? color_C.r : 0;\n"
      "          color.g = (color_A.g == color_B.g) ? color_C.g : 0;\n"
      "          color.b = (color_A.b == color_B.b) ? color_C.b : 0;\n"
      "        } else {\n"
      "          // The remaining ops do one compare which selects all 3 channels\n"
      "          color = tevCompare(color_compare_op, color_A, color_B) ? color_C : int3(0, 0, "
      "0);\n"
      "        }\n"
      "        color = color_D + color;\n"
      "      }\n"
      "\n"
      "      // Clamp result\n"
      "      if (color_clamp)\n"
      "        color = clamp(color, 0, 255);\n"
      "      else\n"
      "        color = clamp(color, -1024, 1023);\n"
      "\n"
      "      // Write result to the correct input register of the next stage\n"
      "      setRegColor(s, color_dest, color);\n"
      "\n");

  // Alpha combiner
  out.Write("      // Alpha Combiner\n");
  out.Write("      uint alpha_a = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.a).c_str());
  out.Write("      uint alpha_b = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.b).c_str());
  out.Write("      uint alpha_c = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.c).c_str());
  out.Write("      uint alpha_d = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.d).c_str());

  out.Write("      uint alpha_bias = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.bias).c_str());
  out.Write("      bool alpha_op = bool(%s);\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.op).c_str());
  out.Write("      bool alpha_clamp = bool(%s);\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.clamp).c_str());
  out.Write("      uint alpha_shift = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.shift).c_str());
  out.Write("      uint alpha_dest = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.dest).c_str());

  out.Write(
      "      uint alpha_compare_op = alpha_shift << 1 | uint(alpha_op);\n"
      "\n"
      "      int alpha_A;\n"
      "      int alpha_B;\n"
      "      if (alpha_bias != 3u || alpha_compare_op > 5u) {\n"
      "        // Small optimisation here: alpha_A and alpha_B are unused by compare ops 0-5\n"
      "        alpha_A = selectAlphaInput(s, ss, %scolors_0, %scolors_1, alpha_a) & 255;\n"
      "        alpha_B = selectAlphaInput(s, ss, %scolors_0, %scolors_1, alpha_b) & 255;\n"
      "      };\n"
      "      int alpha_C = selectAlphaInput(s, ss, %scolors_0, %scolors_1, alpha_c) & 255;\n"
      "      int alpha_D = selectAlphaInput(s, ss, %scolors_0, %scolors_1, alpha_d); // 10 bits + "
      "sign\n"
      "\n",  // TODO: do we need to sign extend?
      color_input_prefix,
      color_input_prefix, color_input_prefix, color_input_prefix, color_input_prefix,
      color_input_prefix, color_input_prefix, color_input_prefix);
  out.Write("\n"
            "      int alpha;\n"
            "      if(alpha_bias != 3u) { // Normal mode\n"
            "        alpha = tevLerp(alpha_A, alpha_B, alpha_C, alpha_D, alpha_bias, alpha_op, "
            "true, alpha_shift);\n"
            "      } else { // Compare mode\n"
            "        if (alpha_compare_op == 6u) {\n"
            "          // TEVCMP_A8_GT\n"
            "          alpha = (alpha_A > alpha_B) ? alpha_C : 0;\n"
            "        } else if (alpha_compare_op == 7u) {\n"
            "          // TEVCMP_A8_EQ\n"
            "          alpha = (alpha_A == alpha_B) ? alpha_C : 0;\n"
            "        } else {\n"
            "          // All remaining alpha compare ops actually compare the color channels\n"
            "          alpha = tevCompare(alpha_compare_op, color_A, color_B) ? alpha_C : 0;\n"
            "        }\n"
            "        alpha = alpha_D + alpha;\n"
            "      }\n"
            "\n"
            "      // Clamp result\n"
            "      if (alpha_clamp)\n"
            "        alpha = clamp(alpha, 0, 255);\n"
            "      else\n"
            "        alpha = clamp(alpha, -1024, 1023);\n"
            "\n"
            "      // Write result to the correct input register of the next stage\n"
            "      setRegAlpha(s, alpha_dest, alpha);\n"
            "    }\n");

  out.Write("  } // Main tev loop\n"
            "\n");

  // Select the output color and alpha registers from the last stage.
  out.Write("  int4 TevResult;\n");
  out.Write(
      "  TevResult.xyz = getTevReg(s, %s).xyz;\n",
      BitfieldExtract("bpmem_combiners(num_stages).x", TevStageCombiner().colorC.dest).c_str());
  out.Write(
      "  TevResult.w = getTevReg(s, %s).w;\n",
      BitfieldExtract("bpmem_combiners(num_stages).y", TevStageCombiner().alphaC.dest).c_str());

  out.Write("  TevResult &= 255;\n\n");

  if (host_config.fast_depth_calc)
  {
    if (ApiType == APIType::D3D || ApiType == APIType::Vulkan)
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
              "  if ((bpmem_genmode & %du) != 0u) {\n",
              1 << GenMode().zfreeze.StartBit());
    out.Write("    float2 screenpos = rawpos.xy * " I_EFBSCALE ".xy;\n");
    if (ApiType == APIType::OpenGL)
      out.Write("    // Opengl has reversed vertical screenspace coordiantes\n"
                "    screenpos.y = 528.0 - screenpos.y;\n");

    out.Write("    zCoord = int(" I_ZSLOPE ".z + " I_ZSLOPE ".x * screenpos.x + " I_ZSLOPE
              ".y * screenpos.y);\n"
              " }\n"
              "\n");
  }

  // =================
  //   Depth Texture
  // =================

  out.Write("  // Depth Texture\n"
            "  int early_zCoord = zCoord;\n"
            "  if (bpmem_ztex_op != 0u) {\n"
            "    int ztex = int(" I_ZBIAS "[1].w); // fixed bias\n"
            "\n"
            "    // Whatever texture was in our last stage, it's now our depth texture\n"
            "    ztex += idot(s.TexColor.xyzw, " I_ZBIAS "[0].xyzw);\n"
            "    ztex += (bpmem_ztex_op == 1u) ? zCoord : 0;\n"
            "    zCoord = ztex & 0xFFFFFF;\n"
            "  }\n"
            "\n");

  if (per_pixel_depth)
  {
    out.Write("  // If early depth is enabled, write to zbuffer before depth textures\n");
    out.Write("  // If early depth isn't enabled, we write to the zbuffer here\n");
    out.Write("  int zbuffer_zCoord = bpmem_late_ztest ? zCoord : early_zCoord;\n");
    if (ApiType == APIType::D3D || ApiType == APIType::Vulkan)
      out.Write("  depth = 1.0 - float(zbuffer_zCoord) / 16777216.0;\n");
    else
      out.Write("  depth = float(zbuffer_zCoord) / 16777216.0;\n");
  }

  out.Write("  // Alpha Test\n"
            "  if (bpmem_alphaTest != 0u) {\n"
            "    bool comp0 = alphaCompare(TevResult.a, " I_ALPHA ".r, %s);\n",
            BitfieldExtract("bpmem_alphaTest", AlphaTest().comp0).c_str());
  out.Write("    bool comp1 = alphaCompare(TevResult.a, " I_ALPHA ".g, %s);\n",
            BitfieldExtract("bpmem_alphaTest", AlphaTest().comp1).c_str());
  out.Write("\n"
            "    // These if statements are written weirdly to work around intel and qualcom bugs "
            "with handling booleans.\n"
            "    switch (%s) {\n",
            BitfieldExtract("bpmem_alphaTest", AlphaTest().logic).c_str());
  out.Write("    case 0u: // AND\n"
            "      if (comp0 && comp1) break; else discard; break;\n"
            "    case 1u: // OR\n"
            "      if (comp0 || comp1) break; else discard; break;\n"
            "    case 2u: // XOR\n"
            "      if (comp0 != comp1) break; else discard; break;\n"
            "    case 3u: // XNOR\n"
            "      if (comp0 == comp1) break; else discard; break;\n"
            "    }\n"
            "  }\n"
            "\n");

  // =========
  // Dithering
  // =========
  out.Write("  if (bpmem_dither) {\n"
            "    // Flipper uses a standard 2x2 Bayer Matrix for 6 bit dithering\n"
            "    // Here the matrix is encoded into the two factor constants\n"
            "    int2 dither = int2(rawpos.xy) & 1;\n"
            "    TevResult.rgb = (TevResult.rgb - (TevResult.rgb >> 6)) + abs(dither.y * 3 - "
            "dither.x * 2);\n"
            "  }\n\n");

  // =========
  //    Fog
  // =========

  // FIXME: Fog is implemented the same as ShaderGen, but ShaderGen's fog is all hacks.
  //        Should be fixed point, and should not make guesses about Range-Based adjustments.
  out.Write("  // Fog\n"
            "  uint fog_function = %s;\n",
            BitfieldExtract("bpmem_fogParam3", FogParam3().fsel).c_str());
  out.Write("  if (fog_function != 0u) {\n"
            "    // TODO: This all needs to be converted from float to fixed point\n"
            "    float ze;\n"
            "    if (%s == 0u) {\n",
            BitfieldExtract("bpmem_fogParam3", FogParam3().proj).c_str());
  out.Write("      // perspective\n"
            "      // ze = A/(B - (Zs >> B_SHF)\n"
            "      ze = (" I_FOGF "[1].x * 16777216.0) / float(" I_FOGI ".y - (zCoord >> " I_FOGI
            ".w));\n"
            "    } else {\n"
            "      // orthographic\n"
            "      // ze = a*Zs    (here, no B_SHF)\n"
            "      ze = " I_FOGF "[1].x * float(zCoord) / 16777216.0;\n"
            "    }\n"
            "\n"
            "    if (bool(%s)) {\n",
            BitfieldExtract("bpmem_fogRangeBase", FogRangeParams::RangeBase().Enabled).c_str());
  out.Write("      // x_adjust = sqrt((x-center)^2 + k^2)/k\n"
            "      // ze *= x_adjust\n"
            "      // TODO Instead of this theoretical calculation, we should use the\n"
            "      //      coefficient table given in the fog range BP registers!\n"
            "      float x_adjust = (2.0 * (rawpos.x / " I_FOGF "[0].y)) - 1.0 - " I_FOGF
            "[0].x; \n"
            "      x_adjust = sqrt(x_adjust * x_adjust + " I_FOGF "[0].z * " I_FOGF
            "[0].z) / " I_FOGF "[0].z;\n"
            "      ze *= x_adjust;\n"
            "    }\n"
            "\n"
            "    float fog = clamp(ze - " I_FOGF "[1].z, 0.0, 1.0);\n"
            "\n"
            "    if (fog_function > 3u) {\n"
            "      switch (fog_function) {\n"
            "      case 4u:\n"
            "        fog = 1.0 - exp2(-8.0 * fog);\n"
            "        break;\n"
            "      case 5u:\n"
            "        fog = 1.0 - exp2(-8.0 * fog * fog);\n"
            "        break;\n"
            "      case 6u:\n"
            "        fog = exp2(-8.0 * (1.0 - fog));\n"
            "        break;\n"
            "      case 7u:\n"
            "        fog = 1.0 - fog;\n"
            "        fog = exp2(-8.0 * fog * fog);\n"
            "        break;\n"
            "      }\n"
            "    }\n"
            "\n"
            "    int ifog = iround(fog * 256.0);\n"
            "    TevResult.rgb = (TevResult.rgb * (256 - ifog) + " I_FOGCOLOR ".rgb * ifog) >> 8;\n"
            "  }\n"
            "\n");

  // D3D requires that the shader outputs be uint when writing to a uint render target for logic op.
  if (ApiType == APIType::D3D && uid_data->uint_output)
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
    out.Write("    ocol0.a = float(%s >> 2) / 63.0;\n",
              BitfieldExtract("bpmem_dstalpha", ConstantAlpha().alpha).c_str());
    out.Write("  else\n"
              "    ocol0.a = float(TevResult.a >> 2) / 63.0;\n"
              "  \n");

    if (use_dual_source)
    {
      out.Write("  // Dest alpha override (dual source blending)\n"
                "  // Colors will be blended against the alpha from ocol1 and\n"
                "  // the alpha from ocol0 will be written to the framebuffer.\n"
                "  ocol1 = float4(0.0, 0.0, 0.0, float(TevResult.a) / 255.0);\n");
    }
  }

  if (bounding_box)
  {
    const char* atomic_op =
        (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan) ? "atomic" : "Interlocked";
    out.Write("  if (bpmem_bounding_box) {\n");
    out.Write("    if(bbox_data[0] > int(rawpos.x)) %sMin(bbox_data[0], int(rawpos.x));\n",
              atomic_op);
    out.Write("    if(bbox_data[1] < int(rawpos.x)) %sMax(bbox_data[1], int(rawpos.x));\n",
              atomic_op);
    out.Write("    if(bbox_data[2] > int(rawpos.y)) %sMin(bbox_data[2], int(rawpos.y));\n",
              atomic_op);
    out.Write("    if(bbox_data[3] < int(rawpos.y)) %sMax(bbox_data[3], int(rawpos.y));\n",
              atomic_op);
    out.Write("  }\n");
  }

  out.Write("}\n"
            "\n"
            "int4 getRasColor(State s, StageState ss, float4 colors_0, float4 colors_1) {\n"
            "  // Select Ras for stage\n"
            "  uint ras = %s;\n",
            BitfieldExtract("ss.order", TwoTevStageOrders().colorchan0).c_str());
  out.Write("  if (ras < 2u) { // Lighting Channel 0 or 1\n"
            "    int4 color = iround(((ras == 0u) ? colors_0 : colors_1) * 255.0);\n"
            "    uint swap = %s;\n",
            BitfieldExtract("ss.ac", TevStageCombiner().alphaC.rswap).c_str());
  out.Write("    return Swizzle(swap, color);\n");
  out.Write("  } else if (ras == 5u) { // Alpha Bumb\n"
            "    return int4(s.AlphaBump, s.AlphaBump, s.AlphaBump, s.AlphaBump);\n"
            "  } else if (ras == 6u) { // Normalzied Alpha Bump\n"
            "    int normalized = s.AlphaBump | s.AlphaBump >> 5;\n"
            "    return int4(normalized, normalized, normalized, normalized);\n"
            "  } else {\n"
            "    return int4(0, 0, 0, 0);\n"
            "  }\n"
            "}\n"
            "\n"
            "int4 getKonstColor(State s, StageState ss) {\n"
            "  // Select Konst for stage\n"
            "  // TODO: a switch case might be better here than an dynamically"
            "  // indexed uniform lookup\n"
            "  uint tevksel = bpmem_tevksel(ss.stage>>1);\n"
            "  if ((ss.stage & 1u) == 0u)\n"
            "    return int4(konstLookup[%s].rgb, konstLookup[%s].a);\n",
            BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel0).c_str(),
            BitfieldExtract("tevksel", bpmem.tevksel[0].kasel0).c_str());
  out.Write("  else\n"
            "    return int4(konstLookup[%s].rgb, konstLookup[%s].a);\n",
            BitfieldExtract("tevksel", bpmem.tevksel[0].kcsel1).c_str(),
            BitfieldExtract("tevksel", bpmem.tevksel[0].kasel1).c_str());
  out.Write("}\n");

  return out;
}

void EnumeratePixelShaderUids(const std::function<void(const PixelShaderUid&)>& callback)
{
  PixelShaderUid uid;
  std::memset(&uid, 0, sizeof(uid));

  for (u32 texgens = 0; texgens <= 8; texgens++)
  {
    auto* puid = uid.GetUidData<UberShader::pixel_ubershader_uid_data>();
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
          callback(uid);
        }
      }
    }
  }
}
}
