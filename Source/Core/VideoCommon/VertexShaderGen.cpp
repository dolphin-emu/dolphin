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

ShaderCode GenerateVertexShaderCode(APIType api_type, const ShaderHostConfig& host_config,
                                    const vertex_shader_uid_data* uid_data)
{
  ShaderCode out;

  const bool per_pixel_lighting = g_ActiveConfig.bEnablePixelLighting;
  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool vertex_rounding = host_config.vertex_rounding;

  ShaderCode input_extract;

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

  out.Write("void main()\n{{\n");

  if (uid_data->vs_expand != VSExpand::None)
  {
    out.Write("bool is_bottom = (gl_VertexID & 2) != 0;\n"
              "bool is_right = (gl_VertexID & 1) != 0;\n");
    // D3D doesn't include the base vertex in SV_VertexID
    // See comment in UberShaderVertex for details
    if (api_type == APIType::D3D)
      out.Write("uint vertex_id = (gl_VertexID >> 2) + base_vertex;\n");
    else
      out.Write("uint vertex_id = uint(gl_VertexID) >> 2u;\n");
    out.Write("InputData i = input_buffer[vertex_id];\n"
              "{}",
              input_extract.GetBuffer());
  }

  out.Write("VS_OUTPUT o;\n");

  // xfmem.numColorChans controls the number of color channels available to TEV, but we still need
  // to generate all channels here, as it can be used in texgen. Cel-damage is an example of this.
  out.Write("float4 vertex_color_0, vertex_color_1;\n");

  // To use color 1, the vertex descriptor must have color 0 and 1.
  // If color 1 is present but not color 0, it is used for lighting channel 0.
  const bool use_color_1 =
      (uid_data->components & (VB_HAS_COL0 | VB_HAS_COL1)) == (VB_HAS_COL0 | VB_HAS_COL1);
  for (u32 color = 0; color < NUM_XF_COLOR_CHANNELS; color++)
  {
    if ((color == 0 || use_color_1) && (uid_data->components & (VB_HAS_COL0 << color)) != 0)
    {
      // Use color0 for channel 0, and color1 for channel 1 if both colors 0 and 1 are present.
      out.Write("vertex_color_{0} = rawcolor{0};\n", color);
    }
    else if (color == 0 && (uid_data->components & VB_HAS_COL1) != 0)
    {
      // Use color1 for channel 0 if color0 is not present.
      out.Write("vertex_color_{} = rawcolor1;\n", color);
    }
    else
    {
      out.Write("vertex_color_{0} = missing_color_value;\n", color);
    }
  }

  // transforms
  if ((uid_data->components & VB_HAS_POSMTXIDX) != 0)
  {
    // Vertex format has a per-vertex matrix
    out.Write("int posidx = int(posmtx.r);\n"
              "float4 P0 = " I_TRANSFORMMATRICES "[posidx];\n"
              "float4 P1 = " I_TRANSFORMMATRICES "[posidx + 1];\n"
              "float4 P2 = " I_TRANSFORMMATRICES "[posidx + 2];\n");
    if ((uid_data->components & VB_HAS_NORMAL) != 0)
    {
      out.Write("int normidx = posidx & 31;\n"
                "float3 N0 = " I_NORMALMATRICES "[normidx].xyz;\n"
                "float3 N1 = " I_NORMALMATRICES "[normidx + 1].xyz;\n"
                "float3 N2 = " I_NORMALMATRICES "[normidx + 2].xyz;\n");
    }
  }
  else
  {
    // One shared matrix
    out.Write("float4 P0 = " I_POSNORMALMATRIX "[0];\n"
              "float4 P1 = " I_POSNORMALMATRIX "[1];\n"
              "float4 P2 = " I_POSNORMALMATRIX "[2];\n");
    if ((uid_data->components & VB_HAS_NORMAL) != 0)
    {
      out.Write("float3 N0 = " I_POSNORMALMATRIX "[3].xyz;\n"
                "float3 N1 = " I_POSNORMALMATRIX "[4].xyz;\n"
                "float3 N2 = " I_POSNORMALMATRIX "[5].xyz;\n");
    }
  }

  out.Write("// Multiply the position vector by the position matrix\n"
            "float4 pos = float4(dot(P0, rawpos), dot(P1, rawpos), dot(P2, rawpos), 1.0);\n");
  if ((uid_data->components & VB_HAS_NORMAL) != 0)
  {
    if ((uid_data->components & VB_HAS_TANGENT) == 0)
      out.Write("float3 rawtangent = " I_CACHED_TANGENT ".xyz;\n");
    if ((uid_data->components & VB_HAS_BINORMAL) == 0)
      out.Write("float3 rawbinormal = " I_CACHED_BINORMAL ".xyz;\n");

    // The scale of the transform matrix is used to control the size of the emboss map effect, by
    // changing the scale of the transformed binormals (which only get used by emboss map texgens).
    // By normalising the first transformed normal (which is used by lighting calculations and needs
    // to be unit length), the same transform matrix can do double duty, scaling for emboss mapping,
    // and not scaling for lighting.
    out.Write("float3 _normal = normalize(float3(dot(N0, rawnormal), dot(N1, rawnormal), dot(N2, "
              "rawnormal)));\n"
              "float3 _tangent = float3(dot(N0, rawtangent), dot(N1, rawtangent), dot(N2, "
              "rawtangent));\n"
              "float3 _binormal = float3(dot(N0, rawbinormal), dot(N1, rawbinormal), dot(N2, "
              "rawbinormal));\n");
  }
  else
  {
    out.Write("float3 _normal = float3(0.0, 0.0, 0.0);\n");
    out.Write("float3 _binormal = float3(0.0, 0.0, 0.0);\n");
    out.Write("float3 _tangent = float3(0.0, 0.0, 0.0);\n");
  }

  out.Write("o.pos = float4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION
            "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n");

  GenerateLightingShaderCode(out, uid_data->lighting, "vertex_color_", "o.colors_");

  // transform texcoords
  out.Write("float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n");
  for (u32 i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    out.Write("{{\n");
    out.Write("coord = float4(0.0, 0.0, 1.0, 1.0);\n");
    switch (texinfo.sourcerow)
    {
    case SourceRow::Geom:
      out.Write("coord.xyz = rawpos.xyz;\n");
      break;
    case SourceRow::Normal:
      if ((uid_data->components & VB_HAS_NORMAL) != 0)
      {
        out.Write("coord.xyz = rawnormal.xyz;\n");
      }
      break;
    case SourceRow::Colors:
      ASSERT(texinfo.texgentype == TexGenType::Color0 || texinfo.texgentype == TexGenType::Color1);
      break;
    case SourceRow::BinormalT:
      if ((uid_data->components & VB_HAS_TANGENT) != 0)
      {
        out.Write("coord.xyz = rawtangent.xyz;\n");
      }
      break;
    case SourceRow::BinormalB:
      if ((uid_data->components & VB_HAS_BINORMAL) != 0)
      {
        out.Write("coord.xyz = rawbinormal.xyz;\n");
      }
      break;
    default:
      ASSERT(texinfo.sourcerow >= SourceRow::Tex0 && texinfo.sourcerow <= SourceRow::Tex7);
      u32 texnum = static_cast<u32>(texinfo.sourcerow) - static_cast<u32>(SourceRow::Tex0);
      if ((uid_data->components & (VB_HAS_UV0 << (texnum))) != 0)
      {
        out.Write("coord = float4(rawtex{}.x, rawtex{}.y, 1.0, 1.0);\n", texnum, texnum);
      }
      break;
    }
    // Input form of AB11 sets z element to 1.0

    if (texinfo.inputform == TexInputForm::AB11)
      out.Write("coord.z = 1.0;\n");

    // Convert NaNs to 1 - needed to fix eyelids in Shadow the Hedgehog during cutscenes
    // See https://bugs.dolphin-emu.org/issues/11458
    out.Write("// Convert NaN to 1\n");
    out.Write("if (dolphin_isnan(coord.x)) coord.x = 1.0;\n");
    out.Write("if (dolphin_isnan(coord.y)) coord.y = 1.0;\n");
    out.Write("if (dolphin_isnan(coord.z)) coord.z = 1.0;\n");

    // first transformation
    switch (texinfo.texgentype)
    {
    case TexGenType::EmbossMap:  // calculate tex coords into bump map

      // transform the light dir into tangent space
      out.Write("float3 ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n",
                LIGHT_POS_PARAMS(texinfo.embosslightshift));
      out.Write(
          "o.tex{}.xyz = o.tex{}.xyz + float3(dot(ldir, _tangent), dot(ldir, _binormal), 0.0);\n",
          i, texinfo.embosssourceshift);

      break;
    case TexGenType::Color0:
      out.Write("o.tex{}.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
      break;
    case TexGenType::Color1:
      out.Write("o.tex{}.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
      break;
    case TexGenType::Regular:
    default:
      if ((uid_data->components & (VB_HAS_TEXMTXIDX0 << i)) != 0)
      {
        out.Write("int tmp = int(rawtex{}.z);\n", i);
        if (static_cast<TexSize>((uid_data->texMtxInfo_n_projection >> i) & 1) == TexSize::STQ)
        {
          out.Write("o.tex{}.xyz = float3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES
                    "[tmp+1]), dot(coord, " I_TRANSFORMMATRICES "[tmp+2]));\n",
                    i);
        }
        else
        {
          out.Write("o.tex{}.xyz = float3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES "[tmp+1]), 1);\n",
                    i);
        }
      }
      else
      {
        if (static_cast<TexSize>((uid_data->texMtxInfo_n_projection >> i) & 1) == TexSize::STQ)
        {
          out.Write("o.tex{}.xyz = float3(dot(coord, " I_TEXMATRICES
                    "[{}]), dot(coord, " I_TEXMATRICES "[{}]), dot(coord, " I_TEXMATRICES
                    "[{}]));\n",
                    i, 3 * i, 3 * i + 1, 3 * i + 2);
        }
        else
        {
          out.Write("o.tex{}.xyz = float3(dot(coord, " I_TEXMATRICES
                    "[{}]), dot(coord, " I_TEXMATRICES "[{}]), 1);\n",
                    i, 3 * i, 3 * i + 1);
        }
      }
      break;
    }

    // CHECKME: does this only work for regular tex gen types?
    if (uid_data->dualTexTrans_enabled && texinfo.texgentype == TexGenType::Regular)
    {
      auto& postInfo = uid_data->postMtxInfo[i];

      out.Write("float4 P0 = " I_POSTTRANSFORMMATRICES "[{}];\n"
                "float4 P1 = " I_POSTTRANSFORMMATRICES "[{}];\n"
                "float4 P2 = " I_POSTTRANSFORMMATRICES "[{}];\n",
                postInfo.index & 0x3f, (postInfo.index + 1) & 0x3f, (postInfo.index + 2) & 0x3f);

      if (postInfo.normalize)
        out.Write("o.tex{}.xyz = normalize(o.tex{}.xyz);\n", i, i);

      // multiply by postmatrix
      out.Write(
          "o.tex{0}.xyz = float3(dot(P0.xyz, o.tex{0}.xyz) + P0.w, dot(P1.xyz, o.tex{0}.xyz) + "
          "P1.w, dot(P2.xyz, o.tex{0}.xyz) + P2.w);\n",
          i);
    }

    // When q is 0, the GameCube appears to have a special case
    // This can be seen in devkitPro's neheGX Lesson08 example for Wii
    // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
    // TODO: check if this only affects XF_TEXGEN_REGULAR
    if (texinfo.texgentype == TexGenType::Regular)
    {
      out.Write(
          "if(o.tex{0}.z == 0.0f)\n"
          "\to.tex{0}.xy = clamp(o.tex{0}.xy / 2.0f, float2(-1.0f,-1.0f), float2(1.0f,1.0f));\n",
          i);
    }

    out.Write("}}\n");
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

  if (per_pixel_lighting)
  {
    // When per-pixel lighting is enabled, the vertex colors are passed through
    // unmodified so we can evaluate the lighting in the pixel shader.

    // Lighting is also still computed in the vertex shader since it can be used to
    // generate texture coordinates. We generated them above, so now the colors can
    // be reverted to their previous stage.
    out.Write("o.colors_0 = vertex_color_0;\n");
    out.Write("o.colors_1 = vertex_color_1;\n");
    // Note that the numColorChans logic is performed in the pixel shader.
  }
  else
  {
    // The number of colors available to TEV is determined by numColorChans.
    // We have to provide the fields to match the interface, so set to zero if it's not enabled.
    if (uid_data->numColorChans == 0)
      out.Write("o.colors_0 = float4(0.0, 0.0, 0.0, 0.0);\n");
    if (uid_data->numColorChans <= 1)
      out.Write("o.colors_1 = float4(0.0, 0.0, 0.0, 0.0);\n");
  }

  // clipPos/w needs to be done in pixel shader, not here
  if (!host_config.fast_depth_calc)
    out.Write("o.clipPos = o.pos;\n");

  if (per_pixel_lighting)
  {
    out.Write("o.Normal = _normal;\n"
              "o.WorldPos = pos.xyz;\n");
  }

  // If we can disable the incorrect depth clipping planes using depth clamping, then we can do
  // our own depth clipping and calculate the depth range before the perspective divide if
  // necessary.
  if (host_config.backend_depth_clamp)
  {
    // Since we're adjusting z for the depth range before the perspective divide, we have to do our
    // own clipping. We want to clip so that -w <= z <= 0, which matches the console -1..0 range.
    // We adjust our depth value for clipping purposes to match the perspective projection in the
    // software backend, which is a hack to fix Sonic Adventure and Unleashed games.
    out.Write("float clipDepth = o.pos.z * (1.0 - 1e-7);\n"
              "float clipDist0 = clipDepth + o.pos.w;\n"  // Near: z < -w
              "float clipDist1 = -clipDepth;\n");         // Far: z > 0

    if (host_config.backend_geometry_shaders)
    {
      out.Write("o.clipDist0 = clipDist0;\n"
                "o.clipDist1 = clipDist1;\n");
    }
  }
  else
  {
    // Same depth adjustment for Sonic. Without depth clamping, it unfortunately
    // affects non-clipping uses of depth too.
    out.Write("o.pos.z = o.pos.z * (1.0 - 1e-7);\n");
  }

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
  out.Write("o.pos.z = o.pos.w * " I_PIXELCENTERCORRECTION ".w - "
            "o.pos.z * " I_PIXELCENTERCORRECTION ".z;\n");

  if (!host_config.backend_clip_control)
  {
    // If the graphics API doesn't support a depth range of 0..1, then we need to map z to
    // the -1..1 range. Unfortunately we have to use a substraction, which is a lossy floating-point
    // operation that can introduce a round-trip error.
    out.Write("o.pos.z = o.pos.z * 2.0 - o.pos.w;\n");
  }

  // Correct for negative viewports by mirroring all vertices. We need to negate the height here,
  // since the viewport height is already negated by the render backend.
  out.Write("o.pos.xy *= sign(" I_PIXELCENTERCORRECTION ".xy * float2(1.0, -1.0));\n");

  // The console GPU places the pixel center at 7/12 in screen space unless
  // antialiasing is enabled, while D3D and OpenGL place it at 0.5. This results
  // in some primitives being placed one pixel too far to the bottom-right,
  // which in turn can be critical if it happens for clear quads.
  // Hence, we compensate for this pixel center difference so that primitives
  // get rasterized correctly.
  out.Write("o.pos.xy = o.pos.xy - o.pos.w * " I_PIXELCENTERCORRECTION ".xy;\n");

  if (vertex_rounding)
  {
    // By now our position is in clip space
    // however, higher resolutions than the Wii outputs
    // cause an additional pixel offset
    // due to a higher pixel density
    // we need to correct this by converting our
    // clip-space position into the Wii's screen-space
    // acquire the right pixel and then convert it back
    out.Write("if (o.pos.w == 1.0f)\n"
              "{{\n"

              "\tfloat ss_pixel_x = ((o.pos.x + 1.0f) * (" I_VIEWPORT_SIZE ".x * 0.5f));\n"
              "\tfloat ss_pixel_y = ((o.pos.y + 1.0f) * (" I_VIEWPORT_SIZE ".y * 0.5f));\n"

              "\tss_pixel_x = round(ss_pixel_x);\n"
              "\tss_pixel_y = round(ss_pixel_y);\n"

              "\to.pos.x = ((ss_pixel_x / (" I_VIEWPORT_SIZE ".x * 0.5f)) - 1.0f);\n"
              "\to.pos.y = ((ss_pixel_y / (" I_VIEWPORT_SIZE ".y * 0.5f)) - 1.0f);\n"
              "}}\n");
  }

  if (host_config.backend_geometry_shaders)
  {
    AssignVSOutputMembers(out, "vs", "o", uid_data->numTexGens, host_config);
  }
  else
  {
    // TODO: Pass interface blocks between shader stages even if geometry shaders
    // are not supported, however that will require at least OpenGL 3.2 support.
    for (u32 i = 0; i < uid_data->numTexGens; ++i)
      out.Write("tex{}.xyz = o.tex{};\n", i, i);
    if (!host_config.fast_depth_calc)
      out.Write("clipPos = o.clipPos;\n");
    if (per_pixel_lighting)
    {
      out.Write("Normal = o.Normal;\n"
                "WorldPos = o.WorldPos;\n");
    }
    out.Write("colors_0 = o.colors_0;\n"
              "colors_1 = o.colors_1;\n");
  }

  if (host_config.backend_depth_clamp)
  {
    out.Write("gl_ClipDistance[0] = clipDist0;\n"
              "gl_ClipDistance[1] = clipDist1;\n");
  }

  // Vulkan NDC space has Y pointing down (right-handed NDC space).
  if (api_type == APIType::Vulkan)
    out.Write("gl_Position = float4(o.pos.x, -o.pos.y, o.pos.z, o.pos.w);\n");
  else
    out.Write("gl_Position = o.pos;\n");
  out.Write("}}\n");

  return out;
}
