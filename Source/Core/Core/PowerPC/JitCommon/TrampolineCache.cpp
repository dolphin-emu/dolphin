// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/MemoryUtil.h"
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
	AllocCodeSpace(size, PPCSTATE_BASE);
	CheckRIPRelative(region, size);
}

void TrampolineCache::ClearCodeSpace()
{
	X64CodeBlock::ClearCodeSpace();
}

void TrampolineCache::Shutdown()
{
	FreeCodeSpace();
}

const u8* TrampolineCache::GenerateReadTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u8* exceptionHandler, u8* returnPtr)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();
	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;
	int stack_offset = 0;
	bool push_param1 = registersInUse[ABI_PARAM1];

	if (push_param1)
	{
		PUSH(ABI_PARAM1);
		stack_offset = 8;
		registersInUse[ABI_PARAM1] = 0;
	}

	int dataRegSize = info.operandSize == 8 ? 64 : 32;
	if (addrReg != ABI_PARAM1 && info.displacement)
		LEA(32, ABI_PARAM1, MDisp(addrReg, info.displacement));
	else if (addrReg != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(addrReg));
	else if (info.displacement)
		ADD(32, R(ABI_PARAM1), Imm32(info.displacement));

	ABI_PushRegistersAndAdjustStack(registersInUse, stack_offset);

	switch (info.operandSize)
	{
	case 8:
		CALL((void*)&PowerPC::Read_U64);
		break;
	case 4:
		CALL((void*)&PowerPC::Read_U32);
		break;
	case 2:
		CALL((void*)&PowerPC::Read_U16);
		break;
	case 1:
		CALL((void*)&PowerPC::Read_U8);
		break;
	}

	ABI_PopRegistersAndAdjustStack(registersInUse, stack_offset);

	if (push_param1)
		POP(ABI_PARAM1);

	if (exceptionHandler)
	{
		TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_DSI));
		J_CC(CC_NZ, exceptionHandler);
	}

	if (info.signExtend)
		MOVSX(dataRegSize, info.operandSize * 8, dataReg, R(ABI_RETURN));
	else if (dataReg != ABI_RETURN || info.operandSize < 4)
		MOVZX(dataRegSize, info.operandSize * 8, dataReg, R(ABI_RETURN));

	JMP(returnPtr, true);

	JitRegister::Register(trampoline, GetCodePtr(), "JIT_ReadTrampoline");
	return trampoline;
}

const u8* TrampolineCache::GenerateWriteTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u8* exceptionHandler, u8* returnPtr, u32 pc)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();

	X64Reg dataReg = (X64Reg)info.regOperandReg;
	X64Reg addrReg = (X64Reg)info.scaledReg;

	// Don't treat FIFO writes specially for now because they require a burst
	// check anyway.

	// PC is used by memory watchpoints (if enabled) or to print accurate PC locations in debug logs
	MOV(32, PPCSTATE(pc), Imm32(pc));

	ABI_PushRegistersAndAdjustStack(registersInUse, 0);

	if (info.hasImmediate)
	{
		if (addrReg != ABI_PARAM2 && info.displacement)
			LEA(32, ABI_PARAM2, MDisp(addrReg, info.displacement));
		else if (addrReg != ABI_PARAM2)
			MOV(32, R(ABI_PARAM2), R(addrReg));
		else if (info.displacement)
			ADD(32, R(ABI_PARAM2), Imm32(info.displacement));

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
		int dataRegSize = info.operandSize == 8 ? 64 : 32;
		MOVTwo(dataRegSize, ABI_PARAM2, addrReg, info.displacement, ABI_PARAM1, dataReg);
	}

	switch (info.operandSize)
	{
	case 8:
		CALL((void *)&PowerPC::Write_U64);
		break;
	case 4:
		CALL((void *)&PowerPC::Write_U32);
		break;
	case 2:
		CALL((void *)&PowerPC::Write_U16);
		break;
	case 1:
		CALL((void *)&PowerPC::Write_U8);
		break;
	}

	ABI_PopRegistersAndAdjustStack(registersInUse, 0);
	if (exceptionHandler)
	{
		TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_DSI));
		J_CC(CC_NZ, exceptionHandler);
	}
	JMP(returnPtr, true);

	JitRegister::Register(trampoline, GetCodePtr(), "JIT_WriteTrampoline_%x", pc);
	return trampoline;
}
