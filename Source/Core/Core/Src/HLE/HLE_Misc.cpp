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

#include <cmath>
#include "Common.h"
#include "HLE_OS.h"

#include "../PowerPC/PowerPC.h"
#include "../HW/Memmap.h"

namespace HLE_Misc
{

inline float F(u32 addr) 
{
	u32 mem = Memory::ReadFast32(addr);
	return *((float*)&mem);
}

inline void FW(u32 addr, float x) 
{
	u32 data = *((u32*)&x);
	Memory::WriteUnchecked_U32(data, addr);
}

void UnimplementedFunction()
{
    NPC = LR;
}

void UnimplementedFunctionTrue()
{
    GPR(3) = 1;
    NPC = LR;
}

void UnimplementedFunctionFalse()
{
    GPR(3) = 0;
    NPC = LR;
}

void GXPeekZ()
{
    Memory::Write_U32(0xFFFFFF, GPR(5));
    NPC = LR;
}

void GXPeekARGB()
{
    Memory::Write_U32(0xFFFFFFFF, GPR(5));
    NPC = LR;
}

void HLEPanicAlert()
{
	::PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

// .evil_vec_cosine
void SMB_EvilVecCosine()
{
	u32 r3 = GPR(3);
	u32 r4 = GPR(4);

	float x1 = F(r3);
	float y1 = F(r3 + 4);
	float z1 = F(r3 + 8);

	float x2 = F(r4);
	float y2 = F(r4 + 4);
	float z2 = F(r4 + 8);

	float s1 = x1*x1 + y1*y1 + z1*z1;
	float s2 = x2*x2 + y2*y2 + z2*z2;
	
	float dot = x1*x2 + y1*y2 + z1*z2;

	rPS0(1) = dot / sqrtf(s1 * s2);
    NPC = LR;
}

void SMB_EvilNormalize()
{
	u32 r3 = GPR(3);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float inv_len = 1.0f / sqrtf(x*x + y*y + z*z);
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r3, x);
	FW(r3 + 4, y);
	FW(r3 + 8, z);
	NPC = LR;
}

void SMB_evil_vec_setlength()
{
	u32 r3 = GPR(3);
	u32 r4 = GPR(4);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float inv_len = (float)(rPS0(1) / sqrt(x*x + y*y + z*z));
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r4, x);
	FW(r4 + 4, y);
	FW(r4 + 8, z);
	NPC = LR;
}


void SMB_sqrt_internal()
{
	double f = sqrt(rPS0(1));
	rPS0(0) = rPS0(1);
	rPS1(0) = rPS0(1);
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

void SMB_rsqrt_internal()
{
	double f = 1.0 / sqrt(rPS0(1));
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

void SMB_atan2()
{
	// in: f1 = x, f2 = y
	// out: r3 = angle
	double angle = atan2(rPS0(1), rPS0(2));
	int angle_fixpt = (int)(angle / 3.14159 * 32767);
	if (angle_fixpt < -32767) angle_fixpt = -32767;
	if (angle_fixpt > 32767) angle_fixpt = 32767;
	GPR(3) = angle_fixpt;
	NPC = LR;
}
}
