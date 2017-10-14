// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/BreakPoints.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/PowerPC.h"

bool BreakPoints::IsAddressBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.address == address; });
}

bool BreakPoints::IsTempBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& bp) {
    return bp.address == address && bp.is_temporary;
  });
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
  TBreakPointsStr bp_strings;
  for (const TBreakPoint& bp : m_breakpoints)
  {
    if (!bp.is_temporary)
    {
      std::stringstream ss;
      ss << std::hex << bp.address << " " << (bp.is_enabled ? "n" : "");
      bp_strings.push_back(ss.str());
    }
  }

  return bp_strings;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bp_strings)
{
  for (const std::string& bp_string : bp_strings)
  {
    TBreakPoint bp;
    std::stringstream ss;
    ss << std::hex << bp_string;
    ss >> bp.address;
    bp.is_enabled = bp_string.find('n') != bp_string.npos;
    bp.is_temporary = false;
    Add(bp);
  }
}

void BreakPoints::Add(const TBreakPoint& bp)
{
  if (!IsAddressBreakPoint(bp.address))
  {
    m_breakpoints.push_back(bp);
    if (g_jit)
      g_jit->GetBlockCache()->InvalidateICache(bp.address, 4, true);
  }
}

void BreakPoints::Add(u32 address, bool temp)
{
  if (!IsAddressBreakPoint(address))  // only add new addresses
  {
    TBreakPoint bp;  // breakpoint settings
    bp.is_enabled = true;
    bp.is_temporary = temp;
    bp.address = address;

    m_breakpoints.push_back(bp);

    if (g_jit)
      g_jit->GetBlockCache()->InvalidateICache(address, 4, true);
  }
}

void BreakPoints::Remove(u32 address)
{
  for (auto i = m_breakpoints.begin(); i != m_breakpoints.end(); ++i)
  {
    if (i->address == address)
    {
      m_breakpoints.erase(i);
      if (g_jit)
        g_jit->GetBlockCache()->InvalidateICache(address, 4, true);
      return;
    }
  }
}

void BreakPoints::Clear()
{
  if (g_jit)
  {
    for (const TBreakPoint& bp : m_breakpoints)
    {
      g_jit->GetBlockCache()->InvalidateICache(bp.address, 4, true);
    }
  }

  m_breakpoints.clear();
}

void BreakPoints::ClearAllTemporary()
{
  auto bp = m_breakpoints.begin();
  while (bp != m_breakpoints.end())
  {
    if (bp->is_temporary)
    {
      if (g_jit)
        g_jit->GetBlockCache()->InvalidateICache(bp->address, 4, true);
      bp = m_breakpoints.erase(bp);
    }
    else
    {
      ++bp;
    }
  }
}

MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
  TMemChecksStr mc_strings;
  for (const TMemCheck& mc : m_mem_checks)
  {
    std::stringstream ss;
    ss << std::hex << mc.start_address;
    ss << " " << (mc.is_ranged ? mc.end_address : mc.start_address) << " "
       << (mc.is_ranged ? "n" : "") << (mc.is_break_on_read ? "r" : "")
       << (mc.is_break_on_write ? "w" : "") << (mc.log_on_hit ? "l" : "")
       << (mc.break_on_hit ? "p" : "");
    mc_strings.push_back(ss.str());
  }

  return mc_strings;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mc_strings)
{
  for (const std::string& mc_string : mc_strings)
  {
    TMemCheck mc;
    std::stringstream ss;
    ss << std::hex << mc_string;
    ss >> mc.start_address;
    mc.is_ranged = mc_string.find('n') != mc_string.npos;
    mc.is_break_on_read = mc_string.find('r') != mc_string.npos;
    mc.is_break_on_write = mc_string.find('w') != mc_string.npos;
    mc.log_on_hit = mc_string.find('l') != mc_string.npos;
    mc.break_on_hit = mc_string.find('p') != mc_string.npos;
    if (mc.is_ranged)
      ss >> mc.end_address;
    else
      mc.end_address = mc.start_address;
    Add(mc);
  }
}

