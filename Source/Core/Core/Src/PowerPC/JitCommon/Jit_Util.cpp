// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "Thunk.h"

#include "CPUDetect.h"
#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "x64ABI.h"
#include "JitBase.h"
#include "Jit_Util.h"

using namespace Gen;

static const u8 GC_ALIGNED16(pbswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static u32 GC_ALIGNED16(float_buffer);

void EmuCodeBlock::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
{
#ifdef _M_X64
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
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
	}
	else if (signExtend)
	{
		// TODO: bake 8-bit into the original load.
		MOVSX(32, accessSize, reg_value, R(reg_value));
	}
}

void EmuCodeBlock::UnsafeLoadRegToRegNoSwap(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset)
{
#ifdef _M_X64
	MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#endif
}

void EmuCodeBlock::UnsafeLoadToEAX(const Gen::OpArg & opAddress, int accessSize, s32 offset, bool signExtend)
{
#ifdef _M_X64
	if (opAddress.IsSimpleReg())
	{
		MOVZX(32, accessSize, EAX, MComplex(RBX, opAddress.GetSimpleReg(), SCALE_1, offset));
	}
	else
	{
		MOV(32, R(EAX), opAddress);
		MOVZX(32, accessSize, EAX, MComplex(RBX, EAX, SCALE_1, offset));
	}
#else
	if (opAddress.IsImm())
	{
		MOVZX(32, accessSize, EAX, M(Memory::base + (((u32)opAddress.offset + offset) & Memory::MEMVIEW32_MASK)));
	}
	else
	{
		if (!opAddress.IsSimpleReg(EAX))
			MOV(32, R(EAX), opAddress);
		AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
		MOVZX(32, accessSize, EAX, MDisp(EAX, (u32)Memory::base + offset));
	}
#endif

	// Add a 2 bytes NOP to have some space for the backpatching
	if (accessSize == 8)
		NOP(2);

	if (accessSize == 32)
	{
		BSWAP(32, EAX);
	}
	else if (accessSize == 16)
	{
		BSWAP(32, EAX);
		if (signExtend)
			SAR(32, R(EAX), Imm8(16));
		else
			SHR(32, R(EAX), Imm8(16));
	}
	else if (signExtend)
	{
		// TODO: bake 8-bit into the original load.
		MOVSX(32, accessSize, EAX, R(EAX));
	}
}

void EmuCodeBlock::SafeLoadToEAX(const Gen::OpArg & opAddress, int accessSize, s32 offset, bool signExtend)
{
#if defined(_M_X64)
#ifdef ENABLE_MEM_CHECK
	if (!Core::g_CoreStartupParameter.bMMU && !Core::g_CoreStartupParameter.bEnableDebugging && Core::g_CoreStartupParameter.bFastmem)
#else
	if (!Core::g_CoreStartupParameter.bMMU && Core::g_CoreStartupParameter.bFastmem)
#endif
	{
		UnsafeLoadToEAX(opAddress, accessSize, offset, signExtend);
	}
	else
#endif
	{
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;
		if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.iTLBHack)
		{
			mem_mask |= Memory::ADDR_MASK_MEM1;
		}

#ifdef ENABLE_MEM_CHECK
		if (Core::g_CoreStartupParameter.bEnableDebugging)
		{
			mem_mask |= Memory::EXRAM_MASK;
		}
#endif

		if (opAddress.IsImm())
		{
			u32 address = (u32)opAddress.offset + offset;
			if ((address & mem_mask) == 0)
			{
				UnsafeLoadToEAX(opAddress, accessSize, offset, signExtend);
			}
			else
			{
				switch (accessSize)
				{
				case 32: ABI_CallFunctionC(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), address); break;
				case 16: ABI_CallFunctionC(thunks.ProtectFunction((void *)&Memory::Read_U16_ZX, 1), address); break;
				case 8:  ABI_CallFunctionC(thunks.ProtectFunction((void *)&Memory::Read_U8_ZX, 1), address); break;
				}
				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, EAX, R(EAX));
				}
			}
		}
		else
		{
			if (offset)
			{
				MOV(32, R(EAX), opAddress);
				ADD(32, R(EAX), Imm32(offset));
				TEST(32, R(EAX), Imm32(mem_mask));
				FixupBranch fast = J_CC(CC_Z);

				switch (accessSize)
				{
				case 32: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), EAX); break;
				case 16: ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16_ZX, 1), EAX); break;
				case 8:  ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8_ZX, 1), EAX);  break;
				}
				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, EAX, R(EAX));
				}

				FixupBranch exit = J();
				SetJumpTarget(fast);
				UnsafeLoadToEAX(R(EAX), accessSize, 0, signExtend);
				SetJumpTarget(exit);
			}
			else
			{
				TEST(32, opAddress, Imm32(mem_mask));
				FixupBranch fast = J_CC(CC_Z);

				switch (accessSize)
				{
				case 32: ABI_CallFunctionA(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), opAddress); break;
				case 16: ABI_CallFunctionA(thunks.ProtectFunction((void *)&Memory::Read_U16_ZX, 1), opAddress); break;
				case 8:  ABI_CallFunctionA(thunks.ProtectFunction((void *)&Memory::Read_U8_ZX, 1), opAddress);  break;
				}
				if (signExtend && accessSize < 32)
				{
					// Need to sign extend values coming from the Read_U* functions.
					MOVSX(32, accessSize, EAX, R(EAX));
				}

				FixupBranch exit = J();
				SetJumpTarget(fast);
				UnsafeLoadToEAX(opAddress, accessSize, offset, signExtend);
				SetJumpTarget(exit);
			}
		}
	}
}

