// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bit>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Core/PowerPC/Gekko.h"

namespace Core
{
class CPUThreadGuard;
}

namespace Core
{
struct FakeBranchWatchCollectionKey
{
  u32 origin_addr;
  u32 destin_addr;

  constexpr operator u64() const { return std::bit_cast<u64>(*this); }
};
struct BranchWatchCollectionKey : FakeBranchWatchCollectionKey
{
  UGeckoInstruction original_inst;
};
struct BranchWatchCollectionValue
{
  std::size_t total_hits = 0;
  std::size_t hits_snapshot = 0;
};
}  // namespace Core

template <>
struct std::hash<Core::BranchWatchCollectionKey>
{
  std::size_t operator()(const Core::BranchWatchCollectionKey& s) const noexcept
  {
    return std::hash<u64>{}(static_cast<const Core::FakeBranchWatchCollectionKey&>(s));
  }
};

namespace Core
{
inline bool operator==(const BranchWatchCollectionKey& lhs,
                       const BranchWatchCollectionKey& rhs) noexcept
{
  const std::hash<BranchWatchCollectionKey> hash;
  return hash(lhs) == hash(rhs) && lhs.original_inst.hex == rhs.original_inst.hex;
}

enum class BranchWatchSelectionInspection : u8
{
  SetOriginNOP = 1u << 0,
  SetDestinBLR = 1u << 1,
  SetOriginSymbolBLR = 1u << 2,
  SetDestinSymbolBLR = 1u << 3,
  InvertBranchOption = 1u << 4,  // Used for both conditions and decrement checks.
  MakeUnconditional = 1u << 5,
  EndOfEnumeration,
};

constexpr BranchWatchSelectionInspection operator|(BranchWatchSelectionInspection lhs,
                                                   BranchWatchSelectionInspection rhs)
{
  return static_cast<BranchWatchSelectionInspection>(Common::ToUnderlying(lhs) |
                                                     Common::ToUnderlying(rhs));
}

constexpr BranchWatchSelectionInspection operator&(BranchWatchSelectionInspection lhs,
                                                   BranchWatchSelectionInspection rhs)
{
  return static_cast<BranchWatchSelectionInspection>(Common::ToUnderlying(lhs) &
                                                     Common::ToUnderlying(rhs));
}

constexpr BranchWatchSelectionInspection& operator|=(BranchWatchSelectionInspection& self,
                                                     BranchWatchSelectionInspection other)
{
  return self = self | other;
}

using BranchWatchCollection =
    std::unordered_map<BranchWatchCollectionKey, BranchWatchCollectionValue>;

struct BranchWatchSelectionValueType
{
  using Inspection = BranchWatchSelectionInspection;

  BranchWatchCollection::value_type* collection_ptr;
  bool is_virtual;
  bool condition;
  // This is moreso a GUI thing, but it works best in the Core code for multiple reasons.
  Inspection inspection;
};

using BranchWatchSelection = std::vector<BranchWatchSelectionValueType>;

enum class BranchWatchPhase : bool
{
  Blacklist,
  Reduction,
};

class BranchWatch final  // Class is final to enforce the safety of GetOffsetOfRecordingActive().
{
public:
  using Collection = BranchWatchCollection;
  using Selection = BranchWatchSelection;
  using Phase = BranchWatchPhase;
  using SelectionInspection = BranchWatchSelectionInspection;

  bool GetRecordingActive() const { return m_recording_active; }
  void SetRecordingActive(const CPUThreadGuard& guard, bool active) { m_recording_active = active; }
  void Clear(const CPUThreadGuard& guard);

  void Save(const CPUThreadGuard& guard, std::FILE* file) const;
  void Load(const CPUThreadGuard& guard, std::FILE* file);

  void IsolateHasExecuted(const CPUThreadGuard& guard);
  void IsolateNotExecuted(const CPUThreadGuard& guard);
  void IsolateWasOverwritten(const CPUThreadGuard& guard);
  void IsolateNotOverwritten(const CPUThreadGuard& guard);
  void UpdateHitsSnapshot();
  void ClearSelectionInspection();
  void SetSelectedInspected(std::size_t idx, SelectionInspection inspection);

  Selection& GetSelection() { return m_selection; }
  const Selection& GetSelection() const { return m_selection; }

