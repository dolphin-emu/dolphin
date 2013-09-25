// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included./

#ifndef _JITUTIL_H
#define _JITUTIL_H

#include "x64Emitter.h"
#include "Thunk.h"

// Like XCodeBlock but has some utilities for memory access.
class EmuCodeBlock : public Gen::XCodeBlock {
public:
	void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
	void UnsafeLoadRegToRegNoSwap(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset);
	void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0, bool swap = true);
	void UnsafeLoadToEAX(const Gen::OpArg & opAddress, int accessSize, s32 offset, bool signExtend);
	void SafeLoadToEAX(const Gen::OpArg & opAddress, int accessSize, s32 offset, bool signExtend);
	enum SafeWriteFlags
	{
		SAFE_WRITE_NO_SWAP = 1,
		SAFE_WRITE_NO_PROLOG = 2,
		SAFE_WRITE_NO_FASTMEM = 4
	};
	void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset, int flags = 0);

	// Trashes both inputs and EAX.
	void SafeWriteFloatToReg(Gen::X64Reg xmm_value, Gen::X64Reg reg_addr, int flags = 0);

	void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void JitClearCA();
	void JitSetCA();
	void JitClearCAOV(bool oe);

	void ForceSinglePrecisionS(Gen::X64Reg xmm);
	void ForceSinglePrecisionP(Gen::X64Reg xmm);
protected:
	ThunkManager thunks;
};

#endif  // _JITUTIL_H
