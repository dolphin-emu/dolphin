// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included./

#ifndef _JITUTIL_H
#define _JITUTIL_H

#include "x64Emitter.h"
#include <unordered_map>

#define MEMCHECK_START \
	FixupBranch memException; \
	if (jit->js.memcheck) \
	{ TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_DSI)); \
	memException = J_CC(CC_NZ, true); }

#define MEMCHECK_END \
	if (jit->js.memcheck) \
	SetJumpTarget(memException);


// Like XCodeBlock but has some utilities for memory access.
class EmuCodeBlock : public Gen::XCodeBlock
{
public:
	void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
	void UnsafeLoadRegToRegNoSwap(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset);
	// these return the address of the MOV, for backpatching
	u8 *UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0, bool swap = true);
	u8 *UnsafeLoadToReg(Gen::X64Reg reg_value, Gen::OpArg opAddress, int accessSize, s32 offset, bool signExtend);
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

	void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void JitClearCA();
	void JitSetCA();
	void JitClearCAOV(bool oe);

	void ForceSinglePrecisionS(Gen::X64Reg xmm);
	void ForceSinglePrecisionP(Gen::X64Reg xmm);
protected:
	std::unordered_map<u8 *, u32> registersInUseAtLoc;
};

#endif  // _JITUTIL_H
