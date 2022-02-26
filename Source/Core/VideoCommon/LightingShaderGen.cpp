// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/LightingShaderGen.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/XFMemory.h"

static void GenerateLightShader(ShaderCode& object, const LightingUidData& uid_data, int index,
                                AttenuationFunc attn_func, DiffuseFunc diffuse_func, bool alpha)
{
  object.Write("  {{ // {} light {}\n", alpha ? "Alpha" : "Color", index);
  const char* swizzle = alpha ? "a" : "rgb";
  const char* swizzle_components = (alpha) ? "" : "3";

  switch (attn_func)
  {
  case AttenuationFunc::None:
  case AttenuationFunc::Dir:
    object.Write("    float3 ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n",
                 LIGHT_POS_PARAMS(index));
    object.Write("    float attn = 1.0;\n");
    break;
  case AttenuationFunc::Spec:
    object.Write("    float3 ldir = normalize(" LIGHT_POS ".xyz - pos.xyz);\n",
                 LIGHT_POS_PARAMS(index));
    object.Write("    float attn = (dot(_normal, ldir) >= 0.0) ? max(0.0, dot(_normal, " LIGHT_DIR
                 ".xyz)) : 0.0;\n",
                 LIGHT_DIR_PARAMS(index));
    object.Write("    float3 cosAttn = " LIGHT_COSATT ".xyz;\n", LIGHT_COSATT_PARAMS(index));
    object.Write("    float3 distAttn = {}(" LIGHT_DISTATT ".xyz);\n",
                 (diffuse_func == DiffuseFunc::None) ? "" : "normalize",
                 LIGHT_DISTATT_PARAMS(index));
    object.Write("    attn = max(0.0f, dot(cosAttn, float3(1.0, attn, attn*attn))) / dot(distAttn, "
                 "float3(1.0, attn, attn*attn));\n");
    break;
  case AttenuationFunc::Spot:
    object.Write("    float3 ldir = " LIGHT_POS ".xyz - pos.xyz;\n", LIGHT_POS_PARAMS(index));
    object.Write("    float dist2 = dot(ldir, ldir);\n"
                 "    float dist = sqrt(dist2);\n"
                 "    ldir = ldir / dist;\n"
                 "    float attn = max(0.0, dot(ldir, " LIGHT_DIR ".xyz));\n",
                 LIGHT_DIR_PARAMS(index));
    // attn*attn may overflow
    object.Write("    attn = max(0.0, " LIGHT_COSATT ".x + " LIGHT_COSATT ".y*attn + " LIGHT_COSATT
                 ".z*attn*attn) / dot(" LIGHT_DISTATT ".xyz, float3(1.0,dist,dist2));\n",
                 LIGHT_COSATT_PARAMS(index), LIGHT_COSATT_PARAMS(index), LIGHT_COSATT_PARAMS(index),
                 LIGHT_DISTATT_PARAMS(index));
    break;
  }

  switch (diffuse_func)
  {
  case DiffuseFunc::None:
    object.Write("    lacc.{} += int{}(round(attn * float{}(" LIGHT_COL ")));\n", swizzle,
                 swizzle_components, swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  case DiffuseFunc::Sign:
  case DiffuseFunc::Clamp:
    object.Write("    lacc.{} += int{}(round(attn * {}dot(ldir, _normal)) * float{}(" LIGHT_COL
                 ")));\n",
                 swizzle, swizzle_components, diffuse_func != DiffuseFunc::Sign ? "max(0.0," : "(",
                 swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
    break;
  default:
    ASSERT(false);
  }

  object.Write("  }}\n");
}

// vertex shader
// lights/colors
// materials name is I_MATERIALS in vs and I_PMATERIALS in ps
// inColorName is color in vs and colors_ in ps
// dest is o.colors_ in vs and colors_ in ps
void GenerateLightingShaderCode(ShaderCode& object, const LightingUidData& uid_data,
                                std::string_view in_color_name, std::string_view dest)
{
  for (u32 chan = 0; chan < NUM_XF_COLOR_CHANNELS; chan++)
  {
    // Data for alpha is stored after all colors
    const u32 chan_a = chan + NUM_XF_COLOR_CHANNELS;

    const auto color_matsource = static_cast<MatSource>((uid_data.matsource >> chan) & 1);
    const auto color_ambsource = static_cast<AmbSource>((uid_data.ambsource >> chan) & 1);
    const bool color_enable = ((uid_data.enablelighting >> chan) & 1) != 0;
    const auto alpha_matsource = static_cast<MatSource>((uid_data.matsource >> chan_a) & 1);
    const auto alpha_ambsource = static_cast<AmbSource>((uid_data.ambsource >> chan_a) & 1);
    const bool alpha_enable = ((uid_data.enablelighting >> chan_a) & 1) != 0;

    object.Write("{{\n"
                 "  // Lighting for channel {}:\n"
                 "  // Color material source: {}\n"
                 "  // Color ambient source: {}\n"
                 "  // Color lighting enabled: {}\n"
                 "  // Alpha material source: {}\n"
                 "  // Alpha ambient source: {}\n"
                 "  // Alpha lighting enabled: {}\n",
                 chan, color_matsource, color_ambsource, color_enable, alpha_matsource,
                 alpha_ambsource, alpha_enable);

    if (color_matsource == MatSource::Vertex)
      object.Write("  int4 mat = int4(round({}{} * 255.0));\n", in_color_name, chan);
    else  // from material color register
      object.Write("  int4 mat = {}[{}];\n", I_MATERIALS, chan + 2);

    if (color_enable)
    {
      if (color_ambsource == AmbSource::Vertex)
        object.Write("  int4 lacc = int4(round({}{} * 255.0));\n", in_color_name, chan);
      else  // from ambient color register
        object.Write("  int4 lacc = {}[{}];\n", I_MATERIALS, chan);
    }
    else
    {
      object.Write("  int4 lacc = int4(255, 255, 255, 255);\n");
    }

    // check if alpha is different
    if (color_matsource != alpha_matsource)
    {
      if (alpha_matsource == MatSource::Vertex)
        object.Write("  mat.w = int(round({}{}.w * 255.0));\n", in_color_name, chan);
      else  // from material color register
        object.Write("  mat.w = {}[{}].w;\n", I_MATERIALS, chan + 2);
    }

    if (alpha_enable)
    {
      if (alpha_ambsource == AmbSource::Vertex)  // from vertex
        object.Write("  lacc.w = int(round({}{}.w * 255.0));\n", in_color_name, chan);
      else  // from ambient color register
        object.Write("  lacc.w = {}[{}].w;\n", I_MATERIALS, chan);
    }
    else
    {
      object.Write("  lacc.w = 255;\n");
    }

    if (color_enable)
    {
      const auto attnfunc = static_cast<AttenuationFunc>((uid_data.attnfunc >> (2 * chan)) & 3);
      const auto diffusefunc = static_cast<DiffuseFunc>((uid_data.diffusefunc >> (2 * chan)) & 3);
      const u32 light_mask =
          (uid_data.light_mask >> (NUM_XF_LIGHTS * chan)) & ((1 << NUM_XF_LIGHTS) - 1);
      object.Write("  // Color attenuation function: {}\n", attnfunc);
      object.Write("  // Color diffuse function: {}\n", diffusefunc);
      object.Write("  // Color light mask: {:08b}\n", light_mask);
      for (u32 light = 0; light < NUM_XF_LIGHTS; light++)
      {
        if ((light_mask & (1 << light)) != 0)
        {
          GenerateLightShader(object, uid_data, light, attnfunc, diffusefunc, false);
        }
      }
    }
    if (alpha_enable)
    {
      const auto attnfunc = static_cast<AttenuationFunc>((uid_data.attnfunc >> (2 * chan_a)) & 3);
      const auto diffusefunc = static_cast<DiffuseFunc>((uid_data.diffusefunc >> (2 * chan_a)) & 3);
      const u32 light_mask =
          (uid_data.light_mask >> (NUM_XF_LIGHTS * chan_a)) & ((1 << NUM_XF_LIGHTS) - 1);
      object.Write("  // Alpha attenuation function: {}\n", attnfunc);
      object.Write("  // Alpha diffuse function: {}\n", diffusefunc);
      object.Write("  // Alpha light mask function: {:08b}\n", light_mask);
      for (u32 light = 0; light < NUM_XF_LIGHTS; light++)
      {
        if ((light_mask & (1 << light)) != 0)
        {
          GenerateLightShader(object, uid_data, light, attnfunc, diffusefunc, true);
        }
      }
    }
    object.Write("  lacc = clamp(lacc, 0, 255);\n");
    object.Write("  {}{} = float4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n", dest, chan);
    object.Write("}}\n");
  }
}

void GetLightingShaderUid(LightingUidData& uid_data)
{
  for (u32 chan = 0; chan < NUM_XF_COLOR_CHANNELS; chan++)
  {
    // Data for alpha is stored after all colors
    const u32 chan_a = chan + NUM_XF_COLOR_CHANNELS;

    uid_data.matsource |= static_cast<u32>(xfmem.color[chan].matsource.Value()) << chan;
    uid_data.matsource |= static_cast<u32>(xfmem.alpha[chan].matsource.Value()) << chan_a;
    uid_data.enablelighting |= xfmem.color[chan].enablelighting << chan;
    uid_data.enablelighting |= xfmem.alpha[chan].enablelighting << chan_a;

    if ((uid_data.enablelighting & (1 << chan)) != 0)  // Color lights
    {
      uid_data.ambsource |= static_cast<u32>(xfmem.color[chan].ambsource.Value()) << chan;
      uid_data.attnfunc |= static_cast<u32>(xfmem.color[chan].attnfunc.Value()) << (2 * chan);
      uid_data.diffusefunc |= static_cast<u32>(xfmem.color[chan].diffusefunc.Value()) << (2 * chan);
      uid_data.light_mask |= xfmem.color[chan].GetFullLightMask() << (NUM_XF_LIGHTS * chan);
    }
    if ((uid_data.enablelighting & (1 << chan_a)) != 0)  // Alpha lights
    {
      uid_data.ambsource |= static_cast<u32>(xfmem.alpha[chan].ambsource.Value()) << chan_a;
      uid_data.attnfunc |= static_cast<u32>(xfmem.alpha[chan].attnfunc.Value()) << (2 * chan_a);
      uid_data.diffusefunc |= static_cast<u32>(xfmem.alpha[chan].diffusefunc.Value())
                              << (2 * chan_a);
      uid_data.light_mask |= xfmem.alpha[chan].GetFullLightMask() << (NUM_XF_LIGHTS * chan_a);
    }
  }
}
