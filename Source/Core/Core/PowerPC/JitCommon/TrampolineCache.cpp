// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/x64ABI.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/TrampolineCache.h"

#ifdef _WIN32
	#include <windows.h>
#endif


using namespace Gen;

extern u8 *trampolineCodePtr;

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
	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;

	// It's a read. Easy.
	// RSP alignment here is 8 due to the call.
	ABI_PushRegistersAndAdjustStack(registersInUse, 8);

	if (addrReg != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(addrReg));

	if (info.displacement)
		ADD(32, R(ABI_PARAM1), Imm32(info.displacement));

	switch (info.operandSize)
	{
	case 4:
		CALL((void *)&Memory::Read_U32);
		break;
	case 2:
		CALL((void *)&Memory::Read_U16);
		SHL(32, R(ABI_RETURN), Imm8(16));
		break;
	case 1:
		CALL((void *)&Memory::Read_U8);
		break;
	}

	if (info.signExtend && info.operandSize == 1)
	{
		// Need to sign extend value from Read_U8.
		MOVSX(32, 8, dataReg, R(ABI_RETURN));
	}
	else if (dataReg != EAX)
	{
		MOV(32, R(dataReg), R(ABI_RETURN));
	}

	ABI_PopRegistersAndAdjustStack(registersInUse, 8);
	RET();
	return trampoline;
}

// Extremely simplistic - just generate the requested trampoline. May reuse them in the future.
const u8 *TrampolineCache::GetWriteTrampoline(const InstructionInfo &info, u32 registersInUse, u32 pc)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8 *trampoline = GetCodePtr();

	X64Reg dataReg = (X64Reg)info.regOperandReg;
	X64Reg addrReg = (X64Reg)info.scaledReg;

	// It's a write. Yay. Remember that we don't have to be super efficient since it's "just" a
	// hardware access - we can take shortcuts.
	// Don't treat FIFO writes specially for now because they require a burst
	// check anyway.

	// PC is used by memory watchpoints (if enabled) or to print accurate PC locations in debug logs
	MOV(32, PPCSTATE(pc), Imm32(pc));

	ABI_PushRegistersAndAdjustStack(registersInUse, 8);

	if (info.hasImmediate)
	{
		if (addrReg != ABI_PARAM2)
			MOV(64, R(ABI_PARAM2), R(addrReg));
		// we have to swap back the immediate to pass it to the write functions
		switch (info.operandSize)
		{
		case 8:
			PanicAlert("Invalid 64-bit immediate!");
			break;
		case 4:
			MOV(32, R(ABI_PARAM1), Imm32(Common::swap32((u32)info.immediate)));
			break;
		case 2:
			MOV(16, R(ABI_PARAM1), Imm16(Common::swap16((u16)info.immediate)));
			break;
		case 1:
			MOV(8, R(ABI_PARAM1), Imm8((u8)info.immediate));
			break;
		}
	}
	else
	{
		MOVTwo(64, ABI_PARAM1, dataReg, ABI_PARAM2, addrReg);
	}
	if (info.displacement)
	{
		ADD(32, R(ABI_PARAM2), Imm32(info.displacement));
	}

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

	ABI_PopRegistersAndAdjustStack(registersInUse, 8);
	RET();

	return trampoline;
}


