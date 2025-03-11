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

  // Create a normalized vector pointing from the light to the current position.  We manualy
  // normalize instead of using normalize() because the raw distance is needed for spot lights.
  object.Write("    float3 ldir = " LIGHT_POS ".xyz - pos.xyz;\n", LIGHT_POS_PARAMS(index));
  object.Write("    float dist2 = dot(ldir, ldir);\n"
               "    float dist = sqrt(dist2);\n"
               "    ldir = ldir / dist;\n");

  switch (attn_func)
  {
  case AttenuationFunc::None:
    // This logic correctly reproduces the behavior (if diffuse > 0, then lacc is 255, if it's < 0
    // lacc is 0, and if it's equal to 0 lacc is unchanged, but with DiffuseFunc::None lacc instead
    // has the light's color added to it), but may be an overly jank implementation (and might give
    // incorrect results for a light value of 1/256, for instance; testing is needed)
    if (diffuse_func == DiffuseFunc::None)
      object.Write("    float attn = 1.0;\n");
    else
      object.Write("    float attn = 1024.0;\n");
    break;
  case AttenuationFunc::Dir:
    object.Write("    float attn = 1.0;\n");
    break;
  case AttenuationFunc::Spec:
    object.Write("    float cosine = 0.0;\n"
                 "    // Ensure that the object is facing the light\n"
                 "    if (dot(_normal, ldir) >= 0.0) {{\n"
                 "      // Compute the cosine of the angle between the object normal\n"
                 "      // and the half-angle direction for the viewer\n"
                 "      // (assuming the half-angle direction is a unit vector)\n"
                 "      cosine = max(0.0, dot(_normal, " LIGHT_DIR ".xyz));\n",
                 LIGHT_DIR_PARAMS(index));
    object.Write("    }}\n"
                 "    // Specular lights use the angle for the denominator as well\n"
                 "    dist = cosine;\n"
                 "    dist2 = dist * dist;\n");
    break;
  case AttenuationFunc::Spot:
    object.Write("    // Compute the cosine of the angle between the vector to the object\n"
                 "    // and the light's direction (assuming the direction is a unit vector)\n"
                 "    float cosine = max(0.0, dot(ldir, " LIGHT_DIR ".xyz));\n",
                 LIGHT_DIR_PARAMS(index));
    break;
  }

  if (attn_func == AttenuationFunc::Spot || attn_func == AttenuationFunc::Spec)
  {
    object.Write("    float3 cosAttn = " LIGHT_COSATT ".xyz;\n", LIGHT_COSATT_PARAMS(index));
    object.Write("    float3 distAttn = " LIGHT_DISTATT ".xyz;\n", LIGHT_DISTATT_PARAMS(index));
    object.Write("    float cosine2 = cosine * cosine;\n"
                 ""
                 "    // This is equivalent to dot(cosAttn, float3(1.0, attn, attn*attn)),\n"
                 "    // except with spot lights games often don't set the direction value,\n"
                 "    // as they configure cosAttn to (1, 0, 0).  GX light objects are often\n"
                 "    // stack-allocated, so those values are uninitialized and may be\n"
                 "    // arbitrary garbage, including NaN or Inf, or become Inf when squared.\n"
                 "    float numerator = cosAttn.x;                            // constant term\n"
                 "    if (cosAttn.y != 0.0) numerator += cosAttn.y * cosine;  // linear term\n"
                 "    if (cosAttn.z != 0.0) numerator += cosAttn.z * cosine2; // quadratic term\n"
                 "    // Same with the denominator, though this generally is not garbage\n"
                 "    // Note that VertexShaderManager ensures that distAttn is not zero, which\n"
                 "    // should prevent division by zero (TODO: what does real hardware do?)\n"
                 "    float denominator = distAttn.x;                           // constant term\n"
                 "    if (distAttn.y != 0.0) denominator += distAttn.y * dist;  // linear term\n"
                 "    if (distAttn.z != 0.0) denominator += distAttn.z * dist2; // quadratic term\n"
                 ""
                 "    float attn = max(0.0f, numerator / denominator);\n");
  }

  switch (diffuse_func)
  {
  case DiffuseFunc::None:
    object.Write("    float diffuse = 1.0;\n");
    break;
  case DiffuseFunc::Sign:
  default:  // Confirmed by hardware testing that invalid values use this
    object.Write("    float diffuse = dot(ldir, _normal);\n");
    break;
  case DiffuseFunc::Clamp:
    object.Write("    float diffuse = max(0.0, dot(ldir, _normal));\n");
    break;
  }

  object.Write("    lacc.{} += int{}(round(attn * diffuse * float{}(" LIGHT_COL ")));\n", swizzle,
               swizzle_components, swizzle_components, LIGHT_COL_PARAMS(index, swizzle));
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

