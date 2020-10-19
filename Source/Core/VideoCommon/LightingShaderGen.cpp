// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  const u32 attnfunc = (uid_data.attnfunc >> (2 * litchan_index)) & 0x3;
  const u32 diffusefunc = (uid_data.diffusefunc >> (2 * litchan_index)) & 0x3;

  switch (attnfunc)
  {
  case LIGHTATTN_NONE:
  case LIGHTATTN_DIR:
    object.WriteFmt("ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(index));
    object.WriteFmt("attn = 1.0;\n");
    object.WriteFmt("if (length(ldir) == 0.0)\n\t ldir = _norm0;\n");
    break;
  case LIGHTATTN_SPEC:
    object.WriteFmt("ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n", LIGHT_POS_PARAMS(index));
    object.WriteFmt("attn = (dot(_norm0, ldir) >= 0.0) ? max(0.0, dot(_norm0, " LIGHT_DIR
                    ".xyz)) : 0.0;\n",
                    LIGHT_DIR_PARAMS(index));
    object.WriteFmt("cosAttn = " LIGHT_COSATT ".xyz;\n", LIGHT_COSATT_PARAMS(index));
    object.WriteFmt("distAttn = {}(" LIGHT_DISTATT ".xyz);\n",
                    (diffusefunc == LIGHTDIF_NONE) ? "" : "normalize", LIGHT_DISTATT_PARAMS(index));
    object.WriteFmt("attn = max(0.0f, dot(cosAttn, float3(1.0, attn, attn*attn))) / dot(distAttn, "
                    "float3(1.0, attn, attn*attn));\n");
    break;
  case LIGHTATTN_SPOT:
    object.WriteFmt("ldir = " LIGHT_POS ".xyz - pos.xyz;\n", LIGHT_POS_PARAMS(index));
    object.WriteFmt("dist2 = dot(ldir, ldir);\n"
                    "dist = sqrt(dist2);\n"
                    "ldir = ldir / dist;\n"
                    "attn = max(0.0, dot(ldir, " LIGHT_DIR ".xyz));\n",
                    LIGHT_DIR_PARAMS(index));
    // attn*attn may overflow
    object.WriteFmt("attn = max(0.0, " LIGHT_COSATT ".x + " LIGHT_COSATT ".y*attn + " LIGHT_COSATT
                    ".z*attn*attn) / dot(" LIGHT_DISTATT ".xyz, float3(1.0,dist,dist2));\n",
                    LIGHT_COSATT_PARAMS(index), LIGHT_COSATT_PARAMS(index),
                    LIGHT_COSATT_PARAMS(index), LIGHT_DISTATT_PARAMS(index));
    break;
  }

  switch (diffusefunc)
  {
  case LIGHTDIF_NONE:
    object.WriteFmt("lacc.{} += int{}(round(attn * float{}(" LIGHT_COL ")));\n", swizzle,
                    swizzle_components, swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  case LIGHTDIF_SIGN:
  case LIGHTDIF_CLAMP:
    object.WriteFmt("lacc.{} += int{}(round(attn * {}dot(ldir, _norm0)) * float{}(" LIGHT_COL
                    ")));\n",
                    swizzle, swizzle_components, diffusefunc != LIGHTDIF_SIGN ? "max(0.0," : "(",
                    swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  default:
    ASSERT(0);
  }

  object.WriteFmt("\n");
}

// vertex shader
// lights/colors
// materials name is I_MATERIALS in vs and I_PMATERIALS in ps
// inColorName is color in vs and colors_ in ps
// dest is o.colors_ in vs and colors_ in ps
void GenerateLightingShaderCode(ShaderCode& object, const LightingUidData& uid_data, int components,
                                std::string_view in_color_name, std::string_view dest)
{
  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    object.WriteFmt("{{\n");

    const bool colormatsource = !!(uid_data.matsource & (1 << j));
    if (colormatsource)  // from vertex
    {
      if ((components & (VB_HAS_COL0 << j)) != 0)
        object.WriteFmt("int4 mat = int4(round({}{} * 255.0));\n", in_color_name, j);
      else if ((components & VB_HAS_COL0) != 0)
        object.WriteFmt("int4 mat = int4(round({}0 * 255.0));\n", in_color_name);
      else
        object.WriteFmt("int4 mat = int4(255, 255, 255, 255);\n");
    }
    else  // from color
    {
      object.WriteFmt("int4 mat = {}[{}];\n", I_MATERIALS, j + 2);
    }

    if ((uid_data.enablelighting & (1 << j)) != 0)
    {
      if ((uid_data.ambsource & (1 << j)) != 0)  // from vertex
      {
        if ((components & (VB_HAS_COL0 << j)) != 0)
        {
          object.WriteFmt("lacc = int4(round({}{} * 255.0));\n", in_color_name, j);
        }
        else if ((components & VB_HAS_COL0) != 0)
        {
          object.WriteFmt("lacc = int4(round({}0 * 255.0));\n", in_color_name);
        }
        else
        {
          // TODO: this isn't verified. Here we want to read the ambient from the vertex,
          // but the vertex itself has no color. So we don't know which value to read.
          // Returning 1.0 is the same as disabled lightning, so this could be fine
          object.WriteFmt("lacc = int4(255, 255, 255, 255);\n");
        }
      }
      else  // from color
      {
        object.WriteFmt("lacc = {}[{}];\n", I_MATERIALS, j);
      }
    }
    else
    {
      object.WriteFmt("lacc = int4(255, 255, 255, 255);\n");
    }

    // check if alpha is different
    const bool alphamatsource = !!(uid_data.matsource & (1 << (j + 2)));
    if (alphamatsource != colormatsource)
    {
      if (alphamatsource)  // from vertex
      {
        if ((components & (VB_HAS_COL0 << j)) != 0)
          object.WriteFmt("mat.w = int(round({}{}.w * 255.0));\n", in_color_name, j);
        else if ((components & VB_HAS_COL0) != 0)
          object.WriteFmt("mat.w = int(round({}0.w * 255.0));\n", in_color_name);
        else
          object.WriteFmt("mat.w = 255;\n");
      }
      else  // from color
      {
        object.WriteFmt("mat.w = {}[{}].w;\n", I_MATERIALS, j + 2);
      }
    }

    if ((uid_data.enablelighting & (1 << (j + 2))) != 0)
    {
      if ((uid_data.ambsource & (1 << (j + 2))) != 0)  // from vertex
      {
        if ((components & (VB_HAS_COL0 << j)) != 0)
        {
          object.WriteFmt("lacc.w = int(round({}{}.w * 255.0));\n", in_color_name, j);
        }
        else if ((components & VB_HAS_COL0) != 0)
        {
          object.WriteFmt("lacc.w = int(round({}0.w * 255.0));\n", in_color_name);
        }
        else
        {
          // TODO: The same for alpha: We want to read from vertex, but the vertex has no color
          object.WriteFmt("lacc.w = 255;\n");
        }
      }
      else  // from color
      {
        object.WriteFmt("lacc.w = {}[{}].w;\n", I_MATERIALS, j);
      }
    }
    else
    {
      object.WriteFmt("lacc.w = 255;\n");
    }

    if ((uid_data.enablelighting & (1 << j)) != 0)  // Color lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((uid_data.light_mask & (1 << (i + 8 * j))) != 0)
          GenerateLightShader(object, uid_data, i, j, false);
      }
    }
    if ((uid_data.enablelighting & (1 << (j + 2))) != 0)  // Alpha lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((uid_data.light_mask & (1 << (i + 8 * (j + 2)))) != 0)
          GenerateLightShader(object, uid_data, i, j + 2, true);
      }
    }
    object.WriteFmt("lacc = clamp(lacc, 0, 255);\n");
    object.WriteFmt("{}{} = float4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n", dest, j);
    object.WriteFmt("}}\n");
  }
}

