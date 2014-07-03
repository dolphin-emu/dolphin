// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>

#include "disasm.h"

#include "Common/Common.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/JitCommon/JitBackpatch.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#ifdef _WIN32
	#include <windows.h>
#endif


using namespace Gen;

extern u8 *trampolineCodePtr;

#if _M_X86_64
static void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress) {
	u64 code_addr = (u64)codePtr;
	disassembler disasm;
	char disbuf[256];
	memset(disbuf, 0, 256);
#if _M_X86_32
	disasm.disasm32(0, code_addr, codePtr, disbuf);
#else
	disasm.disasm64(0, code_addr, codePtr, disbuf);
#endif
	PanicAlert("%s\n\n"
		"Error encountered accessing emulated address %08x.\n"
		"Culprit instruction: \n%s\nat %#" PRIx64,
		text.c_str(), emAddress, disbuf, code_addr);
	return;
}
#endif

void TrampolineCache::Init()
{
	AllocCodeSpace(4 * 1024 * 1024);
}

void TrampolineCache::Shutdown()
{
	FreeCodeSpace();
}

// Extremely simplistic - just generate the requested trampoline. May reuse them in the future.
const u8 *TrampolineCache::GetReadTrampoline(const InstructionInfo &info, u32 registersInUse)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8 *trampoline = GetCodePtr();
#if _M_X86_64
	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;

	// It's a read. Easy.
	// It ought to be necessary to align the stack here.  Since it seems to not
	// affect anybody, I'm not going to add it just to be completely safe about
	// performance.

	if (addrReg != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R((X64Reg)addrReg));
	if (info.displacement) {
		ADD(32, R(ABI_PARAM1), Imm32(info.displacement));
	}
	ABI_PushRegistersAndAdjustStack(registersInUse, true);
	switch (info.operandSize)
	{
	case 4:
		CALL((void *)&Memory::Read_U32);
		break;
	case 2:
		CALL((void *)&Memory::Read_U16);
		SHL(32, R(EAX), Imm8(16));
		break;
	case 1:
		CALL((void *)&Memory::Read_U8);
		break;
	}

	if (info.signExtend && info.operandSize == 1)
	{
		// Need to sign extend value from Read_U8.
		MOVSX(32, 8, dataReg, R(EAX));
	}
	else if (dataReg != EAX)
	{
		MOV(32, R(dataReg), R(EAX));
	}

	ABI_PopRegistersAndAdjustStack(registersInUse, true);
	RET();
#endif
	return trampoline;
}

// Extremely simplistic - just generate the requested trampoline. May reuse them in the future.
const u8 *TrampolineCache::GetWriteTrampoline(const InstructionInfo &info, u32 registersInUse)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8 *trampoline = GetCodePtr();

#if _M_X86_64
	X64Reg dataReg = (X64Reg)info.regOperandReg;
	X64Reg addrReg = (X64Reg)info.scaledReg;

	// It's a write. Yay. Remember that we don't have to be super efficient since it's "just" a
	// hardware access - we can take shortcuts.
	// Don't treat FIFO writes specially for now because they require a burst
	// check anyway.

	if (dataReg == ABI_PARAM2)
		PanicAlert("Incorrect use of SafeWriteRegToReg");
	if (addrReg != ABI_PARAM1)
	{
		if (ABI_PARAM1 != dataReg)
			MOV(64, R(ABI_PARAM1), R((X64Reg)dataReg));
		if (ABI_PARAM2 != addrReg)
			MOV(64, R(ABI_PARAM2), R((X64Reg)addrReg));
	}
	else
	{
		if (ABI_PARAM2 != addrReg)
			MOV(64, R(ABI_PARAM2), R((X64Reg)addrReg));
		if (ABI_PARAM1 != dataReg)
			MOV(64, R(ABI_PARAM1), R((X64Reg)dataReg));
	}

	if (info.displacement)
	{
		ADD(32, R(ABI_PARAM2), Imm32(info.displacement));
	}

	ABI_PushRegistersAndAdjustStack(registersInUse, true);
	switch (info.operandSize)
	{
	case 8:
		CALL((void *)&Memory::Write_U64);
		break;
	case 4:
		CALL((void *)&Memory::Write_U32);
		break;
	case 2:
		CALL((void *)&Memory::Write_U16);
		break;
	case 1:
		CALL((void *)&Memory::Write_U8);
		break;
	}

	ABI_PopRegistersAndAdjustStack(registersInUse, true);
	RET();