void GenerateCustomLightingHeaderDetails(ShaderCode* out, u32 enablelighting, u32 light_mask)
{
  u32 light_count = 0;
  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    if ((enablelighting & (1 << j)) != 0)  // Color lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((light_mask & (1 << (i + 8 * j))) != 0)
        {
          light_count++;
        }
      }
    }
    if ((enablelighting & (1 << (j + 2))) != 0)  // Alpha lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((light_mask & (1 << (i + 8 * (j + 2)))) != 0)
        {
          light_count++;
        }
      }
    }
  }
  if (light_count > 0)
  {
    out->Write("\tCustomShaderLightData[{}] light;\n", light_count);
  }
  else
  {
    // Cheat so shaders compile
    out->Write("\tCustomShaderLightData[1] light;\n", light_count);
  }
  out->Write("\tint light_count;\n");
}

static void GenerateLighting(ShaderCode* out, const LightingUidData& uid_data, int index,
                             int litchan_index, u32 channel_index, u32 custom_light_index,
                             bool alpha)
{
  const auto attnfunc =
      static_cast<AttenuationFunc>((uid_data.attnfunc >> (2 * litchan_index)) & 0x3);

  const std::string_view light_type = alpha ? "alpha" : "color";
  const std::string name = fmt::format("lights_chan{}_{}", channel_index, light_type);

  out->Write("\t{{\n");
  out->Write("\t\tcustom_data.{}[{}].direction = " LIGHT_DIR ".xyz;\n", name, custom_light_index,
             LIGHT_DIR_PARAMS(index));
  out->Write("\t\tcustom_data.{}[{}].position = " LIGHT_POS ".xyz;\n", name, custom_light_index,
             LIGHT_POS_PARAMS(index));
  out->Write("\t\tcustom_data.{}[{}].cosatt = " LIGHT_COSATT ";\n", name, custom_light_index,
             LIGHT_COSATT_PARAMS(index));
  out->Write("\t\tcustom_data.{}[{}].distatt = " LIGHT_DISTATT ";\n", name, custom_light_index,
             LIGHT_DISTATT_PARAMS(index));
  out->Write("\t\tcustom_data.{}[{}].attenuation_type = {};\n", name, custom_light_index,
             static_cast<u32>(attnfunc));
  if (alpha)
  {
    out->Write("\t\tcustom_data.{}[{}].color = float3(" LIGHT_COL
               ") / float3(255.0, 255.0, 255.0);\n",
               name, custom_light_index, LIGHT_COL_PARAMS(index, alpha ? "a" : "rgb"));
  }
  else
  {
    out->Write("\t\tcustom_data.{}[{}].color = " LIGHT_COL " / float3(255.0, 255.0, 255.0);\n",
               name, custom_light_index, LIGHT_COL_PARAMS(index, alpha ? "a" : "rgb"));
  }
  out->Write("\t}}\n");
}

