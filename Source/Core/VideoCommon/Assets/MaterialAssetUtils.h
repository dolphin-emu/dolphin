// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string_view>

struct MaterialData;
struct RasterSurfaceShaderData;

namespace VideoCommon
{
void SetMaterialPropertiesFromShader(const RasterSurfaceShaderData& shader, MaterialData* material);
void SetMaterialTexturesFromShader(const RasterSurfaceShaderData& shader, MaterialData* material);
}  // namespace VideoCommon
