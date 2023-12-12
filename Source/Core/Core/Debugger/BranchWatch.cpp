// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/BranchWatch.h"

#include <algorithm>
#include <cstdio>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/MMU.h"

namespace Core
{
void BranchWatch::Clear(const Core::CPUThreadGuard&)
{
  m_selection.clear();
  m_collection_v.clear();
  m_collection_p.clear();
  m_recording_phase = Phase::Blacklist;
  m_blacklist_size = 0;
}

void BranchWatch::Save(const Core::CPUThreadGuard&, std::FILE* file) const
{
  if (!CanSave())
  {
    ASSERT_MSG(CORE, false, "BranchWatch can not be saved.");
    return;
  }
  if (file == nullptr)
    return;

  for (const Collection::value_type& kv : m_collection_v)
    fmt::println(file, "{:08x} {:08x} {:08x} {} {} {:d} {:d}", kv.first.origin_addr,
                 kv.first.destin_addr, kv.first.original_inst.hex, kv.second.total_hits,
                 kv.second.hits_snapshot, true, IsCollectedSelected(kv));
  for (const Collection::value_type& kv : m_collection_p)
    fmt::println(file, "{:08x} {:08x} {:08x} {} {} {:d} {:d}", kv.first.origin_addr,
                 kv.first.destin_addr, kv.first.original_inst.hex, kv.second.total_hits,
                 kv.second.hits_snapshot, false, IsCollectedSelected(kv));
}

void BranchWatch::Load(const Core::CPUThreadGuard&, std::FILE* file)
{
  if (file == nullptr)
    return;

  m_selection.clear();
  m_collection_v.clear();
  m_collection_p.clear();
  m_blacklist_size = 0;

  u32 origin_addr, destin_addr, hex, total_hits, hits_snapshot, is_selected, is_translated;
  while (std::fscanf(file, "%x %x %x %u %u %u %u", &origin_addr, &destin_addr, &hex, &total_hits,
                     &hits_snapshot, &is_translated, &is_selected) == 7)
  {
    auto [kv_iter, emplace_success] = [&]() {
      // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
      if (is_translated != 0)
      {
        return m_collection_v.try_emplace({{origin_addr, destin_addr}, hex},
                                          BranchWatchValue{total_hits, hits_snapshot});
      }
      else
      {
        return m_collection_p.try_emplace({{origin_addr, destin_addr}, hex},
                                          BranchWatchValue{total_hits, hits_snapshot});
      }
    }();
    if (emplace_success)
    {
      if (is_selected != 0)
        m_selection.emplace_back(&*kv_iter, is_translated != 0);
      else if (hits_snapshot != 0)
        ++m_blacklist_size;  // This will be very wrong when not in Blacklist mode. That's ok.
    }
  }
  m_recording_phase = m_selection.empty() ? Phase::Blacklist : Phase::Reduction;
}

void BranchWatch::IsolateHasExecuted(const Core::CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    m_selection.reserve(GetCollectionSize() - m_blacklist_size);
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        m_selection.emplace_back(&kv, true);
        kv.second.hits_snapshot = kv.second.total_hits;
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        m_selection.emplace_back(&kv, false);
        kv.second.hits_snapshot = kv.second.total_hits;
      }
    m_recording_phase = Phase::Reduction;
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& pair) -> bool {
      Collection::value_type* const kv = pair.first;
      if (kv->second.total_hits == kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateNotExecuted(const Core::CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    for (Collection::value_type& kv : m_collection_v)
      kv.second.hits_snapshot = kv.second.total_hits;
    for (Collection::value_type& kv : m_collection_p)
      kv.second.hits_snapshot = kv.second.total_hits;
    m_blacklist_size = GetCollectionSize();
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& pair) -> bool {
      Collection::value_type* const kv = pair.first;
      if (kv->second.total_hits != kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateWasOverwritten(const Core::CPUThreadGuard& guard)
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    // This is a dirty hack of the assumptions that make the blacklist phase work. If the
    // hits_snapshot is non-zero while in the blacklist phase, that means it has been marked
    // for exclusion from the transition to the reduction phase.
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Virtual);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex == read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Physical);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex == read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& pair) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, pair.first->first.origin_addr,
          pair.second ? PowerPC::RequestedAddressSpace::Virtual :
                        PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return pair.first->first.original_inst.hex == read_result->value;
    });
    return;
  }
}

void BranchWatch::IsolateNotOverwritten(const Core::CPUThreadGuard& guard)
{
  if (Core::GetState() == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
    // Same dirty hack with != rather than ==, see above for details
    for (Collection::value_type& kv : m_collection_v)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Virtual);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex != read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    for (Collection::value_type& kv : m_collection_p)
      if (kv.second.hits_snapshot == 0)
      {
        const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
            guard, kv.first.origin_addr, PowerPC::RequestedAddressSpace::Physical);
        if (!read_result.has_value())
          continue;
        if (kv.first.original_inst.hex != read_result->value)
          kv.second.hits_snapshot = ++m_blacklist_size;  // Literally any non-zero number will work.
      }
    return;
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& pair) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, pair.first->first.origin_addr,
          pair.second ? PowerPC::RequestedAddressSpace::Virtual :
                        PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return pair.first->first.original_inst.hex != read_result->value;
    });
    return;
  }
}

void BranchWatch::UpdateHitsSnapshot(const Core::CPUThreadGuard& guard)
{
  switch (m_recording_phase)
  {
  case Phase::Reduction:
    for (Selection::value_type& pair : m_selection)
      pair.first->second.hits_snapshot = pair.first->second.total_hits;
    [[fallthrough]];
  case Phase::Blacklist:
    return;
  }
}

bool BranchWatch::IsCollectedSelected(const Collection::value_type& v) const
{
  return std::find_if(m_selection.begin(), m_selection.end(), [&](const Selection::value_type& s) {
           return s.first == &v;
         }) != m_selection.end();
}
}  // namespace Core
