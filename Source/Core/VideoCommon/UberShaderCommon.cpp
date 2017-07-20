// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/UberShaderCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace UberShader
{
void WriteUberShaderCommonHeader(ShaderCode& out, APIType api_type,
                                 const ShaderHostConfig& host_config)
{
  // ==============================================
  //  BitfieldExtract for APIs which don't have it
  // ==============================================
  if (!host_config.backend_bitfield)
  {
    out.Write("uint bitfieldExtract(uint val, int off, int size) {\n"
              "	// This built-in function is only support in OpenGL 4.0+ and ES 3.1+\n"
              "	// Microsoft's HLSL compiler automatically optimises this to a bitfield extract "
              "instruction.\n"
              "	uint mask = uint((1 << size) - 1);\n"
              "	return uint(val >> off) & mask;\n"
              "}\n\n");
  }
}
}
