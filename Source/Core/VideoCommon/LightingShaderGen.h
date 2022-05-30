// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include "Common/BitField.h"
#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/XFMemory.h"

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
  u64 hex;

  BFVIEWARRAY_M(hex, MatSource, 0, 1, 4, matsource);        // 4x1 bit
  BFVIEWARRAY_M(hex, bool, 4, 1, 4, enablelighting);        // 4x1 bit
  BFVIEWARRAY_M(hex, AmbSource, 8, 1, 4, ambsource);        // 4x1 bit
  BFVIEWARRAY_M(hex, DiffuseFunc, 16, 2, 4, diffusefunc);   // 4x2 bits
  BFVIEWARRAY_M(hex, AttenuationFunc, 24, 2, 4, attnfunc);  // 4x2 bits
  BFVIEWARRAY_M(hex, u8, 32, 8, 4, light_mask);             // 4x8 bits or 4x8x1 bit
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
