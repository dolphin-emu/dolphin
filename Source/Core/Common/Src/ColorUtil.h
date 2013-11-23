// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _COLORUTIL_H_
#define _COLORUTIL_H_

namespace ColorUtil
{

void decode5A3image(u32* dst, u16* src, int width, int height);
void decodeCI8image(u32* dst, u8* src, u16* pal, int width, int height);

}  // namespace

#endif // _COLORUTIL_H_
