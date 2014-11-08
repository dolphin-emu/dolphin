// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "disasm.h"

#include "Core/PowerPC/JitCommon/JitBackpatch.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

using namespace Gen;

static void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress)
{
	u64 code_addr = (u64)codePtr;
	disassembler disasm;
	char disbuf[256];
	memset(disbuf, 0, 256);
	disasm.disasm64(0, code_addr, codePtr, disbuf);
	PanicAlert("%s\n\n"
		"Error encountered accessing emulated address %08x.\n"
		"Culprit instruction: \n%s\nat %#" PRIx64,
		text.c_str(), emAddress, disbuf, code_addr);
	return;
}

// This generates some fairly heavy trampolines, but it doesn't really hurt.
// Only instructions that access I/O will get these, and there won't be that
// many of them in a typical program/game.
bool Jitx86Base::HandleFault(uintptr_t access_address, SContext* ctx)
{
	// TODO: do we properly handle off-the-end?
	if (access_address >= (uintptr_t)Memory::base && access_address < (uintptr_t)Memory::base + 0x100010000)
		return BackPatch((u32)(access_address - (uintptr_t)Memory::base), ctx);

	return false;
}

bool Jitx86Base::BackPatch(u32 emAddress, SContext* ctx)
{
	u8* codePtr = (u8*) ctx->CTX_PC;

	if (!IsInSpace(codePtr))
		return false;  // this will become a regular crash real soon after this

	InstructionInfo info = {};

	if (!DisassembleMov(codePtr, &info))
	{
		BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
		return false;
	}

	if (info.otherReg != RMEM)
	{
		PanicAlert("BackPatch : Base reg not RMEM."
		           "\n\nAttempted to access %08x.", emAddress);
		return false;
	}

	if (info.byteSwap && info.instructionSize < BACKPATCH_SIZE)
	{
		PanicAlert("BackPatch: MOVBE is too small");
		return false;
	}

	auto it = registersInUseAtLoc.find(codePtr);
	if (it == registersInUseAtLoc.end())
	{
		PanicAlert("BackPatch: no register use entry for address %p", codePtr);
		return false;
	}

	BitSet32 registersInUse = it->second;

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
		ctx->CTX_PC = (u64)codePtr;
	}
	else
	{
		// TODO: special case FIFO writes. Also, support 32-bit mode.
		auto it2 = pcAtLoc.find(codePtr);
		if (it2 == pcAtLoc.end())
		{
			PanicAlert("BackPatch: no pc entry for address %p", codePtr);
			return nullptr;
		}

		u32 pc = it2->second;

		u8 *start;
		if (info.byteSwap || info.hasImmediate)
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
		const u8 *trampoline = trampolines.GetWriteTrampoline(info, registersInUse, pc);
		emitter.CALL((void *)trampoline);
		ptrdiff_t padding = (codePtr - emitter.GetCodePtr()) + info.instructionSize;
		if (padding > 0)
		{
			emitter.NOP(padding);
		}
		ctx->CTX_PC = (u64)start;
	}

	return true;
}
