// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/BreakPoints.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/Debugger/DebugInterface.h"
#include "Core/PowerPC/Expression.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

BreakPoints::BreakPoints(Core::System& system) : m_system(system)
{
}

BreakPoints::~BreakPoints() = default;

bool BreakPoints::IsAddressBreakPoint(u32 address) const
{
  return GetBreakpoint(address) != nullptr;
}

bool BreakPoints::IsBreakPointEnable(u32 address) const
{
  const TBreakPoint* bp = GetBreakpoint(address);
  return bp != nullptr && bp->is_enabled;
}

const TBreakPoint* BreakPoints::GetBreakpoint(u32 address) const
{
  // Give priority to the temporary breakpoint (it could be in the same address of a regular
  // breakpoint that doesn't break)
  if (m_temp_breakpoint && m_temp_breakpoint->address == address)
    return &*m_temp_breakpoint;

  return GetRegularBreakpoint(address);
}

const TBreakPoint* BreakPoints::GetRegularBreakpoint(u32 address) const
{
  auto bp = std::ranges::find_if(m_breakpoints,
                                 [address](const auto& bp_) { return bp_.address == address; });

  if (bp == m_breakpoints.end())
    return nullptr;

  return &*bp;
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
  TBreakPointsStr bp_strings;
  for (const TBreakPoint& bp : m_breakpoints)
  {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << fmt::format("${:08x} ", bp.address);
    if (bp.is_enabled)
      ss << "n";
    if (bp.log_on_hit)
      ss << "l";
    if (bp.break_on_hit)
      ss << "b";
    if (bp.condition)
      ss << "c " << bp.condition->GetText();
    bp_strings.emplace_back(ss.str());
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

    if (iss.peek() == '$')
      iss.ignore();
    iss >> std::hex >> bp.address;
    iss >> flags;
    bp.is_enabled = flags.find('n') != flags.npos;
    bp.log_on_hit = flags.find('l') != flags.npos;
    bp.break_on_hit = flags.find('b') != flags.npos;
    if (flags.find('c') != std::string::npos)
    {
      iss >> std::ws;
      std::string condition;
      std::getline(iss, condition);
      bp.condition = Expression::TryParse(condition);
    }
    Add(std::move(bp));
  }
}

void BreakPoints::Add(TBreakPoint bp)
{
  if (IsAddressBreakPoint(bp.address))
    return;

  m_system.GetJitInterface().InvalidateICache(bp.address, 4, true);

  m_breakpoints.emplace_back(std::move(bp));
}

void BreakPoints::Add(u32 address)
{
  BreakPoints::Add(address, true, false, std::nullopt);
}

void BreakPoints::Add(u32 address, bool break_on_hit, bool log_on_hit,
                      std::optional<Expression> condition)
{
  // Check for existing breakpoint, and overwrite with new info.
  // This is assuming we usually want the new breakpoint over an old one.
  auto iter = std::ranges::find_if(m_breakpoints,
                                   [address](const auto& bp) { return bp.address == address; });

  TBreakPoint bp;  // breakpoint settings
  bp.is_enabled = true;
  bp.break_on_hit = break_on_hit;
  bp.log_on_hit = log_on_hit;
  bp.address = address;
  bp.condition = std::move(condition);

  if (iter != m_breakpoints.end())  // We found an existing breakpoint
  {
    bp.is_enabled = iter->is_enabled;
    *iter = std::move(bp);
  }
  else
  {
    m_breakpoints.emplace_back(std::move(bp));
  }

  m_system.GetJitInterface().InvalidateICache(address, 4, true);
}

void BreakPoints::SetTemporary(u32 address)
{
  TBreakPoint bp;  // breakpoint settings
  bp.is_enabled = true;
  bp.break_on_hit = true;
  bp.log_on_hit = false;
  bp.address = address;
  bp.condition = std::nullopt;

  m_temp_breakpoint.emplace(std::move(bp));

  m_system.GetJitInterface().InvalidateICache(address, 4, true);
}

bool BreakPoints::ToggleBreakPoint(u32 address)
{
  if (!Remove(address))
  {
    Add(address);
    return true;
  }
  return false;
}

bool BreakPoints::ToggleEnable(u32 address)
{
  auto iter = std::ranges::find_if(m_breakpoints,
                                   [address](const auto& bp) { return bp.address == address; });

  if (iter == m_breakpoints.end())
    return false;

  iter->is_enabled = !iter->is_enabled;
  return true;
}

bool BreakPoints::Remove(u32 address)
{
  const auto iter = std::ranges::find_if(
      m_breakpoints, [address](const auto& bp) { return bp.address == address; });

  if (iter == m_breakpoints.cend())
    return false;

  m_breakpoints.erase(iter);
  m_system.GetJitInterface().InvalidateICache(address, 4, true);

  return true;
}

void BreakPoints::Clear()
{
  for (const TBreakPoint& bp : m_breakpoints)
  {
    m_system.GetJitInterface().InvalidateICache(bp.address, 4, true);
  }

  m_breakpoints.clear();
  ClearTemporary();
}

