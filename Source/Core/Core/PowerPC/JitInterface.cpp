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

#include "Common/Assert.h"
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
#include "Core/System.h"

#if _M_X86
#include "Core/PowerPC/Jit64/Jit.h"
#endif

#if _M_ARM_64
#include "Core/PowerPC/JitArm64/Jit.h"
#endif

JitInterface::JitInterface(Core::System& system) : m_system(system)
{
}

JitInterface::~JitInterface() = default;

void JitInterface::SetJit(std::unique_ptr<JitBase> jit)
{
  m_jit = std::move(jit);
}

void JitInterface::DoState(PointerWrap& p)
{
  if (m_jit && p.IsReadMode())
    m_jit->ClearCache();
}

CPUCoreBase* JitInterface::InitJitCore(PowerPC::CPUCore core)
{
  switch (core)
  {
#if _M_X86
  case PowerPC::CPUCore::JIT64:
    m_jit = std::make_unique<Jit64>(m_system);
    break;
#endif
#if _M_ARM_64
  case PowerPC::CPUCore::JITARM64:
    m_jit = std::make_unique<JitArm64>(m_system);
    break;
#endif
  case PowerPC::CPUCore::CachedInterpreter:
    m_jit = std::make_unique<CachedInterpreter>(m_system);
    break;

  default:
    // Under this case the caller overrides the CPU core to the default and logs that
    // it performed the override.
    m_jit.reset();
    return nullptr;
  }
  m_jit->Init();
  return m_jit.get();
}

CPUCoreBase* JitInterface::GetCore() const
{
  return m_jit.get();
}

void JitInterface::SetProfilingState(ProfilingState state)
{
  if (!m_jit)
    return;

  m_jit->jo.profile_blocks = state == ProfilingState::Enabled;
}

void JitInterface::WriteProfileResults(const std::string& filename) const
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

void JitInterface::GetProfileResults(Profiler::ProfileStats* prof_stats) const
{
  // Can't really do this with no m_jit core available
  if (!m_jit)
    return;

  prof_stats->cost_sum = 0;
  prof_stats->timecost_sum = 0;
  prof_stats->block_stats.clear();

  Core::RunAsCPUThread([this, &prof_stats] {
    QueryPerformanceFrequency((LARGE_INTEGER*)&prof_stats->countsPerSec);
    m_jit->GetBlockCache()->RunOnBlocks([&prof_stats](const JitBlock& block) {
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

std::variant<JitInterface::GetHostCodeError, JitInterface::GetHostCodeResult>
JitInterface::GetHostCode(u32 address) const
{
  if (!m_jit)
  {
    return GetHostCodeError::NoJitActive;
  }

  auto& ppc_state = m_system.GetPPCState();
  JitBlock* block = m_jit->GetBlockCache()->GetBlockFromStartAddress(address, ppc_state.msr.Hex);
  if (!block)
  {
    for (int i = 0; i < 500; i++)
    {
      block = m_jit->GetBlockCache()->GetBlockFromStartAddress(address - 4 * i, ppc_state.msr.Hex);
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

bool JitInterface::HandleFault(uintptr_t access_address, SContext* ctx)
{
  // Prevent nullptr dereference on a crash with no JIT present
  if (!m_jit)
  {
    return false;
  }

  return m_jit->HandleFault(access_address, ctx);
}

bool JitInterface::HandleStackFault()
{
  if (!m_jit)
  {
    return false;
  }

  return m_jit->HandleStackFault();
}

void JitInterface::ClearCache()
{
  if (m_jit)
    m_jit->ClearCache();
}

void JitInterface::ClearSafe()
{
  if (m_jit)
    m_jit->GetBlockCache()->Clear();
}

void JitInterface::InvalidateICache(u32 address, u32 size, bool forced)
{
  if (m_jit)
    m_jit->GetBlockCache()->InvalidateICache(address, size, forced);
}

void JitInterface::InvalidateICacheLine(u32 address)
{
  if (m_jit)
    m_jit->GetBlockCache()->InvalidateICacheLine(address);
}

void JitInterface::InvalidateICacheLines(u32 address, u32 count)
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

void JitInterface::InvalidateICacheLineFromJIT(JitInterface& jit_interface, u32 address)
{
  jit_interface.InvalidateICacheLine(address);
}

void JitInterface::InvalidateICacheLinesFromJIT(JitInterface& jit_interface, u32 address, u32 count)
{
  jit_interface.InvalidateICacheLines(address, count);
}

void JitInterface::CompileExceptionCheck(ExceptionType type)
{
  if (!m_jit)
    return;

  std::unordered_set<u32>* exception_addresses = nullptr;

  switch (type)
  {
  case ExceptionType::FIFOWrite:
    exception_addresses = &m_jit->js.fifoWriteAddresses;
    break;
  case ExceptionType::PairedQuantize:
    exception_addresses = &m_jit->js.pairedQuantizeAddresses;
    break;
  case ExceptionType::SpeculativeConstants:
    exception_addresses = &m_jit->js.noSpeculativeConstantsAddresses;
    break;
  }

  auto& ppc_state = m_system.GetPPCState();
  if (ppc_state.pc != 0 &&
      (exception_addresses->find(ppc_state.pc)) == (exception_addresses->end()))
  {
    if (type == ExceptionType::FIFOWrite)
    {
      ASSERT(Core::IsCPUThread());
      Core::CPUThreadGuard guard(m_system);

      // Check in case the code has been replaced since: do we need to do this?
      const OpType optype =
          PPCTables::GetOpInfo(PowerPC::MMU::HostRead_U32(guard, ppc_state.pc), ppc_state.pc)->type;
      if (optype != OpType::Store && optype != OpType::StoreFP && optype != OpType::StorePS)
        return;
    }
    exception_addresses->insert(ppc_state.pc);

    // Invalidate the JIT block so that it gets recompiled with the external exception check
    // included.
    m_jit->GetBlockCache()->InvalidateICache(ppc_state.pc, 4, true);
  }
}

void JitInterface::CompileExceptionCheckFromJIT(JitInterface& jit_interface, ExceptionType type)
{
  jit_interface.CompileExceptionCheck(type);
}

void JitInterface::Shutdown()
{
  if (m_jit)
  {
    m_jit->Shutdown();
    m_jit.reset();
  }
}
