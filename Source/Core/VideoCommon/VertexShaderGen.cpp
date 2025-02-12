// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexShaderGen.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

VertexShaderUid GetVertexShaderUid()
{
  ASSERT(bpmem.genMode.numtexgens == xfmem.numTexGen.numTexGens);
  ASSERT(bpmem.genMode.numcolchans == xfmem.numChan.numColorChans);

  VertexShaderUid out;
  vertex_shader_uid_data* const uid_data = out.GetUidData();
  uid_data->numTexGens = xfmem.numTexGen.numTexGens;
  uid_data->components = VertexLoaderManager::g_current_components;
  uid_data->numColorChans = xfmem.numChan.numColorChans;

  GetLightingShaderUid(uid_data->lighting);

  // transform texcoords
  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    texinfo.sourcerow = xfmem.texMtxInfo[i].sourcerow;
    texinfo.texgentype = xfmem.texMtxInfo[i].texgentype;
    texinfo.inputform = xfmem.texMtxInfo[i].inputform;

    // first transformation
    switch (texinfo.texgentype)
    {
    case TexGenType::EmbossMap:  // calculate tex coords into bump map
      if ((uid_data->components & (VB_HAS_TANGENT | VB_HAS_BINORMAL)) != 0)
      {
        // transform the light dir into tangent space
        texinfo.embosslightshift = xfmem.texMtxInfo[i].embosslightshift;
        texinfo.embosssourceshift = xfmem.texMtxInfo[i].embosssourceshift;
      }
      else
      {
        texinfo.embosssourceshift = xfmem.texMtxInfo[i].embosssourceshift;
      }
      break;
    case TexGenType::Color0:
    case TexGenType::Color1:
      break;
    case TexGenType::Regular:
    default:
      uid_data->texMtxInfo_n_projection |= static_cast<u32>(xfmem.texMtxInfo[i].projection.Value())
                                           << i;
      break;
    }

    uid_data->dualTexTrans_enabled = xfmem.dualTexTrans.enabled;
    // CHECKME: does this only work for regular tex gen types?
    if (uid_data->dualTexTrans_enabled && texinfo.texgentype == TexGenType::Regular)
    {
      auto& postInfo = uid_data->postMtxInfo[i];
      postInfo.index = xfmem.postMtxInfo[i].index;
      postInfo.normalize = xfmem.postMtxInfo[i].normalize;
    }
  }

  return out;
}

namespace VertexShader
{
void WriteTransforms(APIType api_type, const ShaderHostConfig& host_config,
                     const vertex_shader_uid_data* uid_data, ShaderCode& out)
{
  out.Write("vec4 dolphin_transform_position(vec4 rawpos)\n");
  out.Write("{{\n");
  if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
  {
    // Vertex format has a per-vertex matrix
    out.Write("\tint posidx = int(posmtx.r);\n"
              "\tvec4 P0 = " I_TRANSFORMMATRICES "[posidx];\n"
              "\tvec4 P1 = " I_TRANSFORMMATRICES "[posidx + 1];\n"
              "\tvec4 P2 = " I_TRANSFORMMATRICES "[posidx + 2];\n");
  }
  else
  {
    // One shared matrix
    out.Write("\tvec4 P0 = " I_POSNORMALMATRIX "[0];\n"
              "\tvec4 P1 = " I_POSNORMALMATRIX "[1];\n"
              "\tvec4 P2 = " I_POSNORMALMATRIX "[2];\n");
  }
  out.Write("\t// Multiply the position vector by the position matrix\n"
            "\treturn vec4(dot(P0, rawpos), dot(P1, rawpos), dot(P2, rawpos), 1.0);\n");
  out.Write("}}\n\n");

  out.Write("vec4 dolphin_project_position(vec4 pos)\n");
  out.Write("{{\n");
  out.Write("\treturn vec4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION
            "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n");
  out.Write("}}\n\n");

  out.Write("vec3 dolphin_transform_normal(vec3 norm)\n");
  out.Write("{{\n");

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
    {
      // Vertex format has a per-vertex matrix
      out.Write("\tint posidx = int(posmtx.r);\n");
      out.Write("\tint normidx = posidx & 31;\n"
                "\tvec3 N0 = " I_NORMALMATRICES "[normidx].xyz;\n"
                "\tvec3 N1 = " I_NORMALMATRICES "[normidx + 1].xyz;\n"
                "\tvec3 N2 = " I_NORMALMATRICES "[normidx + 2].xyz;\n");
    }
    else
    {
      // One shared matrix
      out.Write("\tvec3 N0 = " I_POSNORMALMATRIX "[3].xyz;\n"
                "\tvec3 N1 = " I_POSNORMALMATRIX "[4].xyz;\n"
                "\tvec3 N2 = " I_POSNORMALMATRIX "[5].xyz;\n");
    }
    // The scale of the transform matrix is used to control the size of the emboss map effect, by
    // changing the scale of the transformed binormals (which only get used by emboss map texgens).
    // By normalising the first transformed normal (which is used by lighting calculations and needs
    // to be unit length), the same transform matrix can do double duty, scaling for emboss mapping,
    // and not scaling for lighting.
    out.Write("\treturn normalize(vec3(dot(N0, norm), dot(N1, norm), dot(N2, "
              "norm)));\n");
  }
  else
  {
    out.Write("\treturn norm;\n");
  }

  out.Write("}}\n\n");

