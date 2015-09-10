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

void JitArm64::EmitBackpatchRoutine(u32 flags, bool fastmem, bool do_farcode,
	ARM64Reg RS, ARM64Reg addr,
	BitSet32 gprs_to_push, BitSet32 fprs_to_push)
{
	bool in_far_code = false;
	const u8* fastmem_start = GetCodePtr();

	if (fastmem)
	{

		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & BackPatchInfo::FLAG_MASK_FLOAT)
		{
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				m_float_emit.FCVT(32, 64, D0, RS);
				m_float_emit.REV32(8, D0, D0);
				m_float_emit.STR(32, D0, X28, addr);
			}
			else if (flags & BackPatchInfo::FLAG_SIZE_F32I)
			{
				m_float_emit.REV32(8, D0, RS);
				m_float_emit.STR(32, D0, X28, addr);
			}
			else if (flags & BackPatchInfo::FLAG_SIZE_F32X2)
			{
				m_float_emit.FCVTN(32, D0, RS);
				m_float_emit.REV32(8, D0, D0);
				m_float_emit.STR(64, Q0, X28, addr);
			}
			else
			{
				m_float_emit.REV64(8, Q0, RS);
				m_float_emit.STR(64, Q0, X28, addr);
			}
		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
		         flags & BackPatchInfo::FLAG_MASK_FLOAT)
		{
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				m_float_emit.LDR(32, EncodeRegToDouble(RS), X28, addr);
				m_float_emit.REV32(8, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
				m_float_emit.FCVT(64, 32, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
			}
			else
			{
				m_float_emit.LDR(64, EncodeRegToDouble(RS), X28, addr);
				m_float_emit.REV64(8, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
			}
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			ARM64Reg temp = W0;
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				REV32(temp, RS);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				REV16(temp, RS);

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				STR(temp, X28, addr);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				STRH(temp, X28, addr);
			else
				STRB(RS, X28, addr);
		}
		else if (flags & BackPatchInfo::FLAG_ZERO_256)
		{
			// This literally only stores 32bytes of zeros to the target address
			ADD(addr, addr, X28);
			STP(INDEX_SIGNED, ZR, ZR, addr, 0);
			STP(INDEX_SIGNED, ZR, ZR, addr, 16);
		}
		else
		{
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				LDR(RS, X28, addr);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				LDRH(RS, X28, addr);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				LDRB(RS, X28, addr);

			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					REV32(RS, RS);
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					REV16(RS, RS);
			}

			if (flags & BackPatchInfo::FLAG_EXTEND)
				SXTH(RS, RS);
		}
	}
	const u8* fastmem_end = GetCodePtr();

	if (!fastmem || do_farcode)
	{
		if (fastmem && do_farcode)
		{
			SlowmemHandler handler;
			handler.dest_reg = RS;
			handler.addr_reg = addr;
			handler.gprs = gprs_to_push;
			handler.fprs = fprs_to_push;
			handler.flags = flags;

			FastmemArea* fastmem_area = &m_fault_to_handler[fastmem_start];
			auto handler_loc_iter = m_handler_to_loc.find(handler);

			if (handler_loc_iter == m_handler_to_loc.end())
			{
				in_far_code = true;
				SwitchToFarCode();
				const u8* handler_loc = GetCodePtr();
				m_handler_to_loc[handler] = handler_loc;
				fastmem_area->slowmem_code = handler_loc;
				fastmem_area->length = fastmem_end - fastmem_start;
			}
			else
			{
				const u8* handler_loc = handler_loc_iter->second;
				fastmem_area->slowmem_code = handler_loc;
				fastmem_area->length = fastmem_end - fastmem_start;
				return;
			}
		}

		ABI_PushRegisters(gprs_to_push);
		m_float_emit.ABI_PushRegisters(fprs_to_push, X30);

		if (flags & BackPatchInfo::FLAG_STORE &&
		    flags & BackPatchInfo::FLAG_MASK_FLOAT)
		{
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				m_float_emit.FCVT(32, 64, D0, RS);
				m_float_emit.UMOV(32, W0, Q0, 0);
				MOVI2R(X30, (u64)&PowerPC::Write_U32);
				BLR(X30);
			}
			else if (flags & BackPatchInfo::FLAG_SIZE_F32I)
			{
				m_float_emit.UMOV(32, W0, RS, 0);
				MOVI2R(X30, (u64)&PowerPC::Write_U32);
				BLR(X30);
			}
			else if (flags & BackPatchInfo::FLAG_SIZE_F32X2)
			{
				m_float_emit.FCVTN(32, D0, RS);
				m_float_emit.UMOV(64, X0, D0, 0);
				ORR(X0, SP, X0, ArithOption(X0, ST_ROR, 32));
				MOVI2R(X30, (u64)PowerPC::Write_U64);
				BLR(X30);
			}
			else
			{
				MOVI2R(X30, (u64)&PowerPC::Write_U64);
				m_float_emit.UMOV(64, X0, RS, 0);
				BLR(X30);
			}

		}
		else if (flags & BackPatchInfo::FLAG_LOAD &&
			   flags & BackPatchInfo::FLAG_MASK_FLOAT)
		{
			if (flags & BackPatchInfo::FLAG_SIZE_F32)
			{
				MOVI2R(X30, (u64)&PowerPC::Read_U32);
				BLR(X30);
				m_float_emit.INS(32, RS, 0, X0);
				m_float_emit.FCVT(64, 32, EncodeRegToDouble(RS), EncodeRegToDouble(RS));
			}
			else
			{
				MOVI2R(X30, (u64)&PowerPC::Read_F64);
				BLR(X30);
				m_float_emit.INS(64, RS, 0, X0);
			}
		}
		else if (flags & BackPatchInfo::FLAG_STORE)
		{
			MOV(W0, RS);

			if (flags & BackPatchInfo::FLAG_SIZE_32)
				MOVI2R(X30, (u64)&PowerPC::Write_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				MOVI2R(X30, (u64)&PowerPC::Write_U16);
			else
				MOVI2R(X30, (u64)&PowerPC::Write_U8);

			BLR(X30);
		}
		else if (flags & BackPatchInfo::FLAG_ZERO_256)
		{
			MOVI2R(X30, (u64)&PowerPC::ClearCacheLine);
			BLR(X30);
		}
		else
		{
			if (flags & BackPatchInfo::FLAG_SIZE_32)
				MOVI2R(X30, (u64)&PowerPC::Read_U32);
			else if (flags & BackPatchInfo::FLAG_SIZE_16)
				MOVI2R(X30, (u64)&PowerPC::Read_U16);
			else if (flags & BackPatchInfo::FLAG_SIZE_8)
				MOVI2R(X30, (u64)&PowerPC::Read_U8);

			BLR(X30);

			if (!(flags & BackPatchInfo::FLAG_REVERSE))
			{
				MOV(RS, W0);
			}
			else
			{
				if (flags & BackPatchInfo::FLAG_SIZE_32)
					REV32(RS, W0);
				else if (flags & BackPatchInfo::FLAG_SIZE_16)
					REV16(RS, W0);
			}

			if (flags & BackPatchInfo::FLAG_EXTEND)
				SXTH(RS, RS);
		}

		m_float_emit.ABI_PopRegisters(fprs_to_push, X30);
		ABI_PopRegisters(gprs_to_push);
	}

	if (in_far_code)
	{
		RET(X30);
		SwitchToNearCode();
	}
}

