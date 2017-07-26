// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

VertexShaderUid GetVertexShaderUid()
{
  VertexShaderUid out;
  vertex_shader_uid_data* uid_data = out.GetUidData<vertex_shader_uid_data>();
  memset(uid_data, 0, sizeof(*uid_data));

  _assert_(bpmem.genMode.numtexgens == xfmem.numTexGen.numTexGens);
  _assert_(bpmem.genMode.numcolchans == xfmem.numChan.numColorChans);

  uid_data->numTexGens = xfmem.numTexGen.numTexGens;
  uid_data->components = VertexLoaderManager::g_current_components;
  uid_data->numColorChans = xfmem.numChan.numColorChans;

  GetLightingShaderUid(uid_data->lighting);

  // transform texcoords
  for (unsigned int i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    texinfo.sourcerow = xfmem.texMtxInfo[i].sourcerow;
    texinfo.texgentype = xfmem.texMtxInfo[i].texgentype;
    texinfo.inputform = xfmem.texMtxInfo[i].inputform;

    // first transformation
    switch (texinfo.texgentype)
    {
    case XF_TEXGEN_EMBOSS_MAP:  // calculate tex coords into bump map
      if (uid_data->components & (VB_HAS_NRM1 | VB_HAS_NRM2))
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
    case XF_TEXGEN_COLOR_STRGBC0:
    case XF_TEXGEN_COLOR_STRGBC1:
      break;
    case XF_TEXGEN_REGULAR:
    default:
      uid_data->texMtxInfo_n_projection |= xfmem.texMtxInfo[i].projection << i;
      break;
    }

    uid_data->dualTexTrans_enabled = xfmem.dualTexTrans.enabled;
    // CHECKME: does this only work for regular tex gen types?
    if (uid_data->dualTexTrans_enabled && texinfo.texgentype == XF_TEXGEN_REGULAR)
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

  out.Write("%s", s_lighting_struct);

  // uniforms
  if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
    out.Write("UBO_BINDING(std140, 2) uniform VSBlock {\n");
  else
    out.Write("cbuffer VSBlock {\n");

  out.Write(s_shader_uniforms);
  out.Write("};\n");

  out.Write("struct VS_OUTPUT {\n");
  GenerateVSOutputMembers(out, api_type, uid_data->numTexGens, per_pixel_lighting, "");
  out.Write("};\n");

  if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
  {
    out.Write("ATTRIBUTE_LOCATION(%d) in float4 rawpos;\n", SHADER_POSITION_ATTRIB);
    if (uid_data->components & VB_HAS_POSMTXIDX)
      out.Write("ATTRIBUTE_LOCATION(%d) in uint4 posmtx;\n", SHADER_POSMTX_ATTRIB);
    if (uid_data->components & VB_HAS_NRM0)
      out.Write("ATTRIBUTE_LOCATION(%d) in float3 rawnorm0;\n", SHADER_NORM0_ATTRIB);
    if (uid_data->components & VB_HAS_NRM1)
      out.Write("ATTRIBUTE_LOCATION(%d) in float3 rawnorm1;\n", SHADER_NORM1_ATTRIB);
    if (uid_data->components & VB_HAS_NRM2)
      out.Write("ATTRIBUTE_LOCATION(%d) in float3 rawnorm2;\n", SHADER_NORM2_ATTRIB);

    if (uid_data->components & VB_HAS_COL0)
      out.Write("ATTRIBUTE_LOCATION(%d) in float4 rawcolor0;\n", SHADER_COLOR0_ATTRIB);
    if (uid_data->components & VB_HAS_COL1)
      out.Write("ATTRIBUTE_LOCATION(%d) in float4 rawcolor1;\n", SHADER_COLOR1_ATTRIB);

    for (int i = 0; i < 8; ++i)
    {
      u32 hastexmtx = (uid_data->components & (VB_HAS_TEXMTXIDX0 << i));
      if ((uid_data->components & (VB_HAS_UV0 << i)) || hastexmtx)
      {
        out.Write("ATTRIBUTE_LOCATION(%d) in float%d rawtex%d;\n", SHADER_TEXTURE0_ATTRIB + i,
                  hastexmtx ? 3 : 2, i);
      }
    }

    // We need to always use output blocks for Vulkan, but geometry shaders are also optional.
    if (host_config.backend_geometry_shaders || api_type == APIType::Vulkan)
    {
      out.Write("VARYING_LOCATION(0) out VertexData {\n");
      GenerateVSOutputMembers(out, api_type, uid_data->numTexGens, per_pixel_lighting,
                              GetInterpolationQualifier(msaa, ssaa, true, false));
      out.Write("} vs;\n");
    }
    else
    {
      // Let's set up attributes
      for (u32 i = 0; i < 8; ++i)
      {
        if (i < uid_data->numTexGens)
        {
          out.Write("%s out float3 tex%u;\n", GetInterpolationQualifier(msaa, ssaa), i);
        }
      }
      out.Write("%s out float4 clipPos;\n", GetInterpolationQualifier(msaa, ssaa));
      if (per_pixel_lighting)
      {
        out.Write("%s out float3 Normal;\n", GetInterpolationQualifier(msaa, ssaa));
        out.Write("%s out float3 WorldPos;\n", GetInterpolationQualifier(msaa, ssaa));
      }
      out.Write("%s out float4 colors_0;\n", GetInterpolationQualifier(msaa, ssaa));
      out.Write("%s out float4 colors_1;\n", GetInterpolationQualifier(msaa, ssaa));
    }

    out.Write("void main()\n{\n");
  }
  else  // D3D
  {
    out.Write("VS_OUTPUT main(\n");

    // inputs
    if (uid_data->components & VB_HAS_NRM0)
      out.Write("  float3 rawnorm0 : NORMAL0,\n");
    if (uid_data->components & VB_HAS_NRM1)
      out.Write("  float3 rawnorm1 : NORMAL1,\n");
    if (uid_data->components & VB_HAS_NRM2)
      out.Write("  float3 rawnorm2 : NORMAL2,\n");
    if (uid_data->components & VB_HAS_COL0)
      out.Write("  float4 rawcolor0 : COLOR0,\n");
    if (uid_data->components & VB_HAS_COL1)
      out.Write("  float4 rawcolor1 : COLOR1,\n");
    for (int i = 0; i < 8; ++i)
    {
      u32 hastexmtx = (uid_data->components & (VB_HAS_TEXMTXIDX0 << i));
      if ((uid_data->components & (VB_HAS_UV0 << i)) || hastexmtx)
        out.Write("  float%d rawtex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i, i);
    }
    if (uid_data->components & VB_HAS_POSMTXIDX)
      out.Write("  uint4 posmtx : BLENDINDICES,\n");
    out.Write("  float4 rawpos : POSITION) {\n");
  }

  out.Write("VS_OUTPUT o;\n");

  // transforms
  if (uid_data->components & VB_HAS_POSMTXIDX)
  {
    out.Write("int posidx = int(posmtx.r);\n");
    out.Write("float4 pos = float4(dot(" I_TRANSFORMMATRICES
              "[posidx], rawpos), dot(" I_TRANSFORMMATRICES
              "[posidx+1], rawpos), dot(" I_TRANSFORMMATRICES "[posidx+2], rawpos), 1);\n");

    if (uid_data->components & VB_HAS_NRMALL)
    {
      out.Write("int normidx = posidx & 31;\n");
      out.Write("float3 N0 = " I_NORMALMATRICES "[normidx].xyz, N1 = " I_NORMALMATRICES
                "[normidx+1].xyz, N2 = " I_NORMALMATRICES "[normidx+2].xyz;\n");
    }

    if (uid_data->components & VB_HAS_NRM0)
      out.Write("float3 _norm0 = normalize(float3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, "
                "rawnorm0)));\n");
    if (uid_data->components & VB_HAS_NRM1)
      out.Write(
          "float3 _norm1 = float3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
    if (uid_data->components & VB_HAS_NRM2)
      out.Write(
          "float3 _norm2 = float3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
  }
  else
  {
    out.Write("float4 pos = float4(dot(" I_POSNORMALMATRIX "[0], rawpos), dot(" I_POSNORMALMATRIX
              "[1], rawpos), dot(" I_POSNORMALMATRIX "[2], rawpos), 1.0);\n");
    if (uid_data->components & VB_HAS_NRM0)
      out.Write("float3 _norm0 = normalize(float3(dot(" I_POSNORMALMATRIX
                "[3].xyz, rawnorm0), dot(" I_POSNORMALMATRIX
                "[4].xyz, rawnorm0), dot(" I_POSNORMALMATRIX "[5].xyz, rawnorm0)));\n");
    if (uid_data->components & VB_HAS_NRM1)
      out.Write("float3 _norm1 = float3(dot(" I_POSNORMALMATRIX
                "[3].xyz, rawnorm1), dot(" I_POSNORMALMATRIX
                "[4].xyz, rawnorm1), dot(" I_POSNORMALMATRIX "[5].xyz, rawnorm1));\n");
    if (uid_data->components & VB_HAS_NRM2)
      out.Write("float3 _norm2 = float3(dot(" I_POSNORMALMATRIX
                "[3].xyz, rawnorm2), dot(" I_POSNORMALMATRIX
                "[4].xyz, rawnorm2), dot(" I_POSNORMALMATRIX "[5].xyz, rawnorm2));\n");
  }

  if (!(uid_data->components & VB_HAS_NRM0))
    out.Write("float3 _norm0 = float3(0.0, 0.0, 0.0);\n");

  out.Write("o.pos = float4(dot(" I_PROJECTION "[0], pos), dot(" I_PROJECTION
            "[1], pos), dot(" I_PROJECTION "[2], pos), dot(" I_PROJECTION "[3], pos));\n");

  out.Write("int4 lacc;\n"
            "float3 ldir, h, cosAttn, distAttn;\n"
            "float dist, dist2, attn;\n");

  if (uid_data->numColorChans == 0)
  {
    if (uid_data->components & VB_HAS_COL0)
      out.Write("o.colors_0 = rawcolor0;\n");
    else
      out.Write("o.colors_0 = float4(1.0, 1.0, 1.0, 1.0);\n");
  }

  GenerateLightingShaderCode(out, uid_data->lighting, uid_data->components, uid_data->numColorChans,
                             "rawcolor", "o.colors_");

  if (uid_data->numColorChans < 2)
  {
    if (uid_data->components & VB_HAS_COL1)
      out.Write("o.colors_1 = rawcolor1;\n");
    else
      out.Write("o.colors_1 = o.colors_0;\n");
  }

  // transform texcoords
  out.Write("float4 coord = float4(0.0, 0.0, 1.0, 1.0);\n");
  for (unsigned int i = 0; i < uid_data->numTexGens; ++i)
  {
    auto& texinfo = uid_data->texMtxInfo[i];

    out.Write("{\n");
    out.Write("coord = float4(0.0, 0.0, 1.0, 1.0);\n");
    switch (texinfo.sourcerow)
    {
    case XF_SRCGEOM_INROW:
      out.Write("coord.xyz = rawpos.xyz;\n");
      break;
    case XF_SRCNORMAL_INROW:
      if (uid_data->components & VB_HAS_NRM0)
      {
        out.Write("coord.xyz = rawnorm0.xyz;\n");
      }
      break;
    case XF_SRCCOLORS_INROW:
      _assert_(texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 ||
               texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1);
      break;
    case XF_SRCBINORMAL_T_INROW:
      if (uid_data->components & VB_HAS_NRM1)
      {
        out.Write("coord.xyz = rawnorm1.xyz;\n");
      }
      break;
    case XF_SRCBINORMAL_B_INROW:
      if (uid_data->components & VB_HAS_NRM2)
      {
        out.Write("coord.xyz = rawnorm2.xyz;\n");
      }
      break;
    default:
      _assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
      if (uid_data->components & (VB_HAS_UV0 << (texinfo.sourcerow - XF_SRCTEX0_INROW)))
        out.Write("coord = float4(rawtex%d.x, rawtex%d.y, 1.0, 1.0);\n",
                  texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);
      break;
    }
    // Input form of AB11 sets z element to 1.0

    if (texinfo.inputform == XF_TEXINPUT_AB11)
      out.Write("coord.z = 1.0;\n");

    // first transformation
    switch (texinfo.texgentype)
    {
    case XF_TEXGEN_EMBOSS_MAP:  // calculate tex coords into bump map

      if (uid_data->components & (VB_HAS_NRM1 | VB_HAS_NRM2))
      {
        // transform the light dir into tangent space
        out.Write("ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n",
                  LIGHT_POS_PARAMS(texinfo.embosslightshift));
        out.Write(
            "o.tex%d.xyz = o.tex%d.xyz + float3(dot(ldir, _norm1), dot(ldir, _norm2), 0.0);\n", i,
            texinfo.embosssourceshift);
      }
      else
      {
        // The following assert was triggered in House of the Dead Overkill and Star Wars Rogue
        // Squadron 2
        //_assert_(0); // should have normals
        out.Write("o.tex%d.xyz = o.tex%d.xyz;\n", i, texinfo.embosssourceshift);
      }

      break;
    case XF_TEXGEN_COLOR_STRGBC0:
      out.Write("o.tex%d.xyz = float3(o.colors_0.x, o.colors_0.y, 1);\n", i);
      break;
    case XF_TEXGEN_COLOR_STRGBC1:
      out.Write("o.tex%d.xyz = float3(o.colors_1.x, o.colors_1.y, 1);\n", i);
      break;
    case XF_TEXGEN_REGULAR:
    default:
      if (uid_data->components & (VB_HAS_TEXMTXIDX0 << i))
      {
        out.Write("int tmp = int(rawtex%d.z);\n", i);
        if (((uid_data->texMtxInfo_n_projection >> i) & 1) == XF_TEXPROJ_STQ)
          out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES
                    "[tmp+1]), dot(coord, " I_TRANSFORMMATRICES "[tmp+2]));\n",
                    i);
        else
          out.Write("o.tex%d.xyz = float3(dot(coord, " I_TRANSFORMMATRICES
                    "[tmp]), dot(coord, " I_TRANSFORMMATRICES "[tmp+1]), 1);\n",
                    i);
      }
      else
      {
        if (((uid_data->texMtxInfo_n_projection >> i) & 1) == XF_TEXPROJ_STQ)
          out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES
                    "[%d]), dot(coord, " I_TEXMATRICES "[%d]), dot(coord, " I_TEXMATRICES
                    "[%d]));\n",
                    i, 3 * i, 3 * i + 1, 3 * i + 2);
        else
          out.Write("o.tex%d.xyz = float3(dot(coord, " I_TEXMATRICES
                    "[%d]), dot(coord, " I_TEXMATRICES "[%d]), 1);\n",
                    i, 3 * i, 3 * i + 1);
      }
      break;
    }

    // CHECKME: does this only work for regular tex gen types?
    if (uid_data->dualTexTrans_enabled && texinfo.texgentype == XF_TEXGEN_REGULAR)
    {
      auto& postInfo = uid_data->postMtxInfo[i];

      out.Write("float4 P0 = " I_POSTTRANSFORMMATRICES "[%d];\n"
                "float4 P1 = " I_POSTTRANSFORMMATRICES "[%d];\n"
                "float4 P2 = " I_POSTTRANSFORMMATRICES "[%d];\n",
                postInfo.index & 0x3f, (postInfo.index + 1) & 0x3f, (postInfo.index + 2) & 0x3f);

      if (postInfo.normalize)
        out.Write("o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

      // multiply by postmatrix
      out.Write("o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + "
                "P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n",
                i, i, i, i);
    }

    // When q is 0, the GameCube appears to have a special case
    // This can be seen in devkitPro's neheGX Lesson08 example for Wii
    // Makes differences in Rogue Squadron 3 (Hoth sky) and The Last Story (shadow culling)
    // TODO: check if this only affects XF_TEXGEN_REGULAR
    if (texinfo.texgentype == XF_TEXGEN_REGULAR)
    {
      out.Write("if(o.tex%d.z == 0.0f)\n", i);
      out.Write(
          "\to.tex%d.xy = clamp(o.tex%d.xy / 2.0f, float2(-1.0f,-1.0f), float2(1.0f,1.0f));\n", i,
          i);
    }

    out.Write("}\n");
  }

  // clipPos/w needs to be done in pixel shader, not here
  out.Write("o.clipPos = o.pos;\n");

  if (per_pixel_lighting)
  {
    out.Write("o.Normal = _norm0;\n");
    out.Write("o.WorldPos = pos.xyz;\n");

    if (uid_data->components & VB_HAS_COL0)
      out.Write("o.colors_0 = rawcolor0;\n");

    if (uid_data->components & VB_HAS_COL1)
      out.Write("o.colors_1 = rawcolor1;\n");
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
    out.Write("float clipDepth = o.pos.z * (1.0 - 1e-7);\n");
    out.Write("o.clipDist0 = clipDepth + o.pos.w;\n");  // Near: z < -w
    out.Write("o.clipDist1 = -clipDepth;\n");           // Far: z > 0
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
    out.Write("if (o.pos.w == 1.0f)\n");
    out.Write("{\n");

    out.Write("\tfloat ss_pixel_x = ((o.pos.x + 1.0f) * (" I_VIEWPORT_SIZE ".x * 0.5f));\n");
    out.Write("\tfloat ss_pixel_y = ((o.pos.y + 1.0f) * (" I_VIEWPORT_SIZE ".y * 0.5f));\n");

    out.Write("\tss_pixel_x = round(ss_pixel_x);\n");
    out.Write("\tss_pixel_y = round(ss_pixel_y);\n");

    out.Write("\to.pos.x = ((ss_pixel_x / (" I_VIEWPORT_SIZE ".x * 0.5f)) - 1.0f);\n");
    out.Write("\to.pos.y = ((ss_pixel_y / (" I_VIEWPORT_SIZE ".y * 0.5f)) - 1.0f);\n");
    out.Write("}\n");
  }

  if (api_type == APIType::OpenGL || api_type == APIType::Vulkan)
  {
    if (host_config.backend_geometry_shaders || api_type == APIType::Vulkan)
    {
      AssignVSOutputMembers(out, "vs", "o", uid_data->numTexGens, per_pixel_lighting);
    }
    else
    {
      // TODO: Pass interface blocks between shader stages even if geometry shaders
      // are not supported, however that will require at least OpenGL 3.2 support.
      for (unsigned int i = 0; i < uid_data->numTexGens; ++i)
        out.Write("tex%d.xyz = o.tex%d;\n", i, i);
      out.Write("clipPos = o.clipPos;\n");
      if (per_pixel_lighting)
      {
        out.Write("Normal = o.Normal;\n");
        out.Write("WorldPos = o.WorldPos;\n");
      }
      out.Write("colors_0 = o.colors_0;\n");
      out.Write("colors_1 = o.colors_1;\n");
    }

    if (host_config.backend_depth_clamp)
    {
      out.Write("gl_ClipDistance[0] = o.clipDist0;\n");
      out.Write("gl_ClipDistance[1] = o.clipDist1;\n");
    }

    // Vulkan NDC space has Y pointing down (right-handed NDC space).
    if (api_type == APIType::Vulkan)
      out.Write("gl_Position = float4(o.pos.x, -o.pos.y, o.pos.z, o.pos.w);\n");
    else
      out.Write("gl_Position = o.pos;\n");
  }
  else  // D3D
  {
    out.Write("return o;\n");
  }
  out.Write("}\n");

  return out;
}
