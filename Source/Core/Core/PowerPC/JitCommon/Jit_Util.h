// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included./

#pragma once

#include <unordered_map>

#include "Common/x64Emitter.h"

namespace MMIO { class Mapping; }

#define MEMCHECK_START \
	FixupBranch memException; \
	if (jit->js.memcheck) \
	{ TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_DSI)); \
	memException = J_CC(CC_NZ, true); }

#define MEMCHECK_END \
	if (jit->js.memcheck) \
	SetJumpTarget(memException);


// Like XCodeBlock but has some utilities for memory access.
class EmuCodeBlock : public Gen::X64CodeBlock
{
public:
	void LoadAndSwap(int size, Gen::X64Reg dst, const Gen::OpArg& src);
	void SwapAndStore(int size, const Gen::OpArg& dst, Gen::X64Reg src);

	void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
	void UnsafeLoadRegToRegNoSwap(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset);
	// these return the address of the MOV, for backpatching
	u8 *UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0, bool swap = true);
	u8 *UnsafeLoadToReg(Gen::X64Reg reg_value, Gen::OpArg opAddress, int accessSize, s32 offset, bool signExtend);

	// Generate a load/write from the MMIO handler for a given address. Only
	// call for known addresses in MMIO range (MMIO::IsMMIOAddress).
	void MMIOLoadToReg(MMIO::Mapping* mmio, Gen::X64Reg reg_value, u32 registers_in_use, u32 address, int access_size, bool sign_extend);

	enum SafeLoadStoreFlags
	{
		SAFE_LOADSTORE_NO_SWAP = 1,
		SAFE_LOADSTORE_NO_PROLOG = 2,
		SAFE_LOADSTORE_NO_FASTMEM = 4
	};
	void SafeLoadToReg(Gen::X64Reg reg_value, const Gen::OpArg & opAddress, int accessSize, s32 offset, u32 registersInUse, bool signExtend, int flags = 0);
	void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset, u32 registersInUse, int flags = 0);

	// Trashes both inputs and EAX.
	void SafeWriteFloatToReg(Gen::X64Reg xmm_value, Gen::X64Reg reg_addr, u32 registersInUse, int flags = 0);

	void WriteToConstRamAddress(int accessSize, Gen::X64Reg arg, u32 address, bool swap = false);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void JitClearCA();
	void JitSetCA();
	void JitClearCAOV(bool oe);

	void ForceSinglePrecisionS(Gen::X64Reg xmm);
	void ForceSinglePrecisionP(Gen::X64Reg xmm);

	// AX might get trashed
	void ConvertSingleToDouble(Gen::X64Reg dst, Gen::X64Reg src, bool src_is_gpr = false);
	void ConvertDoubleToSingle(Gen::X64Reg dst, Gen::X64Reg src);
protected:
	std::unordered_map<u8 *, u32> registersInUseAtLoc;
};