bool JitArm64::HandleFault(uintptr_t access_address, SContext* ctx)
{
	if (!IsInSpace((u8*)ctx->CTX_PC))
	{
		ERROR_LOG(DYNA_REC, "Backpatch location not within codespace 0x%016llx(0x%08x)", ctx->CTX_PC, Common::swap32(*(u32*)ctx->CTX_PC));

		DoBacktrace(access_address, ctx);
		return false;
	}

	if (!(access_address >= (uintptr_t)Memory::physical_base && access_address < (uintptr_t)Memory::physical_base + 0x100010000) &&
		!(access_address >= (uintptr_t)Memory::logical_base && access_address < (uintptr_t)Memory::logical_base + 0x100010000))
	{
		ERROR_LOG(DYNA_REC, "Exception handler - access below memory space. PC: 0x%016llx 0x%016lx < 0x%016lx", ctx->CTX_PC, access_address, (uintptr_t)Memory::physical_base);

		DoBacktrace(access_address, ctx);
		return false;
	}

	auto slow_handler_iter = m_fault_to_handler.upper_bound((const u8*)ctx->CTX_PC);
	slow_handler_iter--;

	// no fastmem area found
	if (slow_handler_iter == m_fault_to_handler.end())
		return false;

	// no overlapping fastmem area found
	if ((const u8*)ctx->CTX_PC - slow_handler_iter->first > slow_handler_iter->second.length)
		return false;

	ARM64XEmitter emitter((u8*) slow_handler_iter->first);

	emitter.BL(slow_handler_iter->second.slowmem_code);

	u32 num_insts_max = slow_handler_iter->second.length / 4 - 1;
	for (u32 i = 0; i < num_insts_max; ++i)
		emitter.HINT(HINT_NOP);

	m_fault_to_handler.erase(slow_handler_iter);

	emitter.FlushIcache();
	ctx->CTX_PC = (u64)slow_handler_iter->first;
	return true;
}