  std::size_t GetCollectionSize() const
  {
    return m_collection_vt.size() + m_collection_vf.size() + m_collection_pt.size() +
           m_collection_pf.size();
  }
  std::size_t GetBlacklistSize() const { return m_blacklist_size; }
  Phase GetRecordingPhase() const { return m_recording_phase; }

  // An empty selection in reduction mode can't be reconstructed when loading from a file.
  bool CanSave() const { return !(m_recording_phase == Phase::Reduction && m_selection.empty()); }

  // All Hit member functions are for the CPUThread only. The static ones are static to remain
  // compatible with the JITs' ABI_CallFunction function, which doesn't support non-static member
  // functions. HitXX_fk are optimized for when origin and destination can be passed in one register
  // easily as a Core::FakeBranchWatchCollectionKey (abbreviated as "fk"). HitXX_fk_n are the same,
  // but also increment the total_hits by N (see dcbx JIT code).
  static void HitVirtualTrue_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_vt[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitPhysicalTrue_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_pt[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitVirtualFalse_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_vf[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitPhysicalFalse_fk(BranchWatch* branch_watch, u64 fake_key, u32 inst)
  {
    branch_watch->m_collection_pf[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += 1;
  }

  static void HitVirtualTrue_fk_n(BranchWatch* branch_watch, u64 fake_key, u32 inst, u32 n)
  {
    branch_watch->m_collection_vt[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += n;
  }

  static void HitPhysicalTrue_fk_n(BranchWatch* branch_watch, u64 fake_key, u32 inst, u32 n)
  {
    branch_watch->m_collection_pt[{std::bit_cast<FakeBranchWatchCollectionKey>(fake_key), inst}]
        .total_hits += n;
  }

  // HitVirtualFalse_fk_n and HitPhysicalFalse_fk_n are never used, so they are omitted here.

  static void HitVirtualTrue(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitVirtualTrue_fk(branch_watch, FakeBranchWatchCollectionKey{origin, destination}, inst);
  }

  static void HitPhysicalTrue(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitPhysicalTrue_fk(branch_watch, FakeBranchWatchCollectionKey{origin, destination}, inst);
  }

  static void HitVirtualFalse(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitVirtualFalse_fk(branch_watch, FakeBranchWatchCollectionKey{origin, destination}, inst);
  }

  static void HitPhysicalFalse(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    HitPhysicalFalse_fk(branch_watch, FakeBranchWatchCollectionKey{origin, destination}, inst);
  }

  void HitTrue(u32 origin, u32 destination, UGeckoInstruction inst, bool translate)
  {
    if (translate)
      HitVirtualTrue(this, origin, destination, inst.hex);
    else
      HitPhysicalTrue(this, origin, destination, inst.hex);
  }

  void HitFalse(u32 origin, u32 destination, UGeckoInstruction inst, bool translate)
  {
    if (translate)
      HitVirtualFalse(this, origin, destination, inst.hex);
    else
      HitPhysicalFalse(this, origin, destination, inst.hex);
  }

  // The JIT needs this value, but doesn't need to be a full-on friend.
  static constexpr int GetOffsetOfRecordingActive()
  {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    return offsetof(BranchWatch, m_recording_active);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  }

private:
  Collection& GetCollectionV(bool condition)
  {
    if (condition)
      return m_collection_vt;
    return m_collection_vf;
  }

  Collection& GetCollectionP(bool condition)
  {
    if (condition)
      return m_collection_pt;
    return m_collection_pf;
  }

  Collection& GetCollection(bool is_virtual, bool condition)
  {
    if (is_virtual)
      return GetCollectionV(condition);
    return GetCollectionP(condition);
  }

  std::size_t m_blacklist_size = 0;
  Phase m_recording_phase = Phase::Blacklist;
  bool m_recording_active = false;
  Collection m_collection_vt;  // virtual address space | true path
  Collection m_collection_vf;  // virtual address space | false path
  Collection m_collection_pt;  // physical address space | true path
  Collection m_collection_pf;  // physical address space | false path
  Selection m_selection;
};

#if _M_X86_64
static_assert(BranchWatch::GetOffsetOfRecordingActive() < 0x80);  // Makes JIT code smaller.
#endif
}  // namespace Core
