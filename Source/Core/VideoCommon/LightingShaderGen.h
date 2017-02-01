// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class ShaderCode;

#define LIGHT_COL "%s[%d].color.%s"
#define LIGHT_COL_PARAMS(index, swizzle) (I_LIGHTS), (index), (swizzle)

#define LIGHT_COSATT "%s[%d].cosatt"
#define LIGHT_COSATT_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_DISTATT "%s[%d].distatt"
#define LIGHT_DISTATT_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_POS "%s[%d].pos"
#define LIGHT_POS_PARAMS(index) (I_LIGHTS), (index)

#define LIGHT_DIR "%s[%d].dir"
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

static const char s_lighting_struct[] = "struct Light {\n"
                                        "\tint4 color;\n"
                                        "\tfloat4 cosatt;\n"
                                        "\tfloat4 distatt;\n"
                                        "\tfloat4 pos;\n"
                                        "\tfloat4 dir;\n"
                                        "};\n";

void GenerateLightingShaderCode(ShaderCode& object, const LightingUidData& uid_data, int components,
                                u32 numColorChans, const char* inColorName, const char* dest);
void GetLightingShaderUid(LightingUidData& uid_data);