void MemChecks::Add(const TMemCheck& memory_check)
{
  if (GetMemCheck(memory_check.start_address) == nullptr)
  {
    bool had_any = HasAny();
    Core::RunAsCPUThread([&] {
      m_mem_checks.push_back(memory_check);
      // If this is the first one, clear the JIT cache so it can switch to
      // watchpoint-compatible code.
      if (!had_any && g_jit)
        g_jit->ClearCache();
      PowerPC::DBATUpdated();
    });
  }
}

void MemChecks::Remove(u32 address)
{
  for (auto i = m_mem_checks.begin(); i != m_mem_checks.end(); ++i)
  {
    if (i->start_address == address)
    {
      Core::RunAsCPUThread([&] {
        m_mem_checks.erase(i);
        if (!HasAny() && g_jit)
          g_jit->ClearCache();
        PowerPC::DBATUpdated();
      });
      return;
    }
  }
}

TMemCheck* MemChecks::GetMemCheck(u32 address, size_t size)
{
  for (TMemCheck& mc : m_mem_checks)
  {
    if (mc.end_address >= address && address + size - 1 >= mc.start_address)
    {
      return &mc;
    }
  }

  // none found
  return nullptr;
}

bool MemChecks::OverlapsMemcheck(u32 address, u32 length)
{
  if (!HasAny())
    return false;
  u32 page_end_suffix = length - 1;
  u32 page_end_address = address | page_end_suffix;
  for (TMemCheck memcheck : m_mem_checks)
  {
    if (((memcheck.start_address | page_end_suffix) == page_end_address ||
         (memcheck.end_address | page_end_suffix) == page_end_address) ||
        ((memcheck.start_address | page_end_suffix) < page_end_address &&
         (memcheck.end_address | page_end_suffix) > page_end_address))
    {
      return true;
    }
  }
  return false;
}

bool TMemCheck::Action(DebugInterface* debug_interface, u32 value, u32 addr, bool write,
                       size_t size, u32 pc)
{
  if ((write && is_break_on_write) || (!write && is_break_on_read))
  {
    if (log_on_hit)
    {
      NOTICE_LOG(MEMMAP, "MBP %08x (%s) %s%zu %0*x at %08x (%s)", pc,
                 debug_interface->GetDescription(pc).c_str(), write ? "Write" : "Read", size * 8,
                 static_cast<int>(size * 2), value, addr,
                 debug_interface->GetDescription(addr).c_str());
    }
    if (break_on_hit)
      return true;
  }
  return false;
}

bool Watches::IsAddressWatch(u32 address) const
{
  return std::any_of(m_watches.begin(), m_watches.end(),
                     [address](const auto& watch) { return watch.address == address; });
}

Watches::TWatchesStr Watches::GetStrings() const
{
  TWatchesStr watch_strings;
  for (const TWatch& watch : m_watches)
  {
    std::stringstream ss;
    ss << std::hex << watch.address << " " << watch.name;
    watch_strings.push_back(ss.str());
  }

  return watch_strings;
}

void Watches::AddFromStrings(const TWatchesStr& watch_strings)
{
  for (const std::string& watch_string : watch_strings)
  {
    TWatch watch;
    std::stringstream ss;
    ss << std::hex << watch_string;
    ss >> watch.address;
    ss >> std::ws;
    std::getline(ss, watch.name);
    Add(watch);
  }
}

void Watches::Add(const TWatch& watch)
{
  if (!IsAddressWatch(watch.address))
  {
    m_watches.push_back(watch);
  }
}

void Watches::Add(u32 address)
{
  if (!IsAddressWatch(address))  // only add new addresses
  {
    TWatch watch;  // watch settings
    watch.is_enabled = true;
    watch.address = address;

    m_watches.push_back(watch);
  }
}

void Watches::Update(int count, u32 address)
{
  m_watches.at(count).address = address;
}

void Watches::UpdateName(int count, const std::string name)
{
  m_watches.at(count).name = name;
}

void Watches::Remove(u32 address)
{
  for (auto i = m_watches.begin(); i != m_watches.end(); ++i)
  {
    if (i->address == address)
    {
      m_watches.erase(i);
      return;
    }
  }
}

void Watches::Clear()
{
  m_watches.clear();
}
