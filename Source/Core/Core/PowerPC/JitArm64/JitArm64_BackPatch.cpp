// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArmCommon/BackPatch.h"

using namespace Arm64Gen;

static void DoBacktrace(uintptr_t access_address, SContext* ctx)
{
	for (int i = 0; i < 30; i += 2)
		ERROR_LOG(DYNA_REC, "R%d: 0x%016llx\tR%d: 0x%016llx", i, ctx->CTX_REG(i), i + 1, ctx->CTX_REG(i + 1));

	ERROR_LOG(DYNA_REC, "R30: 0x%016llx\tSP: 0x%016llx", ctx->CTX_REG(30), ctx->CTX_SP);

	ERROR_LOG(DYNA_REC, "Access Address: 0x%016lx", access_address);
	ERROR_LOG(DYNA_REC, "PC: 0x%016llx", ctx->CTX_PC);

	ERROR_LOG(DYNA_REC, "Memory Around PC");

	std::string pc_memory = "";
	for (u64 pc = (ctx->CTX_PC - 32); pc < (ctx->CTX_PC + 32); pc += 16)
	{
		pc_memory += StringFromFormat("%08x%08x%08x%08x",
			Common::swap32(*(u32*)pc), Common::swap32(*(u32*)(pc + 4)),
			Common::swap32(*(u32*)(pc + 8)), Common::swap32(*(u32*)(pc + 12)));

		ERROR_LOG(DYNA_REC, "0x%016lx: %08x %08x %08x %08x",
			pc, *(u32*)pc, *(u32*)(pc + 4), *(u32*)(pc + 8), *(u32*)(pc + 12));
	}

	ERROR_LOG(DYNA_REC, "Full block: %s", pc_memory.c_str());
}

bool JitArm64::DisasmLoadStore(const u8* ptr, u32* flags, ARM64Reg* reg)
{
	u32 inst = *(u32*)ptr;
	u32 prev_inst = *(u32*)(ptr - 4);
	u32 next_inst = *(u32*)(ptr + 4);

	u8 op = (inst >> 22) & 0xFF;
	u8 size = (inst >> 30) & 0x3;

	if (size == 0) // 8-bit
		*flags |= BackPatchInfo::FLAG_SIZE_8;
	else if (size == 1) // 16-bit
		*flags |= BackPatchInfo::FLAG_SIZE_16;
	else if (size == 2) // 32-bit
		*flags |= BackPatchInfo::FLAG_SIZE_32;
	else if (size == 3) // 64-bit
		*flags |= BackPatchInfo::FLAG_SIZE_F64;

	if (op == 0xF5) // NEON LDR
	{
		if (size == 2) // 32-bit float
		{
			*flags &= ~BackPatchInfo::FLAG_SIZE_32;
			*flags |= BackPatchInfo::FLAG_SIZE_F32;

			// Loads directly in to the target register
			// Duplicates bottom result in to top register
			*reg = (ARM64Reg)(inst & 0x1F);
		}
		else // 64-bit float
		{
			// Real register is in the INS instruction
			u32 ins_inst = *(u32*)(ptr + 8);
			*reg = (ARM64Reg)(ins_inst & 0x1F);
		}
		*flags |= BackPatchInfo::FLAG_LOAD;
		return true;
	}
	else if (op == 0xF4) // NEON STR
	{
		if (size == 2) // 32-bit float
		{
			*flags &= ~BackPatchInfo::FLAG_SIZE_32;
			*flags |= BackPatchInfo::FLAG_SIZE_F32;

			// Real register is in the first FCVT conversion instruction
			u32 fcvt_inst = *(u32*)(ptr - 8);
			*reg = (ARM64Reg)((fcvt_inst >> 5) & 0x1F);
		}
		else // 64-bit float
		{
			// Real register is in the previous REV64 instruction
			*reg = (ARM64Reg)((prev_inst >> 5) & 0x1F);
		}
		*flags |= BackPatchInfo::FLAG_STORE;
		return true;
	}
	else if (op == 0xE5) // Load
	{
		*flags |= BackPatchInfo::FLAG_LOAD;
		*reg = (ARM64Reg)(inst & 0x1F);
		if ((next_inst & 0x7FFFF000) == 0x5AC00000) // REV
		{
			u32 sxth_inst = *(u32*)(ptr + 8);
			if ((sxth_inst & 0x7F800000) == 0x13000000) // SXTH
				*flags |= BackPatchInfo::FLAG_EXTEND;
		}
		else
		{
			*flags |= BackPatchInfo::FLAG_REVERSE;
		}
		return true;
	}
	else if (op == 0xE4) // Store
	{
		*flags |= BackPatchInfo::FLAG_STORE;

		if (size == 0) // 8-bit
			*reg = (ARM64Reg)(inst & 0x1F);
		else // 16-bit/32-bit register is in previous REV instruction
			*reg = (ARM64Reg)((prev_inst >> 5) & 0x1F);
		return true;
	}

	return false;
}