void BreakPoints::ClearTemporary()
{
  if (m_temp_breakpoint)
  {
    m_system.GetJitInterface().InvalidateICache(m_temp_breakpoint->address, 4, true);
    m_temp_breakpoint.reset();
  }
}

MemChecks::MemChecks(Core::System& system) : m_system(system)
{
}

MemChecks::~MemChecks() = default;

MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
  TMemChecksStr mc_strings;
  for (const TMemCheck& mc : m_mem_checks)
  {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << fmt::format("${:08x} {:08x} ", mc.start_address, mc.end_address);
    if (mc.is_enabled)
      ss << 'n';
    if (mc.is_break_on_read)
      ss << 'r';
    if (mc.is_break_on_write)
      ss << 'w';
    if (mc.log_on_hit)
      ss << 'l';
    if (mc.break_on_hit)
      ss << 'b';
    if (mc.condition)
      ss << "c " << mc.condition->GetText();

    mc_strings.emplace_back(ss.str());
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

    if (iss.peek() == '$')
      iss.ignore();

    std::string flags;
    iss >> std::hex >> mc.start_address >> mc.end_address >> flags;

    mc.is_ranged = mc.start_address != mc.end_address;
    mc.is_enabled = flags.find('n') != flags.npos;
    mc.is_break_on_read = flags.find('r') != flags.npos;
    mc.is_break_on_write = flags.find('w') != flags.npos;
    mc.log_on_hit = flags.find('l') != flags.npos;
    mc.break_on_hit = flags.find('b') != flags.npos;
    if (flags.find('c') != std::string::npos)
    {
      iss >> std::ws;
      std::string condition;
      std::getline(iss, condition);
      mc.condition = Expression::TryParse(condition);
    }

    Add(std::move(mc));
  }
}

void MemChecks::Add(TMemCheck memory_check)
{
  bool had_any = HasAny();

  const Core::CPUThreadGuard guard(m_system);
  // Check for existing breakpoint, and overwrite with new info.
  // This is assuming we usually want the new breakpoint over an old one.
  const u32 address = memory_check.start_address;
  auto old_mem_check = std::ranges::find_if(
      m_mem_checks, [address](const auto& check) { return check.start_address == address; });
  if (old_mem_check != m_mem_checks.end())
  {
    memory_check.is_enabled = old_mem_check->is_enabled;  // Preserve enabled status
    *old_mem_check = std::move(memory_check);
    old_mem_check->num_hits = 0;
  }
  else
  {
    m_mem_checks.emplace_back(std::move(memory_check));
  }
  // If this is the first one, clear the JIT cache so it can switch to
  // watchpoint-compatible code.
  if (!had_any)
    m_system.GetJitInterface().ClearCache(guard);
  m_system.GetMMU().DBATUpdated();
}

bool MemChecks::ToggleEnable(u32 address)
{
  auto iter = std::ranges::find_if(
      m_mem_checks, [address](const auto& bp) { return bp.start_address == address; });

  if (iter == m_mem_checks.end())
    return false;

  iter->is_enabled = !iter->is_enabled;
  return true;
}

bool MemChecks::Remove(u32 address)
{
  const auto iter = std::ranges::find_if(
      m_mem_checks, [address](const auto& check) { return check.start_address == address; });

  if (iter == m_mem_checks.cend())
    return false;

  const Core::CPUThreadGuard guard(m_system);
  m_mem_checks.erase(iter);
  if (!HasAny())
    m_system.GetJitInterface().ClearCache(guard);
  m_system.GetMMU().DBATUpdated();
  return true;
}

void MemChecks::Clear()
{
  const Core::CPUThreadGuard guard(m_system);
  m_mem_checks.clear();
  m_system.GetJitInterface().ClearCache(guard);
  m_system.GetMMU().DBATUpdated();
}

TMemCheck* MemChecks::GetMemCheck(u32 address, size_t size)
{
  const auto iter = std::ranges::find_if(m_mem_checks, [address, size](const auto& mc) {
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

  return std::ranges::any_of(m_mem_checks, [&](const auto& mc) {
    return ((mc.start_address | page_end_suffix) == page_end_address ||
            (mc.end_address | page_end_suffix) == page_end_address) ||
           ((mc.start_address | page_end_suffix) < page_end_address &&
            (mc.end_address | page_end_suffix) > page_end_address);
  });
}

bool TMemCheck::Action(Core::System& system, u64 value, u32 addr, bool write, size_t size, u32 pc)
{
  if (!is_enabled)
    return false;

  if (((write && is_break_on_write) || (!write && is_break_on_read)) &&
      EvaluateCondition(system, this->condition))
  {
    if (log_on_hit)
    {
      auto& ppc_symbol_db = system.GetPPCSymbolDB();
      NOTICE_LOG_FMT(MEMMAP, "MBP {:08x} ({}) {}{} {:x} at {:08x} ({})", pc,
                     ppc_symbol_db.GetDescription(pc), write ? "Write" : "Read", size * 8, value,
                     addr, ppc_symbol_db.GetDescription(addr));
    }
    if (break_on_hit)
      return true;
  }
  return false;
}
