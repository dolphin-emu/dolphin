// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <cstddef>
#include <optional>
#include <string>

#include "Common/Align.h"
#include "Common/BitSet.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitArm64/Jit_Util.h"
#include "Core/PowerPC/JitArmCommon/BackPatch.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::DoBacktrace(uintptr_t access_address, SContext* ctx)
{
  for (int i = 0; i < 30; i += 2)
  {
    ERROR_LOG_FMT(DYNA_REC, "R{}: {:#018x}\tR{}: {:#018x}", i, ctx->CTX_REG(i), i + 1,
                  ctx->CTX_REG(i + 1));
  }

  ERROR_LOG_FMT(DYNA_REC, "R30: {:#018x}\tSP: {:#018x}", ctx->CTX_LR, ctx->CTX_SP);

  ERROR_LOG_FMT(DYNA_REC, "Access Address: {:#018x}", access_address);
  ERROR_LOG_FMT(DYNA_REC, "PC: {:#018x}", ctx->CTX_PC);

  ERROR_LOG_FMT(DYNA_REC, "Memory Around PC");

  std::string pc_memory;
  for (u64 pc = (ctx->CTX_PC - 32); pc < (ctx->CTX_PC + 32); pc += 16)
  {
    pc_memory += fmt::format("{:08x}{:08x}{:08x}{:08x}", Common::swap32(*(u32*)pc),
                             Common::swap32(*(u32*)(pc + 4)), Common::swap32(*(u32*)(pc + 8)),
                             Common::swap32(*(u32*)(pc + 12)));

    ERROR_LOG_FMT(DYNA_REC, "{:#018x}: {:08x} {:08x} {:08x} {:08x}", pc, *(u32*)pc, *(u32*)(pc + 4),
                  *(u32*)(pc + 8), *(u32*)(pc + 12));
  }

  ERROR_LOG_FMT(DYNA_REC, "Full block: {}", pc_memory);
}

