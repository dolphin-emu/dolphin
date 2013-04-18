// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSPLLE_TOOLS_H
#define _DSPLLE_TOOLS_H

bool DumpDSPCode(const u8 *code_be, int size_in_bytes, u32 crc);
bool DumpCWCode(u32 _Address, u32 _Length);

#endif //_DSPLLE_TOOLS_H
