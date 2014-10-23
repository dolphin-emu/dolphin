// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

bool DumpDSPCode(const u8 *code_be, int size_in_bytes, u32 crc);
bool DumpCWCode(u32 _Address, u32 _Length);
