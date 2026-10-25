// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Integer loads and stores.
//
// With fastmem, an access is a single byte-reversed load or store (the guest is big-endian, the
// host little-endian) relative to MEM_REG, the base of the fastmem arena for the current MSR[DR].
// Addresses that aren't mapped in the arena, such as MMIO, fault. The fault handler then patches
// the access into a branch to its slow path in far code, which calls the MMU and branches back.
// Without fastmem, accesses always take the slow path, inline.

#include "Core/PowerPC/JitPPC64/Jit.h"

#include <array>
#include <cstddef>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/PPC64Emitter.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace PPC64Gen;

void JitPPC64::ComputeAddress(GPR addr, UGeckoInstruction inst, bool indexed)
{
  // EA = (rA|0) + d or (rA|0) + rB, zero-extended to 64 bits for indexing the arena.
  if (indexed)
  {
    if (inst.RA == 0)
    {
      CLRLDI(addr, gpr.R(inst.RB), 32);
      return;
    }
    ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  }
  else
  {
    if (inst.RA == 0)
    {
      MOVI2R(addr, static_cast<u32>(static_cast<s32>(inst.SIMM_16)));
      return;
    }
    if (inst.SIMM_16 == 0)
    {
      CLRLDI(addr, gpr.R(inst.RA), 32);
      return;
    }
    ADDI(addr, gpr.R(inst.RA), inst.SIMM_16);
  }
  CLRLDI(addr, addr, 32);
}

BitSet32 JitPPC64::SaveScratchRegs(BitSet32 except)
{
  // Scratch registers are volatile, so save those in use around a call; guest registers are
  // non-volatile. No scratch VSX registers may be in use.
  ASSERT(!fpr.AnyScratchRegsInUse());
  const BitSet32 saved = gpr.GetScratchRegsInUse() & ~except;
  for (const int i : saved)
    STD(static_cast<GPR>(i), GPR::SP, ScratchSaveOffset(static_cast<GPR>(i)));
  return saved;
}

void JitPPC64::RestoreScratchRegs(BitSet32 saved)
{
  for (const int i : saved)
    LD(static_cast<GPR>(i), GPR::SP, ScratchSaveOffset(static_cast<GPR>(i)));
}

namespace
{
constexpr u32 GATHER_PIPE_ADDRESS = 0xCC008000;
}

bool JitPPC64::IsGatherPipeStore() const
{
  return js.fifoWriteAddresses.TestBit(js.compilerPC / sizeof(UGeckoInstruction)) &&
         m_mmu.IsOptimizableGatherPipeWrite(GATHER_PIPE_ADDRESS);
}

void JitPPC64::EmitLoadToVSR(u32 size, VSR raw, GPR value, GPR addr)
{
  ASSERT(size == 32 || size == 64);
  const auto move = [&] {
    if (size == 64)
    {
      MTVSRD(raw, value);
    }
    else if (m_isa_3_0)
    {
      MTVSRWS(raw, value);
    }
    else
    {
      SLDI(value, value, 32);
      MTVSRD(raw, value);
    }
  };
  if (!m_isa_3_0 || !jo.fastmem || m_accurate_cpu_cache_enabled)
  {
    EmitMemoryAccess({.size = size}, value, addr);
    move();
    return;
  }

  // The VSX load (into doubleword 0, or every word) is the access that faults.
  const u8* fault_pc = GetCodePtr();
  if (size == 64)
  {
    LXSDX(raw, MEM_REG, addr);
    XXBRD(raw, raw);
  }
  else
  {
    LXVWSX(raw, MEM_REG, addr);
    XXBRW(raw, raw);
  }
  const u8* fast_end = GetCodePtr();

  SwitchToFarCode();
  m_fault_to_handler[fault_pc] = GetCodePtr();
  EmitSlowAccess({.size = size}, value, addr);
  move();
  B(fast_end);
  SwitchToNearCode();
}

void JitPPC64::EmitStoreFromVSR(u32 size, VSR src, GPR value, GPR addr)
{
  ASSERT(size == 32 || size == 64);
  if (!m_isa_3_0 || !jo.fastmem || m_accurate_cpu_cache_enabled || IsGatherPipeStore())
  {
    MFVSRD(value, src);
    EmitMemoryAccess({.store = true, .size = size}, value, addr);
    return;
  }

  // The byte-reversed value is in VS0, which isn't allocated. The store is the access that faults.
  if (size == 64)
    XXBRD(VSR::VS0, src);
  else
    XXBRW(VSR::VS0, src);
  const u8* fault_pc = GetCodePtr();
  if (size == 64)
    STXSDX(VSR::VS0, MEM_REG, addr);
  else
    STXSIWX(VSR::VS0, MEM_REG, addr);
  const u8* fast_end = GetCodePtr();

  SwitchToFarCode();
  m_fault_to_handler[fault_pc] = GetCodePtr();
  MFVSRD(value, src);
  EmitSlowAccess({.store = true, .size = size}, value, addr);
  B(fast_end);
  SwitchToNearCode();
}

