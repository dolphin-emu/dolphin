// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

class ShaderCode;
enum class APIType;
union ShaderHostConfig;

namespace UberShader
{
// Vertex lighting
void WriteLightingFunction(ShaderCode& out);
void WriteVertexLighting(ShaderCode& out, APIType api_type, std::string_view world_pos_var,
                         std::string_view normal_var, std::string_view in_color_0_var,
                         std::string_view in_color_1_var, std::string_view out_color_0_var,
                         std::string_view out_color_1_var);
}  // namespace UberShader
