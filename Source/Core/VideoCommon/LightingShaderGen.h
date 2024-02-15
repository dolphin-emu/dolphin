// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include "Common/CommonTypes.h"

class ShaderCode;

#define LIGHT_COL "{}[{}].color.{}"
#define LIGHT_COL_PARAMS(index, swizzle) (I_LIGHTS), (index), (swizzle)

#define LIGHT_COSATT "{}[{}].cosatt"
#define LIGHT_COSATT_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_DISTATT "{}[{}].distatt"
#define LIGHT_DISTATT_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_POS "{}[{}].pos"
#define LIGHT_POS_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_DIR "{}[{}].dir"
#define LIGHT_DIR_PARAMS(index) (I_LIGHTS), (index)

/**
 * Common uid data used for shader generators that use lighting calculations.
 */
struct LightingUidData
{
  u32 matsource : 4;       // 4x1 bit
  u32 enablelighting : 4;  // 4x1 bit
  u32 ambsource : 4;       // 4x1 bit
  u32 diffusefunc : 8;     // 4x2 bits
  u32 attnfunc : 8;        // 4x2 bits
  u32 light_mask : 32;     // 4x8 bits
};

constexpr char s_lighting_struct[] = "struct Light {\n"
                                     "\tint4 color;\n"
                                     "\tfloat4 cosatt;\n"
                                     "\tfloat4 distatt;\n"
                                     "\tfloat4 pos;\n"
                                     "\tfloat4 dir;\n"
                                     "};\n";

void GenerateLightingShaderCode(ShaderCode& object, const LightingUidData& uid_data,
                                std::string_view in_color_name, std::string_view dest);
void GetLightingShaderUid(LightingUidData& uid_data);

void GenerateCustomLightingHeaderDetails(ShaderCode* out, u32 enablelighting, u32 light_mask);
void GenerateCustomLightingImplementation(ShaderCode* out, const LightingUidData& uid_data,
                                          std::string_view in_color_name);
