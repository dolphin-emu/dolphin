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

void TrampolineCache::Init()
{
	AllocCodeSpace(8 * 1024 * 1024);
}

void TrampolineCache::ClearCodeSpace()
{
	X64CodeBlock::ClearCodeSpace();
	cachedTrampolines.clear();
}

void TrampolineCache::Shutdown()
{
	FreeCodeSpace();
	cachedTrampolines.clear();
}

const u8* TrampolineCache::GetReadTrampoline(const InstructionInfo &info, BitSet32 registersInUse)
{
	TrampolineCacheKey key = { registersInUse, 0, info };

	auto it = cachedTrampolines.find(key);
	if (it != cachedTrampolines.end())
		return it->second;

	const u8* trampoline = GenerateReadTrampoline(info, registersInUse);
	cachedTrampolines[key] = trampoline;
	return trampoline;
}

const u8* TrampolineCache::GenerateReadTrampoline(const InstructionInfo &info, BitSet32 registersInUse)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();
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

const u8* TrampolineCache::GetWriteTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u32 pc)
{
	TrampolineCacheKey key = { registersInUse, pc, info };

	auto it = cachedTrampolines.find(key);
	if (it != cachedTrampolines.end())
		return it->second;

	const u8* trampoline = GenerateWriteTrampoline(info, registersInUse, pc);
	cachedTrampolines[key] = trampoline;
	return trampoline;
}

const u8* TrampolineCache::GenerateWriteTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u32 pc)
{
	if (GetSpaceLeft() < 1024)
		PanicAlert("Trampoline cache full");

	const u8* trampoline = GetCodePtr();

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

size_t TrampolineCacheKeyHasher::operator()(const TrampolineCacheKey& k) const
{
	size_t res = std::hash<int>()(k.registersInUse.m_val);
	res ^= std::hash<int>()(k.info.operandSize)    >> 1;
	res ^= std::hash<int>()(k.info.regOperandReg)  >> 2;
	res ^= std::hash<int>()(k.info.scaledReg)      >> 3;
	res ^= std::hash<u64>()(k.info.immediate)      >> 4;
	res ^= std::hash<int>()(k.pc)                  >> 5;
	res ^= std::hash<int>()(k.info.displacement)   << 1;
	res ^= std::hash<bool>()(k.info.signExtend)    << 2;
	res ^= std::hash<bool>()(k.info.hasImmediate)  << 3;
	res ^= std::hash<bool>()(k.info.isMemoryWrite) << 4;

	return res;
}

bool TrampolineCacheKey::operator==(const TrampolineCacheKey &other) const
{
	return pc == other.pc &&
	       registersInUse == other.registersInUse &&
	       info == other.info;
}
