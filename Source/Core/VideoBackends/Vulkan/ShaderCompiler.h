// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace Vulkan::ShaderCompiler
{
// SPIR-V compiled code type
using SPIRVCodeType = u32;
using SPIRVCodeVector = std::vector<SPIRVCodeType>;

// Compile a vertex shader to SPIR-V.
std::optional<SPIRVCodeVector> CompileVertexShader(std::string_view source_code);

// Compile a geometry shader to SPIR-V.
std::optional<SPIRVCodeVector> CompileGeometryShader(std::string_view source_code);

// Compile a fragment shader to SPIR-V.
std::optional<SPIRVCodeVector> CompileFragmentShader(std::string_view source_code);

// Compile a compute shader to SPIR-V.
std::optional<SPIRVCodeVector> CompileComputeShader(std::string_view source_code);
}  // namespace Vulkan::ShaderCompiler
