// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"
#include "Thunk.h"

#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "ABI.h"

#ifdef JITTEST
#include "../Jit64IL/Jit.h"
#include "JitCache.h"
#include "../Jit64IL/JitAsm.h"
#else
#include "../Jit64/Jit.h"
#include "JitCache.h"
#include "../Jit64/JitAsm.h"
#include "../Jit64/JitRegCache.h"
#endif

using namespace Gen;

void Jit64::JitClearCA()
{
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}

void Jit64::JitSetCA()
{
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK)); //XER.CA = 1
}

void Jit64::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
{
#ifdef _M_IX86
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#else
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#endif
	if (accessSize == 32)
	{
		BSWAP(32, reg_value);
	}
	else if (accessSize == 16)
	{
		BSWAP(32, reg_value);
		if (signExtend)
			SAR(32, R(reg_value), Imm8(16));
		else
			SHR(32, R(reg_value), Imm8(16));
	} else if (signExtend) {
		// TODO: bake 8-bit into the original load.
		MOVSX(32, accessSize, reg_value, R(reg_value));   
	}
}

void Jit64::SafeLoadRegToEAX(X64Reg reg, int accessSize, s32 offset, bool signExtend)
{
	if (offset)
		ADD(32, R(reg), Imm32((u32)offset));
	TEST(32, R(reg), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_Z);
	switch (accessSize)
	{
	case 32: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), reg); break;
	case 16: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16, 1), reg); break;
	case 8:  ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8, 1), reg);  break;
	}
	if (signExtend && accessSize < 32) {
		// Need to sign extend values coming from the Read_U* functions.
		MOVSX(32, accessSize, EAX, R(EAX));
	}
	FixupBranch arg2 = J();
	SetJumpTarget(argh);
	UnsafeLoadRegToReg(reg, EAX, accessSize, 0, signExtend);
	SetJumpTarget(arg2);
}

void Jit64::UnsafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset)
{
	if (accessSize == 8 && reg_value >= 4) {
		PanicAlert("WARNING: likely incorrect use of UnsafeWriteRegToReg!");
	}
	BSWAP(accessSize, reg_value);
#ifdef _M_IX86
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOV(accessSize, MDisp(reg_addr, (u32)Memory::base + offset), R(reg_value));
#else
	MOV(accessSize, MComplex(RBX, reg_addr, SCALE_1, offset), R(reg_value));
#endif
}

// Destroys both arg registers
void Jit64::SafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset)
{
	if (offset)
		ADD(32, R(reg_addr), Imm32(offset));
	TEST(32, R(reg_addr), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_Z);
	switch (accessSize)
	{
	case 32: ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U32, 2), reg_value, reg_addr); break;
	case 16: ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U16, 2), reg_value, reg_addr); break;
	case 8:  ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U8, 2), reg_value, reg_addr);  break;
	}
	FixupBranch arg2 = J();
	SetJumpTarget(argh);
	UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0);
	SetJumpTarget(arg2);
}

void Jit64::WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address)
{
#ifdef _M_X64
 	MOV(accessSize, MDisp(RBX, address & 0x3FFFFFFF), arg);
#else
	MOV(accessSize, M((void*)(Memory::base + (address & Memory::MEMVIEW32_MASK))), arg);
#endif
}

void Jit64::WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address)
{
#ifdef _M_X64
	MOV(32, R(RAX), Imm32(address));
	MOVSS(MComplex(RBX, RAX, 1, 0), xmm_reg);
#else
	MOVSS(M((void*)((u32)Memory::base + (address & Memory::MEMVIEW32_MASK))), xmm_reg);
#endif
}

void Jit64::ForceSinglePrecisionS(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jo.accurateSinglePrecision)
	{
		CVTSD2SS(xmm, R(xmm));
		CVTSS2SD(xmm, R(xmm));
	}
}

void Jit64::ForceSinglePrecisionP(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jo.accurateSinglePrecision)
	{
		CVTPD2PS(xmm, R(xmm));
		CVTPS2PD(xmm, R(xmm));
	}
}
