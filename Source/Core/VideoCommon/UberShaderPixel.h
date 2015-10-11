// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/PixelShaderGen.h"

namespace UberShader
{
	ShaderCode GenPixelShader(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, bool per_pixel_depth);
}