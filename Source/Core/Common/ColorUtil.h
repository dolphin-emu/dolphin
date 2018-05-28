// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace Common
{
void Decode5A3Image(u32* dst, const u16* src, int width, int height);
void DecodeCI8Image(u32* dst, const u8* src, const u16* pal, int width, int height);
}  // namespace Common
