// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoCommon.h"

namespace TextureConversionShader
{
u16 GetEncodedSampleCount(u32 format);

const char *GenerateEncodingShader(u32 format, API_TYPE ApiType = API_OPENGL);

}
