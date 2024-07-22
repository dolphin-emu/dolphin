// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/BranchWatch.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/MMU.h"

namespace Core
{
void BranchWatch::Clear(const CPUThreadGuard&)
{
  m_selection.clear();
  m_collection_vt.clear();
  m_collection_vf.clear();
  m_collection_pt.clear();
  m_collection_pf.clear();
  m_recording_phase = Phase::Blacklist;
  m_blacklist_size = 0;
}

// This is a bitfield aggregate of metadata required to reconstruct a BranchWatch's Collections and
// Selection from a text file (a snapshot). For maximum forward compatibility, should that ever be
// required, the StorageType is an unsigned long long instead of something more reasonable like an
// unsigned int or u8. This is because the snapshot text file format contains no version info.
union USnapshotMetadata
{
  using Inspection = BranchWatch::SelectionInspection;
  using StorageType = unsigned long long;

  static_assert(Inspection::EndOfEnumeration == Inspection{(1u << 3) + 1});

  StorageType hex;

  BitField<0, 1, bool, StorageType> is_virtual;
  BitField<1, 1, bool, StorageType> condition;
  BitField<2, 1, bool, StorageType> is_selected;
  BitField<3, 4, Inspection, StorageType> inspection;

  USnapshotMetadata() : hex(0) {}
  explicit USnapshotMetadata(bool is_virtual_, bool condition_, bool is_selected_,
                             Inspection inspection_)
      : USnapshotMetadata()
  {
    is_virtual = is_virtual_;
    condition = condition_;
    is_selected = is_selected_;
    inspection = inspection_;
  }
};

void BranchWatch::Save(const CPUThreadGuard& guard, std::FILE* file) const
{
  if (!CanSave())
  {
    ASSERT_MSG(CORE, false, "BranchWatch can not be saved.");
    return;
  }
  if (file == nullptr)
    return;

  const auto routine = [&](const Collection& collection, bool is_virtual, bool condition) {
    for (const Collection::value_type& kv : collection)
    {
      const auto iter = std::ranges::find_if(m_selection, [&](const Selection::value_type& value) {
        return value.collection_ptr == &kv;
      });
      fmt::println(file, "{:08x} {:08x} {:08x} {} {} {:x}", kv.first.origin_addr,
                   kv.first.destin_addr, kv.first.original_inst.hex, kv.second.total_hits,
                   kv.second.hits_snapshot,
                   iter == m_selection.end() ?
                       USnapshotMetadata(is_virtual, condition, false, {}).hex :
                       USnapshotMetadata(is_virtual, condition, true, iter->inspection).hex);
    }
  };
  routine(m_collection_vt, true, true);
  routine(m_collection_pt, false, true);
  routine(m_collection_vf, true, false);
  routine(m_collection_pf, false, false);
}

void BranchWatch::Load(const CPUThreadGuard& guard, std::FILE* file)
{
  if (file == nullptr)
    return;

  Clear(guard);

  u32 origin_addr, destin_addr, inst_hex;
  std::size_t total_hits, hits_snapshot;
  USnapshotMetadata snapshot_metadata = {};
  while (std::fscanf(file, "%x %x %x %zu %zu %llx", &origin_addr, &destin_addr, &inst_hex,
                     &total_hits, &hits_snapshot, &snapshot_metadata.hex) == 6)
  {
    const bool is_virtual = snapshot_metadata.is_virtual;
    const bool condition = snapshot_metadata.condition;

    const auto [kv_iter, emplace_success] =
        GetCollection(is_virtual, condition)
            .try_emplace({{origin_addr, destin_addr}, inst_hex},
                         BranchWatchCollectionValue{total_hits, hits_snapshot});

    if (!emplace_success)
      continue;

    if (snapshot_metadata.is_selected)
    {
      // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
      m_selection.emplace_back(BranchWatchSelectionValueType{&*kv_iter, is_virtual, condition,
                                                             snapshot_metadata.inspection});
    }
    else if (hits_snapshot != 0)
    {
      ++m_blacklist_size;  // This will be very wrong when not in Blacklist mode. That's ok.
    }
  }

  if (!m_selection.empty())
    m_recording_phase = Phase::Reduction;
}

void BranchWatch::IsolateHasExecuted(const CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
  {
    m_selection.reserve(GetCollectionSize() - m_blacklist_size);
    const auto routine = [&](Collection& collection, bool is_virtual, bool condition) {
      for (Collection::value_type& kv : collection)
      {
        if (kv.second.hits_snapshot == 0)
        {
          // TODO C++20: Parenthesized initialization of aggregates has bad compiler support.
          m_selection.emplace_back(
              BranchWatchSelectionValueType{&kv, is_virtual, condition, SelectionInspection{}});
          kv.second.hits_snapshot = kv.second.total_hits;
        }
      }
    };
    routine(m_collection_vt, true, true);
    routine(m_collection_vf, true, false);
    routine(m_collection_pt, false, true);
    routine(m_collection_pf, false, false);
    m_recording_phase = Phase::Reduction;
    return;
  }
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& value) -> bool {
      Collection::value_type* const kv = value.collection_ptr;
      if (kv->second.total_hits == kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateNotExecuted(const CPUThreadGuard&)
{
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
  {
    const auto routine = [&](Collection& collection) {
      for (Collection::value_type& kv : collection)
        kv.second.hits_snapshot = kv.second.total_hits;
    };
    routine(m_collection_vt);
    routine(m_collection_vf);
    routine(m_collection_pt);
    routine(m_collection_pf);
    m_blacklist_size = GetCollectionSize();
    return;
  }
  case Phase::Reduction:
    std::erase_if(m_selection, [](const Selection::value_type& value) -> bool {
      Collection::value_type* const kv = value.collection_ptr;
      if (kv->second.total_hits != kv->second.hits_snapshot)
        return true;
      kv->second.hits_snapshot = kv->second.total_hits;
      return false;
    });
    return;
  }
}

void BranchWatch::IsolateWasOverwritten(const CPUThreadGuard& guard)
{
  if (Core::GetState(guard.GetSystem()) == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
  {
    // This is a dirty hack of the assumptions that make the blacklist phase work. If the
    // hits_snapshot is non-zero while in the blacklist phase, that means it has been marked
    // for exclusion from the transition to the reduction phase.
    const auto routine = [&](Collection& collection, PowerPC::RequestedAddressSpace address_space) {
      for (Collection::value_type& kv : collection)
      {
        if (kv.second.hits_snapshot == 0)
        {
          const std::optional read_result =
              PowerPC::MMU::HostTryReadInstruction(guard, kv.first.origin_addr, address_space);
          if (!read_result.has_value())
            continue;
          if (kv.first.original_inst.hex == read_result->value)
            kv.second.hits_snapshot = ++m_blacklist_size;  // Any non-zero number will work.
        }
      }
    };
    routine(m_collection_vt, PowerPC::RequestedAddressSpace::Virtual);
    routine(m_collection_vf, PowerPC::RequestedAddressSpace::Virtual);
    routine(m_collection_pt, PowerPC::RequestedAddressSpace::Physical);
    routine(m_collection_pf, PowerPC::RequestedAddressSpace::Physical);
    return;
  }
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& value) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, value.collection_ptr->first.origin_addr,
          value.is_virtual ? PowerPC::RequestedAddressSpace::Virtual :
                             PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return value.collection_ptr->first.original_inst.hex == read_result->value;
    });
    return;
  }
}

