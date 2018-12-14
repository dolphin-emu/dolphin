// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitTiered/JitTiered.h"

#include "Common/Logging/Log.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

static constexpr u32 EXCEPTION_SYNC =
    ~(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER);

void JitTieredGeneric::ClearCache()
{
  // invalidate dispatch cache
  std::memset(&disp_cache, 0, sizeof(disp_cache));
  BaselineReport& report = baseline_report.GetWriter();
  report.invalidation_bloom = BloomAll();
  report.invalidations.push_back({0, 0xffffffff});
}

static bool overlaps(u32 block_start, u32 block_len, u32 first_inval, u32 last_inval)
{
  // (block_start + block_len) is *exclusive* end of the block interval
  return (block_start <= last_inval && (block_start + block_len) > first_inval) ||
         (block_start >= first_inval && block_start <= last_inval);
}

void JitTieredGeneric::InvalidateICache(u32 address, u32 size, bool forced)
{
  if (size == 0)
  {
    return;  // zero-sized invalidations don't have a 'last invalidated byte' so exit early
  }
  u32 end = address + size - 1;
  BaselineReport& report = baseline_report.GetWriter();
  report.invalidation_bloom |= BloomRange(address, end);
  report.invalidations.push_back({address, end});
  for (size_t i = 0; i < DISP_CACHE_SIZE; i += 1)
  {
    if ((disp_cache[i].address & FLAG_MASK) <= 1)
    {
      // catches 0 case (invalid entry) too, but this invalidates anyway (⇒ safe)
      if (overlaps(disp_cache[i].address & ~FLAG_MASK, disp_cache[i].len * 4, address, end))
      {
        disp_cache[i].address = 0;
      }
    }
  }
}

void JitTieredGeneric::CompactInterpreterBlocks()
{
  auto report = baseline_report.GetWriter();
  report.instructions.clear();
  report.blocks.clear();
  for (size_t i = 0; i < DISP_CACHE_SIZE; i += 1)
  {
    u32 address = disp_cache[i].address;
    if ((address & FLAG_MASK) == 0 && address)
    {
      u32 offset = disp_cache[i].offset;
      if (offset >= offset_new)
      {
        u32 start = static_cast<u32>(report.instructions.size());
        report.blocks.push_back({address, start, disp_cache[i].usecount});
        u32 len = static_cast<u32>(disp_cache[i].len);
        for (u32 n = 0; n < len; n += 1)
        {
          report.instructions.push_back(inst_cache[offset + n]);
        }
        // update offset to position after compaction
        disp_cache[i].offset = start;
      }
      else
      {
        // block is old and will disappear with this compaction ⇒ invalidate
        disp_cache[i].address = 0;
      }
    }
  }
  // copy compacted blocks into interpreter cache
  inst_cache = report.instructions;
  offset_new = report.instructions.size();
  // sentinel value to make calculating the length in Baseline easier
  report.blocks.push_back({0, static_cast<u32>(offset_new), 0});
}

u32 JitTieredGeneric::FindBlock(u32 address)
{
  u32 key = DispatchCacheKey(address);
  u32 cache_addr = disp_cache[key].address;
  if ((cache_addr & ~FLAG_MASK) == address)
  {
    // best outcome: we have the block in primary cache
    return key;
  }
  // not in primary cache, search victim cache
  u32 victim_set = key >> (DISP_CACHE_SHIFT - VICTIM_SETS_SHIFT);
  u32 victim_start = DISP_PRIMARY_CACHE_SIZE + (victim_set << VICTIM_WAYS_SHIFT);
  u32 victim_end = victim_start + VICTIM_WAYS;
  bool found_vacancy = false;
  u32 pos = 0;
  for (u32 i = victim_start; i < victim_end; i += 1)
  {
    if ((disp_cache[i].address & ~FLAG_MASK) == address)
    {
      // second-best outcome: we find the block in victim cache ⇒ swap and done
      DispCacheEntry tmp = disp_cache[key];
      disp_cache[key] = disp_cache[i];
      disp_cache[i] = tmp;
      victim_second_chance[i] = true;
      return key;
    }
    if (disp_cache[i].address == 0)
    {
      // third-best outcome: we have a vacancy in victim cache
      // (but continue searching for a real match)
      found_vacancy = true;
      pos = i;
    }
  }
  if (!found_vacancy && cache_addr != 0)
  {
    // worst outcome: we have to evict from victim cache
    u32 clock = victim_clocks[victim_set];
    for (u32 i = 0; i < VICTIM_WAYS; i += 1)
    {
      pos = victim_start + clock;
      if (victim_second_chance[pos])
      {
        victim_second_chance[pos] = false;
      }
      else
      {
        break;
      }
      clock = (clock + 1) % VICTIM_WAYS;
    }
    victim_clocks[victim_set] = clock;
    disp_cache[pos] = disp_cache[key];
    victim_second_chance[pos] = true;
  }
  // at this point, the primary cache contains an invalid entry
  // (or one that has already been saved to victim cache)

  // (look up in JIT DB here once that's implemented)

  // not in cache: create new Interpreter block
  disp_cache[key].address = address;
  disp_cache[key].offset = (u32)inst_cache.size();
  disp_cache[key].len = disp_cache[key].usecount = 0;
  return key;
}

static bool IsRedispatchInstruction(UGeckoInstruction inst)
{
  auto info = PPCTables::GetOpInfo(inst);
  return inst.OPCD == 9                               // sc
         || (inst.OPCD == 31 && inst.SUBOP10 == 146)  // mtmsr
         || (info->flags & FL_CHECKEXCEPTIONS)        // rfi
         || (info->type == OpType::InstructionCache)  // isync
      ;
}

