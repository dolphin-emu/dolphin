// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>

#include "disasm.h"

#include "Core/PowerPC/JitCommon/JitBase.h"

using namespace Gen;

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

	auto it = backPatchInfo.find(codePtr);
	if (it == backPatchInfo.end())
	{
		PanicAlert("BackPatch: no register use entry for address %p", codePtr);
		return false;
	}

	BackPatchInfo& info = it->second;

	u8* exceptionHandler = nullptr;
	if (jit->jo.memcheck)
	{
		auto it2 = exceptionHandlerAtLoc.find(codePtr);
		if (it2 != exceptionHandlerAtLoc.end())
			exceptionHandler = it2->second;
	}

	// In the trampoline code, we jump back into the block at the beginning
	// of the next instruction. The next instruction comes immediately
	// after the backpatched operation, or BACKPATCH_SIZE bytes after the start
	// of the backpatched operation, whichever comes last. (The JIT inserts NOPs
	// into the original code if necessary to ensure there is enough space
	// to insert the backpatch jump.)

	jit->js.generatingTrampoline = true;
	jit->js.trampolineExceptionHandler = exceptionHandler;

	// Generate the trampoline.
	const u8* trampoline;
	if (info.read)
	{
		trampoline = trampolines.GenerateReadTrampoline(info);
	}
	else
	{
		trampoline = trampolines.GenerateWriteTrampoline(info);
	}

	jit->js.generatingTrampoline = false;
	jit->js.trampolineExceptionHandler = nullptr;

	u8* start = info.start;

	// Patch the original memory operation.
	XEmitter emitter(start);
	emitter.JMP(trampoline, true);
	// NOPs become dead code
	for (const u8* i = emitter.GetCodePtr(); i < info.end; ++i)
		emitter.INT3();

	// Rewind time to just before the start of the write block. If we swapped memory
	// before faulting (eg: the store+swap was not an atomic op like MOVBE), let's
	// swap it back so that the swap can happen again (this double swap isn't ideal but
	// only happens the first time we fault).
	if (info.mov.nonAtomicSwapStore)
	{
		X64Reg reg = info.mov.nonAtomicSwapStoreSrc;
		u64* ptr = ContextRN(ctx, reg);
		switch (info.accessSize)
		{
		case 8:
			break;
		case 16:
			*ptr = Common::swap16((u16) *ptr);
			break;
		case 32:
			*ptr = Common::swap32((u32) *ptr);
			break;
		case 64:
			*ptr = Common::swap64(*ptr);
			break;
		}
	}

	ctx->CTX_PC = (u64)start;

	return true;
}
