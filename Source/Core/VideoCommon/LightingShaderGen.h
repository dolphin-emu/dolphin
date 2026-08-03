// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

enum class MatSource : u8;
enum class AmbSource : u8;
enum class AttenuationFunc : u8;
enum class DiffuseFunc : u8;

struct LightFunc
{
  MatSource matsource : 1;
  bool enablelighting : 1;
  AmbSource ambsource : 1;       // used if enablelighting is true
  DiffuseFunc diffusefunc : 2;   // used if at least one light is enabled
  AttenuationFunc attnfunc : 2;  // used if at least one light is enabled
  u8 pad : 1;
};

static_assert(sizeof(LightFunc) == 1, "sizeof(LightFunc) must be 1 byte");

struct LightChannel
{
  LightFunc func = {};
  u8 mask = 0;
};

static_assert(sizeof(LightChannel) == 2, "sizeof(LightChannel) must be 2 bytes");

/**
 * Common uid data used for shader generators that use lighting calculations.
 */
struct LightingUidData
{
  LightChannel color[2];
  LightChannel alpha[2];
};

static_assert(sizeof(LightingUidData) == 8, "sizeof(LightingUidData) must be 8 bytes");

constexpr char s_lighting_struct[] = "struct Light {\n"
                                     "\tint4 color;\n"
                                     "\tfloat4 cosatt;\n"
                                     "\tfloat4 distatt;\n"
                                     "\tfloat4 pos;\n"
                                     "\tfloat4 dir;\n"
                                     "};\n";

void GenerateLightingShaderHeader(ShaderCode& object, const LightingUidData& uid_data);
void GetLightingShaderUid(LightingUidData& uid_data, size_t numColorChans);
void GenerateCustomLighting(ShaderCode* out, const LightingUidData& uid_data);
