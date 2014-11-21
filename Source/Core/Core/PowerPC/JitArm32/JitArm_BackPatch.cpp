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
bool JitArm::DisasmLoadStore(const u8* ptr, u32* flags, ARMReg* rD, ARMReg* V1)
{
	u32 inst = *(u32*)ptr;
	u32 prev_inst = *(u32*)(ptr - 4);
	u32 next_inst = *(u32*)(ptr + 4);
	u8 op = (inst >> 20) & 0xFF;
	*rD = (ARMReg)((inst >> 12) & 0xF);

	switch (op)
	{
		case 0x58: // STR
		{
			*flags |=
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_32;
			*rD = (ARMReg)(prev_inst & 0xF);
		}
		break;
		case 0x59: // LDR
		{
			*flags |=
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_32;
			// REV
			if ((next_inst & 0x0FFF0FF0) != 0x06BF0F30)
				*flags |= BackPatchInfo::FLAG_REVERSE;
		}
		break;
		case 0x1D: // LDRH
		{
			*flags |=
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_16;
			// REV16
			if((next_inst & 0x0FFF0FF0) != 0x06BF0FB0)
				*flags |= BackPatchInfo::FLAG_REVERSE;
		}
		break;
		case 0x45 + 0x18: // LDRB
		{
			*flags |=
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_8;
		}
		break;
		case 0x5C: // STRB
		{
			*flags |=
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_8;
			*rD = (ARMReg)((inst >> 12) & 0xF);
		}
		break;
		case 0x1C: // STRH
		{
			*flags |=
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_16;
			*rD = (ARMReg)(prev_inst & 0xF);
		}
		break;
		default:
		{
			// Could be a floating point loadstore
			u8 op2 = (inst >> 24) & 0xF;
			switch (op2)
			{
			case 0xD: // VLDR/VSTR
			{
				bool load = (inst >> 20) & 1;
				bool single = !((inst >> 8) & 1);

				if (load)
					*flags |= BackPatchInfo::FLAG_LOAD;
				else
					*flags |= BackPatchInfo::FLAG_STORE;

				if (single)
					*flags |= BackPatchInfo::FLAG_SIZE_F32;
				else
					*flags |= BackPatchInfo::FLAG_SIZE_F64;
				if (single)
				{
					if (!load)
					{
						u32 vcvt = *(u32*)(ptr - 8);
						u32 src_register = vcvt & 0xF;
						src_register |= (vcvt >> 1) & 0x10;
						*rD = (ARMReg)(src_register + D0);
					}
				}
			}
			break;
			case 0x4: // VST1/VLD1
			{
				u32 size = (inst >> 6) & 0x3;
				bool load = (inst >> 21) & 1;
				if (load)
					*flags |= BackPatchInfo::FLAG_LOAD;
				else
					*flags |= BackPatchInfo::FLAG_STORE;


				if (size == 2) // 32bit
				{
					if (load)
					{
						// For 32bit loads we are loading to a temporary
						// So we need to read PC+8,PC+12 to get the two destination registers
						u32 vcvt_1 = *(u32*)(ptr + 8);
						u32 vcvt_2 = *(u32*)(ptr + 12);

						u32 dest_register_1 = (vcvt_1 >> 12) & 0xF;
						dest_register_1 |= (vcvt_1 >> 18) & 0x10;

						u32 dest_register_2 = (vcvt_2 >> 12) & 0xF;
						dest_register_2 |= (vcvt_2 >> 18) & 0x10;

						// Make sure to encode the destination register to something our emitter understands
						*rD = (ARMReg)(dest_register_1 + D0);
						*V1 = (ARMReg)(dest_register_2 + D0);
					}
					else
					{
						// For 32bit stores we are storing from a temporary
						// So we need to check the VCVT at PC-8 for the source register
						u32 vcvt = *(u32*)(ptr - 8);
						u32 src_register = vcvt & 0xF;
						src_register |= (vcvt >> 1) & 0x10;
						*rD = (ARMReg)(src_register + D0);
					}
					*flags |= BackPatchInfo::FLAG_SIZE_F32;
				}
				else if (size == 3) // 64bit
				{
					if (load)
					{
						// For 64bit loads we load directly in to the VFP register
						u32 dest_register = (inst >> 12) & 0xF;
						dest_register |= (inst >> 18) & 0x10;
						// Make sure to encode the destination register to something our emitter understands
						*rD = (ARMReg)(dest_register + D0);
					}
					else
					{
						// For 64bit stores we are storing from a temporary
						// Check the previous VREV64 instruction for the real register
						u32 src_register = prev_inst & 0xF;
						src_register |= (prev_inst >> 1) & 0x10;
						*rD = (ARMReg)(src_register + D0);
					}
					*flags |= BackPatchInfo::FLAG_SIZE_F64;
				}
			}
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
	ARMReg rD = INVALID_REG;
	ARMReg V1 = INVALID_REG;
	u32 flags = 0;

	if (!DisasmLoadStore(codePtr, &flags, &rD, &V1))
	{
		printf("Invalid backpatch at location 0x%08lx(0x%08x)\n", ctx->CTX_PC, Value);
		exit(0);
	}

	BackPatchInfo& info = m_backpatch_info[flags];
	ARMXEmitter emitter(codePtr - info.m_fastmem_trouble_inst_offset * 4);
	u32 new_pc = (u32)emitter.GetCodePtr();
	EmitBackpatchRoutine(&emitter, flags, false, true, rD, V1);
	emitter.FlushIcache();
	ctx->CTX_PC = new_pc;
	return true;
}

u32 JitArm::EmitBackpatchRoutine(ARMXEmitter* emit, u32 flags, bool fastmem, bool do_padding, ARMReg RS, ARMReg V1)
{
	ARMReg addr = R12;
	ARMReg temp = R11;
	u32 trouble_offset = 0;
	const u8* code_base = emit->GetCodePtr();

	if (fastmem)
	{
		ARMReg temp2 = R10;
		Operand2 mask(2, 1); // ~(Memory::MEMVIEW32_MASK)
		emit->BIC(temp, addr, mask); // 1
		emit->MOVI2R(temp2, (u32)Memory::base); // 2-3
		emit->ADD(temp, temp, temp2); // 4

		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			NEONXEmitter nemit(emit);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				emit->VCVT(S0, RS, 0);
				nemit.VREV32(I_8, D0, D0);
				trouble_offset = (emit->GetCodePtr() - code_base) / 4;
				emit->VSTR(S0, temp, 0);
			}
			else
			{
				nemit.VREV64(I_8, D0, RS);
				trouble_offset = (emit->GetCodePtr() - code_base) / 4;
				nemit.VST1(I_64, D0, temp);
			}
		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
		         flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			NEONXEmitter nemit(emit);

			trouble_offset = (emit->GetCodePtr() - code_base) / 4;
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				nemit.VLD1(F_32, D0, temp);
				nemit.VREV32(I_8, D0, D0); // Byte swap to result
				emit->VCVT(RS, S0, 0);
				emit->VCVT(V1, S0, 0);
			}
			else
			{
				nemit.VLD1(I_64, RS, temp);
				nemit.VREV64(I_8, RS, RS); // Byte swap to result
			}
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->REV(temp2, RS);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->REV16(temp2, RS);

			trouble_offset = (emit->GetCodePtr() - code_base) / 4;

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->STR(temp2, temp);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->STRH(temp2, temp);
			else
				emit->STRB(RS, temp);
		}
		else
		{
			trouble_offset = (emit->GetCodePtr() - code_base) / 4;

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->LDR(RS, temp); // 5
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->LDRH(RS, temp);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				emit->LDRB(RS, temp);


			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					emit->REV(RS, RS); // 6
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					emit->REV16(RS, RS);
			}
		}
	}
	else
	{
		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			emit->PUSH(4, R0, R1, R2, R3);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				emit->MOV(R1, addr);
				emit->VCVT(S0, RS, 0);
				emit->VMOV(R0, S0);
				emit->MOVI2R(temp, (u32)&Memory::Write_U32);
				emit->BL(temp);
			}
			else
			{
				emit->MOVI2R(temp, (u32)&Memory::Write_F64);
#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
				emit->VMOV(R0, RS);
				emit->MOV(R2, addr);
#else
				emit->VMOV(D0, RS);
				emit->MOV(R0, addr);
#endif
				emit->BL(temp);
			}
			emit->POP(4, R0, R1, R2, R3);
		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
		         flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			emit->PUSH(4, R0, R1, R2, R3);
			emit->MOV(R0, addr);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				emit->MOVI2R(temp, (u32)&Memory::Read_U32);
				emit->BL(temp);
				emit->VMOV(S0, R0);
				emit->VCVT(RS, S0, 0);
				emit->VCVT(V1, S0, 0);
			}
			else
			{
				emit->MOVI2R(temp, (u32)&Memory::Read_F64);
				emit->BL(temp);

#if !defined(__ARM_PCS_VFP) // SoftFP returns in R0 and R1
				emit->VMOV(RS, R0);
#else
				emit->VMOV(RS, D0);
#endif
			}
			emit->POP(4, R0, R1, R2, R3);
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			emit->PUSH(4, R0, R1, R2, R3);
			emit->MOV(R0, RS);
			emit->MOV(R1, addr);

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->MOVI2R(temp, (u32)&Memory::Write_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->MOVI2R(temp, (u32)&Memory::Write_U16);
			else
				emit->MOVI2R(temp, (u32)&Memory::Write_U8);

			emit->BL(temp);
			emit->POP(4, R0, R1, R2, R3);
		}
		else
		{
			emit->PUSH(4, R0, R1, R2, R3);
			emit->MOV(R0, addr);

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->MOVI2R(temp, (u32)&Memory::Read_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->MOVI2R(temp, (u32)&Memory::Read_U16);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				emit->MOVI2R(temp, (u32)&Memory::Read_U8);

			emit->BL(temp);
			emit->MOV(temp, R0);
			emit->POP(4, R0, R1, R2, R3);

			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				emit->MOV(RS, temp);
			}
			else
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					emit->REV(RS, temp); // 6
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					emit->REV16(RS, temp);
			}
		}
	}

	if (do_padding)
	{
		BackPatchInfo& info = m_backpatch_info[flags];
		u32 num_insts_max = std::max(info.m_fastmem_size, info.m_slowmem_size);

		u32 code_size = emit->GetCodePtr() - code_base;
		code_size /= 4;

		emit->NOP(num_insts_max - code_size);
	}

	return trouble_offset;
}

void JitArm::InitBackpatch()
{
	u32 flags = 0;
	BackPatchInfo info;
	u8* code_base = GetWritableCodePtr();
	u8* code_end;

	// Writes
	{
		// 8bit
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_8;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 16bit
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_16;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_32;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit float
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_F32;
			EmitBackpatchRoutine(this, flags, false, false, D0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, D0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 64bit float
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_F64;
			EmitBackpatchRoutine(this, flags, false, false, D0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, D0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}

	}

	// Loads
	{
		// 8bit
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_8;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 16bit
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_16;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_32;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}

		// 16bit - reverse
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_16 |
				BackPatchInfo::FLAG_REVERSE;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit - reverse
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_32 |
				BackPatchInfo::FLAG_REVERSE;
			EmitBackpatchRoutine(this, flags, false, false, R0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, R0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit float
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_F32;
			EmitBackpatchRoutine(this, flags, false, false, D0, D1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, D0, D1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 64bit float
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_F64;
			EmitBackpatchRoutine(this, flags, false, false, D0);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, D0);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
	}
}