void EmuCodeBlock::UnsafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap)
{
	if (accessSize == 8 && reg_value >= 4) {
		PanicAlert("WARNING: likely incorrect use of UnsafeWriteRegToReg!");
	}
	if (swap) BSWAP(accessSize, reg_value);
#ifdef _M_X64
	MOV(accessSize, MComplex(RBX, reg_addr, SCALE_1, offset), R(reg_value));
#else
	AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
	MOV(accessSize, MDisp(reg_addr, (u32)Memory::base + offset), R(reg_value));
#endif
}

// Destroys both arg registers
void EmuCodeBlock::SafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset, bool swap)
{
	if (offset)
		ADD(32, R(reg_addr), Imm32((u32)offset));

	u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

	if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.iTLBHack)
	{
		mem_mask |= Memory::ADDR_MASK_MEM1;
	}

#ifdef ENABLE_MEM_CHECK
	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		mem_mask |= Memory::EXRAM_MASK;
	}
#endif

	TEST(32, R(reg_addr), Imm32(mem_mask));
	FixupBranch fast = J_CC(CC_Z);
	MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
	switch (accessSize)
	{
	case 32: ABI_CallFunctionRR(thunks.ProtectFunction(swap ? ((void *)&Memory::Write_U32) : ((void *)&Memory::Write_U32_Swap), 2), reg_value, reg_addr); break;
	case 16: ABI_CallFunctionRR(thunks.ProtectFunction(swap ? ((void *)&Memory::Write_U16) : ((void *)&Memory::Write_U16_Swap), 2), reg_value, reg_addr); break;
	case 8:  ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U8, 2), reg_value, reg_addr);  break;
	}
	FixupBranch exit = J();
	SetJumpTarget(fast);
	UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
	SetJumpTarget(exit);
}

void EmuCodeBlock::SafeWriteFloatToReg(X64Reg xmm_value, X64Reg reg_addr)
{
	if (false && cpu_info.bSSSE3) {
		// This path should be faster but for some reason it causes errors so I've disabled it.
		u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

		if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.iTLBHack)
			mem_mask |= Memory::ADDR_MASK_MEM1;

#ifdef ENABLE_MEM_CHECK
		if (Core::g_CoreStartupParameter.bEnableDebugging)
			mem_mask |= Memory::EXRAM_MASK;
#endif
		TEST(32, R(reg_addr), Imm32(mem_mask));
		FixupBranch argh = J_CC(CC_Z);
		MOVSS(M(&float_buffer), xmm_value);
		MOV(32, R(EAX), M(&float_buffer));
		BSWAP(32, EAX);
		MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
		ABI_CallFunctionRR(thunks.ProtectFunction(((void *)&Memory::Write_U32), 2), EAX, reg_addr);
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		PSHUFB(xmm_value, M((void *)pbswapShuffle1x4));
#ifdef _M_X64
		MOVD_xmm(MComplex(RBX, reg_addr, SCALE_1, 0), xmm_value);
#else
		AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(MDisp(reg_addr, (u32)Memory::base), xmm_value);
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

void EmuCodeBlock::JitClearCA()
{
	AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}

void EmuCodeBlock::JitSetCA()
{
	OR(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(XER_CA_MASK)); //XER.CA = 1
}

void EmuCodeBlock::JitClearCAOV(bool oe)
{
	if (oe)
		AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK & ~XER_OV_MASK)); //XER.CA, XER.OV = 0
	else
		AND(32, M(&PowerPC::ppcState.spr[SPR_XER]), Imm32(~XER_CA_MASK)); //XER.CA = 0
}
