// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


#include "x64Emitter.h"

namespace Jit64
{

// Memory Load/Store
void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0);
void SafeLoadRegToEAX(Gen::X64Reg reg, int accessSize, s32 offset, bool signExtend = false);
void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset);

void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);

}  // namespace