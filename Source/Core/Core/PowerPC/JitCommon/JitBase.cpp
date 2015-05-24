// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>

#include "disasm.h"

#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

JitBase *jit;

void Jit(u32 em_address)
{
	jit->Jit(em_address);
}

u32 Helper_Mask(u8 mb, u8 me)
{
	u32 mask = ((u32)-1 >> mb) ^ (me >= 31 ? 0 : (u32)-1 >> (me + 1));
	return mb > me ? ~mask : mask;
}

void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer *code_buffer, const u8 *normalEntry, JitBlock *b)
{
	for (int i = 0; i < size; i++)
	{
		const PPCAnalyst::CodeOp &op = code_buffer->codebuffer[i];
		std::string temp = StringFromFormat("%08x %s", op.address, GekkoDisassembler::Disassemble(op.inst.hex, op.address).c_str());
		DEBUG_LOG(DYNA_REC, "IR_X86 PPC: %s\n", temp.c_str());
	}

	disassembler x64disasm;
	x64disasm.set_syntax_intel();

	u64 disasmPtr = (u64)normalEntry;
	const u8 *end = normalEntry + b->codeSize;

	while ((u8*)disasmPtr < end)
	{
		char sptr[1000] = "";
		disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, sptr);
		DEBUG_LOG(DYNA_REC,"IR_X86 x86: %s", sptr);
	}

	if (b->codeSize <= 250)
	{
		std::stringstream ss;
		ss << std::hex;
		for (u8 i = 0; i <= b->codeSize; i++)
		{
			ss.width(2);
			ss.fill('0');
			ss << (u32)*(normalEntry + i);
		}
		DEBUG_LOG(DYNA_REC,"IR_X86 bin: %s\n\n\n", ss.str().c_str());
	}
}

bool JitBase::MergeAllowedNextInstructions(int count)
{
	if (PowerPC::GetState() == PowerPC::CPU_STEPPING || js.instructionsLeft < count)
		return false;
	// Be careful: a breakpoint kills flags in between instructions
	for (int i = 1; i <= count; i++)
	{
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging &&
			PowerPC::breakpoints.IsAddressBreakPoint(js.op[i].address))
			return false;
		if (js.op[i].isBranchTarget)
			return false;
	}
	return true;
}

void JitBase::UpdateMemoryOptions()
{
	bool any_watchpoints = PowerPC::memchecks.HasAny();
	jo.fastmem = SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem &&
	             !any_watchpoints;
	jo.memcheck = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU ||
	              any_watchpoints;
	jo.alwaysUseMemFuncs = any_watchpoints;

}
