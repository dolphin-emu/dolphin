// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included./

#pragma once

#include <unordered_map>

#include "Common/BitSet.h"
#include "Common/CPUDetect.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/PowerPC.h"

namespace MMIO { class Mapping; }

// We offset by 0x80 because the range of one byte memory offsets is
// -0x80..0x7f.
#define PPCSTATE(x) MDisp(RPPCSTATE, \
	(int) ((char *) &PowerPC::ppcState.x - (char *) &PowerPC::ppcState) - 0x80)
// In case you want to disable the ppcstate register:
// #define PPCSTATE(x) M(&PowerPC::ppcState.x)
#define PPCSTATE_LR PPCSTATE(spr[SPR_LR])
#define PPCSTATE_CTR PPCSTATE(spr[SPR_CTR])
#define PPCSTATE_SRR0 PPCSTATE(spr[SPR_SRR0])
#define PPCSTATE_SRR1 PPCSTATE(spr[SPR_SRR1])

// A place to throw blocks of code we don't want polluting the cache, e.g. rarely taken
// exception branches.
class FarCodeCache : public Gen::X64CodeBlock
{
private:
	bool m_enabled = false;
public:
	bool Enabled() const { return m_enabled; }
	void Init(int size) { AllocCodeSpace(size); m_enabled = true; }
	void Shutdown() { FreeCodeSpace(); m_enabled = false; }
};

static const int CODE_SIZE = 1024 * 1024 * 32;

// a bit of a hack; the MMU results in a vast amount more code ending up in the far cache,
// mostly exception handling, so give it a whole bunch more space if the MMU is on.
static const int FARCODE_SIZE = 1024 * 1024 * 8;
static const int FARCODE_SIZE_MMU = 1024 * 1024 * 48;

// same for the trampoline code cache, because fastmem results in far more backpatches in MMU mode
static const int TRAMPOLINE_CODE_SIZE = 1024 * 1024 * 8;
static const int TRAMPOLINE_CODE_SIZE_MMU = 1024 * 1024 * 32;

// Stores information we need to batch-patch a MOV with a call to the slow read/write path after
// it faults
struct TrampolineInfo final
{
	// The PPC PC for the current load/store block
	u32 pc;

	// true if this is a read op vs a write
	bool read;

	// for read operations, true if needs sign-extension after load
	bool signExtend;

	// Saved because we need these to make the ABI call in the trampoline
	BitSet32 registersInUse;

	// Memory access size (in bits)
	int accessSize;

	// Original SafeLoadXXX/SafeStoreXXX flags
	int flags;

	// The start of the store operation that failed -- we will patch a JMP here
	u8* start;

	// The end of the store operation (points to the next instruction)
	const u8* end;

	// The MOV operation
	Gen::MovInfo mov;

	// Set to true if we added the offset to the address and need to undo it
	bool offsetAddedToAddress;

	// src/dest for load/store
	s32 offset;
	Gen::X64Reg op_reg;
	Gen::OpArg op_arg;
};

// Like XCodeBlock but has some utilities for memory access.
class EmuCodeBlock : public Gen::X64CodeBlock
{
public:
	FarCodeCache farcode;
	u8* nearcode; // Backed up when we switch to far code.

	void MemoryExceptionCheck();

	// Simple functions to switch between near and far code emitting
	void SwitchToFarCode()
	{
		nearcode = GetWritableCodePtr();
		SetCodePtr(farcode.GetWritableCodePtr());
	}

	void SwitchToNearCode()
	{
		farcode.SetCodePtr(GetWritableCodePtr());
		SetCodePtr(nearcode);
	}

