// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

enum class APIType;

namespace TextureConversionShader
{
u16 GetEncodedSampleCount(u32 format);

const char* GenerateEncodingShader(u32 format, APIType ApiType);
}
