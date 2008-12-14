// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <string>

#include "Common.h"
#include "disasm.h"
#include "JitAsm.h"
#include "JitBackpatch.h"
#include "../../HW/Memmap.h"

#include "x64Emitter.h"
#include "ABI.h"
#include "Thunk.h"
#include "x64Analyzer.h"

#include "StringUtil.h"
#include "Jit.h"

using namespace Gen;

namespace Jit64 {

extern u8 *trampolineCodePtr;
	
void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress) {
	u64 code_addr = (u64)codePtr;
	disassembler disasm;
	char disbuf[256];
	memset(disbuf, 0, 256);
#ifdef _M_IX86
	disasm.disasm32(0, code_addr, codePtr, disbuf);
#else
	disasm.disasm64(0, code_addr, codePtr, disbuf);
#endif
	PanicAlert("%s\n\n"
       "Error encountered accessing emulated address %08x.\n"
	   "Culprit instruction: \n%s\nat %08x%08x",
	   text.c_str(), emAddress, disbuf, code_addr>>32, code_addr);
	return;
}

// This generates some fairly heavy trampolines, but:
// 1) It's really necessary. We don't know anything about the context.
// 2) It doesn't really hurt. Only instructions that access I/O will get these, and there won't be 
//    that many of them in a typical program/game.
u8 *BackPatch(u8 *codePtr, int accessType, u32 emAddress, CONTEXT *ctx)
{
#ifdef _M_X64
	if (!IsInJitCode(codePtr))
		return 0;  // this will become a regular crash real soon after this
	
	u8 *oldCodePtr = GetWritableCodePtr();
	InstructionInfo info;
	if (!DisassembleMov(codePtr, info, accessType)) {
		BackPatchError("BackPatch - failed to disassemble MOV instruction", codePtr, emAddress);
	}

	/*
	if (info.isMemoryWrite) {
		if (!Memory::IsRAMAddress(emAddress, true)) {
			PanicAlert("Exception: Caught write to invalid address %08x", emAddress);
			return;
		}
		BackPatchError("BackPatch - determined that MOV is write, not yet supported and should have been caught before",
					   codePtr, emAddress);
	}*/

	if (info.operandSize != 4) {
		BackPatchError(StringFromFormat("BackPatch - no support for operand size %i", info.operandSize), codePtr, emAddress);
	}

	X64Reg addrReg = (X64Reg)info.scaledReg;
	X64Reg dataReg = (X64Reg)info.regOperandReg;
	if (info.otherReg != RBX)
		PanicAlert("BackPatch : Base reg not RBX."
		           "\n\nAttempted to access %08x.", emAddress);
	//if (accessType == OP_ACCESS_WRITE)
	//	PanicAlert("BackPatch : Currently only supporting reads."
	//	           "\n\nAttempted to write to %08x.", emAddress);

	// OK, let's write a trampoline, and a jump to it.
	// Later, let's share trampolines.

	// In the first iteration, we assume that all accesses are 32-bit. We also only deal with reads.
	// Next step - support writes, special case FIFO writes. Also, support 32-bit mode.
	u8 *trampoline = trampolineCodePtr;
	SetCodePtr(trampolineCodePtr);

	if (accessType == 0)
	{
		// It's a read. Easy.
		ABI_PushAllCallerSavedRegsAndAdjustStack();
		if (addrReg != ABI_PARAM1)
			MOV(32, R(ABI_PARAM1), R((X64Reg)addrReg));
		if (info.displacement) {
			ADD(32, R(ABI_PARAM1), Imm32(info.displacement));
		}
		switch (info.operandSize) {
		case 4:
			CALL(ProtectFunction((void *)&Memory::Read_U32, 1));
			break;
		default:
			BackPatchError(StringFromFormat("We don't handle the size %i yet in backpatch", info.operandSize), codePtr, emAddress);
			break;
		}
		ABI_PopAllCallerSavedRegsAndAdjustStack();
		MOV(32, R(dataReg), R(EAX));
		RET();
		trampolineCodePtr = GetWritableCodePtr();

		SetCodePtr(codePtr);
		int bswapNopCount;
		// Check the following BSWAP for REX byte
		if ((GetCodePtr()[info.instructionSize] & 0xF0) == 0x40)
			bswapNopCount = 3;
		else
			bswapNopCount = 2;
		CALL(trampoline);
		NOP((int)info.instructionSize + bswapNopCount - 5);
		SetCodePtr(oldCodePtr);

		return codePtr;
	}
	else if (accessType == 1)
	{
		// It's a write. Yay. Remember that we don't have to be super efficient since it's "just" a 
		// hardware access - we can take shortcuts.
		//if (emAddress == 0xCC008000)
		//	PanicAlert("caught a fifo write");
		if (dataReg != EAX)
			PanicAlert("Backpatch write - not through EAX");
		CMP(32, R(addrReg), Imm32(0xCC008000));
		FixupBranch skip_fast = J_CC(CC_NE, false);
		MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
		CALL((void*)Asm::fifoDirectWrite32);
		RET();
		SetJumpTarget(skip_fast);
		ABI_PushAllCallerSavedRegsAndAdjustStack();
		if (addrReg != ABI_PARAM1) {
			//INT3();
			MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
			MOV(32, R(ABI_PARAM2), R((X64Reg)addrReg));
		} else {
			MOV(32, R(ABI_PARAM2), R((X64Reg)addrReg));
			MOV(32, R(ABI_PARAM1), R((X64Reg)dataReg));
		}
		if (info.displacement) {
			ADD(32, R(ABI_PARAM2), Imm32(info.displacement));
		}
		switch (info.operandSize) {
		case 4:
			CALL(ProtectFunction((void *)&Memory::Write_U32, 2));
			break;
		default:
			BackPatchError(StringFromFormat("We don't handle the size %i yet in backpatch", info.operandSize), codePtr, emAddress);
			break;
		}
		ABI_PopAllCallerSavedRegsAndAdjustStack();
		RET();

		trampolineCodePtr = GetWritableCodePtr();

		// We know it's EAX so the BSWAP before will be two byte. Overwrite it.
		SetCodePtr(codePtr - 2);
		CALL(trampoline);
		NOP((int)info.instructionSize - 3);
		if (info.instructionSize < 3)
			PanicAlert("instruction too small");
		SetCodePtr(oldCodePtr);

		// We entered here with a BSWAP-ed EAX. We'll have to swap it back.
		ctx->Rax = Common::swap32(ctx->Rax);

		return codePtr - 2;
	}
	return 0;
#else
	return 0;
#endif
}

}  // namespace
