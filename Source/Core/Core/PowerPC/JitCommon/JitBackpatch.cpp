// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>

#include "disasm.h"

#include "Core/ARBruteForcer.h"
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
		"Culprit instruction: \n%s\nat %#llx",
		text.c_str(), emAddress, disbuf, code_addr);
	return;
}

// This generates some fairly heavy trampolines, but it doesn't really hurt.
// Only instructions that access I/O will get these, and there won't be that
// many of them in a typical program/game.
bool Jitx86Base::HandleFault(uintptr_t access_address, SContext* ctx)
{
	// TODO: do we properly handle off-the-end?
	if (access_address >= (uintptr_t)Memory::physical_base && access_address < (uintptr_t)Memory::physical_base + 0x100010000)
		return BackPatch((u32)(access_address - (uintptr_t)Memory::physical_base), ctx);
	if (access_address >= (uintptr_t)Memory::logical_base && access_address < (uintptr_t)Memory::logical_base + 0x100010000)
		return BackPatch((u32)(access_address - (uintptr_t)Memory::logical_base), ctx);


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
		if (ARBruteForcer::ch_bruteforce)
		{
			Core::KillDolphinAndRestart();
		}
		else
		{
			BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
			return false;
		}
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

	u8* exceptionHandler = nullptr;
	if (jit->jo.memcheck)
	{
		auto it2 = exceptionHandlerAtLoc.find(codePtr);
		if (it2 != exceptionHandlerAtLoc.end())
			exceptionHandler = it2->second;
	}

	// Compute the start and length of the memory operation, including
	// any byteswapping.
	int totalSize = info.instructionSize;
	u8 *start = codePtr;
	if (!info.isMemoryWrite)
	{
		// MOVBE and single bytes don't need to be swapped.
		if (!info.byteSwap && info.operandSize > 1)
		{
			// REX
			if ((codePtr[totalSize] & 0xF0) == 0x40)
				totalSize++;

			// BSWAP
			if (codePtr[totalSize] == 0x0F && (codePtr[totalSize + 1] & 0xF8) == 0xC8)
				totalSize += 2;

			if (info.operandSize == 2)
			{
				// operand size override
				if (codePtr[totalSize] == 0x66)
					totalSize++;
				// REX
				if ((codePtr[totalSize] & 0xF0) == 0x40)
					totalSize++;
				// SAR/ROL
				_assert_(codePtr[totalSize] == 0xC1 && (codePtr[totalSize + 2] == 0x10 ||
				                                        codePtr[totalSize + 2] == 0x08));
				info.signExtend = (codePtr[totalSize + 1] & 0x10) != 0;
				totalSize += 3;
			}
		}
	}
	else
	{
		if (info.byteSwap || info.hasImmediate)
		{
			// The instruction is a MOVBE but it failed so the value is still in little-endian byte order.
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
			totalSize += bswapSize;
		}
	}

	// In the trampoline code, we jump back into the block at the beginning
	// of the next instruction. The next instruction comes immediately
	// after the backpatched operation, or BACKPATCH_SIZE bytes after the start
	// of the backpatched operation, whichever comes last. (The JIT inserts NOPs
	// into the original code if necessary to ensure there is enough space
	// to insert the backpatch jump.)
	int padding = totalSize > BACKPATCH_SIZE ? totalSize - BACKPATCH_SIZE : 0;
	u8* returnPtr = start + 5 + padding;

	// Generate the trampoline.
	const u8* trampoline;
	if (info.isMemoryWrite)
	{
		// TODO: special case FIFO writes.
		auto it3 = pcAtLoc.find(codePtr);
		if (it3 == pcAtLoc.end())
		{
			PanicAlert("BackPatch: no pc entry for address %p", codePtr);
			return false;
		}

		u32 pc = it3->second;
		trampoline = trampolines.GenerateWriteTrampoline(info, registersInUse, exceptionHandler, returnPtr, pc);
	}
	else
	{
		trampoline = trampolines.GenerateReadTrampoline(info, registersInUse, exceptionHandler, returnPtr);
	}

	// Patch the original memory operation.
	XEmitter emitter(start);
	emitter.JMP(trampoline, true);
	for (int i = 0; i < padding; ++i)
		emitter.INT3();
	ctx->CTX_PC = (u64)start;

	return true;
}
