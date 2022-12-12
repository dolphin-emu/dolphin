// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitInterface.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#else
#include "Common/PerformanceCounter.h"
#endif

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/MsgHandler.h"

#include "Core/Core.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"

#if _M_X86
#include "Core/PowerPC/Jit64/Jit.h"
#endif

#if _M_ARM_64
#include "Core/PowerPC/JitArm64/Jit.h"
#endif

namespace JitInterface
{
static JitBase* g_jit = nullptr;
void SetJit(JitBase* jit)
{
  g_jit = jit;
}
void DoState(PointerWrap& p)
{
  if (g_jit && p.IsReadMode())
    g_jit->ClearCache();
}
CPUCoreBase* InitJitCore(PowerPC::CPUCore core)
{
  switch (core)
  {
#if _M_X86
  case PowerPC::CPUCore::JIT64:
    g_jit = new Jit64();
    break;
#endif
#if _M_ARM_64
  case PowerPC::CPUCore::JITARM64:
    g_jit = new JitArm64();
    break;
#endif
  case PowerPC::CPUCore::CachedInterpreter:
    g_jit = new CachedInterpreter();
    break;

  default:
    // Under this case the caller overrides the CPU core to the default and logs that
    // it performed the override.
    g_jit = nullptr;
    return nullptr;
  }
  g_jit->Init();
  return g_jit;
}

CPUCoreBase* GetCore()
{
  return g_jit;
}

void SetProfilingState(ProfilingState state)
{
  if (!g_jit)
    return;

  g_jit->jo.profile_blocks = state == ProfilingState::Enabled;
}

void WriteProfileResults(const std::string& filename)
{
  Profiler::ProfileStats prof_stats;
  GetProfileResults(&prof_stats);

  File::IOFile f(filename, "w");
  if (!f)
  {
    PanicAlertFmt("Failed to open {}", filename);
    return;
  }
  f.WriteString("origAddr\tblkName\trunCount\tcost\ttimeCost\tpercent\ttimePercent\tOvAllinBlkTime("
                "ms)\tblkCodeSize\n");
  for (auto& stat : prof_stats.block_stats)
  {
    std::string name = g_symbolDB.GetDescription(stat.addr);
    double percent = 100.0 * (double)stat.cost / (double)prof_stats.cost_sum;
    double timePercent = 100.0 * (double)stat.tick_counter / (double)prof_stats.timecost_sum;
    f.WriteString(fmt::format("{0:08x}\t{1}\t{2}\t{3}\t{4}\t{5:.2f}\t{6:.2f}\t{7:.2f}\t{8}\n",
                              stat.addr, name, stat.run_count, stat.cost, stat.tick_counter,
                              percent, timePercent,
                              static_cast<double>(stat.tick_counter) * 1000.0 /
                                  static_cast<double>(prof_stats.countsPerSec),
                              stat.block_size));
  }
}

void GetProfileResults(Profiler::ProfileStats* prof_stats)
{
  // Can't really do this with no g_jit core available
  if (!g_jit)
    return;

  prof_stats->cost_sum = 0;
  prof_stats->timecost_sum = 0;
  prof_stats->block_stats.clear();

  Core::RunAsCPUThread([&prof_stats] {
    QueryPerformanceFrequency((LARGE_INTEGER*)&prof_stats->countsPerSec);
    g_jit->GetBlockCache()->RunOnBlocks([&prof_stats](const JitBlock& block) {
      const auto& data = block.profile_data;
      u64 cost = data.downcountCounter;
      u64 timecost = data.ticCounter;
      // Todo: tweak.
      if (data.runCount >= 1)
        prof_stats->block_stats.emplace_back(block.effectiveAddress, cost, timecost, data.runCount,
                                             block.codeSize);
      prof_stats->cost_sum += cost;
      prof_stats->timecost_sum += timecost;
    });

    sort(prof_stats->block_stats.begin(), prof_stats->block_stats.end());
  });
}

std::variant<GetHostCodeError, GetHostCodeResult> GetHostCode(u32 address)
{
  if (!g_jit)
  {
    return GetHostCodeError::NoJitActive;
  }

  JitBlock* block = g_jit->GetBlockCache()->GetBlockFromStartAddress(address, MSR.Hex);
  if (!block)
  {
    for (int i = 0; i < 500; i++)
    {
      block = g_jit->GetBlockCache()->GetBlockFromStartAddress(address - 4 * i, MSR.Hex);
      if (block)
        break;
    }

    if (block)
    {
      if (!(block->effectiveAddress <= address &&
            block->originalSize + block->effectiveAddress >= address))
        block = nullptr;
    }

    // Do not merge this "if" with the above - block changes inside it.
    if (!block)
    {
      return GetHostCodeError::NoTranslation;
    }
  }

  GetHostCodeResult result;
  result.code = block->checkedEntry;
  result.code_size = block->codeSize;
  result.entry_address = block->effectiveAddress;
  return result;
}

bool HandleFault(uintptr_t access_address, SContext* ctx)
{
  // Prevent nullptr dereference on a crash with no JIT present
  if (!g_jit)
  {
    return false;
  }

  return g_jit->HandleFault(access_address, ctx);
}

bool HandleStackFault()
{
  if (!g_jit)
  {
    return false;
  }

  return g_jit->HandleStackFault();
}

void ClearCache()
{
  if (g_jit)
    g_jit->ClearCache();
}
void ClearSafe()
{
  if (g_jit)
    g_jit->GetBlockCache()->Clear();
}

void InvalidateICache(u32 address, u32 size, bool forced)
{
  if (g_jit)
    g_jit->GetBlockCache()->InvalidateICache(address, size, forced);
}

void InvalidateICacheLine(u32 address)
{
  if (g_jit)
    g_jit->GetBlockCache()->InvalidateICacheLine(address);
}

void InvalidateICacheLines(u32 address, u32 count)
{
  // This corresponds to a PPC code loop that:
  // - calls some form of dcb* instruction on 'address'
  // - increments 'address' by the size of a cache line (0x20 bytes)
  // - decrements 'count' by 1
  // - jumps back to the dcb* instruction if 'count' != 0
  // with an extra optimization for the case of a single cache line invalidation
  if (count == 1)
    InvalidateICacheLine(address);
  else if (count == 0 || count >= static_cast<u32>(0x1'0000'0000 / 32))
    InvalidateICache(address & ~0x1f, 0xffffffff, false);
  else
    InvalidateICache(address & ~0x1f, 32 * count, false);
}

void CompileExceptionCheck(ExceptionType type)
{
  if (!g_jit)
    return;

  std::unordered_set<u32>* exception_addresses = nullptr;

  switch (type)
  {
  case ExceptionType::FIFOWrite:
    exception_addresses = &g_jit->js.fifoWriteAddresses;
    break;
  case ExceptionType::PairedQuantize:
    exception_addresses = &g_jit->js.pairedQuantizeAddresses;
    break;
  case ExceptionType::SpeculativeConstants:
    exception_addresses = &g_jit->js.noSpeculativeConstantsAddresses;
    break;
  }

  if (PC != 0 && (exception_addresses->find(PC)) == (exception_addresses->end()))
  {
    if (type == ExceptionType::FIFOWrite)
    {
      // Check in case the code has been replaced since: do we need to do this?
      const OpType optype = PPCTables::GetOpInfo(PowerPC::HostRead_U32(PC))->type;
      if (optype != OpType::Store && optype != OpType::StoreFP && optype != OpType::StorePS)
        return;
    }
    exception_addresses->insert(PC);

    // Invalidate the JIT block so that it gets recompiled with the external exception check
    // included.
    g_jit->GetBlockCache()->InvalidateICache(PC, 4, true);
  }
}

void Shutdown()
{
  if (g_jit)
  {
    g_jit->Shutdown();
    delete g_jit;
    g_jit = nullptr;
  }
}
}  // namespace JitInterface
