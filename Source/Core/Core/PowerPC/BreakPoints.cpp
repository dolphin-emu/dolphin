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
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"

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
