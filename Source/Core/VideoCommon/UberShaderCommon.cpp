// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/UberShaderCommon.h"

#include "Common/EnumUtils.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

namespace UberShader
{
void WriteLightingFunction(ShaderCode& out)
{
  // ==============================================
  // Lighting channel calculation helper
  // ==============================================
  out.Write("int4 CalculateLighting(uint index, uint attnfunc, uint diffusefunc, float3 pos, "
            "float3 normal) {{\n"
            "  float3 ldir, h, cosAttn, distAttn;\n"
            "  float dist, dist2, attn;\n"
            "\n"
            "  switch (attnfunc) {{\n");
  out.Write("  case {:s}:\n", AttenuationFunc::None);
  out.Write("  case {:s}:\n", AttenuationFunc::Dir);
  out.Write("    ldir = normalize(" I_LIGHTS "[index].pos.xyz - pos.xyz);\n"
            "    attn = 1.0;\n"
            "    if (length(ldir) == 0.0)\n"
            "      ldir = normal;\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", AttenuationFunc::Spec);
  out.Write("    ldir = normalize(" I_LIGHTS "[index].pos.xyz - pos.xyz);\n"
            "    attn = (dot(normal, ldir) >= 0.0) ? max(0.0, dot(normal, " I_LIGHTS
            "[index].dir.xyz)) : 0.0;\n"
            "    cosAttn = " I_LIGHTS "[index].cosatt.xyz;\n");
  out.Write("    if (diffusefunc == {:s})\n", DiffuseFunc::None);
  out.Write("      distAttn = " I_LIGHTS "[index].distatt.xyz;\n"
            "    else\n"
            "      distAttn = normalize(" I_LIGHTS "[index].distatt.xyz);\n"
            "    attn = max(0.0, dot(cosAttn, float3(1.0, attn, attn*attn))) / dot(distAttn, "
            "float3(1.0, attn, attn*attn));\n"
            "    break;\n\n");
  out.Write("  case {:s}:\n", AttenuationFunc::Spot);
  out.Write("    ldir = " I_LIGHTS "[index].pos.xyz - pos.xyz;\n"
            "    dist2 = dot(ldir, ldir);\n"
            "    dist = sqrt(dist2);\n"
            "    ldir = ldir / dist;\n"
            "    attn = max(0.0, dot(ldir, " I_LIGHTS "[index].dir.xyz));\n"
            "    attn = max(0.0, " I_LIGHTS "[index].cosatt.x + " I_LIGHTS
            "[index].cosatt.y * attn + " I_LIGHTS "[index].cosatt.z * attn * attn) / dot(" I_LIGHTS
            "[index].distatt.xyz, float3(1.0, dist, dist2));\n"
            "    break;\n\n");
  out.Write("  default:\n"
            "    attn = 1.0;\n"
            "    ldir = normal;\n"
            "    break;\n"
            "  }}\n"
            "\n"
            "  switch (diffusefunc) {{\n");
  out.Write("  case {:s}:\n", DiffuseFunc::None);
  out.Write("    return int4(round(attn * float4(" I_LIGHTS "[index].color)));\n\n");
  out.Write("  case {:s}:\n", DiffuseFunc::Sign);
  out.Write("    return int4(round(attn * dot(ldir, normal) * float4(" I_LIGHTS
            "[index].color)));\n\n");
  out.Write("  case {:s}:\n", DiffuseFunc::Clamp);
  out.Write("    return int4(round(attn * max(0.0, dot(ldir, normal)) * float4(" I_LIGHTS
            "[index].color)));\n\n");
  out.Write("  default:\n"
            "    return int4(0, 0, 0, 0);\n"
            "  }}\n"
            "}}\n\n");
}

void WriteVertexLighting(ShaderCode& out, APIType api_type, std::string_view world_pos_var,
                         std::string_view normal_var, std::string_view in_color_0_var,
                         std::string_view in_color_1_var, std::string_view out_color_0_var,
                         std::string_view out_color_1_var)
{
  out.Write("// Lighting\n");
  out.Write("for (uint chan = 0u; chan < {}u; chan++) {{\n", NUM_XF_COLOR_CHANNELS);
  out.Write("  uint colorreg = xfmem_color(chan);\n"
            "  uint alphareg = xfmem_alpha(chan);\n"
            "  int4 mat = " I_MATERIALS "[chan + 2u]; \n"
            "  int4 lacc = int4(255, 255, 255, 255);\n"
            "\n");

  out.Write("  if ({} != 0u)\n", BitfieldExtract<&LitChannel::matsource>("colorreg"));
  out.Write("    mat.xyz = int3(round(((chan == 0u) ? {}.xyz : {}.xyz) * 255.0));\n",
            in_color_0_var, in_color_1_var);

  out.Write("  if ({} != 0u)\n", BitfieldExtract<&LitChannel::matsource>("alphareg"));
  out.Write("    mat.w = int(round(((chan == 0u) ? {}.w : {}.w) * 255.0));\n", in_color_0_var,
            in_color_1_var);
  out.Write("  else\n"
            "    mat.w = " I_MATERIALS " [chan + 2u].w;\n"
            "\n");

  out.Write("  if ({} != 0u) {{\n", BitfieldExtract<&LitChannel::enablelighting>("colorreg"));
  out.Write("    if ({} != 0u)\n", BitfieldExtract<&LitChannel::ambsource>("colorreg"));
  out.Write("      lacc.xyz = int3(round(((chan == 0u) ? {}.xyz : {}.xyz) * 255.0));\n",
            in_color_0_var, in_color_1_var);
  out.Write("    else\n"
            "      lacc.xyz = " I_MATERIALS " [chan].xyz;\n"
            "\n");
  out.Write("    uint light_mask = {} | ({} << 4u);\n",
            BitfieldExtract<&LitChannel::lightMask0_3>("colorreg"),
            BitfieldExtract<&LitChannel::lightMask4_7>("colorreg"));
  out.Write("    uint attnfunc = {};\n", BitfieldExtract<&LitChannel::attnfunc>("colorreg"));
  out.Write("    uint diffusefunc = {};\n", BitfieldExtract<&LitChannel::diffusefunc>("colorreg"));
  out.Write(
      "    for (uint light_index = 0u; light_index < 8u; light_index++) {{\n"
      "      if ((light_mask & (1u << light_index)) != 0u)\n"
      "        lacc.xyz += CalculateLighting(light_index, attnfunc, diffusefunc, {}, {}).xyz;\n",
      world_pos_var, normal_var);
  out.Write("    }}\n"
            "  }}\n"
            "\n");

  out.Write("  if ({} != 0u) {{\n", BitfieldExtract<&LitChannel::enablelighting>("alphareg"));
  out.Write("    if ({} != 0u) {{\n", BitfieldExtract<&LitChannel::ambsource>("alphareg"));
  out.Write("      if ((components & ({}u << chan)) != 0u) // VB_HAS_COL0\n",
            Common::ToUnderlying(VB_HAS_COL0));
  out.Write("        lacc.w = int(round(((chan == 0u) ? {}.w : {}.w) * 255.0));\n", in_color_0_var,
            in_color_1_var);
  out.Write("      else if ((components & {}u) != 0u) // VB_HAS_COLO0\n",
            Common::ToUnderlying(VB_HAS_COL0));
  out.Write("        lacc.w = int(round({}.w * 255.0));\n", in_color_0_var);
  out.Write("      else\n"
            "        lacc.w = 255;\n"
            "    }} else {{\n"
            "      lacc.w = " I_MATERIALS " [chan].w;\n"
            "    }}\n"
            "\n");
  out.Write("    uint light_mask = {} | ({} << 4u);\n",
            BitfieldExtract<&LitChannel::lightMask0_3>("alphareg"),
            BitfieldExtract<&LitChannel::lightMask4_7>("alphareg"));
  out.Write("    uint attnfunc = {};\n", BitfieldExtract<&LitChannel::attnfunc>("alphareg"));
  out.Write("    uint diffusefunc = {};\n", BitfieldExtract<&LitChannel::diffusefunc>("alphareg"));
  out.Write("    for (uint light_index = 0u; light_index < 8u; light_index++) {{\n\n"
            "      if ((light_mask & (1u << light_index)) != 0u)\n\n"
            "        lacc.w += CalculateLighting(light_index, attnfunc, diffusefunc, {}, {}).w;\n",
            world_pos_var, normal_var);
  out.Write("    }}\n"
            "  }}\n"
            "\n");

  out.Write("  lacc = clamp(lacc, 0, 255);\n"
            "\n"
            "  // Hopefully GPUs that can support dynamic indexing will optimize this.\n"
            "  float4 lit_color = float4((mat * (lacc + (lacc >> 7))) >> 8) / 255.0;\n"
            "  switch (chan) {{\n"
            "  case 0u: {} = lit_color; break;\n",
            out_color_0_var);
  out.Write("  case 1u: {} = lit_color; break;\n", out_color_1_var);
  out.Write("  }}\n"
            "}}\n"
            "\n");
}
}  // namespace UberShader