u32 JitArm64::EmitBackpatchRoutine(ARM64XEmitter* emit, u32 flags, bool fastmem, bool do_padding, ARM64Reg RS, ARM64Reg addr)
{
	u32 trouble_offset = 0;
	const u8* code_base = emit->GetCodePtr();

	if (fastmem)
	{
		u8* base = UReg_MSR(MSR).DR ? Memory::logical_base : Memory::physical_base;
		MOVK(addr, ((u64)base >> 32) & 0xFFFF, SHIFT_32);

		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			ARM64FloatEmitter float_emit(emit);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				float_emit.FCVT(32, 64, D0, RS);
				float_emit.REV32(8, D0, D0);
				trouble_offset = (emit->GetCodePtr() - code_base) / 4;
				float_emit.STR(32, INDEX_UNSIGNED, D0, addr, 0);
			}
			else
			{
				float_emit.REV64(8, Q0, RS);
				trouble_offset = (emit->GetCodePtr() - code_base) / 4;
				float_emit.STR(64, INDEX_UNSIGNED, Q0, addr, 0);
			}
		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
		         flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			ARM64FloatEmitter float_emit(emit);
			trouble_offset = (emit->GetCodePtr() - code_base) / 4;
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				float_emit.LD1R(32, EncodeRegToDouble(RS), addr);
				float_emit.REV32(8, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
				float_emit.FCVTL(64, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
			}
			else
			{
				float_emit.LDR(64, INDEX_UNSIGNED, Q0, addr, 0);
				float_emit.REV64(8, Q0, Q0);
				float_emit.INS(64, RS, 0, Q0, 0);
			}
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			ARM64Reg temp = W0;
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->REV32(temp, RS);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->REV16(temp, RS);

			trouble_offset = (emit->GetCodePtr() - code_base) / 4;

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->STR(INDEX_UNSIGNED, temp, addr, 0);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->STRH(INDEX_UNSIGNED, temp, addr, 0);
			else
				emit->STRB(INDEX_UNSIGNED, RS, addr, 0);
		}
		else
		{
			trouble_offset = (emit->GetCodePtr() - code_base) / 4;

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->LDR(INDEX_UNSIGNED, RS, addr, 0);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->LDRH(INDEX_UNSIGNED, RS, addr, 0);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				emit->LDRB(INDEX_UNSIGNED, RS, addr, 0);

			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					emit->REV32(RS, RS);
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					emit->REV16(RS, RS);
			}

			if (flags & BackPatchInfo::FLAG_EXTEND)
				emit->SXTH(RS, RS);
		}
	}
	else
	{
		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			ARM64FloatEmitter float_emit(emit);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				float_emit.FCVT(32, 64, D0, RS);
				float_emit.UMOV(32, W0, Q0, 0);
				emit->MOVI2R(X30, (u64)&PowerPC::Write_U32);
				emit->BLR(X30);
			}
			else
			{
				emit->MOVI2R(X30, (u64)&PowerPC::Write_U64);
				float_emit.UMOV(64, X0, RS, 0);
				emit->BLR(X30);
			}

		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
			   flags & (BackPatchInfo::FLAG_SIZE_F32 | BackPatchInfo::FLAG_SIZE_F64))
		{
			ARM64FloatEmitter float_emit(emit);
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				emit->MOVI2R(X30, (u64)&PowerPC::Read_U32);
				emit->BLR(X30);
				float_emit.DUP(32, RS, X0);
				float_emit.FCVTL(64, RS, RS);
			}
			else
			{
				emit->MOVI2R(X30, (u64)&PowerPC::Read_F64);
				emit->BLR(X30);
				float_emit.INS(64, RS, 0, X0);
			}
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			emit->MOV(W0, RS);

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->MOVI2R(X30, (u64)&PowerPC::Write_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->MOVI2R(X30, (u64)&PowerPC::Write_U16);
			else
				emit->MOVI2R(X30, (u64)&PowerPC::Write_U8);

			emit->BLR(X30);
		}
		else
		{
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				emit->MOVI2R(X30, (u64)&PowerPC::Read_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				emit->MOVI2R(X30, (u64)&PowerPC::Read_U16);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				emit->MOVI2R(X30, (u64)&PowerPC::Read_U8);

			emit->BLR(X30);

			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				emit->MOV(RS, W0);
			}
			else
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					emit->REV32(RS, W0);
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					emit->REV16(RS, W0);
			}

			if (flags & BackPatchInfo::FLAG_EXTEND)
				emit->SXTH(RS, RS);
		}
	}

	if (do_padding)
	{
		BackPatchInfo& info = m_backpatch_info[flags];
		u32 num_insts_max = std::max(info.m_fastmem_size, info.m_slowmem_size);

		u32 code_size = emit->GetCodePtr() - code_base;
		code_size /= 4;

		for (u32 i = 0; i < (num_insts_max - code_size); ++i)
			emit->HINT(HINT_NOP);
	}

	return trouble_offset;
}