void JitTieredGeneric::InterpretBlock()
{
  if (PC == 0)
  {
    // zero is a null value in the cache tags, so handle it specially in case code jumps there
    u32 inst = PowerPC::Read_Opcode(0);
    if (inst == 0)
    {
      PowerPC::CheckExceptions();
      return;
    }
    PPCTables::GetInterpreterOp(UGeckoInstruction(inst))(inst);
    PowerPC::ppcState.downcount -= PPCTables::GetOpInfo(inst)->numCycles;
    if (PowerPC::ppcState.Exceptions)
    {
      PowerPC::CheckExceptions();
      return;
    }
    PC = NPC;
    if (PC != 4)
    {
      return;
    }
  }
  u32 start_addr = PC;
  u32 key = FindBlock(start_addr);
  if ((disp_cache[key].address & FLAG_MASK) == 0)
  {
    // flag bits are zero: interpreter block
    u32 len = disp_cache[key].len;
    disp_cache[key].usecount += 1;
    u32 offset = disp_cache[key].offset;
    for (u32 pos = offset; pos < offset + len; pos += 1)
    {
      auto& inst = inst_cache[pos];
      NPC = PC + 4;
      if (inst.uses_fpu && !MSR.FP)
      {
        PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
      }
      else
      {
        inst.func(inst.inst);
      }
      if (PowerPC::ppcState.Exceptions & EXCEPTION_SYNC)
      {
        PowerPC::CheckExceptions();
        PowerPC::ppcState.downcount -= inst.cycles;
        PowerPC::ppcState.Exceptions &= ~EXCEPTION_SYNC;
        return;
      }
      if (NPC != PC + 4)
      {
        PowerPC::ppcState.downcount -= inst.cycles;
        PC = NPC;
        return;
      }
      PC = NPC;
    }
    u32 cycles = 0;
    if (len != 0)
    {
      auto& last = inst_cache[offset + len - 1];
      cycles = last.cycles;
      if (IsRedispatchInstruction(last.inst) || PowerPC::breakpoints.IsAddressBreakPoint(PC))
      {
        // even if the block didn't end here, we have to go back to dispatcher
        // because of e. g. invalidation or breakpoints
        PowerPC::ppcState.downcount -= cycles;
        return;
      }
    }
    // overrun: add more instructions to block
    if (offset + len != inst_cache.size())
    {
      // we can only append to the last block, so copy this one to the end
      disp_cache[key].offset = inst_cache.size();
      for (u32 pos = offset; pos < offset + len; pos += 1)
      {
        inst_cache.push_back(inst_cache[pos]);
      }
      // following code should not index into inst_cache, but just to be sure
      offset = disp_cache[key].offset;
    }
    do
    {
      // handle temporary breakpoints
      PowerPC::CheckBreakPoints();
      auto inst = UGeckoInstruction(PowerPC::Read_Opcode(PC));
      if (inst.hex == 0)
      {
        PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
        PowerPC::CheckExceptions();
        return;
      }
      // end the block only on permanent breakpoints
      if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
      {
        INFO_LOG(DYNA_REC, "%8x: breakpoint", PC);
        break;
      }
      cycles += PPCTables::GetOpInfo(inst)->numCycles;
      auto func = PPCTables::GetInterpreterOp(inst);
      bool uses_fpu = PPCTables::UsesFPU(inst);
      inst_cache.push_back({func, inst, cycles, static_cast<u32>(uses_fpu)});
      disp_cache[key].len += 1;
      NPC = PC + 4;
      if (uses_fpu && !MSR.FP)
      {
        PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
      }
      else
      {
        func(inst);
      }
      if (PowerPC::ppcState.Exceptions & EXCEPTION_SYNC)
      {
        PowerPC::CheckExceptions();
        PowerPC::ppcState.downcount -= cycles;
        PowerPC::ppcState.Exceptions &= ~EXCEPTION_SYNC;
        return;
      }
      PC += 4;
      if (IsRedispatchInstruction(inst))
      {
        break;
      }
    } while (PC == NPC);
    PowerPC::ppcState.downcount -= cycles;
    PC = NPC;
  }
}

void JitTieredGeneric::SingleStep()
{
  CoreTiming::Advance();
  InterpretBlock();
}

void JitTieredGeneric::Run()
{
  const CPU::State* state = CPU::GetStatePtr();
  while (*state == CPU::State::Running)
  {
    CoreTiming::Advance();
    // this heuristic works well for the interpreter-only case,
    // but probably doesn't for a real on-thread JIT situation
    // (in the off-thread JIT case this is not necessary at all)
    if (on_thread_baseline && inst_cache.size() >= (1 << 16))
    {
      BaselineIteration();
    }
    {
      auto guard = baseline_report.Yield();
      if (guard.has_value())
      {
        CompactInterpreterBlocks();
        old_bloom = guard.invalidation_bloom;
      }
    }
    do
    {
      InterpretBlock();
    } while (PowerPC::ppcState.downcount > 0 && *state == CPU::State::Running);
  }
}

void JitTieredGeneric::BaselineIteration()
{
  {
    // this will switch sides on the Baseline report, causing compaction
    auto guard = baseline_report.Wait();
    // in subclasses: invalidate JIT blocks here and do the most urgent of compilation
    report.invalidation_bloom = BloomNone();
  }
  // in subclasses: do less urgent work here
}