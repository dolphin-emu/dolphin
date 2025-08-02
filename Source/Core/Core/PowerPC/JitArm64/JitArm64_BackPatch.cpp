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
                                    BitSet32 scratch_gprs, BitSet32 scratch_fprs,
                                    bool emitting_routine)
{
  const u32 access_size = BackPatchInfo::GetFlagSize(flags);

  if (m_accurate_cpu_cache_enabled)
    mode = MemAccessMode::AlwaysSlowAccess;

  const bool emit_fast_access = mode != MemAccessMode::AlwaysSlowAccess;
  const bool emit_slow_access = mode != MemAccessMode::AlwaysFastAccess;

  const bool memcheck = jo.memcheck && !emitting_routine;

  if ((flags & BackPatchInfo::FLAG_LOAD))
  {
    if ((flags & BackPatchInfo::FLAG_FLOAT))
      scratch_fprs[DecodeReg(RS)] = !memcheck;
    else
      scratch_gprs[DecodeReg(RS)] = !memcheck;
  }

  BitSet32 temp_gpr_candidates = scratch_gprs;
  BitSet32 temp_fpr_candidates = scratch_fprs;
  temp_gpr_candidates[DecodeReg(addr)] = false;
  if (flags & BackPatchInfo::FLAG_FLOAT)
    temp_fpr_candidates[DecodeReg(RS)] = false;
  else if (!(flags & BackPatchInfo::FLAG_ZERO_256))
    temp_gpr_candidates[DecodeReg(RS)] = false;
  if (!emitting_routine && mode == MemAccessMode::Auto && jo.fastmem)
    temp_gpr_candidates[30] = true;

  const auto allocate_temp_reg = [this](Arm64RegCache& reg_cache,
                                        BitSet32& candidates) -> Arm64RegCache::ScopedARM64Reg {
    for (int i : candidates)
    {
      candidates[i] = false;
      ARM64Reg reg = ARM64Reg(i);
      if (&reg_cache == &fpr)
        reg = EncodeRegToQuad(reg);
      return reg;
    }
    return reg_cache.GetScopedReg();
  };

  const auto can_allocate_temp_reg_for_free = [](Arm64RegCache& reg_cache, BitSet32& candidates) {
    return candidates != BitSet32{} || reg_cache.GetUnlockedRegisterCount() > 0;
  };

  Arm64RegCache::ScopedARM64Reg temp_gpr_1;
  Arm64RegCache::ScopedARM64Reg temp_gpr_2;
  Arm64RegCache::ScopedARM64Reg temp_gpr_3;
  Arm64RegCache::ScopedARM64Reg temp_fpr_1;

  if (emit_fast_access)
  {
    if ((flags & BackPatchInfo::FLAG_STORE) && (flags & BackPatchInfo::FLAG_FLOAT))
    {
      temp_fpr_1 = emitting_routine ? Arm64RegCache::ScopedARM64Reg(ARM64Reg::Q0) :
                                      allocate_temp_reg(fpr, temp_fpr_candidates);
      scratch_fprs[DecodeReg(temp_fpr_1)] = true;
    }
    else if (flags & BackPatchInfo::FLAG_STORE)
    {
      temp_gpr_1 = emitting_routine ? Arm64RegCache::ScopedARM64Reg(ARM64Reg::W1) :
                                      allocate_temp_reg(gpr, temp_gpr_candidates);
      scratch_gprs[DecodeReg(temp_gpr_1)] = true;
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      temp_gpr_1 = emitting_routine ? Arm64RegCache::ScopedARM64Reg(ARM64Reg::W30) :
                                      allocate_temp_reg(gpr, temp_gpr_candidates);
      scratch_gprs[DecodeReg(temp_gpr_1)] = true;
    }

    if (!jo.fastmem)
    {
      temp_gpr_2 = emitting_routine ? Arm64RegCache::ScopedARM64Reg(ARM64Reg::W0) :
                                      allocate_temp_reg(gpr, temp_gpr_candidates);
      temp_gpr_3 = emitting_routine ? Arm64RegCache::ScopedARM64Reg(ARM64Reg::W3) :
                                      allocate_temp_reg(gpr, temp_gpr_candidates);
      scratch_gprs[DecodeReg(temp_gpr_2)] = true;
      scratch_gprs[DecodeReg(temp_gpr_3)] = true;
    }
    else if (emit_slow_access && emitting_routine)
    {
      temp_gpr_2 = ARM64Reg::W0;
      temp_gpr_3 = flags & BackPatchInfo::FLAG_STORE ? ARM64Reg::W1 : ARM64Reg::W3;
      scratch_gprs[DecodeReg(temp_gpr_2)] = true;
      scratch_gprs[DecodeReg(temp_gpr_3)] = true;
    }
  }

  // Setting memcheck_temp_gpr to W30 works, but because W30 is a register that needs to be pushed
  // and popped, using W30 may require us to emit an extra push and pop instruction, depending on
  // what other registers need pushing and popping. If we can find another register to use without
  // having to evict anything from the register cache, let's do that instead of using W30.
  ARM64Reg memcheck_temp_gpr = ARM64Reg::W30;
  if (emit_slow_access && memcheck)
  {
    const auto is_suitable_as_memcheck_temp_gpr = [flags](ARM64Reg reg) {
      return reg != ARM64Reg::INVALID_REG && reg != ARM64Reg::W30 &&
             (reg != ARM64Reg::W0 || !(flags & BackPatchInfo::FLAG_LOAD));
    };

    const auto get_unset_temp_gpr = [&]() -> Arm64RegCache::ScopedARM64Reg& {
      if (temp_gpr_1 == ARM64Reg::INVALID_REG)
        return temp_gpr_1;
      if (temp_gpr_2 == ARM64Reg::INVALID_REG)
        return temp_gpr_2;
      ASSERT(temp_gpr_3 == ARM64Reg::INVALID_REG);
      return temp_gpr_3;
    };

    if (is_suitable_as_memcheck_temp_gpr(temp_gpr_1))
    {
      memcheck_temp_gpr = temp_gpr_1;
    }
    else if (is_suitable_as_memcheck_temp_gpr(temp_gpr_2))
    {
      memcheck_temp_gpr = temp_gpr_2;
    }
    else if (is_suitable_as_memcheck_temp_gpr(temp_gpr_3))
    {
      memcheck_temp_gpr = temp_gpr_3;
    }
    else
    {
      while (can_allocate_temp_reg_for_free(gpr, temp_gpr_candidates))
      {
        Arm64RegCache::ScopedARM64Reg& temp_gpr_x = get_unset_temp_gpr();
        temp_gpr_x = allocate_temp_reg(gpr, temp_gpr_candidates);
        scratch_gprs[DecodeReg(temp_gpr_x)] = true;
        if (is_suitable_as_memcheck_temp_gpr(temp_gpr_x))
          break;
      }
    }

    if (temp_fpr_1 == ARM64Reg::INVALID_REG &&
        can_allocate_temp_reg_for_free(fpr, temp_fpr_candidates))
    {
      temp_fpr_1 = allocate_temp_reg(fpr, temp_fpr_candidates);
      scratch_fprs[DecodeReg(temp_fpr_1)] = true;
    }
  }

  BitSet32 gprs_to_push =
      (emitting_routine ? CALLER_SAVED_GPRS : gpr.GetCallerSavedUsed()) & ~scratch_gprs;
  BitSet32 fprs_to_push =
      (emitting_routine ? BitSet32(0xFFFFFFFF) : fpr.GetCallerSavedUsed()) & ~scratch_fprs;

  if (!emitting_routine && mode == MemAccessMode::Auto && jo.fastmem)
    gprs_to_push[30] = true;

  bool in_far_code = false;
  const u8* fast_access_start = GetCodePtr();
  std::optional<FixupBranch> slow_access_fixup;

  if (emit_fast_access)
  {
    ARM64Reg memory_base = MEM_REG;
    ARM64Reg memory_offset = addr;

    if (!jo.fastmem)
    {
      memory_base = EncodeRegTo64(temp_gpr_3);
      memory_offset = temp_gpr_2;

      LSR(temp_gpr_3, addr, PowerPC::BAT_INDEX_SHIFT);
      LDR(memory_base, MEM_REG, ArithOption(temp_gpr_3, true));

      if (emit_slow_access)
      {
        FixupBranch pass = CBNZ(memory_base);
        slow_access_fixup = B();
        SetJumpTarget(pass);
      }

      AND(memory_offset, addr, LogicalImm(PowerPC::BAT_PAGE_SIZE - 1, GPRSize::B64));
    }
    else if (emit_slow_access && emitting_routine)
    {
      slow_access_fixup = CheckIfSafeAddress(addr, temp_gpr_3, temp_gpr_2);
    }

    if ((flags & BackPatchInfo::FLAG_STORE) && (flags & BackPatchInfo::FLAG_FLOAT))
    {
      ARM64Reg temp = EncodeRegToDouble(temp_fpr_1);
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
      ARM64Reg temp = temp_gpr_1;
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
      ARM64Reg temp = EncodeRegTo64(temp_gpr_1);
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
  const u8* fast_access_end = GetCodePtr();

  if (emit_slow_access)
  {
    if (emit_fast_access)
    {
      in_far_code = true;
      SwitchToFarCode();

      if (jo.fastmem && !emitting_routine)
      {
        FastmemArea* fastmem_area = &m_fault_to_handler[fast_access_end];
        fastmem_area->fast_access_code = fast_access_start;
        fastmem_area->slow_access_code = GetCodePtr();
      }
    }

    if (slow_access_fixup)
      SetJumpTarget(*slow_access_fixup);

    BitSet32 gprs_to_push_early = {};
    if (memcheck)
      gprs_to_push_early[DecodeReg(memcheck_temp_gpr)] = true;
    if (flags & BackPatchInfo::FLAG_LOAD)
      gprs_to_push_early[0] = true;

    // If we're already pushing one register in the first PushRegisters call, we can push a
    // second one for free. Let's do so, since it might save one instruction in the second
    // PushRegisters call. (Do not do this for caller-saved registers which may be in the register
    // cache, or WriteConditionalExceptionExit won't be able to flush the register cache correctly!)
    if ((gprs_to_push & gprs_to_push_early).Count() & 1)
      gprs_to_push_early[30] = true;

    // This temp GPR is only used when GPRs have been pushed, so we can choose almost any register
    ARM64Reg temp_gpr_for_function_call = ARM64Reg::W8;
    while (temp_gpr_for_function_call == addr ||
           (temp_gpr_for_function_call == RS && (flags & BackPatchInfo::FLAG_STORE)))
    {
      temp_gpr_for_function_call =
          static_cast<ARM64Reg>(static_cast<int>(temp_gpr_for_function_call) + 1);
    }

    ABI_PushRegisters(gprs_to_push & gprs_to_push_early);
    ABI_PushRegisters(gprs_to_push & ~gprs_to_push_early);
    m_float_emit.ABI_PushRegisters(fprs_to_push, EncodeRegTo64(temp_gpr_for_function_call));

    // PC is used by memory watchpoints (if enabled), profiling where to insert gather pipe
    // interrupt checks, and printing accurate PC locations in debug logs.
    //
    // In the case of JitAsm routines, we don't know the PC here,
    // so the caller has to store the PC themselves.
    if (!emitting_routine)
    {
      MOVI2R(temp_gpr_for_function_call, js.compilerPC);
      STR(IndexType::Unsigned, temp_gpr_for_function_call, PPC_REG, PPCSTATE_OFF(pc));
    }

    if (flags & BackPatchInfo::FLAG_STORE)
    {
      ARM64Reg src_reg = RS;
      const ARM64Reg dst_reg = access_size == 64 ? ARM64Reg::X1 : ARM64Reg::W1;
      ARM64Reg temp_addr_reg = addr;
      if (addr == ARM64Reg::W1)
      {
        // If addr is W1, we must move the address to a different register so we don't
        // overwrite it when moving RS to W1. W2 is the optimal register to move to,
        // because that's the register the address needs to be in for the function call.
        temp_addr_reg = RS != ARM64Reg::W2 ? ARM64Reg::W2 : temp_gpr_for_function_call;
        MOV(temp_addr_reg, addr);
      }

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

      const bool reverse = (flags & BackPatchInfo::FLAG_REVERSE) != 0;

      if (access_size == 64)
      {
        ABI_CallFunction(reverse ? &PowerPC::WriteU64SwapFromJit : &PowerPC::WriteU64FromJit,
                         &m_mmu, src_reg, temp_addr_reg);
      }
      else if (access_size == 32)
      {
        ABI_CallFunction(reverse ? &PowerPC::WriteU32SwapFromJit : &PowerPC::WriteU32FromJit,
                         &m_mmu, src_reg, temp_addr_reg);
      }
      else if (access_size == 16)
      {
        ABI_CallFunction(reverse ? &PowerPC::WriteU16SwapFromJit : &PowerPC::WriteU16FromJit,
                         &m_mmu, src_reg, temp_addr_reg);
      }
      else
      {
        ABI_CallFunction(&PowerPC::WriteU8FromJit, &m_mmu, src_reg, addr);
      }
    }
    else if (flags & BackPatchInfo::FLAG_ZERO_256)
    {
      ABI_CallFunction(&PowerPC::ClearDCacheLineFromJit, &m_mmu, addr);
    }
    else
    {
      if (access_size == 64)
        ABI_CallFunction(&PowerPC::ReadU64FromJit, &m_mmu, addr);
      else if (access_size == 32)
        ABI_CallFunction(&PowerPC::ReadU32FromJit, &m_mmu, addr);
      else if (access_size == 16)
        ABI_CallFunction(&PowerPC::ReadU16FromJit, &m_mmu, addr);
      else
        ABI_CallFunction(&PowerPC::ReadU8FromJit, &m_mmu, addr);
    }

    m_float_emit.ABI_PopRegisters(fprs_to_push, EncodeRegTo64(temp_gpr_for_function_call));
    ABI_PopRegisters(gprs_to_push & ~gprs_to_push_early);

    if (memcheck)
    {
      const u64 early_push_count = (gprs_to_push & gprs_to_push_early).Count();
      const u64 early_push_size = Common::AlignUp(early_push_count, 2) * 8;

      WriteConditionalExceptionExit(EXCEPTION_DSI, memcheck_temp_gpr, temp_fpr_1, early_push_size);
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
    if (slow_access_fixup)
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

  const u8* fastmem_area_start = slow_handler_iter->second.fast_access_code;
  const u8* fastmem_area_end = slow_handler_iter->first;

  // no overlapping fastmem area found
  if (pc < fastmem_area_start)
    return false;

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ARM64XEmitter emitter(const_cast<u8*>(fastmem_area_start), const_cast<u8*>(fastmem_area_end));

  emitter.BL(slow_handler_iter->second.slow_access_code);

  while (emitter.GetCodePtr() < fastmem_area_end)
    emitter.NOP();

  m_fault_to_handler.erase(slow_handler_iter);

  emitter.FlushIcache();

  ctx->CTX_PC = reinterpret_cast<std::uintptr_t>(fastmem_area_start);
  return true;
}