	Gen::FixupBranch CheckIfSafeAddress(const Gen::OpArg& reg_value, Gen::X64Reg reg_addr, BitSet32 registers_in_use, u32 mem_mask);
	void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
	void UnsafeLoadRegToRegNoSwap(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset, bool signExtend = false);
	// these return the address of the MOV, for backpatching
	void UnsafeWriteRegToReg(Gen::OpArg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0, bool swap = true, Gen::MovInfo* info = nullptr);
	void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0, bool swap = true, Gen::MovInfo* info = nullptr)
	{
		UnsafeWriteRegToReg(R(reg_value), reg_addr, accessSize, offset, swap, info);
	}
	bool UnsafeLoadToReg(Gen::X64Reg reg_value, Gen::OpArg opAddress, int accessSize, s32 offset, bool signExtend, Gen::MovInfo* info = nullptr);
	void UnsafeWriteGatherPipe(int accessSize);

	// Generate a load/write from the MMIO handler for a given address. Only
	// call for known addresses in MMIO range (MMIO::IsMMIOAddress).
	void MMIOLoadToReg(MMIO::Mapping* mmio, Gen::X64Reg reg_value, BitSet32 registers_in_use, u32 address, int access_size, bool sign_extend);

	enum SafeLoadStoreFlags
	{
		SAFE_LOADSTORE_NO_SWAP = 1,
		SAFE_LOADSTORE_NO_PROLOG = 2,
		// This indicates that the write being generated cannot be patched (and thus can't use fastmem)
		SAFE_LOADSTORE_NO_FASTMEM = 4,
		SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR = 8,
		// Force slowmem (used when generating fallbacks in trampolines)
		SAFE_LOADSTORE_FORCE_SLOWMEM = 16,
	};

	void SafeLoadToReg(Gen::X64Reg reg_value, const Gen::OpArg & opAddress, int accessSize, s32 offset, BitSet32 registersInUse, bool signExtend, int flags = 0);
	void SafeLoadToRegImmediate(Gen::X64Reg reg_value, u32 address, int accessSize, BitSet32 registersInUse, bool signExtend);

	// Clobbers RSCRATCH or reg_addr depending on the relevant flag.  Preserves
	// reg_value if the load fails and js.memcheck is enabled.
	// Works with immediate inputs and simple registers only.
	void SafeWriteRegToReg(Gen::OpArg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset, BitSet32 registersInUse, int flags = 0);
	void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset, BitSet32 registersInUse, int flags = 0)
	{
		SafeWriteRegToReg(R(reg_value), reg_addr, accessSize, offset, registersInUse, flags);
	}

	// applies to safe and unsafe WriteRegToReg
	bool WriteClobbersRegValue(int accessSize, bool swap)
	{
		return swap && !cpu_info.bMOVBE && accessSize > 8;
	}

	void WriteToConstRamAddress(int accessSize, Gen::OpArg arg, u32 address, bool swap = true);
	// returns true if an exception could have been caused
	bool WriteToConstAddress(int accessSize, Gen::OpArg arg, u32 address, BitSet32 registersInUse);
	void JitGetAndClearCAOV(bool oe);
	void JitSetCA();
	void JitSetCAIf(Gen::CCFlags conditionCode);
	void JitClearCA();

	void avx_op(void (Gen::XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, const Gen::OpArg&), void (Gen::XEmitter::*sseOp)(Gen::X64Reg, const Gen::OpArg&),
	            Gen::X64Reg regOp, const Gen::OpArg& arg1, const Gen::OpArg& arg2, bool packed = true, bool reversible = false);
	void avx_op(void (Gen::XEmitter::*avxOp)(Gen::X64Reg, Gen::X64Reg, const Gen::OpArg&, u8), void (Gen::XEmitter::*sseOp)(Gen::X64Reg, const Gen::OpArg&, u8),
	            Gen::X64Reg regOp, const Gen::OpArg& arg1, const Gen::OpArg& arg2, u8 imm);

	void ForceSinglePrecision(Gen::X64Reg output, const Gen::OpArg& input, bool packed = true, bool duplicate = false);
	void Force25BitPrecision(Gen::X64Reg output, const Gen::OpArg& input, Gen::X64Reg tmp);

	// RSCRATCH might get trashed
	void ConvertSingleToDouble(Gen::X64Reg dst, Gen::X64Reg src, bool src_is_gpr = false);
	void ConvertDoubleToSingle(Gen::X64Reg dst, Gen::X64Reg src);
	void SetFPRF(Gen::X64Reg xmm);
	void Clear();
protected:
	std::unordered_map<u8 *, TrampolineInfo> backPatchInfo;
	std::unordered_map<u8 *, u8 *> exceptionHandlerAtLoc;
};
