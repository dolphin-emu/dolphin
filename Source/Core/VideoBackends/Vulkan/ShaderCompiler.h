// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace Vulkan
{
namespace ShaderCompiler
{
// SPIR-V compiled code type
using SPIRVCodeType = u32;
using SPIRVCodeVector = std::vector<SPIRVCodeType>;

// Compile a vertex shader to SPIR-V.
bool CompileVertexShader(SPIRVCodeVector* out_code, std::string_view source_code);

// Compile a geometry shader to SPIR-V.
bool CompileGeometryShader(SPIRVCodeVector* out_code, std::string_view source_code);

// Compile a fragment shader to SPIR-V.
bool CompileFragmentShader(SPIRVCodeVector* out_code, std::string_view source_code);

// Compile a compute shader to SPIR-V.
bool CompileComputeShader(SPIRVCodeVector* out_code, std::string_view source_code);

}  // namespace ShaderCompiler
}  // namespace Vulkan