void GetLightingShaderUid(LightingUidData& uid_data)
{
  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    uid_data.matsource |= xfmem.color[j].matsource << j;
    uid_data.matsource |= xfmem.alpha[j].matsource << (j + 2);
    uid_data.enablelighting |= xfmem.color[j].enablelighting << j;
    uid_data.enablelighting |= xfmem.alpha[j].enablelighting << (j + 2);

    if ((uid_data.enablelighting & (1 << j)) != 0)  // Color lights
    {
      uid_data.ambsource |= xfmem.color[j].ambsource << j;
      uid_data.attnfunc |= xfmem.color[j].attnfunc << (2 * j);
      uid_data.diffusefunc |= xfmem.color[j].diffusefunc << (2 * j);
      uid_data.light_mask |= xfmem.color[j].GetFullLightMask() << (8 * j);
    }
    if ((uid_data.enablelighting & (1 << (j + 2))) != 0)  // Alpha lights
    {
      uid_data.ambsource |= xfmem.alpha[j].ambsource << (j + 2);
      uid_data.attnfunc |= xfmem.alpha[j].attnfunc << (2 * (j + 2));
      uid_data.diffusefunc |= xfmem.alpha[j].diffusefunc << (2 * (j + 2));
      uid_data.light_mask |= xfmem.alpha[j].GetFullLightMask() << (8 * (j + 2));
    }
  }
}