void JitPPC64::EmitMemoryAccess(MemAccess access, GPR value, GPR addr)
{
  // Stores that wrote to the gather pipe before, through the slow path, write to it directly when
  // they're to its address, which fastmem can't do. Like GPFifo's writes, they burst the pipe as
  // soon as it's full.
  if (access.store && IsGatherPipeStore())
  {
    const auto ptr = gpr.GetScopedReg();
    MOVI2R(ptr, GATHER_PIPE_ADDRESS);
    CMPLW(CRF::CR1, addr, ptr);
    const FixupBranch not_gather_pipe = BC(CCFlags::NE, CRF::CR1);

    LD(ptr, PPC_REG, PPCSTATE_OFF(gather_pipe_ptr));
    switch (access.size)
    {
    case 64:
      STDBRX(value, GPR::R0, ptr);
      break;
    case 32:
      access.reverse ? STWX(value, GPR::R0, ptr) : STWBRX(value, GPR::R0, ptr);
      break;
    case 16:
      access.reverse ? STHX(value, GPR::R0, ptr) : STHBRX(value, GPR::R0, ptr);
      break;
    default:
      STBX(value, GPR::R0, ptr);
      break;
    }
    ADDI(ptr, ptr, static_cast<s16>(access.size / 8));
    STD(ptr, PPC_REG, PPCSTATE_OFF(gather_pipe_ptr));
    {
      const auto base = gpr.GetScopedReg();
      LD(base, PPC_REG, PPCSTATE_OFF(gather_pipe_base_ptr));
      SUBF(ptr, base, ptr);
    }
    CMPLDI(CRF::CR1, ptr, GPFifo::GATHER_PIPE_SIZE);
    const FixupBranch full = BCFar(CCFlags::GE, CRF::CR1);
    const u8* written = GetCodePtr();

    SwitchToFarCode();
    SetJumpTarget(full);
    {
      const BitSet32 saved = SaveScratchRegs();
      ABI_CallFunction(&GPFifo::UpdateGatherPipe, &m_system.GetGPFifo());
      RestoreScratchRegs(saved);
    }
    B(written);
    SwitchToNearCode();

    const FixupBranch done = B();
    SetJumpTarget(not_gather_pipe);
    EmitMemoryAccessToAddress(access, value, addr);
    SetJumpTarget(done);
    return;
  }

  EmitMemoryAccessToAddress(access, value, addr);
}

void JitPPC64::EmitMemoryAccessToAddress(MemAccess access, GPR value, GPR addr)
{
  if (!jo.fastmem || m_accurate_cpu_cache_enabled)
  {
    EmitSlowAccess(access, value, addr);
    return;
  }

  const u8* fault_pc = GetCodePtr();
  EmitFastAccess(access, value, addr);
  const u8* fast_end = GetCodePtr();

  SwitchToFarCode();
  m_fault_to_handler[fault_pc] = GetCodePtr();
  EmitSlowAccess(access, value, addr);
  B(fast_end);
  SwitchToNearCode();
}

void JitPPC64::EmitFastAccess(MemAccess access, GPR value, GPR addr, GPR base)
{
  // The first instruction is the one that faults, and is patched.
  if (access.store)
  {
    switch (access.size)
    {
    case 64:
      STDBRX(value, base, addr);
      break;
    case 32:
      access.reverse ? STWX(value, base, addr) : STWBRX(value, base, addr);
      break;
    case 16:
      access.reverse ? STHX(value, base, addr) : STHBRX(value, base, addr);
      break;
    default:
      STBX(value, base, addr);
      break;
    }
    return;
  }

  switch (access.size)
  {
  case 64:
    LDBRX(value, base, addr);
    break;
  case 32:
    access.reverse ? LWZX(value, base, addr) : LWBRX(value, base, addr);
    break;
  case 16:
    access.reverse ? LHZX(value, base, addr) : LHBRX(value, base, addr);
    if (access.sign_extend)
      EXTSH(value, value);
    break;
  default:
    LBZX(value, base, addr);
    break;
  }
}