#endif

	return trampoline;
}


// This generates some fairly heavy trampolines, but:
// 1) It's really necessary. We don't know anything about the context.
// 2) It doesn't really hurt. Only instructions that access I/O will get these, and there won't be
//    that many of them in a typical program/game.
const u8 *Jitx86Base::BackPatch(u8 *codePtr, u32 emAddress, void *ctx_void)
{
#if _M_X86_64
	SContext *ctx = (SContext *)ctx_void;

	if (!jit->IsInCodeSpace(codePtr))
		return nullptr;  // this will become a regular crash real soon after this

	InstructionInfo info = {};

	if (!DisassembleMov(codePtr, &info)) {
		BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
		return nullptr;
	}

	if (info.otherReg != RBX)
	{
		PanicAlert("BackPatch : Base reg not RBX."
		           "\n\nAttempted to access %08x.", emAddress);
		return nullptr;
	}

	if (info.byteSwap && info.instructionSize < BACKPATCH_SIZE)
	{
		PanicAlert("BackPatch: MOVBE is too small");
		return nullptr;
	}

	auto it = registersInUseAtLoc.find(codePtr);
	if (it == registersInUseAtLoc.end())
	{
		PanicAlert("BackPatch: no register use entry for address %p", codePtr);
		return nullptr;
	}

	u32 registersInUse = it->second;

	if (!info.isMemoryWrite)
	{
		XEmitter emitter(codePtr);
		int bswapNopCount;
		if (info.byteSwap || info.operandSize == 1)
			bswapNopCount = 0;
		// Check the following BSWAP for REX byte
		else if ((codePtr[info.instructionSize] & 0xF0) == 0x40)
			bswapNopCount = 3;
		else
			bswapNopCount = 2;

		const u8 *trampoline = trampolines.GetReadTrampoline(info, registersInUse);
		emitter.CALL((void *)trampoline);
		int padding = info.instructionSize + bswapNopCount - BACKPATCH_SIZE;
		if (padding > 0)
		{
			emitter.NOP(padding);
		}
		return codePtr;
	}
	else
	{
		// TODO: special case FIFO writes. Also, support 32-bit mode.

		u8 *start;
		if (info.byteSwap)
		{
			// The instruction is a MOVBE but it failed so the value is still in little-endian byte order.
			start = codePtr;
		}
		else
		{
			// We entered here with a BSWAP-ed register. We'll have to swap it back.
			u64 *ptr = ContextRN(ctx, info.regOperandReg);
			int bswapSize = 0;
			switch (info.operandSize)
			{
			case 1:
				bswapSize = 0;
				break;
			case 2:
				bswapSize = 4 + (info.regOperandReg >= 8 ? 1 : 0);
				*ptr = Common::swap16((u16) *ptr);
				break;
			case 4:
				bswapSize = 2 + (info.regOperandReg >= 8 ? 1 : 0);
				*ptr = Common::swap32((u32) *ptr);
				break;
			case 8:
				bswapSize = 3;
				*ptr = Common::swap64(*ptr);
				break;
			}
			start = codePtr - bswapSize;
		}
		XEmitter emitter(start);
		const u8 *trampoline = trampolines.GetWriteTrampoline(info, registersInUse);
		emitter.CALL((void *)trampoline);
		int padding = codePtr + info.instructionSize - emitter.GetCodePtr();
		if (padding > 0)
		{
			emitter.NOP(padding);
		}
		return start;
	}
#else
	return 0;
#endif
}