  out.Write("vec3 dolphin_transform_binormal(vec3 binormal)\n");
  out.Write("{{\n");

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
    {
      // Vertex format has a per-vertex matrix
      out.Write("\tint posidx = int(posmtx.r);\n");
      out.Write("\tint normidx = posidx & 31;\n"
                "\tvec3 N0 = " I_NORMALMATRICES "[normidx].xyz;\n"
                "\tvec3 N1 = " I_NORMALMATRICES "[normidx + 1].xyz;\n"
                "\tvec3 N2 = " I_NORMALMATRICES "[normidx + 2].xyz;\n");
    }
    else
    {
      // One shared matrix
      out.Write("\tvec3 N0 = " I_POSNORMALMATRIX "[3].xyz;\n"
                "\tvec3 N1 = " I_POSNORMALMATRIX "[4].xyz;\n"
                "\tvec3 N2 = " I_POSNORMALMATRIX "[5].xyz;\n");
    }

    // The scale of the transform matrix is used to control the size of the emboss map effect, by
    // changing the scale of the transformed binormals (which only get used by emboss map texgens).
    // By normalising the first transformed normal (which is used by lighting calculations and needs
    // to be unit length), the same transform matrix can do double duty, scaling for emboss mapping,
    // and not scaling for lighting.
    out.Write("\treturn vec3(dot(N0, binormal), dot(N1, binormal), dot(N2, "
              "binormal));\n");
  }
  else
  {
    out.Write("\treturn vec3(0, 0, 0);\n");
  }

  out.Write("}}\n\n");

  out.Write("vec3 dolphin_transform_tangent(vec3 tangent)\n");
  out.Write("{{\n");

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
    {
      // Vertex format has a per-vertex matrix
      out.Write("\tint posidx = int(posmtx.r);\n");
      out.Write("\tint normidx = posidx & 31;\n"
                "\tvec3 N0 = " I_NORMALMATRICES "[normidx].xyz;\n"
                "\tvec3 N1 = " I_NORMALMATRICES "[normidx + 1].xyz;\n"
                "\tvec3 N2 = " I_NORMALMATRICES "[normidx + 2].xyz;\n");
    }
    else
    {
      // One shared matrix
      out.Write("\tvec3 N0 = " I_POSNORMALMATRIX "[3].xyz;\n"
                "\tvec3 N1 = " I_POSNORMALMATRIX "[4].xyz;\n"
                "\tvec3 N2 = " I_POSNORMALMATRIX "[5].xyz;\n");
    }

    // The scale of the transform matrix is used to control the size of the emboss map effect, by
    // changing the scale of the transformed binormals (which only get used by emboss map texgens).
    // By normalising the first transformed normal (which is used by lighting calculations and needs
    // to be unit length), the same transform matrix can do double duty, scaling for emboss mapping,
    // and not scaling for lighting.
    out.Write("\treturn vec3(dot(N0, tangent), dot(N1, tangent), dot(N2, "
              "tangent));\n");
  }
  else
  {
    out.Write("\treturn vec3(0, 0, 0);\n");
  }

  out.Write("}}\n\n");

  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];
    out.Write("vec3 dolphin_transform_texcoord{}(vec4 coord)\n", i);
    out.Write("{{\n");
    if (texinfo.texgentype != TexGenType::Regular)
    {
      out.Write("\treturn vec3(coord.xyz);\n");
    }
    else
    {
      out.Write("\tvec3 result;\n");
      if ((uid_data->components & (VB_HAS_TEXMTXIDX0 << i)) != 0)
      {
        out.Write("\tint tmp = int(rawtex{}.z);\n", i);
        if (static_cast<TexSize>((uid_data->texMtxInfo_n_projection >> i) & 1) == TexSize::STQ)
        {
          out.Write("\tresult = vec3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES
                    "[tmp+1]), dot(coord, " I_TRANSFORMMATRICES "[tmp+2]));\n");
        }
        else
        {
          out.Write("\tresult = vec3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES "[tmp+1]), 1);\n");
        }
      }
      else
      {
        if (static_cast<TexSize>((uid_data->texMtxInfo_n_projection >> i) & 1) == TexSize::STQ)
        {
          out.Write("\tresult = vec3(dot(coord, " I_TEXMATRICES "[{}]), dot(coord, " I_TEXMATRICES
                    "[{}]), dot(coord, " I_TEXMATRICES "[{}]));\n",
                    3 * i, 3 * i + 1, 3 * i + 2);
        }
        else
        {
          out.Write("\tresult = vec3(dot(coord, " I_TEXMATRICES "[{}]), dot(coord, " I_TEXMATRICES
                    "[{}]), 1);\n",
                    3 * i, 3 * i + 1);
        }
      }
      // CHECKME: does this only work for regular tex gen types?
      if (uid_data->dualTexTrans_enabled)
      {
        auto& postInfo = uid_data->postMtxInfo[i];

        out.Write("\tvec4 P0 = " I_POSTTRANSFORMMATRICES "[{}];\n"
                  "\tvec4 P1 = " I_POSTTRANSFORMMATRICES "[{}];\n"
                  "\tvec4 P2 = " I_POSTTRANSFORMMATRICES "[{}];\n",
                  postInfo.index & 0x3f, (postInfo.index + 1) & 0x3f, (postInfo.index + 2) & 0x3f);

        if (postInfo.normalize)
          out.Write("\tresult = normalize(result);\n");

        // multiply by postmatrix
        out.Write("\tresult = vec3(dot(P0.xyz, result) + P0.w, dot(P1.xyz, result) + "
                  "P1.w, dot(P2.xyz, result) + P2.w);\n");
      }

      // When q is 0, the GameCube appears to have a special case
      // This can be seen in devkitPro's neheGX Lesson08 example for Wii
      // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
      // TODO: check if this only affects XF_TEXGEN_REGULAR
      out.Write("\tif(result.z == 0.0f)\n"
                "\t\tresult.xy = clamp(result.xy / 2.0f, vec2(-1.0f,-1.0f), vec2(1.0f,1.0f));\n");
      out.Write("\treturn result;\n");
    }
    out.Write("}}\n\n");
  }
}