u32 JitPPC64::SlowAccessRoutineIndex(MemAccess access)
{
  const u32 size_index = access.size == 64 ? 3 : access.size == 32 ? 2 : access.size == 16 ? 1 : 0;
  return (access.store ? 16 : 0) | (size_index << 2) | (access.sign_extend ? 2 : 0) |
         (access.reverse ? 1 : 0);
}

void JitPPC64::GenerateSlowAccessRoutines()
{
  // The slow path of each kind of access, shared by the accesses, which keeps far code small. They
  // take the address in R3, zero-extended, and a stored value in R4, zero-extended to its size,
  // return a loaded value in R3, and preserve the scratch registers.
  //
  // With the MMU, they first translate the address themselves when they can. Memory checks and the
  // accurate CPU cache need the MMU's accesses; changing those clears the cache, which generates
  // the routines again.
  const bool translate_inline = m_system.IsMMUMode() && !m_accurate_cpu_cache_enabled &&
                                !m_system.GetPowerPC().GetMemChecks().HasAny();
  m_slow_access_routines = {};
  for (const bool store : {false, true})
  {
    for (const u32 size : {8u, 16u, 32u, 64u})
    {
      for (const bool sign_extend : {false, true})
      {
        for (const bool reverse : {false, true})
        {
          if ((sign_extend && (store || size != 16)) || (reverse && (size == 8 || size == 64)) ||
              (sign_extend && reverse))
          {
            continue;
          }
          const MemAccess access{store, size, sign_extend, reverse};
          m_slow_access_routines[SlowAccessRoutineIndex(access)] = GetCodePtr();
          GenerateSlowAccessRoutine(access, translate_inline);
        }
      }
    }
  }
}

