// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/LightingShaderGen.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/XFMemory.h"

static void GenerateLightShader(ShaderCode& object, const LightingUidData& uid_data, int index,
                                int litchan_index, bool alpha)
{
  const char* swizzle = alpha ? "a" : "rgb";
  const char* swizzle_components = (alpha) ? "" : "3";

  const AttenuationFunc attnfunc = uid_data.attnfunc()[litchan_index];
  const DiffuseFunc diffusefunc = uid_data.diffusefunc()[litchan_index];

  switch (attnfunc)
  {
  case AttenuationFunc::None:
  case AttenuationFunc::Dir:
    object.Write("ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(index));
    object.Write("attn = 1.0;\n");
    object.Write("if (length(ldir) == 0.0)\n\t ldir = _normal;\n");
    break;
  case AttenuationFunc::Spec:
    object.Write("ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(index));
    object.Write("attn = (dot(_normal, ldir) >= 0.0) ? max(0.0, dot(_normal, " LIGHT_DIR
                 ".xyz)) : 0.0;\n",
                 LIGHT_DIR_PARAMS(index));
    object.Write("cosAttn = " LIGHT_COSATT ".xyz;\n", LIGHT_COSATT_PARAMS(index));
    object.Write("distAttn = {}(" LIGHT_DISTATT ".xyz);\n",
                 (diffusefunc == DiffuseFunc::None) ? "" : "normalize",
                 LIGHT_DISTATT_PARAMS(index));
    object.Write("attn = max(0.0f, dot(cosAttn, float3(1.0, attn, attn*attn))) / dot(distAttn, "
                 "float3(1.0, attn, attn*attn));\n");
    break;
  case AttenuationFunc::Spot:
    object.Write("ldir = " LIGHT_POS ".xyz - pos.xyz;\n", LIGHT_POS_PARAMS(index));
    object.Write("dist2 = dot(ldir, ldir);\n"
                 "dist = sqrt(dist2);\n"
                 "ldir = ldir / dist;\n"
                 "attn = max(0.0, dot(ldir, " LIGHT_DIR ".xyz));\n",
                 LIGHT_DIR_PARAMS(index));
    // attn*attn may overflow
    object.Write("attn = max(0.0, " LIGHT_COSATT ".x + " LIGHT_COSATT ".y*attn + " LIGHT_COSATT
                 ".z*attn*attn) / dot(" LIGHT_DISTATT ".xyz, float3(1.0,dist,dist2));\n",
                 LIGHT_COSATT_PARAMS(index), LIGHT_COSATT_PARAMS(index), LIGHT_COSATT_PARAMS(index),
                 LIGHT_DISTATT_PARAMS(index));
    break;
  }

  switch (diffusefunc)
  {
  case DiffuseFunc::None:
    object.Write("lacc.{} += int{}(round(attn * float{}(" LIGHT_COL ")));\n", swizzle,
                 swizzle_components, swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  case DiffuseFunc::Sign:
  case DiffuseFunc::Clamp:
    object.Write("lacc.{} += int{}(round(attn * {}dot(ldir, _normal)) * float{}(" LIGHT_COL
                 ")));\n",
                 swizzle, swizzle_components, diffusefunc != DiffuseFunc::Sign ? "max(0.0," : "(",
                 swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  default:
    ASSERT(0);
  }

  object.Write("\n");
}

// vertex shader
// lights/colors
// materials name is I_MATERIALS in vs and I_PMATERIALS in ps
// inColorName is color in vs and colors_ in ps
// dest is o.colors_ in vs and colors_ in ps
void GenerateLightingShaderCode(ShaderCode& object, const LightingUidData& uid_data,
                                std::string_view in_color_name, std::string_view dest)
{
  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    object.Write("{{\n");

    const MatSource colormatsource = uid_data.matsource()[j];
    if (colormatsource == MatSource::Vertex)  // from vertex
      object.Write("int4 mat = int4(round({}{} * 255.0));\n", in_color_name, j);
    else  // from color
      object.Write("int4 mat = {}[{}];\n", I_MATERIALS, j + 2);

    if (uid_data.enablelighting()[j])
    {
      if (uid_data.ambsource()[j] == AmbSource::Vertex)  // from vertex
        object.Write("lacc = int4(round({}{} * 255.0));\n", in_color_name, j);
      else  // from color
        object.Write("lacc = {}[{}];\n", I_MATERIALS, j);
    }
    else
    {
      object.Write("lacc = int4(255, 255, 255, 255);\n");
    }

    // check if alpha is different
    const MatSource alphamatsource = uid_data.matsource()[j + 2];
    if (alphamatsource != colormatsource)
    {
      if (alphamatsource == MatSource::Vertex)  // from vertex
        object.Write("mat.w = int(round({}{}.w * 255.0));\n", in_color_name, j);
      else  // from color
        object.Write("mat.w = {}[{}].w;\n", I_MATERIALS, j + 2);
    }

    if (uid_data.enablelighting()[j + 2])
    {
      if (uid_data.ambsource()[j + 2] == AmbSource::Vertex)  // from vertex
        object.Write("lacc.w = int(round({}{}.w * 255.0));\n", in_color_name, j);
      else  // from color
        object.Write("lacc.w = {}[{}].w;\n", I_MATERIALS, j);
    }
    else
    {
      object.Write("lacc.w = 255;\n");
    }

    if (uid_data.enablelighting()[j])  // Color lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if (uid_data.light_mask_bits()[j][i])
          GenerateLightShader(object, uid_data, i, j, false);
      }
    }
    if (uid_data.enablelighting()[j + 2])  // Alpha lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if (uid_data.light_mask_bits()[j + 2][i])
          GenerateLightShader(object, uid_data, i, j + 2, true);
      }
    }
    object.Write("lacc = clamp(lacc, 0, 255);\n");
    object.Write("{}{} = float4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n", dest, j);
    object.Write("}}\n");
  }
}

void GetLightingShaderUid(LightingUidData& uid_data)
{
  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    uid_data.matsource()[j] = xfmem.color[j].matsource();
    uid_data.matsource()[j + 2] = xfmem.alpha[j].matsource();
    uid_data.enablelighting()[j] = xfmem.color[j].enablelighting();
    uid_data.enablelighting()[j + 2] = xfmem.alpha[j].enablelighting();

    if (uid_data.enablelighting()[j])  // Color lights
    {
      uid_data.ambsource()[j] = xfmem.color[j].ambsource();
      uid_data.attnfunc()[j] = xfmem.color[j].attnfunc();
      uid_data.diffusefunc()[j] = xfmem.color[j].diffusefunc();
      uid_data.light_mask()[j] = xfmem.color[j].GetFullLightMask();
    }
    if (uid_data.enablelighting()[j + 2])  // Alpha lights
    {
      uid_data.ambsource()[j + 2] = xfmem.alpha[j].ambsource();
      uid_data.attnfunc()[j + 2] = xfmem.alpha[j].attnfunc();
      uid_data.diffusefunc()[j + 2] = xfmem.alpha[j].diffusefunc();
      uid_data.light_mask()[j + 2] = xfmem.alpha[j].GetFullLightMask();
    }
  }
}
