// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitInterface.h"

#include <algorithm>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/Core.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#ifdef _M_X86_64
#include "Core/PowerPC/Jit64/Jit.h"
#endif

#ifdef _M_ARM_64
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
#ifdef _M_X86_64
  case PowerPC::CPUCore::JIT64:
    m_jit = std::make_unique<Jit64>(m_system);
    break;
#endif
#ifdef _M_ARM_64
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

void JitInterface::UpdateMembase()
{
  if (!m_jit)
    return;

  auto& ppc_state = m_system.GetPPCState();
  auto& memory = m_system.GetMemory();
#ifdef _M_ARM_64
  // JitArm64 is currently using the no fastmem arena code path even when only fastmem is off.
  const bool fastmem_arena = m_jit->jo.fastmem;
#else
  const bool fastmem_arena = m_jit->jo.fastmem_arena;
#endif
  if (ppc_state.msr.DR)
  {
    ppc_state.mem_ptr =
        fastmem_arena ? memory.GetLogicalBase() : memory.GetLogicalPageMappingsBase();
  }
  else
  {
    ppc_state.mem_ptr =
        fastmem_arena ? memory.GetPhysicalBase() : memory.GetPhysicalPageMappingsBase();
  }
}

static std::string_view GetDescription(const CPUEmuFeatureFlags flags)
{
  static constexpr std::array<std::string_view, (FEATURE_FLAG_END_OF_ENUMERATION - 1) << 1>
      descriptions = {
          "", "DR", "IR", "DR|IR", "PERFMON", "DR|PERFMON", "IR|PERFMON", "DR|IR|PERFMON",
      };
  return descriptions[flags];
}

void JitInterface::JitBlockLogDump(const Core::CPUThreadGuard& guard, std::FILE* file) const
{
  std::fputs(
      "ppcFeatureFlags\tppcAddress\tppcSize\thostNearSize\thostFarSize\trunCount\tcyclesSpent"
      "\tcyclesAverage\tcyclesPercent\ttimeSpent(ns)\ttimeAverage(ns)\ttimePercent\tsymbol\n",
      file);

  if (!m_jit)
    return;

  if (m_jit->IsProfilingEnabled())
  {
    u64 overall_cycles_spent = 0;
    JitBlock::ProfileData::Clock::duration overall_time_spent = {};
    m_jit->GetBlockCache()->RunOnBlocks(guard, [&](const JitBlock& block) {
      overall_cycles_spent += block.profile_data->cycles_spent;
      overall_time_spent += block.profile_data->time_spent;
    });
    m_jit->GetBlockCache()->RunOnBlocks(guard, [&](const JitBlock& block) {
      const Common::Symbol* const symbol =
          m_jit->m_ppc_symbol_db.GetSymbolFromAddr(block.effectiveAddress);
      const JitBlock::ProfileData* const data = block.profile_data.get();

      const double cycles_percent =
          overall_cycles_spent == 0 ? double{} : 100.0 * data->cycles_spent / overall_cycles_spent;
      const double time_percent = overall_time_spent == JitBlock::ProfileData::Clock::duration{} ?
                                      double{} :
                                      100.0 * data->time_spent.count() / overall_time_spent.count();
      const double cycles_average = data->run_count == 0 ?
                                        double{} :
                                        static_cast<double>(data->cycles_spent) / data->run_count;
      const double time_average =
          data->run_count == 0 ?
              double{} :
              std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(data->time_spent)
                      .count() /
                  data->run_count;

      const std::size_t host_near_code_size = block.near_end - block.near_begin;
      const std::size_t host_far_code_size = block.far_end - block.far_begin;

      fmt::println(
          file, "{}\t{:08x}\t{}\t{}\t{}\t{}\t{}\t{:.6f}\t{:.6f}\t{}\t{:.6f}\t{:.6f}\t\"{}\"",
          GetDescription(block.feature_flags), block.effectiveAddress,
          block.originalSize * sizeof(UGeckoInstruction), host_near_code_size, host_far_code_size,
          data->run_count, data->cycles_spent, cycles_average, cycles_percent,
          std::chrono::duration_cast<std::chrono::nanoseconds>(data->time_spent).count(),
          time_average, time_percent, symbol ? std::string_view{symbol->name} : "");
    });
  }
  else
  {
    m_jit->GetBlockCache()->RunOnBlocks(guard, [&](const JitBlock& block) {
      const Common::Symbol* const symbol =
          m_jit->m_ppc_symbol_db.GetSymbolFromAddr(block.effectiveAddress);

      const std::size_t host_near_code_size = block.near_end - block.near_begin;
      const std::size_t host_far_code_size = block.far_end - block.far_begin;

      fmt::println(file, "{}\t{:08x}\t{}\t{}\t{}\t-\t-\t-\t-\t-\t-\t-\t\"{}\"",
                   GetDescription(block.feature_flags), block.effectiveAddress,
                   block.originalSize * sizeof(UGeckoInstruction), host_near_code_size,
                   host_far_code_size, symbol ? std::string_view{symbol->name} : "");
    });
  }
}

std::variant<JitInterface::GetHostCodeError, JitInterface::GetHostCodeResult>
JitInterface::GetHostCode(u32 address) const
{
  if (!m_jit)
  {
    return GetHostCodeError::NoJitActive;
  }

  auto& ppc_state = m_system.GetPPCState();
  JitBlock* block =
      m_jit->GetBlockCache()->GetBlockFromStartAddress(address, ppc_state.feature_flags);
  if (!block)
  {
    for (int i = 0; i < 500; i++)
    {
      block = m_jit->GetBlockCache()->GetBlockFromStartAddress(address - 4 * i,
                                                               ppc_state.feature_flags);
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
  result.code = block->normalEntry;
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

void JitInterface::ClearCache(const Core::CPUThreadGuard&)
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