void JitPPC64::EmitTranslatedRAMAccess(MemAccess access)
{
  // Accesses through the page table can't use fastmem when the host's pages are larger than the
  // guest's 4 KiB, which leaves them to the MMU. Translate the address instead, by MemoryManager's
  // page translations, or as TranslateAddress does, by a DBAT or the data TLB, and access RAM
  // directly. Anything else falls through to the MMU, which repeats the lookup: TLB misses, stores
  // that set the C bit, page crossings, real mode, MMIO, and the stores to uncached memory that
  // WriteToHardware handles specially.
  // Uses R0 and R12, and R5 and R6, which it saves.
  const u32 size = access.size / 8;
  auto& memory = m_system.GetMemory();
  ASSERT(memory.GetRAM() != nullptr);
  // Misses before R5 and R6 are saved, and after.
  std::vector<FixupBranch> early_misses;
  std::vector<FixupBranch> misses;

  if (size > 1)
  {
    CLRLWI(GPR::R0, GPR::R3, 20);
    CMPLWI(CRF::CR0, GPR::R0, PowerPC::HW_PAGE_SIZE - size);
    early_misses.push_back(BC(CCFlags::GT, CRF::CR0));
  }
  LWZ(GPR::R12, PPC_REG, PPCSTATE_OFF(msr));
  ANDI_(GPR::R12, GPR::R12, 1 << UReg_MSR{}.DR.StartBit());
  early_misses.push_back(BC(CCFlags::EQ, CRF::CR0));

  // Most pages are in the page table translations that MemoryManager keeps with fastmem, for the
  // pages it can't map, with 64 KiB host pages, and the accesses that faulted before the page was
  // mapped. They leave out BATs, memory checks, uncached pages and pages not in RAM, and pages
  // whose R bit, or C bit for stores, isn't set yet, which the TLB lookup below takes care of.
  if (const u32* translations = memory.GetPageTranslations())
  {
    MOVP2R(GPR::R12, translations);
    RLWINM(GPR::R0, GPR::R3, 32 - PowerPC::HW_PAGE_INDEX_SHIFT + 2,
           PowerPC::HW_PAGE_INDEX_SHIFT - 2, 29);
    LWZX(GPR::R12, GPR::R12, GPR::R0);
    ANDI_(GPR::R0, GPR::R12,
          access.store ? Memory::MemoryManager::PAGE_TRANSLATION_WRITEABLE :
                         Memory::MemoryManager::PAGE_TRANSLATION_READABLE);
    const FixupBranch not_translated = BC(CCFlags::EQ, CRF::CR0);
    // The offset into RAM.
    RLWIMI(GPR::R12, GPR::R3, 0, 32 - PowerPC::HW_PAGE_INDEX_SHIFT, 31);
    MOVP2R(GPR::R0, memory.GetRAM());
    EmitFastAccess(access, access.store ? GPR::R4 : GPR::R3, GPR::R0, GPR::R12);
    BLR();
    SetJumpTarget(not_translated);
  }

  STD(GPR::R5, GPR::SP, ScratchSaveOffset(GPR::R5));
  STD(GPR::R6, GPR::SP, ScratchSaveOffset(GPR::R6));

  // In the end, R12 holds the physical address, and R5 is nonzero when it's uncached.
  MOVP2R(GPR::R12, m_mmu.GetDBATTable().data());
  RLWINM(GPR::R0, GPR::R3, 32 - PowerPC::BAT_INDEX_SHIFT + 2, PowerPC::BAT_INDEX_SHIFT - 2, 29);
  LWZX(GPR::R12, GPR::R12, GPR::R0);
  ANDI_(GPR::R0, GPR::R12, PowerPC::BAT_MAPPED_BIT);
  const FixupBranch no_bat = BC(CCFlags::EQ, CRF::CR0);
  ANDI_(GPR::R5, GPR::R12, PowerPC::BAT_WI_BIT);
  RLWIMI(GPR::R12, GPR::R3, 0, 32 - PowerPC::BAT_INDEX_SHIFT, 31);
  const FixupBranch bat_translated = B();

  // As LookupTLBPageAddress: the entry for the page, with its tag and the segment's VSID in either
  // way.
  SetJumpTarget(no_bat);
  RLWINM(GPR::R0, GPR::R3, 4 + 2, 26, 29);
  ADD(GPR::R12, PPC_REG, GPR::R0);
  LWZ(GPR::R12, GPR::R12, PPCSTATE_OFF(sr));
  CLRLWI(GPR::R12, GPR::R12, 32 - 24);
  RLWINM(GPR::R6, GPR::R3, 32 - PowerPC::HW_PAGE_INDEX_SHIFT, 32 - 6, 31);
  static_assert(PowerPC::HW_PAGE_INDEX_MASK == 0x3f);
  MULLI(GPR::R6, GPR::R6, sizeof(PowerPC::TLBEntry));
  ADD(GPR::R6, GPR::R6, PPC_REG);
  SRWI(GPR::R5, GPR::R3, PowerPC::HW_PAGE_INDEX_SHIFT);

  const s16 entry =
      PPCSTATE_OFF(tlb) + sizeof(PowerPC::PowerPCState::tlb[0]) * PowerPC::DATA_TLB_INDEX;
  const auto field = [&](size_t offset, u32 way) {
    return static_cast<s16>(entry + offset + sizeof(u32) * way);
  };
  std::array<FixupBranch, 2> hits;
  for (u32 way = 0; way < 2; way++)
  {
    LWZ(GPR::R0, GPR::R6, field(offsetof(PowerPC::TLBEntry, tag), way));
    CMPLW(CRF::CR0, GPR::R0, GPR::R5);
    LWZ(GPR::R0, GPR::R6, field(offsetof(PowerPC::TLBEntry, vsid), way));
    CMPLW(CRF::CR1, GPR::R0, GPR::R12);
    CRAND(CRBit(CRF::CR0, CCFlags::EQ), CRBit(CRF::CR0, CCFlags::EQ), CRBit(CRF::CR1, CCFlags::EQ));
    hits[way] = BC(CCFlags::EQ, CRF::CR0);
  }
  misses.push_back(B());

  std::array<FixupBranch, 2> tlb_translated;
  for (u32 way = 0; way < 2; way++)
  {
    SetJumpTarget(hits[way]);
    LWZ(GPR::R12, GPR::R6, field(offsetof(PowerPC::TLBEntry, pte), way));
    // A store to a page whose C bit isn't set yet sets it in the page table.
    if (access.store)
    {
      ANDI_(GPR::R0, GPR::R12, 1 << UPTE_Hi{}.C.StartBit());
      misses.push_back(BC(CCFlags::EQ, CRF::CR0));
    }
    LI(GPR::R0, way);
    STW(GPR::R0, GPR::R6, field(offsetof(PowerPC::TLBEntry, recent), 0));
    ANDI_(GPR::R5, GPR::R12, 0b1100 << UPTE_Hi{}.WIMG.StartBit());
    LWZ(GPR::R12, GPR::R6, field(offsetof(PowerPC::TLBEntry, paddr), way));
    RLWIMI(GPR::R12, GPR::R3, 0, 32 - PowerPC::HW_PAGE_INDEX_SHIFT, 31);
    tlb_translated[way] = B();
  }

  SetJumpTarget(bat_translated);
  for (const FixupBranch& branch : tlb_translated)
    SetJumpTarget(branch);
  // RAM, which is at physical addresses below 0x08000000, mirrored.
  RLWINM(GPR::R0, GPR::R12, 0, 0, 4, true);
  misses.push_back(BC(CCFlags::NE, CRF::CR0));
  if (access.store)
  {
    // Uncached stores of less than a word, or not word-aligned, write differently.
    CMPLWI(CRF::CR1, GPR::R5, 0);
    if (size < 4)
    {
      misses.push_back(BC(CCFlags::NE, CRF::CR1));
    }
    else
    {
      ANDI_(GPR::R0, GPR::R3, 3);
      CROR(CRBit(CRF::CR0, CCFlags::EQ), CRBit(CRF::CR0, CCFlags::EQ),
           CRBit(CRF::CR1, CCFlags::EQ));
      misses.push_back(BC(CCFlags::NE, CRF::CR0));
    }
  }
  MOVI2R(GPR::R0, memory.GetRamMask());
  AND(GPR::R12, GPR::R12, GPR::R0);
  MOVP2R(GPR::R0, memory.GetRAM());
  ADD(GPR::R12, GPR::R12, GPR::R0);
  LD(GPR::R5, GPR::SP, ScratchSaveOffset(GPR::R5));
  LD(GPR::R6, GPR::SP, ScratchSaveOffset(GPR::R6));
  // R0 as the base is zero.
  EmitFastAccess(access, access.store ? GPR::R4 : GPR::R3, GPR::R12, GPR::R0);
  BLR();

  for (const FixupBranch& branch : misses)
    SetJumpTarget(branch);
  LD(GPR::R5, GPR::SP, ScratchSaveOffset(GPR::R5));
  LD(GPR::R6, GPR::SP, ScratchSaveOffset(GPR::R6));
  for (const FixupBranch& branch : early_misses)
    SetJumpTarget(branch);
}