bool JitArm64::HandleFault(uintptr_t access_address, SContext* ctx)
{
	if (!(access_address >= (uintptr_t)Memory::physical_base && access_address < (uintptr_t)Memory::physical_base + 0x100010000) &&
		!(access_address >= (uintptr_t)Memory::logical_base && access_address < (uintptr_t)Memory::logical_base + 0x100010000))
	{
		ERROR_LOG(DYNA_REC, "Exception handler - access below memory space. PC: 0x%016llx 0x%016lx < 0x%016lx", ctx->CTX_PC, access_address, (uintptr_t)Memory::physical_base);

		DoBacktrace(access_address, ctx);
		return false;
	}

	if (!IsInSpace((u8*)ctx->CTX_PC))
	{
		ERROR_LOG(DYNA_REC, "Backpatch location not within codespace 0x%016llx(0x%08x)", ctx->CTX_PC, Common::swap32(*(u32*)ctx->CTX_PC));

		DoBacktrace(access_address, ctx);
		return false;
	}

	ARM64Reg reg = INVALID_REG;
	u32 flags = 0;

	if (!DisasmLoadStore((const u8*)ctx->CTX_PC, &flags, &reg))
	{
		ERROR_LOG(DYNA_REC, "Error disassembling address 0x%016llx(0x%08x)", ctx->CTX_PC, Common::swap32(*(u32*)ctx->CTX_PC));

		DoBacktrace(access_address, ctx);
		return false;
	}

	BackPatchInfo& info = m_backpatch_info[flags];
	ARM64XEmitter emitter((u8*)(ctx->CTX_PC - info.m_fastmem_trouble_inst_offset * 4));
	u64 new_pc = (u64)emitter.GetCodePtr();

	// Slowmem routine doesn't need the address location
	// It is already in the correct location
	EmitBackpatchRoutine(&emitter, flags, false, true, reg, INVALID_REG);

	emitter.FlushIcache();
	ctx->CTX_PC = new_pc;

	// Wipe the top bits of the addr_register
	if (flags & BackPatchInfo::FLAG_STORE &&
	    !(flags & BackPatchInfo::FLAG_SIZE_F64))
		ctx->CTX_REG(1) &= 0xFFFFFFFFUll;
	else
		ctx->CTX_REG(0) &= 0xFFFFFFFFUll;
	return true;
}

void JitArm64::InitBackpatch()
{
	u32 flags = 0;
	BackPatchInfo info;
	u8* code_base = GetWritableCodePtr();
	u8* code_end;

	// Loads
	{
		// 8bit
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_8;
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 16bit - Extend
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_16 |
				BackPatchInfo::FLAG_EXTEND;
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 16bit - Reverse
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_16 |
				BackPatchInfo::FLAG_REVERSE;
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
		// 32bit - Reverse
		{
			flags =
				BackPatchInfo::FLAG_LOAD |
				BackPatchInfo::FLAG_SIZE_32 |
				BackPatchInfo::FLAG_REVERSE;
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, Q0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
	}

	// Stores
	{
		// 8bit
		{
			flags =
				BackPatchInfo::FLAG_STORE |
				BackPatchInfo::FLAG_SIZE_8;
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, W0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, W0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, Q0, X1);
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
			EmitBackpatchRoutine(this, flags, false, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_slowmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			info.m_fastmem_trouble_inst_offset =
				EmitBackpatchRoutine(this, flags, true, false, Q0, X1);
			code_end = GetWritableCodePtr();
			info.m_fastmem_size = (code_end - code_base) / 4;

			SetCodePtr(code_base);

			m_backpatch_info[flags] = info;
		}
	}
}