void WriteHeader(APIType api_type, const ShaderHostConfig& host_config,
                 const vertex_shader_uid_data* uid_data, ShaderCode& out, ShaderCode& input_extract)
{
  out.Write("{}", s_lighting_struct);

  // uniforms
  out.Write("UBO_BINDING(std140, 2) uniform VSBlock {{\n");

  out.Write("{}", s_shader_uniforms);
  out.Write("}};\n");

  if (uid_data->vs_expand != VSExpand::None)
  {
    out.Write("UBO_BINDING(std140, 4) uniform GSBlock {{\n");
    out.Write("{}", s_geometry_shader_uniforms);
    out.Write("}};\n");

    if (api_type == APIType::D3D)
    {
      // D3D doesn't include the base vertex in SV_VertexID
      out.Write("UBO_BINDING(std140, 5) uniform DX_Constants {{\n"
                "  uint base_vertex;\n"
                "}};\n\n");
    }
  }

  out.Write("struct VS_OUTPUT {{\n");
  GenerateVSOutputMembers(out, api_type, uid_data->numTexGens, host_config, "",
                          ShaderStage::Vertex);
  out.Write("}};\n\n");

  WriteIsNanHeader(out, api_type);

  GenerateLightingShaderHeader(out, uid_data->lighting);

  if (uid_data->vs_expand == VSExpand::None)
  {
    out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawpos;\n", ShaderAttrib::Position);
    if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in uint4 posmtx;\n", ShaderAttrib::PositionMatrix);
    if ((uid_data->components & VB_HAS_NORMAL) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawnormal;\n", ShaderAttrib::Normal);
    if ((uid_data->components & VB_HAS_TANGENT) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawtangent;\n", ShaderAttrib::Tangent);
    if ((uid_data->components & VB_HAS_BINORMAL) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float3 rawbinormal;\n", ShaderAttrib::Binormal);

    if ((uid_data->components & VB_HAS_COL0) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawcolor0;\n", ShaderAttrib::Color0);
    if ((uid_data->components & VB_HAS_COL1) != 0)
      out.Write("ATTRIBUTE_LOCATION({:s}) in float4 rawcolor1;\n", ShaderAttrib::Color1);

    for (u32 i = 0; i < 8; ++i)
    {
      const u32 has_texmtx = (uid_data->components & (VB_HAS_TEXMTXIDX0 << i));

      if ((uid_data->components & (VB_HAS_UV0 << i)) != 0 || has_texmtx != 0)
      {
        out.Write("ATTRIBUTE_LOCATION({:s}) in float{} rawtex{};\n", ShaderAttrib::TexCoord0 + i,
                  has_texmtx != 0 ? 3 : 2, i);
      }
    }
  }
  else
  {
    // Can't use float3, etc because we want 4-byte alignment
    out.Write(
        "uint4 unpack_ubyte4(uint value) {{\n"
        "  return uint4(value & 0xffu, (value >> 8) & 0xffu, (value >> 16) & 0xffu, value >> 24);\n"
        "}}\n\n"
        "struct InputData {{\n");
    if (uid_data->components & VB_HAS_POSMTXIDX)
    {
      out.Write("  uint posmtx;\n");
      input_extract.Write("uint4 posmtx = unpack_ubyte4(i.posmtx);\n");
    }
    if (uid_data->position_has_3_elems)
    {
      out.Write("  float pos0;\n"
                "  float pos1;\n"
                "  float pos2;\n");
      input_extract.Write("float4 rawpos = float4(i.pos0, i.pos1, i.pos2, 1.0f);\n");
    }
    else
    {
      out.Write("  float pos0;\n"
                "  float pos1;\n");
      input_extract.Write("float4 rawpos = float4(i.pos0, i.pos1, 0.0f, 1.0f);\n");
    }
    std::array<std::string_view, 3> names = {"normal", "binormal", "tangent"};
    for (int i = 0; i < 3; i++)
    {
      if (uid_data->components & (VB_HAS_NORMAL << i))
      {
        out.Write("  float {0}0;\n"
                  "  float {0}1;\n"
                  "  float {0}2;\n",
                  names[i]);
        input_extract.Write("float3 raw{0} = float3(i.{0}0, i.{0}1, i.{0}2);\n", names[i]);
      }
    }
    for (int i = 0; i < 2; i++)
    {
      if (uid_data->components & (VB_HAS_COL0 << i))
      {
        out.Write("  uint color{};\n", i);
        input_extract.Write("float4 rawcolor{0} = float4(unpack_ubyte4(i.color{0})) / 255.0f;\n",
                            i);
      }
    }
    for (int i = 0; i < 8; i++)
    {
      if (uid_data->components & (VB_HAS_UV0 << i))
      {
        u32 ncomponents = (uid_data->texcoord_elem_count >> (2 * i)) & 3;
        if (ncomponents < 2)
        {
          out.Write("  float tex{};\n", i);
          input_extract.Write("float3 rawtex{0} = float3(i.tex{0}, 0.0f, 0.0f);\n", i);
        }
        else if (ncomponents == 2)
        {
          out.Write("  float tex{0}_0;\n"
                    "  float tex{0}_1;\n",
                    i);
          input_extract.Write("float3 rawtex{0} = float3(i.tex{0}_0, i.tex{0}_1, 0.0f);\n", i);
        }
        else
        {
          out.Write("  float tex{0}_0;\n"
                    "  float tex{0}_1;\n"
                    "  float tex{0}_2;\n",
                    i);
          input_extract.Write("float3 rawtex{0} = float3(i.tex{0}_0, i.tex{0}_1, i.tex{0}_2);\n",
                              i);
        }
      }
    }
    out.Write("}};\n\n"
              "SSBO_BINDING(1) readonly restrict buffer InputBuffer {{\n"
              "  InputData input_buffer[];\n"
              "}};\n\n");
  }

  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool per_pixel_lighting = g_ActiveConfig.bEnablePixelLighting;

  if (host_config.backend_geometry_shaders)
  {
    out.Write("VARYING_LOCATION(0) out VertexData {{\n");
    GenerateVSOutputMembers(out, api_type, uid_data->numTexGens, host_config,
                            GetInterpolationQualifier(msaa, ssaa, true, false),
                            ShaderStage::Vertex);
    out.Write("}} vs;\n");
  }
  else
  {
    // Let's set up attributes
    u32 counter = 0;
    out.Write("VARYING_LOCATION({}) {} out float4 colors_0;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    out.Write("VARYING_LOCATION({}) {} out float4 colors_1;\n", counter++,
              GetInterpolationQualifier(msaa, ssaa));
    for (u32 i = 0; i < uid_data->numTexGens; ++i)
    {
      out.Write("VARYING_LOCATION({}) {} out float3 tex{};\n", counter++,
                GetInterpolationQualifier(msaa, ssaa), i);
    }
    if (!host_config.fast_depth_calc)
    {
      out.Write("VARYING_LOCATION({}) {} out float4 clipPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
    if (per_pixel_lighting)
    {
      out.Write("VARYING_LOCATION({}) {} out float3 Normal;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
      out.Write("VARYING_LOCATION({}) {} out float3 WorldPos;\n", counter++,
                GetInterpolationQualifier(msaa, ssaa));
    }
  }

  // Write the transforms after so they can use the position matrix if it's available
  // or should the functions take the posmtx too?
  WriteTransforms(api_type, host_config, uid_data, out);

  // TODO: move...
  out.Write("vec4 dolphin_pixel_correction(vec4 pos)\n");
  out.Write("{{\n");

  // Write the true depth value. If the game uses depth textures, then the pixel shader will
  // override it with the correct values if not then early z culling will improve speed.
  // There are two different ways to do this, when the depth range is oversized, we process
  // the depth range in the vertex shader, if not we let the host driver handle it.
  //
  // Adjust z for the depth range. We're using an equation which incorperates a depth inversion,
  // so we can map the console -1..0 range to the 0..1 range used in the depth buffer.
  // We have to handle the depth range in the vertex shader instead of after the perspective
  // divide, because some games will use a depth range larger than what is allowed by the
  // graphics API. These large depth ranges will still be clipped to the 0..1 range, so these
  // games effectively add a depth bias to the values written to the depth buffer.
  out.Write("\tpos.z = pos.w * " I_PIXELCENTERCORRECTION ".w - "
            "\tpos.z * " I_PIXELCENTERCORRECTION ".z;\n");

  if (!host_config.backend_clip_control)
  {
    // If the graphics API doesn't support a depth range of 0..1, then we need to map z to
    // the -1..1 range. Unfortunately we have to use a substraction, which is a lossy
    // floating-point operation that can introduce a round-trip error.
    out.Write("\tpos.z = pos.z * 2.0 - pos.w;\n");
  }

  // Correct for negative viewports by mirroring all vertices. We need to negate the height here,
  // since the viewport height is already negated by the render backend.
  out.Write("\tpos.xy *= sign(" I_PIXELCENTERCORRECTION ".xy * float2(1.0, -1.0));\n");

  // The console GPU places the pixel center at 7/12 in screen space unless
  // antialiasing is enabled, while D3D and OpenGL place it at 0.5. This results
  // in some primitives being placed one pixel too far to the bottom-right,
  // which in turn can be critical if it happens for clear quads.
  // Hence, we compensate for this pixel center difference so that primitives
  // get rasterized correctly.
  out.Write("\tpos.xy = pos.xy - pos.w * " I_PIXELCENTERCORRECTION ".xy;\n");

  const bool vertex_rounding = host_config.vertex_rounding;
  if (vertex_rounding)
  {
    // By now our position is in clip space
    // however, higher resolutions than the Wii outputs
    // cause an additional pixel offset
    // due to a higher pixel density
    // we need to correct this by converting our
    // clip-space position into the Wii's screen-space
    // acquire the right pixel and then convert it back
    out.Write("\tif (pos.w == 1.0f)\n"
              "\t{{\n"

              "\t\tfloat ss_pixel_x = ((pos.x + 1.0f) * (" I_VIEWPORT_SIZE ".x * 0.5f));\n"
              "\t\tfloat ss_pixel_y = ((pos.y + 1.0f) * (" I_VIEWPORT_SIZE ".y * 0.5f));\n"

              "\t\tss_pixel_x = round(ss_pixel_x);\n"
              "\t\tss_pixel_y = round(ss_pixel_y);\n"

              "\t\tpos.x = ((ss_pixel_x / (" I_VIEWPORT_SIZE ".x * 0.5f)) - 1.0f);\n"
              "\t\tpos.y = ((ss_pixel_y / (" I_VIEWPORT_SIZE ".y * 0.5f)) - 1.0f);\n"
              "\t}}\n");
  }

  out.Write("\treturn pos;\n");

  out.Write("}}\n");
}

void WriteEmulatedVertexBodyHeader(APIType api_type, const ShaderHostConfig& host_config,
                                   const vertex_shader_uid_data* uid_data, ShaderCode& out)
{
  constexpr std::string_view emulated_vertex_definition =
      "void dolphin_emulated_vertex(in DolphinVertexInput vertex_input, out DolphinVertexOutput "
      "vertex_output)";
  out.Write("{}\n", emulated_vertex_definition);
  out.Write("{{\n");

  WriteVertexBody(api_type, host_config, uid_data, out);

  out.Write("}}\n");
}

void WriteVertexStructs(APIType api_type, const ShaderHostConfig& host_config,
                        const vertex_shader_uid_data* uid_data, ShaderCode& out)
{
  out.Write("struct DolphinVertexInput\n");
  out.Write("{{\n");
  out.Write("\tvec4 color_0;\n");
  out.Write("\tvec4 color_1;\n");
  out.Write("\tvec4 position;\n");
  out.Write("\tvec3 normal;\n");
  out.Write("\tvec3 binormal;\n");
  out.Write("\tvec3 tangent;\n");
  for (u32 i = 0; i < uid_data->numTexGens; i++)
  {
    out.Write("\tvec4 texture_coord_{};\n", i);
  }
  for (u32 i = uid_data->numTexGens; i < 8; i++)
  {
    out.Write("\tvec4 texture_coord_{};\n", i);
  }
  out.Write("}};\n\n");

  out.Write("struct DolphinVertexOutput\n");
  out.Write("{{\n");
  out.Write("\tvec4 color_0;\n");
  out.Write("\tvec4 color_1;\n");
  out.Write("\tvec4 position;\n");
  out.Write("\tvec3 normal;\n");
  for (u32 i = 0; i < uid_data->numTexGens; i++)
  {
    out.Write("\tvec3 texture_coord_{};\n", i);
  }
  for (u32 i = uid_data->numTexGens; i < 8; i++)
  {
    out.Write("\tvec3 texture_coord_{};\n", i);
  }
  out.Write("}};\n\n");
}

void WriteVertexDefines(APIType, const ShaderHostConfig&, const vertex_shader_uid_data* uid_data,
                        ShaderCode& out)
{
  if ((uid_data->components & VB_HAS_COL0) != 0)
  {
    out.Write("#define HAS_COLOR_0 1\n");
  }

  if ((uid_data->components & VB_HAS_COL1) != 0)
  {
    out.Write("#define HAS_COLOR_1 1\n");
  }

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    out.Write("#define HAS_NORMAL 1\n");
  }

  if ((uid_data->components & VB_HAS_BINORMAL) != 0)
  {
    out.Write("#define HAS_BINORMAL 1\n");
  }

  if ((uid_data->components & VB_HAS_TANGENT) != 0)
  {
    out.Write("#define HAS_TANGENT 1\n");
  }

  for (u32 i = 0; i < uid_data->numTexGens; i++)
  {
    if ((uid_data->components & (VB_HAS_UV0 << i)) != 0)
    {
      out.Write("#define HAS_TEXTURE_COORD_{} 1\n", i);
    }
  }

  for (u32 i = uid_data->numTexGens; i < 8; i++)
  {
    out.Write("#define HAS_TEXTURE_COORD_{} 0\n", i);
  }
}

void WriteVertexBody(APIType api_type, const ShaderHostConfig& host_config,
                     const vertex_shader_uid_data* uid_data, ShaderCode& out)
{
  out.Write("\tvertex_output.position = dolphin_transform_position(vertex_input.position);\n");

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    out.Write("\tvertex_output.normal = dolphin_transform_normal(vertex_input.normal);\n");
  }
  else
  {
    out.Write("\tvertex_output.normal = vec3(0, 0, 0);\n");
  }

  const bool has_color0_texture_coordinate =
      std::ranges::any_of(uid_data->texMtxInfo, [](const auto& texinfo) {
        return texinfo.texgentype == TexGenType::Color0;
      });

  const bool has_color1_texture_coordinate =
      std::ranges::any_of(uid_data->texMtxInfo, [](const auto& texinfo) {
        return texinfo.texgentype == TexGenType::Color1;
      });

  const bool per_pixel_lighting = host_config.per_pixel_lighting;
  if (per_pixel_lighting)
  {
    // When per-pixel lighting is enabled, the vertex colors are passed through
    // unmodified so we can evaluate the lighting in the pixel shader.
    out.Write("\tvertex_output.color_0 = vertex_input.color_0;\n");
    out.Write("\tvertex_output.color_1 = vertex_input.color_1;\n");
    // Note that the numColorChans logic is performed in the pixel shader.

    // We may still need to calculate the lighting per vertex if the vertex
    // shader generates texture coordinates with this information
    if (has_color0_texture_coordinate)
    {
      out.Write("\tvec4 vertex_lighting_0 = dolphin_calculate_lighting_chn0(vertex_input.color_0, "
                "vertex_input.position, "
                "vertex_input.normal);\n");
    }
    if (has_color1_texture_coordinate)
    {
      out.Write("\tvec4 vertex_lighting_1 = dolphin_calculate_lighting_chn1(vertex_input.color_1, "
                "vertex_input.position, "
                "vertex_input.normal);\n");
    }
  }
  else
  {
    if (uid_data->numColorChans > 0)
    {
      out.Write("\tvec4 vertex_lighting_0 = dolphin_calculate_lighting_chn0(vertex_input.color_0, "
                "vertex_input.position, "
                "vertex_input.normal);\n");
      out.Write("\tvertex_output.color_0 = vertex_lighting_0;\n");
    }
    else
    {
      // The number of colors available to TEV is determined by numColorChans.
      // We have to provide the fields to match the interface, so set to zero if it's not enabled.
      out.Write("\tvertex_output.color_0 = vec4(0.0, 0.0, 0.0, 0.0);\n");
      if (has_color0_texture_coordinate)
      {
        out.Write("\tvec4 vertex_lighting_0 = "
                  "dolphin_calculate_lighting_chn0(vertex_input.color_0, vertex_input.position,"
                  "vertex_input.normal);\n");
      }
    }

    if (uid_data->numColorChans == 2)
    {
      out.Write("\tvec4 vertex_lighting_1 = dolphin_calculate_lighting_chn1(vertex_input.color_1, "
                "vertex_input.position, "
                "vertex_input.normal);\n");
      out.Write("\tvertex_output.color_1 = vertex_lighting_1;\n");
    }
    else
    {
      // The number of colors available to TEV is determined by numColorChans.
      // We have to provide the fields to match the interface, so set to zero if it's not enabled.
      out.Write("\tvertex_output.color_1 = vec4(0.0, 0.0, 0.0, 0.0);\n");
      if (has_color1_texture_coordinate)
      {
        out.Write("\tvec4 vertex_lighting_1 = "
                  "dolphin_calculate_lighting_chn1(vertex_input.color_1, vertex_input.position,"
                  "normal);\n");
      }
    }
  }

  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    switch (texinfo.texgentype)
    {
    case TexGenType::EmbossMap:  // calculate tex coords into bump map

      // transform the light dir into tangent space
      out.Write("\tvec3 ldir = normalize(" LIGHT_POS ".xyz - vertex_input.position.xyz);\n",
                LIGHT_POS_PARAMS(texinfo.embosslightshift));

      if ((uid_data->components & VB_HAS_TANGENT) == 0)
        out.Write("\tvec3 rawtangent = " I_CACHED_TANGENT ".xyz;\n");
      else
        out.Write("\tvec3 rawtangent = vertex_input.tangent;\n");

      if ((uid_data->components & VB_HAS_BINORMAL) == 0)
        out.Write("\tvec3 rawbinormal = " I_CACHED_BINORMAL ".xyz;\n");
      else
        out.Write("\tvec3 rawbinormal = vertex_input.binormal;\n");

      out.Write("\tvertex_output.texture_coord_{}.xyz = vertex_output.texture_coord_{}.xyz + "
                "vec3(dot(ldir, "
                "dolphin_transform_tangent(rawtangent)), "
                "dot(ldir, dolphin_transform_binormal(rawbinormal)), 0.0);\n",
                i, texinfo.embosssourceshift);

      break;
    case TexGenType::Color0:
      out.Write("\tvertex_output.texture_coord_{}.xyz = vec3(vertex_lighting_0.x, "
                "vertex_lighting_0.y, 1);\n",
                i);
      break;
    case TexGenType::Color1:
      out.Write("\tvertex_output.texture_coord_{}.xyz = vec3(vertex_lighting_1.x, "
                "vertex_lighting_1.y, 1);\n",
                i);
      break;
    case TexGenType::Regular:
      out.Write("\tvertex_output.texture_coord_{0} = "
                "dolphin_transform_texcoord{0}(vertex_input.texture_coord_{0});\n",
                i);
      break;
    };
  }

  // Fill out output that is unused
  for (u32 i = uid_data->numTexGens; i < 8; i++)
  {
    out.Write("\tvertex_output.texture_coord_{0} = vec3(0, 0, 0);\n", i);
  }
}

ShaderCode WriteFullShader(APIType api_type, const ShaderHostConfig& host_config,
                           const vertex_shader_uid_data* uid_data, std::string_view custom_vertex,
                           std::string_view custom_uniforms)
{
  ShaderCode out;

  const bool per_pixel_lighting = g_ActiveConfig.bEnablePixelLighting;

  ShaderCode input_extract;

  WriteHeader(api_type, host_config, uid_data, out, input_extract);

  WriteVertexStructs(api_type, host_config, uid_data, out);
  WriteVertexDefines(api_type, host_config, uid_data, out);

  if (!custom_uniforms.empty())
  {
    out.Write("UBO_BINDING(std140, 3) uniform CustomShaderBlock {{\n");
    out.Write("{}", custom_uniforms);
    out.Write("}} custom_uniforms;\n");
  }

  WriteEmulatedVertexBodyHeader(api_type, host_config, uid_data, out);

  if (custom_vertex.empty())
  {
    out.Write("{}\n", vertex_definition);
    out.Write("{{\n");

    out.Write("\tdolphin_emulated_vertex(vertex_input, vertex_output);\n");

    out.Write("}}\n");
  }
  else
  {
    out.Write("{}\n", custom_vertex);
  }

  out.Write("void main()\n{{\n");

  if (uid_data->vs_expand != VSExpand::None)
  {
    out.Write("\tbool is_bottom = (gl_VertexID & 2) != 0;\n"
              "\tbool is_right = (gl_VertexID & 1) != 0;\n");
    // D3D doesn't include the base vertex in SV_VertexID
    // See comment in UberShaderVertex for details
    if (api_type == APIType::D3D)
      out.Write("\tuint vertex_id = (gl_VertexID >> 2) + base_vertex;\n");
    else
      out.Write("\tuint vertex_id = uint(gl_VertexID) >> 2u;\n");
    out.Write("\tInputData i = input_buffer[vertex_id];\n"
              "{}",
              input_extract.GetBuffer());
  }

  out.Write("\tVS_OUTPUT o;\n");

  // xfmem.numColorChans controls the number of color channels available to TEV, but we still need
  // to generate all channels here, as it can be used in texgen. Cel-damage is an example of this.
  out.Write("\tvec4 vertex_color_0, vertex_color_1;\n");

  // To use color 1, the vertex descriptor must have color 0 and 1.
  // If color 1 is present but not color 0, it is used for lighting channel 0.
  const bool use_color_1 =
      (uid_data->components & (VB_HAS_COL0 | VB_HAS_COL1)) == (VB_HAS_COL0 | VB_HAS_COL1);
  for (u32 color = 0; color < NUM_XF_COLOR_CHANNELS; color++)
  {
    if ((color == 0 || use_color_1) && (uid_data->components & (VB_HAS_COL0 << color)) != 0)
    {
      // Use color0 for channel 0, and color1 for channel 1 if both colors 0 and 1 are present.
      out.Write("\tvertex_color_{0} = rawcolor{0};\n", color);
    }
    else if (color == 0 && (uid_data->components & VB_HAS_COL1) != 0)
    {
      // Use color1 for channel 0 if color0 is not present.
      out.Write("\tvertex_color_{} = rawcolor1;\n", color);
    }
    else
    {
      out.Write("\tvertex_color_{0} = missing_color_value;\n", color);
    }
  }

  out.Write("\tDolphinVertexInput vertex_input;\n");
  out.Write("\tvertex_input.color_0 = vertex_color_0;\n");
  out.Write("\tvertex_input.color_1 = vertex_color_1;\n");
  out.Write("\tvertex_input.position = rawpos;\n");

  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    out.Write("\tvertex_input.normal = rawnormal;\n");
  }
  else
  {
    out.Write("\tvertex_input.normal = vec3(0, 0, 0);\n");
  }

  if ((uid_data->components & VB_HAS_BINORMAL) != 0)
  {
    out.Write("\tvertex_input.binormal = rawbinormal;\n");
  }
  else
  {
    out.Write("\tvertex_input.binormal = vec3(0, 0, 0);\n");
  }

  if ((uid_data->components & VB_HAS_TANGENT) != 0)
  {
    out.Write("\tvertex_input.tangent = rawtangent;\n");
  }
  else
  {
    out.Write("\tvertex_input.tangent = vec3(0, 0, 0);\n");
  }

  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    out.Write("\t{{\n");
    out.Write("\t\tvec4 coord = vec4(0.0, 0.0, 1.0, 1.0);\n");
    switch (texinfo.sourcerow)
    {
    case SourceRow::Geom:
      out.Write("\t\tcoord.xyz = rawpos.xyz;\n");
      break;
    case SourceRow::Normal:
      if ((uid_data->components & VB_HAS_NORMAL) != 0)
      {
        out.Write("\t\tcoord.xyz = rawnormal.xyz;\n");
      }
      break;
    case SourceRow::Colors:
      ASSERT(texinfo.texgentype == TexGenType::Color0 || texinfo.texgentype == TexGenType::Color1);
      break;
    case SourceRow::BinormalT:
      if ((uid_data->components & VB_HAS_TANGENT) != 0)
      {
        out.Write("\t\tcoord.xyz = rawtangent.xyz;\n");
      }
      break;
    case SourceRow::BinormalB:
      if ((uid_data->components & VB_HAS_BINORMAL) != 0)
      {
        out.Write("\t\tcoord.xyz = rawbinormal.xyz;\n");
      }
      break;
    default:
      ASSERT(texinfo.sourcerow >= SourceRow::Tex0 && texinfo.sourcerow <= SourceRow::Tex7);
      u32 texnum = static_cast<u32>(texinfo.sourcerow) - static_cast<u32>(SourceRow::Tex0);
      if ((uid_data->components & (VB_HAS_UV0 << (texnum))) != 0)
      {
        out.Write("\t\tcoord = vec4(rawtex{}.x, rawtex{}.y, 1.0, 1.0);\n", texnum, texnum);
      }
      break;
    }
    // Input form of AB11 sets z element to 1.0

    if (texinfo.inputform == TexInputForm::AB11)
      out.Write("\t\tcoord.z = 1.0;\n");

    // Convert NaNs to 1 - needed to fix eyelids in Shadow the Hedgehog during cutscenes
    // See https://bugs.dolphin-emu.org/issues/11458
    out.Write("\t\t// Convert NaN to 1\n");
    out.Write("\t\tif (dolphin_isnan(coord.x)) coord.x = 1.0;\n");
    out.Write("\t\tif (dolphin_isnan(coord.y)) coord.y = 1.0;\n");
    out.Write("\t\tif (dolphin_isnan(coord.z)) coord.z = 1.0;\n");

    out.Write("\t\tvertex_input.texture_coord_{0} = coord;\n", i);
    out.Write("\t}}\n");
  }

  // Initialize other texture coordinates that are unused
  for (u32 i = uid_data->numTexGens; i < 8; i++)
  {
    out.Write("\tvertex_input.texture_coord_{0} = vec4(0, 0, 0, 0);\n", i);
  }

  out.Write("\tDolphinVertexOutput vertex_output;\n");
  out.Write("\tvertex(vertex_input, vertex_output);\n");

  out.Write("\to.pos = dolphin_project_position(vertex_output.position);\n");
  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    out.Write("\to.tex{0} = vertex_output.texture_coord_{0};\n", i);
  }

  out.Write("\to.colors_0 = vertex_output.color_0;\n");
  out.Write("\to.colors_1 = vertex_output.color_1;\n");
  if (per_pixel_lighting)
  {
    out.Write("\to.Normal = vertex_output.normal;\n");

    // TODO: Rename, this is actually in Viewspace...
    out.Write("\to.WorldPos = vertex_output.position.xyz;\n");
  }

  if (uid_data->vs_expand == VSExpand::Line)
  {
    out.Write("// Line expansion\n"
              "uint other_id = vertex_id;\n"
              "if (is_bottom) {{\n"
              "  other_id -= 1u;\n"
              "}} else {{\n"
              "  other_id += 1u;\n"
              "}}\n"
              "InputData other = input_buffer[other_id];\n");
    if (uid_data->position_has_3_elems)
      out.Write("float4 other_pos = float4(other.pos0, other.pos1, other.pos2, 1.0f);\n");
    else
      out.Write("float4 other_pos = float4(other.pos0, other.pos1, 0.0f, 1.0f);\n");
    if (uid_data->components & VB_HAS_POSMTXIDX)
    {
      out.Write("uint other_posidx = other.posmtx & 0xff;\n"
                "float4 other_p0 = " I_TRANSFORMMATRICES "[other_posidx];\n"
                "float4 other_p1 = " I_TRANSFORMMATRICES "[other_posidx + 1];\n"
                "float4 other_p2 = " I_TRANSFORMMATRICES "[other_posidx + 2];\n"
                "other_pos = float4(dot(other_p0, other_pos), dot(other_p1, other_pos), "
                "dot(other_p2, other_pos), 1.0f);\n");
    }
    else
    {
      out.Write("other_pos = float4(dot(P0, other_pos), dot(P1, other_pos), dot(P2, other_pos), "
                "1.0f);\n");
    }
    GenerateVSLineExpansion(out, "", uid_data->numTexGens);
  }
  else if (uid_data->vs_expand == VSExpand::Point)
  {
    out.Write("// Point expansion\n");
    GenerateVSPointExpansion(out, "", uid_data->numTexGens);
  }

  // clipPos/w needs to be done in pixel shader, not here
  if (!host_config.fast_depth_calc)
    out.Write("\to.clipPos = o.pos;\n");

  // If we can disable the incorrect depth clipping planes using depth clamping, then we can do
  // our own depth clipping and calculate the depth range before the perspective divide if
  // necessary.
  if (host_config.backend_depth_clamp)
  {
    // Since we're adjusting z for the depth range before the perspective divide, we have to do
    // our own clipping. We want to clip so that -w <= z <= 0, which matches the console -1..0
    // range. We adjust our depth value for clipping purposes to match the perspective projection
    // in the software backend, which is a hack to fix Sonic Adventure and Unleashed games.
    out.Write("\tfloat clipDepth = o.pos.z * (1.0 - 1e-7);\n"
              "\tfloat clipDist0 = clipDepth + o.pos.w;\n"  // Near: z < -w
              "\tfloat clipDist1 = -clipDepth;\n");         // Far: z > 0

    if (host_config.backend_geometry_shaders)
    {
      out.Write("\to.clipDist0 = clipDist0;\n"
                "\to.clipDist1 = clipDist1;\n");
    }
  }
  else
  {
    // Same depth adjustment for Sonic. Without depth clamping, it unfortunately
    // affects non-clipping uses of depth too.
    out.Write("\to.pos.z = o.pos.z * (1.0 - 1e-7);\n");
  }

  out.Write("\to.pos = dolphin_pixel_correction(o.pos);\n");

  if (host_config.backend_geometry_shaders)
  {
    AssignVSOutputMembers(out, "vs", "o", uid_data->numTexGens, host_config);
  }
  else
  {
    // TODO: Pass interface blocks between shader stages even if geometry shaders
    // are not supported, however that will require at least OpenGL 3.2 support.
    for (u32 i = 0; i < uid_data->numTexGens; ++i)
      out.Write("\ttex{}.xyz = o.tex{};\n", i, i);
    if (!host_config.fast_depth_calc)
      out.Write("\tclipPos = o.clipPos;\n");
    if (per_pixel_lighting)
    {
      out.Write("\tNormal = o.Normal;\n"
                "\tWorldPos = o.WorldPos;\n");
    }
    out.Write("\tcolors_0 = o.colors_0;\n"
              "\tcolors_1 = o.colors_1;\n");
  }

  if (host_config.backend_depth_clamp)
  {
    out.Write("\tgl_ClipDistance[0] = clipDist0;\n"
              "\tgl_ClipDistance[1] = clipDist1;\n");
  }

  // Vulkan NDC space has Y pointing down (right-handed NDC space).
  if (api_type == APIType::Vulkan)
    out.Write("\tgl_Position = float4(o.pos.x, -o.pos.y, o.pos.z, o.pos.w);\n");
  else
    out.Write("\tgl_Position = o.pos;\n");
  out.Write("}}\n");

  return out;
}
}  // namespace VertexShader