void JitPPC64::GenerateSlowAccessRoutine(MemAccess access, bool translate_inline)
{
  if (translate_inline)
    EmitTranslatedRAMAccess(access);

  MFLR(GPR::R0);
  STD(GPR::R0, GPR::SP, ROUTINE_LR_SAVE);
  for (u32 i = 5; i <= 11; i++)
    STD(static_cast<GPR>(i), GPR::SP, ScratchSaveOffset(static_cast<GPR>(i)));

  if (access.store)
  {
    MR(GPR::R5, GPR::R3);
    switch (access.size)
    {
    case 64:
      ABI_CallFunction(&PowerPC::WriteFromJit<u64>, &m_mmu, GPR::R4, GPR::R5);
      break;
    case 32:
      // Power10 reverses the bytes itself.
      if (access.reverse && m_isa_3_1)
        BRW(GPR::R4, GPR::R4);
      ABI_CallFunction(access.reverse && !m_isa_3_1 ? &PowerPC::WriteU32SwapFromJit :
                                                      &PowerPC::WriteFromJit<u32>,
                       &m_mmu, GPR::R4, GPR::R5);
      break;
    case 16:
      // Write_U16_Swap keeps the upper halfword, which uncached stores write, so brh can't do it.
      ABI_CallFunction(access.reverse ? &PowerPC::WriteU16SwapFromJit : &PowerPC::WriteFromJit<u16>,
                       &m_mmu, GPR::R4, GPR::R5);
      break;
    default:
      ABI_CallFunction(&PowerPC::WriteFromJit<u8>, &m_mmu, GPR::R4, GPR::R5);
      break;
    }
  }
  else
  {
    MR(GPR::R4, GPR::R3);
    switch (access.size)
    {
    case 64:
      ABI_CallFunction(&PowerPC::ReadFromJit<u64>, &m_mmu, GPR::R4);
      break;
    case 32:
      ABI_CallFunction(&PowerPC::ReadFromJit<u32>, &m_mmu, GPR::R4);
      if (access.reverse && m_isa_3_1)
      {
        BRW(GPR::R3, GPR::R3);
      }
      else if (access.reverse)
      {
        ROTLWI(GPR::R4, GPR::R3, 8);
        RLWIMI(GPR::R4, GPR::R3, 24, 0, 7);
        RLWIMI(GPR::R4, GPR::R3, 24, 16, 23);
        MR(GPR::R3, GPR::R4);
      }
      break;
    case 16:
      ABI_CallFunction(&PowerPC::ReadFromJit<u16>, &m_mmu, GPR::R4);
      if (access.reverse && m_isa_3_1)
      {
        BRH(GPR::R3, GPR::R3);
      }
      else if (access.reverse)
      {
        RLWINM(GPR::R4, GPR::R3, 24, 24, 31);
        RLWIMI(GPR::R4, GPR::R3, 8, 16, 23);
        MR(GPR::R3, GPR::R4);
      }
      else if (access.sign_extend)
      {
        EXTSH(GPR::R3, GPR::R3);
      }
      break;
    default:
      ABI_CallFunction(&PowerPC::ReadFromJit<u8>, &m_mmu, GPR::R4);
      break;
    }
  }

  for (u32 i = 5; i <= 11; i++)
    LD(static_cast<GPR>(i), GPR::SP, ScratchSaveOffset(static_cast<GPR>(i)));
  LD(GPR::R0, GPR::SP, ROUTINE_LR_SAVE);
  MTLR(GPR::R0);
  BLR();
}

