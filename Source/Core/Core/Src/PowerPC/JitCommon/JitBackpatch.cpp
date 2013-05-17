// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common.h"
#include "disasm.h"
#include "../JitCommon/JitBase.h"
#include "../JitCommon/JitBackpatch.h"

#include "../../HW/Memmap.h"

#include "x64Emitter.h"
#include "x64ABI.h"
#include "Thunk.h"
#include "x64Analyzer.h"

#include "StringUtil.h"
#ifdef _WIN32
	#include <windows.h>
#endif


using namespace Gen;

extern u8 *trampolineCodePtr;

#ifdef _M_X64
static void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress) {
	u64 code_addr = (u64)codePtr;
	disassembler disasm;
	char disbuf[256];
	memset(disbuf, 0, 256);
#ifdef _M_IX86
	disasm.disasm32(0, code_addr, codePtr, disbuf);
#else
	disasm.disasm64(0, code_addr, codePtr, disbuf);
#endif
	PanicAlert("%s\n\n"
		"Error encountered accessing emulated address %08x.\n"
		"Culprit instruction: \n%s\nat %#llx",
		text.c_str(), emAddress, disbuf, code_addr);
	return;
}
#endif

void TrampolineCache::Init()
{
	AllocCodeSpace(1024 * 1024);
}

void TrampolineCache::Shutdown()
{
	FreeCodeSpace();
}

// Extremely simplistic - just generate the requested trampoline. May reuse them in the future.
const u8 *TrampolineCache::GetReadTrampoline(const InstructionInfo &info)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8 *trampoline = GetCodePtr();
#ifdef _M_X64
	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;

	// It's a read. Easy.
	ABI_PushAllCallerSavedRegsAndAdjustStack();
	if (addrReg != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R((X64Reg)addrReg));
	if (info.displacement) {
		ADD(32, R(ABI_PARAM1), Imm32(info.displacement));
	}
	switch (info.operandSize)
	{
	case 4:
		CALL(thunks.ProtectFunction((void *)&Memory::Read_U32, 1));
		break;
	case 2:
		CALL(thunks.ProtectFunction((void *)&Memory::Read_U16, 1));
		SHL(32, R(EAX), Imm8(16));
		break;
	case 1:
		CALL(thunks.ProtectFunction((void *)&Memory::Read_U8, 1));
		break;
	}

	ABI_PopAllCallerSavedRegsAndAdjustStack();

	if (dataReg != EAX)
	{
		MOV(32, R(dataReg), R(EAX));
	}

	RET();
#endif
	return trampoline;
}

// Extremely simplistic - just generate the requested trampoline. May reuse them in the future.
const u8 *TrampolineCache::GetWriteTrampoline(const InstructionInfo &info)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8 *trampoline = GetCodePtr();

#ifdef _M_X64
	X64Reg dataReg = (X64Reg)info.regOperandReg;
	if (dataReg != EAX)
		PanicAlert("Backpatch write - not through EAX");

	X64Reg addrReg = (X64Reg)info.scaledReg;

	// It's a write. Yay. Remember that we don't have to be super efficient since it's "just" a 
	// hardware access - we can take shortcuts.
	//if (emAddress == 0xCC008000)
	//	PanicAlert("Caught a FIFO write");
	CMP(32, R(addrReg), Imm32(0xCC008000));
	FixupBranch skip_fast = J_CC(CC_NE, false);
	MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
	CALL((void*)jit->GetAsmRoutines()->fifoDirectWrite32);
	RET();
	SetJumpTarget(skip_fast);
	ABI_PushAllCallerSavedRegsAndAdjustStack();

	if (addrReg != ABI_PARAM1)
	{
		MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
		MOV(32, R(ABI_PARAM2), R((X64Reg)addrReg));
	}
	else
	{
		MOV(32, R(ABI_PARAM2), R((X64Reg)addrReg));
		MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
	}

	if (info.displacement)
	{
		ADD(32, R(ABI_PARAM2), Imm32(info.displacement));
	}

	switch (info.operandSize)
	{
	case 4:
		CALL(thunks.ProtectFunction((void *)&Memory::Write_U32, 2));
		break;
	}
	ABI_PopAllCallerSavedRegsAndAdjustStack();
	RET();
#endif

	return trampoline;
}


// This generates some fairly heavy trampolines, but:
// 1) It's really necessary. We don't know anything about the context.
// 2) It doesn't really hurt. Only instructions that access I/O will get these, and there won't be 
//    that many of them in a typical program/game.
const u8 *Jitx86Base::BackPatch(u8 *codePtr, int accessType, u32 emAddress, void *ctx_void)
{
#ifdef _M_X64
	CONTEXT *ctx = (CONTEXT *)ctx_void;

	if (!jit->IsInCodeSpace(codePtr))
		return 0;  // this will become a regular crash real soon after this
	
	InstructionInfo info;
	if (!DisassembleMov(codePtr, info, accessType)) {
		BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
	}

	/*
	if (info.isMemoryWrite) {
		if (!Memory::IsRAMAddress(emAddress, true)) {
			PanicAlert("Exception: Caught write to invalid address %08x", emAddress);
			return;
		}
		BackPatchError("BackPatch - determined that MOV is write, not yet supported and should have been caught before",
					   codePtr, emAddress);
	}*/

	if (info.otherReg != RBX)
		PanicAlert("BackPatch : Base reg not RBX."
		           "\n\nAttempted to access %08x.", emAddress);
	
	if (accessType == OP_ACCESS_WRITE)
		PanicAlert("BackPatch : Currently only supporting reads."
		           "\n\nAttempted to write to %08x.", emAddress);

	if (accessType == 0)
	{
		XEmitter emitter(codePtr);
		int bswapNopCount;
		// Check the following BSWAP for REX byte
		if ((codePtr[info.instructionSize] & 0xF0) == 0x40)
			bswapNopCount = 3;
		else
			bswapNopCount = 2;
		const u8 *trampoline = trampolines.GetReadTrampoline(info);
		emitter.CALL((void *)trampoline);
		emitter.NOP((int)info.instructionSize + bswapNopCount - 5);
		return codePtr;
	}
	else if (accessType == 1)
	{
		// TODO: special case FIFO writes. Also, support 32-bit mode.
		// Also, debug this so that it actually works correctly :P
		XEmitter emitter(codePtr - 2);
		// We know it's EAX so the BSWAP before will be two byte. Overwrite it.
		const u8 *trampoline = trampolines.GetWriteTrampoline(info);
		emitter.CALL((void *)trampoline);
		emitter.NOP((int)info.instructionSize - 3);
		if (info.instructionSize < 3)
			PanicAlert("Instruction too small");
		// We entered here with a BSWAP-ed EAX. We'll have to swap it back.
		ctx->Rax = Common::swap32((u32)ctx->Rax);
		return codePtr - 2;
	}
	return 0;
#else
	return 0;
#endif
}

