// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/BreakPoints.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"

bool BreakPoints::IsAddressBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.address == address; });
}

bool BreakPoints::IsBreakPointEnable(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.is_enabled && bp.address == address; });
}

bool BreakPoints::IsTempBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& bp) {
    return bp.address == address && bp.is_temporary;
  });
}

bool BreakPoints::IsBreakPointBreakOnHit(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& bp) {
    return bp.address == address && bp.break_on_hit;
  });
}

bool BreakPoints::IsBreakPointLogOnHit(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.address == address && bp.log_on_hit; });
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
  TBreakPointsStr bp_strings;
  for (const TBreakPoint& bp : m_breakpoints)
  {
    if (!bp.is_temporary)
    {
      std::ostringstream ss;
      ss.imbue(std::locale::classic());

      ss << std::hex << bp.address << " " << (bp.is_enabled ? "n" : "")
         << (bp.log_on_hit ? "l" : "") << (bp.break_on_hit ? "b" : "");
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
    std::string flags;
    std::istringstream iss(bp_string);
    iss.imbue(std::locale::classic());

    iss >> std::hex >> bp.address;
    iss >> flags;
    bp.is_enabled = flags.find('n') != flags.npos;
    bp.log_on_hit = flags.find('l') != flags.npos;
    bp.break_on_hit = flags.find('b') != flags.npos;
    bp.is_temporary = false;
    Add(bp);
  }
}

void BreakPoints::Add(const TBreakPoint& bp)
{
  if (IsAddressBreakPoint(bp.address))
    return;

  m_breakpoints.push_back(bp);

  JitInterface::InvalidateICache(bp.address, 4, true);
}

void BreakPoints::Add(u32 address, bool temp)
{
  BreakPoints::Add(address, temp, true, false);
}

void BreakPoints::Add(u32 address, bool temp, bool break_on_hit, bool log_on_hit)
{
  // Only add new addresses
  if (IsAddressBreakPoint(address))
    return;

  TBreakPoint bp;  // breakpoint settings
  bp.is_enabled = true;
  bp.is_temporary = temp;
  bp.break_on_hit = break_on_hit;
  bp.log_on_hit = log_on_hit;
  bp.address = address;

  m_breakpoints.push_back(bp);

  JitInterface::InvalidateICache(address, 4, true);
}

bool BreakPoints::ToggleBreakPoint(u32 address)
{
  auto iter = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [address](const auto& bp) { return bp.address == address; });

  if (iter == m_breakpoints.end())
    return false;

  iter->is_enabled = !iter->is_enabled;
  return true;
}

void BreakPoints::Remove(u32 address)
{
  const auto iter = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                                 [address](const auto& bp) { return bp.address == address; });

  if (iter == m_breakpoints.cend())
    return;

  m_breakpoints.erase(iter);
  JitInterface::InvalidateICache(address, 4, true);
}

void BreakPoints::Clear()
{
  for (const TBreakPoint& bp : m_breakpoints)
  {
    JitInterface::InvalidateICache(bp.address, 4, true);
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
      JitInterface::InvalidateICache(bp->address, 4, true);
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
    std::ostringstream ss;
    ss.imbue(std::locale::classic());

    ss << std::hex << mc.start_address << " " << mc.end_address << " " << (mc.is_enabled ? "n" : "")
       << (mc.is_break_on_read ? "r" : "") << (mc.is_break_on_write ? "w" : "")
       << (mc.log_on_hit ? "l" : "") << (mc.break_on_hit ? "b" : "");
    mc_strings.push_back(ss.str());
  }

  return mc_strings;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mc_strings)
{
  for (const std::string& mc_string : mc_strings)
  {
    TMemCheck mc;
    std::istringstream iss(mc_string);
    iss.imbue(std::locale::classic());

    std::string flags;
    iss >> std::hex >> mc.start_address >> mc.end_address >> flags;

    mc.is_ranged = mc.start_address != mc.end_address;
    mc.is_enabled = flags.find('n') != flags.npos;
    mc.is_break_on_read = flags.find('r') != flags.npos;
    mc.is_break_on_write = flags.find('w') != flags.npos;
    mc.log_on_hit = flags.find('l') != flags.npos;
    mc.break_on_hit = flags.find('b') != flags.npos;

    Add(mc);
  }
}

void MemChecks::Add(const TMemCheck& memory_check)
{
  if (GetMemCheck(memory_check.start_address) != nullptr)
    return;

  bool had_any = HasAny();
  Core::RunAsCPUThread([&] {
    m_mem_checks.push_back(memory_check);
    // If this is the first one, clear the JIT cache so it can switch to
    // watchpoint-compatible code.
    if (!had_any)
      JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

bool MemChecks::ToggleBreakPoint(u32 address)
{
  auto iter = std::find_if(m_mem_checks.begin(), m_mem_checks.end(),
                           [address](const auto& bp) { return bp.start_address == address; });

  if (iter == m_mem_checks.end())
    return false;

  iter->is_enabled = !iter->is_enabled;
  return true;
}

void MemChecks::Remove(u32 address)
{
  const auto iter =
      std::find_if(m_mem_checks.cbegin(), m_mem_checks.cend(),
                   [address](const auto& check) { return check.start_address == address; });

  if (iter == m_mem_checks.cend())
    return;

  Core::RunAsCPUThread([&] {
    m_mem_checks.erase(iter);
    if (!HasAny())
      JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

void MemChecks::Clear()
{
  Core::RunAsCPUThread([&] {
    m_mem_checks.clear();
    JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

TMemCheck* MemChecks::GetMemCheck(u32 address, size_t size)
{
  const auto iter =
      std::find_if(m_mem_checks.begin(), m_mem_checks.end(), [address, size](const auto& mc) {
        return mc.end_address >= address && address + size - 1 >= mc.start_address;
      });

  // None found
  if (iter == m_mem_checks.cend())
    return nullptr;

  return &*iter;
}

bool MemChecks::OverlapsMemcheck(u32 address, u32 length) const
{
  if (!HasAny())
    return false;

  const u32 page_end_suffix = length - 1;
  const u32 page_end_address = address | page_end_suffix;

  return std::any_of(m_mem_checks.cbegin(), m_mem_checks.cend(), [&](const auto& mc) {
    return ((mc.start_address | page_end_suffix) == page_end_address ||
            (mc.end_address | page_end_suffix) == page_end_address) ||
           ((mc.start_address | page_end_suffix) < page_end_address &&
            (mc.end_address | page_end_suffix) > page_end_address);
  });
}

bool TMemCheck::Action(Common::DebugInterface* debug_interface, u64 value, u32 addr, bool write,
                       size_t size, u32 pc)
{
  if (!is_enabled)
    return false;

  if ((write && is_break_on_write) || (!write && is_break_on_read))
  {
    if (log_on_hit)
    {
      NOTICE_LOG_FMT(MEMMAP, "MBP {:08x} ({}) {}{} {:x} at {:08x} ({})", pc,
                     debug_interface->GetDescription(pc), write ? "Write" : "Read", size * 8, value,
                     addr, debug_interface->GetDescription(addr));
    }
    if (break_on_hit)
      return true;
  }
  return false;
}