void JitPPC64::EmitSlowAccess(MemAccess access, GPR value, GPR addr)
{
  // The routine saves the scratch registers, which are volatile; guest registers are
  // non-volatile. No scratch VSX registers may be in use.
  ASSERT(!fpr.AnyScratchRegsInUse());

  // The PC is used by memory checks, by GPFifo to find stores to the gather pipe, and in logs.
  MOVI2R(GPR::R12, js.compilerPC);
  STW(GPR::R12, PPC_REG, PPCSTATE_OFF(pc));

  CLRLDI(GPR::R3, addr, 32);
  if (access.store)
  {
    if (access.size == 64)
      MR(GPR::R4, value);
    else
      CLRLDI(GPR::R4, value, 32);
  }
  BL(Routine(m_slow_access_routines[SlowAccessRoutineIndex(access)]));
  // With memory checks, an access the MMU can't translate raises a DSI exception, which is taken
  // before the instruction writes any register: instructions bind the registers they write after
  // their access.
  if (jo.memcheck)
    WriteConditionalExceptionExit(EXCEPTION_DSI, GPR::R12);
  if (!access.store)
    MR(value, GPR::R3);
}

bool JitPPC64::HandleFastmemFault(SContext* ctx)
{
  const u8* pc = reinterpret_cast<const u8*>(ctx->CTX_PC);
  const auto handler = m_fault_to_handler.find(pc);
  if (handler == m_fault_to_handler.end())
    return false;

  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    PPC64XEmitter emitter(const_cast<u8*>(pc), const_cast<u8*>(pc) + sizeof(u32));
    emitter.B(handler->second);
    emitter.FlushIcache();
  }
  m_fault_to_handler.erase(handler);

  // Run the slow path now, from the patched branch.
  return true;
}

