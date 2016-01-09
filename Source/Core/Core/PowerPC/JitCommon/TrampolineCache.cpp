// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/StringUtil.h"
#include "Common/x64ABI.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/TrampolineCache.h"

#ifdef _WIN32
	#include <windows.h>
#endif


using namespace Gen;

void TrampolineCache::Init(int size)
{
	AllocCodeSpace(size);
}

void TrampolineCache::ClearCodeSpace()
{
	X64CodeBlock::ClearCodeSpace();
}

void TrampolineCache::Shutdown()
{
	FreeCodeSpace();
}

const u8* TrampolineCache::GenerateReadTrampoline(const BackPatchInfo &info)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();

	SafeLoadToReg(info.op_reg, info.op_arg, info.accessSize, info.offset, info.registersInUse, info.signExtend, info.flags | SAFE_LOADSTORE_FORCE_SLOWMEM);

	JMP(info.end, true);

	JitRegister::Register(trampoline, GetCodePtr(), "JIT_ReadTrampoline_%x", info.pc);
	return trampoline;
}

const u8* TrampolineCache::GenerateWriteTrampoline(const BackPatchInfo &info)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();

	// Don't treat FIFO writes specially for now because they require a burst
	// check anyway.

	// PC is used by memory watchpoints (if enabled) or to print accurate PC locations in debug logs
	MOV(32, PPCSTATE(pc), Imm32(info.pc));

	SafeWriteRegToReg(info.op_arg, info.op_reg, info.accessSize, info.offset, info.registersInUse, info.flags | SAFE_LOADSTORE_FORCE_SLOWMEM);

	JMP(info.end, true);

	JitRegister::Register(trampoline, GetCodePtr(), "JIT_WriteTrampoline_%x", info.pc);
	return trampoline;
}
