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

#include "CPUDetect.h"
#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "JitBase.h"
#include "Jit_Util.h"

using namespace Gen;

void EmuCodeBlock::JitClearCA()
{
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}

void EmuCodeBlock::JitSetCA()
{
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK)); //XER.CA = 1
}

void EmuCodeBlock::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
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

void EmuCodeBlock::UnsafeLoadRegToRegNoSwap(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset)
{
#ifdef _M_IX86
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#else
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#endif
}

void EmuCodeBlock::SafeLoadRegToEAX(X64Reg reg, int accessSize, s32 offset, bool signExtend)
{
	if (offset)
		ADD(32, R(reg), Imm32((u32)offset));
	TEST(32, R(reg), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_Z);
	switch (accessSize)
	{
	case 32: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), reg); break;
	case 16: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16_ZX, 1), reg); break;
	case 8:  ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8_ZX, 1), reg);  break;
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

void EmuCodeBlock::UnsafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap)
{
	if (accessSize == 8 && reg_value >= 4) {
		PanicAlert("WARNING: likely incorrect use of UnsafeWriteRegToReg!");
	}
	if (swap) BSWAP(accessSize, reg_value);
#ifdef _M_IX86
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOV(accessSize, MDisp(reg_addr, (u32)Memory::base + offset), R(reg_value));
#else
	MOV(accessSize, MComplex(RBX, reg_addr, SCALE_1, offset), R(reg_value));
#endif
}

// Destroys both arg registers
void EmuCodeBlock::SafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap)
{
	if (offset)
		ADD(32, R(reg_addr), Imm32(offset));
	TEST(32, R(reg_addr), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_Z);
	switch (accessSize)
	{
	case 32: ABI_CallFunctionRR(thunks.ProtectFunction(swap ? ((void *)&Memory::Write_U32) : ((void *)&Memory::Write_U32_Swap), 2), reg_value, reg_addr); break;
	case 16: ABI_CallFunctionRR(thunks.ProtectFunction(swap ? ((void *)&Memory::Write_U16) : ((void *)&Memory::Write_U16_Swap), 2), reg_value, reg_addr); break;
	case 8:  ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U8, 2), reg_value, reg_addr);  break;
	}
	FixupBranch arg2 = J();
	SetJumpTarget(argh);
	UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
	SetJumpTarget(arg2);
}

static const u8 GC_ALIGNED16(pbswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static u32 GC_ALIGNED16(float_buffer);

void EmuCodeBlock::SafeWriteFloatToReg(X64Reg xmm_value, X64Reg reg_addr)
{
	TEST(32, R(reg_addr), Imm32(0x0C000000));
	if (false && cpu_info.bSSSE3) {
		// This path should be faster but for some reason it causes errors so I've disabled it.
		FixupBranch argh = J_CC(CC_Z);
		MOVSS(M(&float_buffer), xmm_value);
		MOV(32, R(EAX), M(&float_buffer));
		BSWAP(32, EAX);
		ABI_CallFunctionRR(thunks.ProtectFunction(((void *)&Memory::Write_U32), 2), EAX, reg_addr);
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		PSHUFB(xmm_value, M((void *)pbswapShuffle1x4));
	#ifdef _M_IX86
		AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(MDisp(reg_addr, (u32)Memory::base), xmm_value);
	#else
		MOVD_xmm(MComplex(RBX, reg_addr, SCALE_1, 0), xmm_value);
	#endif
		SetJumpTarget(arg2);
	} else {
		MOVSS(M(&float_buffer), xmm_value);
		MOV(32, R(EAX), M(&float_buffer));
		SafeWriteRegToReg(EAX, reg_addr, 32, 0, true);
	}
}

void EmuCodeBlock::WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address)
{
#ifdef _M_X64
 	MOV(accessSize, MDisp(RBX, address & 0x3FFFFFFF), arg);
#else
	MOV(accessSize, M((void*)(Memory::base + (address & Memory::MEMVIEW32_MASK))), arg);
#endif
}

void EmuCodeBlock::WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address)
{
#ifdef _M_X64
	MOV(32, R(RAX), Imm32(address));
	MOVSS(MComplex(RBX, RAX, 1, 0), xmm_reg);
#else
	MOVSS(M((void*)((u32)Memory::base + (address & Memory::MEMVIEW32_MASK))), xmm_reg);
#endif
}

void EmuCodeBlock::ForceSinglePrecisionS(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jit->jo.accurateSinglePrecision)
	{
		CVTSD2SS(xmm, R(xmm));
		CVTSS2SD(xmm, R(xmm));
	}
}

void EmuCodeBlock::ForceSinglePrecisionP(X64Reg xmm) {
	// Most games don't need these. Zelda requires it though - some platforms get stuck without them.
	if (jit->jo.accurateSinglePrecision)
	{
		CVTPD2PS(xmm, R(xmm));
		CVTPS2PD(xmm, R(xmm));
	}
}
