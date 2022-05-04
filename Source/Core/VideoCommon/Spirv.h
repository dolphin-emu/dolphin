// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace SPIRV
{
// SPIR-V compiled code type
using CodeType = u32;
using CodeVector = std::vector<CodeType>;

// Compile a vertex shader to SPIR-V.
std::optional<CodeVector> CompileVertexShader(std::string_view source_code);

// Compile a geometry shader to SPIR-V.
std::optional<CodeVector> CompileGeometryShader(std::string_view source_code);

// Compile a fragment shader to SPIR-V.
std::optional<CodeVector> CompileFragmentShader(std::string_view source_code);

// Compile a compute shader to SPIR-V.
std::optional<CodeVector> CompileComputeShader(std::string_view source_code);
}  // namespace SPIRV