void BranchWatch::IsolateNotOverwritten(const CPUThreadGuard& guard)
{
  if (Core::GetState(guard.GetSystem()) == Core::State::Uninitialized)
  {
    ASSERT_MSG(CORE, false, "Core is uninitialized.");
    return;
  }
  switch (m_recording_phase)
  {
  case Phase::Blacklist:
  {
    // Same dirty hack with != rather than ==, see above for details
    const auto routine = [&](Collection& collection, PowerPC::RequestedAddressSpace address_space) {
      for (Collection::value_type& kv : collection)
        if (kv.second.hits_snapshot == 0)
        {
          const std::optional read_result =
              PowerPC::MMU::HostTryReadInstruction(guard, kv.first.origin_addr, address_space);
          if (!read_result.has_value())
            continue;
          if (kv.first.original_inst.hex != read_result->value)
            kv.second.hits_snapshot = ++m_blacklist_size;  // Any non-zero number will work.
        }
    };
    routine(m_collection_vt, PowerPC::RequestedAddressSpace::Virtual);
    routine(m_collection_vf, PowerPC::RequestedAddressSpace::Virtual);
    routine(m_collection_pt, PowerPC::RequestedAddressSpace::Physical);
    routine(m_collection_pf, PowerPC::RequestedAddressSpace::Physical);
    return;
  }
  case Phase::Reduction:
    std::erase_if(m_selection, [&guard](const Selection::value_type& value) -> bool {
      const std::optional read_result = PowerPC::MMU::HostTryReadInstruction(
          guard, value.collection_ptr->first.origin_addr,
          value.is_virtual ? PowerPC::RequestedAddressSpace::Virtual :
                             PowerPC::RequestedAddressSpace::Physical);
      if (!read_result.has_value())
        return false;
      return value.collection_ptr->first.original_inst.hex != read_result->value;
    });
    return;
  }
}

void BranchWatch::UpdateHitsSnapshot()
{
  switch (m_recording_phase)
  {
  case Phase::Reduction:
    for (Selection::value_type& value : m_selection)
      value.collection_ptr->second.hits_snapshot = value.collection_ptr->second.total_hits;
    return;
  case Phase::Blacklist:
    return;
  }
}

void BranchWatch::ClearSelectionInspection()
{
  std::ranges::for_each(m_selection, [](Selection::value_type& value) { value.inspection = {}; });
}

void BranchWatch::SetSelectedInspected(std::size_t idx, SelectionInspection inspection)
{
  m_selection[idx].inspection |= inspection;
}
}  // namespace Core
