// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common.h"
#include "TextureDecoder.h"
#include "VideoCommon.h"

namespace TextureConversionShader
{
u16 GetEncodedSampleCount(u32 format);

const char *GenerateEncodingShader(u32 format, API_TYPE ApiType = API_OPENGL);

}
