// Copyright (C) 2003 Dolphin Project.

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

#include "../../HW/Memmap.h"
#include "Jit.h"
#include "../JitCommon/JitBackpatch.h"
#include "StringUtil.h"

#ifdef _M_X64
static void BackPatchError(const std::string &text, u8 *codePtr, u32 emAddress) {
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
	   "Culprit instruction: \n%s\nat %#llx",
	   text.c_str(), emAddress, disbuf, code_addr);
	return;
}
#endif

// This generates some fairly heavy trampolines, but:
// 1) It's really necessary. We don't know anything about the context.
// 2) It doesn't really hurt. Only instructions that access I/O will get these, and there won't be 
//    that many of them in a typical program/game.
bool DisamLoadStore(const u32 inst, ARMReg &rD, u8 &accessSize, bool &Store)
{
	u8 op = (inst >> 20) & 0xFF;
	rD = (ARMReg)((inst >> 12) & 0xF);

	switch (op)
	{
		case 0x58: // STR
		{
			Store = true;
			accessSize = 32;
		}
		break;
		case 0x59: // LDR
		{
			Store = false;
			accessSize = 32;
		}
		break;
		case 0x1D: // LDRH
		{
			Store = false;
			accessSize = 16;
		}
		break;
		case 0x45 + 0x18: // LDRB
		{
			Store = false;
			accessSize = 8;
		}
		break;
		case 0x5C: // STRB
		{
			Store = true;
			accessSize = 8;
		}
		break;
		case 0x1C: // STRH
		{
			Store = true;
			accessSize = 16;
		}
		break;
		default:
			printf("Op is 0x%02x\n", op);
			return false;
	}
	return true;
}
const u8 *JitArm::BackPatch(u8 *codePtr, int accessType, u32 emAddress, void *ctx_void)
{
	// TODO: This ctx needs to be filled with our information
	CONTEXT *ctx = (CONTEXT *)ctx_void;
	
	// We need to get the destination register before we start
	u32 Value = *(u32*)codePtr;
	ARMReg rD;
	u8 accessSize;
	bool Store;

	if (!DisamLoadStore(Value, rD, accessSize, Store))
	{
		printf("Invalid backpatch at location 0x%08x(0x%08x)\n", ctx->reg_pc, Value);
		exit(0);
	}

	if (Store)
	{
		const u32 ARMREGOFFSET = 4 * 7;
		ARMXEmitter emitter(codePtr - ARMREGOFFSET);
		switch (accessSize)
		{
			case 8: // 8bit
				emitter.MOVI2R(R14, (u32)&Memory::Write_U8, false); // 1-2
				return 0;
			break;
			case 16: // 16bit
				emitter.MOVI2R(R14, (u32)&Memory::Write_U16, false); // 1-2
				return 0;
			break;
			case 32: // 32bit
				emitter.MOVI2R(R14, (u32)&Memory::Write_U32, false); // 1-2
			break;
		}
		emitter.PUSH(4, R0, R1, R2, R3); // 3
		emitter.MOV(R0, rD); // Value - 4
		emitter.MOV(R1, R10); // Addr- 5 
		emitter.BL(R14); // 6
		emitter.POP(4, R0, R1, R2, R3); // 7
		emitter.NOP(1); // 8
		u32 newPC = ctx->reg_pc - (ARMREGOFFSET + 4 * 4);
		ctx->reg_pc = newPC;
		emitter.FlushIcache();
		return codePtr;
	}
	else
	{
		const u32 ARMREGOFFSET = 4 * 6;
		ARMXEmitter emitter(codePtr - ARMREGOFFSET);
		switch (accessSize)
		{
			case 8: // 8bit
				emitter.MOVI2R(R14, (u32)&Memory::Read_U8, false); // 2	
			break;
			case 16: // 16bit
				emitter.MOVI2R(R14, (u32)&Memory::Read_U16, false); // 2	
			break;
			case 32: // 32bit
				emitter.MOVI2R(R14, (u32)&Memory::Read_U32, false); // 2	
			break;
		}
		emitter.PUSH(4, R0, R1, R2, R3); // 3
		emitter.MOV(R0, R10); // 4
		emitter.BL(R14); // 5
		emitter.MOV(R14, R0); // 6
		emitter.POP(4, R0, R1, R2, R3); // 7
		emitter.MOV(rD, R14); // 8
		ctx->reg_pc -= ARMREGOFFSET + (4 * 4);
		emitter.FlushIcache();
		return codePtr;
	}
	return 0;
}