void GenerateCustomLightingImplementation(ShaderCode* out, const LightingUidData& uid_data,
                                          std::string_view in_color_name)
{
  for (u32 i = 0; i < 8; i++)
  {
    for (u32 channel_index = 0; channel_index < NUM_XF_COLOR_CHANNELS; channel_index++)
    {
      out->Write("\tcustom_data.lights_chan{}_color[{}].direction = float3(0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_color[{}].position = float3(0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_color[{}].color = float3(0, 0, 0);\n", channel_index,
                 i);
      out->Write("\tcustom_data.lights_chan{}_color[{}].cosatt = float4(0, 0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_color[{}].distatt = float4(0, 0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_color[{}].attenuation_type = 0;\n", channel_index, i);

      out->Write("\tcustom_data.lights_chan{}_alpha[{}].direction = float3(0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].position = float3(0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].color = float3(0, 0, 0);\n", channel_index,
                 i);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].cosatt = float4(0, 0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].distatt = float4(0, 0, 0, 0);\n",
                 channel_index, i);
      out->Write("\tcustom_data.lights_chan{}_alpha[{}].attenuation_type = 0;\n", channel_index, i);
    }
  }

  for (u32 j = 0; j < NUM_XF_COLOR_CHANNELS; j++)
  {
    const bool colormatsource = !!(uid_data.matsource & (1 << j));
    if (colormatsource)  // from vertex
      out->Write("custom_data.base_material[{}] = {}{};\n", j, in_color_name, j);
    else  // from color
      out->Write("custom_data.base_material[{}] = {}[{}] / 255.0;\n", j, I_MATERIALS, j + 2);

    if ((uid_data.enablelighting & (1 << j)) != 0)
    {
      if ((uid_data.ambsource & (1 << j)) != 0)  // from vertex
        out->Write("custom_data.ambient_lighting[{}] = {}{};\n", j, in_color_name, j);
      else  // from color
        out->Write("custom_data.ambient_lighting[{}] = {}[{}] / 255.0;\n", j, I_MATERIALS, j);
    }
    else
    {
      out->Write("custom_data.ambient_lighting[{}] = float4(1, 1, 1, 1);\n", j);
    }

    // check if alpha is different
    const bool alphamatsource = !!(uid_data.matsource & (1 << (j + 2)));
    if (alphamatsource != colormatsource)
    {
      if (alphamatsource)  // from vertex
        out->Write("custom_data.base_material[{}].w = {}{}.w;\n", j, in_color_name, j);
      else  // from color
        out->Write("custom_data.base_material[{}].w = {}[{}].w / 255.0;\n", j, I_MATERIALS, j + 2);
    }

    if ((uid_data.enablelighting & (1 << (j + 2))) != 0)
    {
      if ((uid_data.ambsource & (1 << (j + 2))) != 0)  // from vertex
        out->Write("custom_data.ambient_lighting[{}].w = {}{}.w;\n", j, in_color_name, j);
      else  // from color
        out->Write("custom_data.ambient_lighting[{}].w = {}[{}].w / 255.0;\n", j, I_MATERIALS, j);
    }
    else
    {
      out->Write("custom_data.ambient_lighting[{}].w = 1;\n", j);
    }

    u32 light_count = 0;
    if ((uid_data.enablelighting & (1 << j)) != 0)  // Color lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((uid_data.light_mask & (1 << (i + 8 * j))) != 0)
        {
          GenerateLighting(out, uid_data, i, j, j, light_count, false);
          light_count++;
        }
      }
    }
    out->Write("\tcustom_data.light_chan{}_color_count = {};\n", j, light_count);

    light_count = 0;
    if ((uid_data.enablelighting & (1 << (j + 2))) != 0)  // Alpha lights
    {
      for (int i = 0; i < 8; ++i)
      {
        if ((uid_data.light_mask & (1 << (i + 8 * (j + 2)))) != 0)
        {
          GenerateLighting(out, uid_data, i, j + 2, j, light_count, true);
          light_count++;
        }
      }
    }
    out->Write("\tcustom_data.light_chan{}_alpha_count = {};\n", j, light_count);
  }
}
