// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common.h"

namespace MMIO { class Mapping; }
class PointerWrap;

namespace MemoryInterface
{
void DoState(PointerWrap &p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void Read16(u16& _uReturnValue, const u32 _iAddress);
void Read32(u32& _uReturnValue, const u32 _iAddress);
void Write32(const u32 _iValue, const u32 _iAddress);
void Write16(const u16 _iValue, const u32 _iAddress);
} // end of namespace MemoryInterface