void JitArm64::EmitBackpatchRoutine(u32 flags, MemAccessMode mode, ARM64Reg RS, ARM64Reg addr,
                                    BitSet32 gprs_to_push, BitSet32 fprs_to_push,
                                    bool emitting_routine)
{
  const u32 access_size = BackPatchInfo::GetFlagSize(flags);

  if (m_accurate_cpu_cache_enabled)
    mode = MemAccessMode::AlwaysSafe;

  const bool emit_fastmem = mode != MemAccessMode::AlwaysSafe;
  const bool emit_slowmem = mode != MemAccessMode::AlwaysUnsafe;

  bool in_far_code = false;
  const u8* fastmem_start = GetCodePtr();
  std::optional<FixupBranch> slowmem_fixup;

  if (emit_fastmem)
  {
    ARM64Reg memory_base = MEM_REG;
    ARM64Reg memory_offset = addr;

    if (!jo.fastmem_arena)
    {
      const ARM64Reg temp = emitting_routine ? ARM64Reg::W3 : ARM64Reg::W30;

      memory_base = EncodeRegTo64(temp);
      memory_offset = ARM64Reg::W2;

      LSR(temp, addr, PowerPC::BAT_INDEX_SHIFT);
      LDR(memory_base, MEM_REG, ArithOption(temp, true));

      if (emit_slowmem)
      {
        FixupBranch pass = CBNZ(memory_base);
        slowmem_fixup = B();
        SetJumpTarget(pass);
      }

      AND(memory_offset, addr, LogicalImm(PowerPC::BAT_PAGE_SIZE - 1, 64));
    }
    else if (emit_slowmem && emitting_routine)
    {
      const ARM64Reg temp1 = flags & BackPatchInfo::FLAG_STORE ? ARM64Reg::W0 : ARM64Reg::W3;
      const ARM64Reg temp2 = ARM64Reg::W2;

      slowmem_fixup = CheckIfSafeAddress(addr, temp1, temp2);
    }

    if ((flags & BackPatchInfo::FLAG_STORE) && (flags & BackPatchInfo::FLAG_FLOAT))
    {
      ARM64Reg temp = ARM64Reg::D0;
      temp = ByteswapBeforeStore(this, &m_float_emit, temp, EncodeRegToDouble(RS), flags, true);

      m_float_emit.STR(access_size, temp, memory_base, memory_offset);
    }
    else if ((flags & BackPatchInfo::FLAG_LOAD) && (flags & BackPatchInfo::FLAG_FLOAT))
    {
      m_float_emit.LDR(access_size, EncodeRegToDouble(RS), memory_base, memory_offset);

      ByteswapAfterLoad(this, &m_float_emit, EncodeRegToDouble(RS), EncodeRegToDouble(RS), flags,
                        true, false);
    }
    else if (flags & BackPatchInfo::FLAG_STORE)
    {
      ARM64Reg temp = ARM64Reg::W0;
      temp = ByteswapBeforeStore(this, &m_float_emit, temp, RS, flags, true);

      if (flags & BackPatchInfo::FLAG_SIZE_32)
        STR(temp, memory_base, memory_offset);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        STRH(temp, memory_base, memory_offset);
      else
        STRB(temp, memory_base, memory_offset);
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      // This literally only stores 32bytes of zeros to the target address
      ARM64Reg temp = ARM64Reg::X30;
      ADD(temp, memory_base, memory_offset);
      STP(IndexType::Signed, ARM64Reg::ZR, ARM64Reg::ZR, temp, 0);
      STP(IndexType::Signed, ARM64Reg::ZR, ARM64Reg::ZR, temp, 16);
    }
    else
    {
      if (flags & BackPatchInfo::FLAG_SIZE_32)
        LDR(RS, memory_base, memory_offset);
      else if (flags & BackPatchInfo::FLAG_SIZE_16)
        LDRH(RS, memory_base, memory_offset);
      else if (flags & BackPatchInfo::FLAG_SIZE_8)
        LDRB(RS, memory_base, memory_offset);

      ByteswapAfterLoad(this, &m_float_emit, RS, RS, flags, true, false);
    }
  }
  const u8* fastmem_end = GetCodePtr();

  if (emit_slowmem)
  {
    const bool memcheck = jo.memcheck && !emitting_routine;

    if (emit_fastmem)
    {
      in_far_code = true;
      SwitchToFarCode();

      if (jo.fastmem_arena && !emitting_routine)
      {
        FastmemArea* fastmem_area = &m_fault_to_handler[fastmem_end];
        fastmem_area->fastmem_code = fastmem_start;
        fastmem_area->slowmem_code = GetCodePtr();
      }
    }

    if (slowmem_fixup)
      SetJumpTarget(*slowmem_fixup);

    const ARM64Reg temp_gpr = flags & BackPatchInfo::FLAG_LOAD ? ARM64Reg::W30 : ARM64Reg::W0;
    const int temp_gpr_index = DecodeReg(temp_gpr);

    BitSet32 gprs_to_push_early = {};
    if (memcheck)
      gprs_to_push_early[temp_gpr_index] = true;
    if (flags & BackPatchInfo::FLAG_LOAD)
      gprs_to_push_early[0] = true;

    // If we're already pushing one register in the first PushRegisters call, we can push a
    // second one for free. Let's do so, since it might save one instruction in the second
    // PushRegisters call. (Do not do this for caller-saved registers which may be in the register
    // cache, or else EmitMemcheck will not be able to flush the register cache correctly!)
    if (gprs_to_push & gprs_to_push_early)
      gprs_to_push_early[30] = true;

    ABI_PushRegisters(gprs_to_push & gprs_to_push_early);
    ABI_PushRegisters(gprs_to_push & ~gprs_to_push_early);
    m_float_emit.ABI_PushRegisters(fprs_to_push, ARM64Reg::X30);

    if (flags & BackPatchInfo::FLAG_STORE)
    {
      ARM64Reg src_reg = RS;
      const ARM64Reg dst_reg = access_size == 64 ? ARM64Reg::X0 : ARM64Reg::W0;

      if (flags & BackPatchInfo::FLAG_FLOAT)
      {
        if (access_size == 64)
          m_float_emit.FMOV(dst_reg, EncodeRegToDouble(RS));
        else
          m_float_emit.FMOV(dst_reg, EncodeRegToSingle(RS));

        src_reg = dst_reg;
      }

      if (flags & BackPatchInfo::FLAG_PAIR)
      {
        // Compensate for the Write_ functions swapping the whole write instead of each pair
        SwapPairs(this, dst_reg, src_reg, flags);
        src_reg = dst_reg;
      }

      if (dst_reg != src_reg)
        MOV(dst_reg, src_reg);

      const bool reverse = (flags & BackPatchInfo::FLAG_REVERSE) != 0;

      MOVP2R(ARM64Reg::X2, &m_mmu);

      if (access_size == 64)
      {
        MOVP2R(ARM64Reg::X8,
               reverse ? &PowerPC::WriteU64SwapFromJitArm64 : &PowerPC::WriteU64FromJitArm64);
      }
      else if (access_size == 32)
      {
        MOVP2R(ARM64Reg::X8,
               reverse ? &PowerPC::WriteU32SwapFromJitArm64 : &PowerPC::WriteU32FromJitArm64);
      }
      else if (access_size == 16)
      {
        MOVP2R(ARM64Reg::X8,
               reverse ? &PowerPC::WriteU16SwapFromJitArm64 : &PowerPC::WriteU16FromJitArm64);
      }
      else
      {
        MOVP2R(ARM64Reg::X8, &PowerPC::WriteU8FromJitArm64);
      }

      BLR(ARM64Reg::X8);
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      MOVP2R(ARM64Reg::X1, &m_mmu);
      MOVP2R(ARM64Reg::X8, &PowerPC::ClearDCacheLineFromJitArm64);
      BLR(ARM64Reg::X8);
    }
    else
    {
      MOVP2R(ARM64Reg::X1, &m_mmu);

      if (access_size == 64)
        MOVP2R(ARM64Reg::X8, &PowerPC::ReadU64FromJitArm64);
      else if (access_size == 32)
        MOVP2R(ARM64Reg::X8, &PowerPC::ReadU32FromJitArm64);
      else if (access_size == 16)
        MOVP2R(ARM64Reg::X8, &PowerPC::ReadU16FromJitArm64);
      else
        MOVP2R(ARM64Reg::X8, &PowerPC::ReadU8FromJitArm64);

      BLR(ARM64Reg::X8);
    }

    m_float_emit.ABI_PopRegisters(fprs_to_push, ARM64Reg::X30);
    ABI_PopRegisters(gprs_to_push & ~gprs_to_push_early);

    if (memcheck)
    {
      const ARM64Reg temp_fpr = fprs_to_push[0] ? ARM64Reg::INVALID_REG : ARM64Reg::Q0;
      const u64 early_push_count = (gprs_to_push & gprs_to_push_early).Count();
      const u64 early_push_size = Common::AlignUp(early_push_count, 2) * 8;

      WriteConditionalExceptionExit(EXCEPTION_DSI, temp_gpr, temp_fpr, early_push_size);
    }

    if (flags & BackPatchInfo::FLAG_LOAD)
    {
      ARM64Reg src_reg = access_size == 64 ? ARM64Reg::X0 : ARM64Reg::W0;

      if (flags & BackPatchInfo::FLAG_PAIR)
      {
        // Compensate for the Read_ functions swapping the whole read instead of each pair
        const ARM64Reg dst_reg = flags & BackPatchInfo::FLAG_FLOAT ? src_reg : RS;
        SwapPairs(this, dst_reg, src_reg, flags);
        src_reg = dst_reg;
      }

      if (flags & BackPatchInfo::FLAG_FLOAT)
      {
        if (access_size == 64)
          m_float_emit.FMOV(EncodeRegToDouble(RS), src_reg);
        else
          m_float_emit.FMOV(EncodeRegToSingle(RS), src_reg);

        src_reg = RS;
      }

      ByteswapAfterLoad(this, &m_float_emit, RS, src_reg, flags, false, false);
    }

    ABI_PopRegisters(gprs_to_push & gprs_to_push_early);
  }

  if (in_far_code)
  {
    if (slowmem_fixup)
    {
      FixupBranch done = B();
      SwitchToNearCode();
      SetJumpTarget(done);
    }
    else
    {
      RET(ARM64Reg::X30);
      SwitchToNearCode();
    }
  }
}

bool JitArm64::HandleFastmemFault(SContext* ctx)
{
  const u8* pc = reinterpret_cast<const u8*>(ctx->CTX_PC);
  auto slow_handler_iter = m_fault_to_handler.upper_bound(pc);

  // no fastmem area found
  if (slow_handler_iter == m_fault_to_handler.end())
    return false;

  const u8* fastmem_area_start = slow_handler_iter->second.fastmem_code;
  const u8* fastmem_area_end = slow_handler_iter->first;

  // no overlapping fastmem area found
  if (pc < fastmem_area_start)
    return false;

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ARM64XEmitter emitter(const_cast<u8*>(fastmem_area_start), const_cast<u8*>(fastmem_area_end));

  emitter.BL(slow_handler_iter->second.slowmem_code);

  while (emitter.GetCodePtr() < fastmem_area_end)
    emitter.NOP();

  m_fault_to_handler.erase(slow_handler_iter);

  emitter.FlushIcache();

  ctx->CTX_PC = reinterpret_cast<std::uintptr_t>(fastmem_area_start);
  return true;
}
