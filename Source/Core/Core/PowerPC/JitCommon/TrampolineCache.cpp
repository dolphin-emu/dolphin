// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
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

const u8* TrampolineCache::GenerateReadTrampoline(const TrampolineInfo& info)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();

	SafeLoadToReg(info.op_reg, info.op_arg, info.accessSize, info.offset, info.registersInUse, info.signExtend, info.flags | SAFE_LOADSTORE_FORCE_SLOWMEM);

	JMP(info.end, true);

	JitRegister::Register(trampoline, GetCodePtr(), "JIT_ReadTrampoline_%x", info.pc);
	return trampoline;
}

const u8* TrampolineCache::GenerateWriteTrampoline(const TrampolineInfo& info)
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