void JitPPC64::lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  MemAccess access{};
  bool indexed = false;
  bool update = false;
  switch (inst.OPCD)
  {
  case 31:
    indexed = true;
    switch (inst.SUBOP10)
    {
    case 55:  // lwzux
      update = true;
      [[fallthrough]];
    case 23:  // lwzx
      access.size = 32;
      break;
    case 119:  // lbzux
      update = true;
      [[fallthrough]];
    case 87:  // lbzx
      access.size = 8;
      break;
    case 311:  // lhzux
      update = true;
      [[fallthrough]];
    case 279:  // lhzx
      access.size = 16;
      break;
    case 375:  // lhaux
      update = true;
      [[fallthrough]];
    case 343:  // lhax
      access.size = 16;
      access.sign_extend = true;
      break;
    case 534:  // lwbrx
      access.size = 32;
      access.reverse = true;
      break;
    case 790:  // lhbrx
      access.size = 16;
      access.reverse = true;
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unexpected load {:#010x}", inst.hex);
      break;
    }
    break;
  case 33:  // lwzu
    update = true;
    [[fallthrough]];
  case 32:  // lwz
    access.size = 32;
    break;
  case 35:  // lbzu
    update = true;
    [[fallthrough]];
  case 34:  // lbz
    access.size = 8;
    break;
  case 41:  // lhzu
    update = true;
    [[fallthrough]];
  case 40:  // lhz
    access.size = 16;
    break;
  case 43:  // lhau
    update = true;
    [[fallthrough]];
  case 42:  // lha
    access.size = 16;
    access.sign_extend = true;
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Unexpected load {:#010x}", inst.hex);
    break;
  }

  // The invalid forms are left to the interpreter.
  FALLBACK_IF(update && (inst.RA == 0 || inst.RA == inst.RD));

  const auto addr = gpr.GetScopedReg();
  ComputeAddress(addr, inst, indexed);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  if (jo.memcheck)
  {
    const auto value = gpr.GetScopedReg();
    EmitMemoryAccess(access, value, addr);
    MR(gpr.RW(inst.RD), value);
  }
  else
  {
    EmitMemoryAccess(access, gpr.RW(inst.RD), addr);
  }

  if (update)
    MR(a, addr);
}

void JitPPC64::stX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  MemAccess access{.store = true};
  bool indexed = false;
  bool update = false;
  switch (inst.OPCD)
  {
  case 31:
    indexed = true;
    switch (inst.SUBOP10)
    {
    case 183:  // stwux
      update = true;
      [[fallthrough]];
    case 151:  // stwx
      access.size = 32;
      break;
    case 247:  // stbux
      update = true;
      [[fallthrough]];
    case 215:  // stbx
      access.size = 8;
      break;
    case 439:  // sthux
      update = true;
      [[fallthrough]];
    case 407:  // sthx
      access.size = 16;
      break;
    case 662:  // stwbrx
      access.size = 32;
      access.reverse = true;
      break;
    case 918:  // sthbrx
      access.size = 16;
      access.reverse = true;
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unexpected store {:#010x}", inst.hex);
      break;
    }
    break;
  case 37:  // stwu
    update = true;
    [[fallthrough]];
  case 36:  // stw
    access.size = 32;
    break;
  case 39:  // stbu
    update = true;
    [[fallthrough]];
  case 38:  // stb
    access.size = 8;
    break;
  case 45:  // sthu
    update = true;
    [[fallthrough]];
  case 44:  // sth
    access.size = 16;
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Unexpected store {:#010x}", inst.hex);
    break;
  }

  // The invalid form is left to the interpreter.
  FALLBACK_IF(update && inst.RA == 0);

  const auto addr = gpr.GetScopedReg();
  ComputeAddress(addr, inst, indexed);
  const GPR s = gpr.R(inst.RS);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  EmitMemoryAccess(access, s, addr);

  if (update)
    MR(a, addr);
}

void JitPPC64::dcbx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);
  FALLBACK_IF(m_accurate_cpu_cache_enabled);

  // dcbst, dcbf and dcbi invalidate the JIT's code for the cache line, as a heuristic for games
  // that don't use icbi. Check the block cache's bit set for the physical line first, as JitArm64
  // does, so that only lines with code call into the block cache.
  const auto addr = gpr.GetScopedReg();
  const auto physical = gpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();
  if (inst.RA)
    ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  else
    MR(addr, gpr.R(inst.RB));
  CLRLDI(addr, addr, 32);

  FixupBranch not_mapped;
  const bool translate = (m_ppc_state.feature_flags & FEATURE_FLAG_MSR_IR) != 0;
  if (translate)
  {
    // physical = (ibat[addr >> 17] & ~0x1ffff) | (addr & 0x1ffff), if the entry is mapped.
    MOVP2R(tmp, m_mmu.GetIBATTable().data());
    RLWINM(physical, addr, 32 - PowerPC::BAT_INDEX_SHIFT + 2, PowerPC::BAT_INDEX_SHIFT - 2, 29);
    LWZX(physical, tmp, physical);
    ANDI_(tmp, physical, PowerPC::BAT_MAPPED_BIT);
    not_mapped = BCFar(CCFlags::EQ, CRF::CR0);
    RLWIMI(physical, addr, 0, 32 - PowerPC::BAT_INDEX_SHIFT, 31);
  }
  else
  {
    MR(physical, addr);
  }

  // Bit (physical >> 5) of the bit set, in 32-bit words.
  MOVP2R(tmp, GetBlockCache()->GetBlockBitSet());
  {
    const auto word = gpr.GetScopedReg();
    RLWINM(word, physical, 32 - 10 + 2, 10 - 2, 29);
    LWZX(word, tmp, word);
    RLWINM(physical, physical, 32 - 5, 27, 31);
    SRW(word, word, physical);
    ANDI_(word, word, 1);
  }
  const FixupBranch has_code = BCFar(CCFlags::NE, CRF::CR0);
  const u8* done = GetCodePtr();

  SwitchToFarCode();
  SetJumpTarget(has_code);
  if (translate)
    SetJumpTarget(not_mapped);
  {
    MR(GPR::R12, addr);
    const BitSet32 saved = SaveScratchRegs();
    ABI_CallFunction(&JitInterface::InvalidateICacheLineFromJIT, &m_system.GetJitInterface(),
                     GPR::R12);
    RestoreScratchRegs(saved);
  }
  B(done);
  SwitchToNearCode();
}

