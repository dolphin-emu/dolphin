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

// Helper to quickly read the floating point value at a memory location.
inline float F(u32 addr) 
{
	u32 mem = Memory::ReadFast32(addr);
	return *((float*)&mem);
}

// Helper to quickly write a floating point value to a memory location.
inline void FW(u32 addr, float x) 
{
	u32 data = *((u32*)&x);
	Memory::WriteUnchecked_U32(data, addr);
}

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
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
	// Just some fake Z value.
    Memory::Write_U32(0xFFFFFF, GPR(5));
    NPC = LR;
}

void GXPeekARGB()
{
	// Just some fake color value.
    Memory::Write_U32(0xFFFFFFFF, GPR(5));
    NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
	::PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

// Computes the cosine of the angle between the two fvec3s pointed at by r3 and r4.
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

// Normalizes the vector pointed at by r3.
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

// Scales the vector pointed at by r3 to the length specified by f0.
// Writes results to vector pointed at by r4.
void SMB_evil_vec_setlength()
{
	u32 r3 = GPR(3);
	u32 r4 = GPR(4);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float inv_len = (float)(rPS0(0) / sqrt(x*x + y*y + z*z));
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r4, x);
	FW(r4 + 4, y);
	FW(r4 + 8, z);
	NPC = LR;
}

// Internal square root function in the crazy math lib. Acts a bit odd, just read it. It's not a bug :p
void SMB_sqrt_internal()
{
	double f = sqrt(rPS0(1));
	rPS0(0) = rPS0(1);
	rPS1(0) = rPS0(1);
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

// Internal inverse square root function in the crazy math lib. 
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


// F-zero math lib range: 8006d044 - 8006f770

void FZero_kill_infinites()
{
	// TODO: Kill infinites in FPR(1)

	NPC = LR;
}

void FZ_sqrt() {
	u32 r3 = GPR(3);
	double x = rPS0(0);
	x = sqrt(x);
	FW(r3, (float)x);
	rPS0(0) = x;
	NPC = LR;
}

// Internal square root function in the crazy math lib. Acts a bit odd, just read it. It's not a bug :p
void FZ_sqrt_internal()
{
	double f = sqrt(rPS0(1));
	rPS0(0) = rPS0(1);
	rPS1(0) = rPS0(1);
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

// Internal inverse square root function in the crazy math lib. 
void FZ_rsqrt_internal()
{
	double f = 1.0 / sqrt(rPS0(1));
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

void FZero_evil_vec_normalize()
{
	u32 r3 = GPR(3);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float sq_len = x*x + y*y + z*z;
	float inv_len = 1.0f / sqrtf(sq_len);
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r3, x);
	FW(r3 + 4, y);
	FW(r3 + 8, z);
	rPS0(1) = inv_len * sq_len;  // len
	rPS1(1) = inv_len * sq_len;  // len
	NPC = LR;

	/*
.evil_vec_something

(f6, f7, f8) <- [r3]
f1 = f6 * f6
f1 += f7 * f7
f1 += f8 * f8
f2 = mystery
f4 = f2 * f1
f3 = f2 + f2
f1 = 1/f0

f6 *= f1
f7 *= f1
f8 *= f1

8006d668: lis	r5, 0xE000
8006d684: lfs	f2, 0x01A0 (r5)
8006d69c: fmr	f0,f2
8006d6a0: fmuls	f4,f2,f1
8006d6a4: fadds	f3,f2,f2
8006d6a8: frsqrte	f1,f0,f1
8006d6ac: fadds	f3,f3,f2
8006d6b0: fmuls	f5,f1,f1
8006d6b4: fnmsubs	f5,f5,f4,f3
8006d6b8: fmuls	f1,f1,f5
8006d6bc: fmuls	f5,f1,f1
8006d6c0: fnmsubs	f5,f5,f4,f3
8006d6c4: fmuls	f1,f1,f5
8006d6c8: fmuls	f6,f6,f1
8006d6cc: stfs	f6, 0 (r3)
8006d6d0: fmuls	f7,f7,f1
8006d6d4: stfs	f7, 0x0004 (r3)
8006d6d8: fmuls	f8,f8,f1
8006d6dc: stfs	f8, 0x0008 (r3)
8006d6e0: fmuls	f1,f1,f0
8006d6e4: blr	
*/
	NPC = LR;
}

}
