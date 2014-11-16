// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitArm32/Jit.h"

using namespace ArmGen;

// This generates some fairly heavy trampolines, but:
// 1) It's really necessary. We don't know anything about the context.
// 2) It doesn't really hurt. Only instructions that access I/O will get these, and there won't be
//    that many of them in a typical program/game.
static bool DisamLoadStore(const u32 inst, ARMReg &rD, u8 &accessSize, bool &Store, bool *new_system)
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
		{
			// Could be a floating point loadstore
			u8 op2 = (inst >> 24) & 0xF;
			switch (op2)
			{
			case 0xD: // VLDR/VSTR
				*new_system = true;
			break;
			case 0x4: // VST1/VLD1
				*new_system = true;
			break;
			default:
				printf("Op is 0x%02x\n", op);
				return false;
			break;
			}
		}
	}
	return true;
}

bool JitArm::HandleFault(uintptr_t access_address, SContext* ctx)
{
	if (access_address < (uintptr_t)Memory::base)
		PanicAlertT("Exception handler - access below memory space. 0x%08x", access_address);
	return BackPatch(ctx);
}

bool JitArm::BackPatch(SContext* ctx)
{
	// TODO: This ctx needs to be filled with our information

	// We need to get the destination register before we start
	u8* codePtr = (u8*)ctx->CTX_PC;
	u32 Value = *(u32*)codePtr;
	ARMReg rD;
	u8 accessSize;
	bool Store;
	bool new_system = false;

	if (!DisamLoadStore(Value, rD, accessSize, Store, &new_system))
	{
		printf("Invalid backpatch at location 0x%08lx(0x%08x)\n", ctx->CTX_PC, Value);
		exit(0);
	}

	if (new_system)
	{
		// The new system is a lot easier to backpatch than the old crap.
		// Instead of backpatching over code and making sure we NOP pad and other crap
		// We emit both the slow and fast path and branch over the slow path each time
		// We search backwards until we find the second branch instruction
		// Then proceed to replace it with a NOP and set that to the new PC.
		// This ensures that we run the slow path and then branch over the fast path.

		// Run backwards until we find the branch we want to NOP
		for (int branches = 2; branches > 0; ctx->CTX_PC -= 4)
			if ((*(u32*)ctx->CTX_PC & 0x0F000000) == 0x0A000000) // B
				--branches;

		ctx->CTX_PC += 4;
		ARMXEmitter emitter((u8*)ctx->CTX_PC);
		emitter.NOP(1);
		emitter.FlushIcache();
		return true;
	}
	else
	{
		if (Store)
		{
			const u32 ARMREGOFFSET = 4 * 5;
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
			u32 newPC = ctx->CTX_PC - (ARMREGOFFSET + 4 * 4);
			ctx->CTX_PC = newPC;
			emitter.FlushIcache();
			return true;
		}
		else
		{
			const u32 ARMREGOFFSET = 4 * 4;
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
			ctx->CTX_PC -= ARMREGOFFSET + (4 * 4);
			emitter.FlushIcache();
			return true;
		}
	}
	return 0;
}