void JitPPC64::dcbz(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);
  FALLBACK_IF(m_accurate_cpu_cache_enabled);

  // As JitArm64: zero the cache line, without the alignment exception that the interpreter raises
  // when HID0[DCE] is off, as games turn the data cache on. dcbz_l, for the locked cache, is
  // illegal unless HID2[LCE] enables it, and the low dcbz hack doesn't apply to it.
  const bool locked = inst.OPCD == 4;
  if (locked)
  {
    LWZ(GPR::R0, PPC_REG, PPCSTATE_OFF_SPR(SPR_HID2));
    RLWINM(GPR::R0, GPR::R0, 0, 31 - UReg_HID2{}.LCE.StartBit(), 31 - UReg_HID2{}.LCE.StartBit(),
           true);
    const FixupBranch illegal = BCFar(CCFlags::EQ, CRF::CR0);
    SwitchToFarCode();
    SetJumpTarget(illegal);
    WriteProgramExceptionExit(static_cast<u32>(ProgramExceptionCause::IllegalInstruction));
    SwitchToNearCode();
  }

  const auto addr = gpr.GetScopedReg();
  if (inst.RA)
    ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  else
    MR(addr, gpr.R(inst.RB));

  // The low dcbz hack skips [0x80000000, 0x80008000), which is below 0x80008000 as signed.
  FixupBranch low_dcbz;
  const bool low_dcbz_hack = m_low_dcbz_hack && !locked;
  if (low_dcbz_hack)
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, 0x80008000);
    CMPW(CRF::CR1, addr, tmp);
    low_dcbz = BC(CCFlags::LT, CRF::CR1);
  }
  // Aligned to the line, zero-extended.
  RLWINM(addr, addr, 0, 0, 26);

  const auto emit_slow = [&] {
    // The PC is used by memory checks and in logs.
    MOVI2R(GPR::R12, js.compilerPC);
    STW(GPR::R12, PPC_REG, PPCSTATE_OFF(pc));
    MR(GPR::R12, addr);
    const BitSet32 saved = SaveScratchRegs();
    ABI_CallFunction(&PowerPC::ClearDCacheLineFromJit, &m_mmu, GPR::R12);
    RestoreScratchRegs(saved);
    if (jo.memcheck)
      WriteConditionalExceptionExit(EXCEPTION_DSI, GPR::R12);
  };

  if (!jo.fastmem)
  {
    emit_slow();
  }
  else
  {
    // With fastmem, four doubleword stores to the line, which is in one page: the first faults if
    // any does, and is patched into a branch to the slow path.
    const auto host = gpr.GetScopedReg();
    const auto zero = gpr.GetScopedReg();
    ADD(host, MEM_REG, addr);
    LI(zero, 0);
    const u8* fault_pc = GetCodePtr();
    for (s16 offset = 0; offset < 32; offset += 8)
      STD(zero, host, offset);
    const u8* fast_end = GetCodePtr();

    SwitchToFarCode();
    m_fault_to_handler[fault_pc] = GetCodePtr();
    emit_slow();
    B(fast_end);
    SwitchToNearCode();
  }

  if (low_dcbz_hack)
    SetJumpTarget(low_dcbz);
}

void JitPPC64::lmw_stmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // These access up to 32 guest registers, more than fit in host registers at once, so they work on
  // PowerPCState: lmw flushes the registers it overwrites, and stmw writes back the ones it stores.
  // With memory checks, a DSI exception leaves the words before it loaded or stored, as the
  // interpreter does.
  const bool store = inst.OPCD == 47;
  const auto addr = gpr.GetScopedReg();
  if (inst.RA == 0)
    MOVI2R(addr, static_cast<u32>(static_cast<s32>(inst.SIMM_16)));
  else
    ADDI(addr, gpr.R(inst.RA), inst.SIMM_16);
  CLRLDI(addr, addr, 32);
  gpr.Flush(store ? FlushMode::MaintainState : FlushMode::All);

  // A misaligned address raises an alignment exception, which the interpreter does. (So does
  // MSR[LE], which games don't use.)
  ANDI_(GPR::R0, addr, 3);
  const FixupBranch misaligned = BCFar(CCFlags::NE, CRF::CR0);

  {
    const auto value = gpr.GetScopedReg();
    const auto word_addr = gpr.GetScopedReg();
    for (u32 i = inst.RD; i <= 31; i++)
    {
      ADDI(word_addr, addr, static_cast<s16>(4 * (i - inst.RD)));
      CLRLDI(word_addr, word_addr, 32);
      if (store)
      {
        LWZ(value, PPC_REG, PPCSTATE_OFF_GPR(i));
        EmitMemoryAccess({.store = true, .size = 32}, value, word_addr);
      }
      else
      {
        EmitMemoryAccess({.size = 32}, value, word_addr);
        STW(value, PPC_REG, PPCSTATE_OFF_GPR(i));
      }
    }
  }
  const u8* done = GetCodePtr();

  SwitchToFarCode();
  SetJumpTarget(misaligned);
  ABI_CallFunction(Interpreter::GetInterpreterOp(inst), &m_system.GetInterpreter(), inst.hex);
  B(done);
  SwitchToNearCode();
}
