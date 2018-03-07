// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractShader.h"

namespace Metal
{
namespace ShaderCompiler
{
// SPIR-V compiled code type
using SPIRVCodeType = u32;
using SPIRVCodeVector = std::vector<SPIRVCodeType>;

// Returns a string for the shader stage as a prefix (vs/ps/cs).
const char* GetStagePrefix(ShaderStage stage);

// Compile a vertex shader to SPIR-V.
bool CompileShaderToSPV(SPIRVCodeVector* out_code, ShaderStage stage, const char* source_code,
                        size_t source_code_length);

// Translates a SPIR-V shader to MSL.
bool TranslateSPVToMSL(std::string* out_msl, ShaderStage stage, const SPIRVCodeVector* in_spv);

}  // namespace ShaderCompiler
}  // namespace Metal
