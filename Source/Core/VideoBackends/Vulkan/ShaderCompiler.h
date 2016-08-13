// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
namespace ShaderCompiler
{
// SPIR-V compiled code type
using SPIRVCodeType = u32;
using SPIRVCodeVector = std::vector<SPIRVCodeType>;

// Compile a vertex shader to SPIR-V.
bool CompileVertexShader(SPIRVCodeVector* out_code, const char* source_code,
                         size_t source_code_length, bool prepend_header = true);

// Compile a geometry shader to SPIR-V.
bool CompileGeometryShader(SPIRVCodeVector* out_code, const char* source_code,
                           size_t source_code_length, bool prepend_header = true);

// Compile a fragment shader to SPIR-V.
bool CompileFragmentShader(SPIRVCodeVector* out_code, const char* source_code,
                           size_t source_code_length, bool prepend_header = true);

}  // namespace ShaderCompiler
}  // namespace Vulkan
