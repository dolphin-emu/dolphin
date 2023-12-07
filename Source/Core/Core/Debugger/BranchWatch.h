// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdio>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"

namespace Core
{
class CPUThreadGuard;
}

namespace Core
{
struct FakeBranchWatchKey
{
  u32 origin_addr;
  u32 destin_addr;
};
struct BranchWatchKey : FakeBranchWatchKey
{
  UGeckoInstruction original_inst;
};
struct BranchWatchValue
{
  std::size_t total_hits = 0;
  std::size_t hits_snapshot = 0;
};
}  // namespace Core

template <>
struct std::hash<Core::BranchWatchKey>
{
  std::size_t operator()(const Core::BranchWatchKey& s) const noexcept
  {
    return std::hash<u64>{}(Common::BitCast<u64>(static_cast<const Core::FakeBranchWatchKey&>(s)));
  }
};

namespace Core
{
inline bool operator==(const BranchWatchKey& lhs, const BranchWatchKey& rhs) noexcept
{
  std::hash<BranchWatchKey> hash;
  return hash(lhs) == hash(rhs) && lhs.original_inst.hex == rhs.original_inst.hex;
}

class BranchWatch
{
public:
  enum class Phase
  {
    Disabled,
    Blacklist,
    Reduction,
  };

  using Collection = std::unordered_map<BranchWatchKey, BranchWatchValue>;
  using Selection = std::vector<std::pair<Collection::value_type*, bool>>;

  // Start() is not CPUThread safe if Stop() was called very recently (microseconds)
  bool Start();
  void Stop();
  bool Pause();
  bool Resume();

  void Save(const Core::CPUThreadGuard& guard, std::FILE* file) const;
  void Load(const Core::CPUThreadGuard& guard, std::FILE* file);

  void IsolateHasExecuted(const Core::CPUThreadGuard& guard);
  void IsolateNotExecuted(const Core::CPUThreadGuard& guard);
  void IsolateWasOverwritten(const Core::CPUThreadGuard& guard);
  void IsolateNotOverwritten(const Core::CPUThreadGuard& guard);
  void ResetSelection(const Core::CPUThreadGuard& guard);

  Selection& GetSelection() { return m_selection; }
  const Selection& GetSelection() const { return m_selection; }

  std::size_t GetCollectionSize() const { return m_collection_v.size() + m_collection_p.size(); }
  std::size_t GetBlacklistSize() const { return m_blacklist_size; }
  Phase GetRecordingPhase() const { return m_recording_phase; };
  bool IsRecordingActive() const { return m_recording_active; }

  // There are some corner cases which can not be reconstructed when loading from a file.
  bool CanSave() const
  {
    return m_recording_phase != Phase::Disabled &&
           !(m_recording_phase == Phase::Reduction && m_selection.empty());
  }

  // CPUThread only
  void Hit(u32 origin, u32 destination, UGeckoInstruction inst, bool translate)
  {
    if (translate)
      HitV(this, origin, destination, inst.hex);
    else
      HitP(this, origin, destination, inst.hex);
  }

  static void HitV(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    branch_watch->m_collection_v[{{origin, destination}, inst}].total_hits += 1;
  }

  static void HitP(BranchWatch* branch_watch, u32 origin, u32 destination, u32 inst)
  {
    branch_watch->m_collection_p[{{origin, destination}, inst}].total_hits += 1;
  }

  static void HitV_n(BranchWatch* branch_watch, u64 fk, u32 inst, u32 n)
  {
    // This std::bit_cast trick is necessitated by fastcall ABI limitations in the JIT on Windows.
    branch_watch->m_collection_v[{Common::BitCast<FakeBranchWatchKey>(fk), inst}].total_hits += n;
  }

  static void HitP_n(BranchWatch* branch_watch, u64 fk, u32 inst, u32 n)
  {
    // This std::bit_cast trick is necessitated by fastcall ABI limitations in the JIT on Windows.
    branch_watch->m_collection_p[{Common::BitCast<FakeBranchWatchKey>(fk), inst}].total_hits += n;
  }

  // The JIT needs this value, but doesn't need to be a full-on friend.
  static constexpr std::size_t GetOffsetOfRecordingActive()
  {
    return offsetof(BranchWatch, m_recording_active);
  }

private:
  bool IsCollectedSelected(const Collection::value_type& v) const;

  Collection m_collection_v;  // Virtual memory
  Collection m_collection_p;  // Physical memory
  Selection m_selection;
  std::size_t m_blacklist_size;
  Phase m_recording_phase = Phase::Disabled;
  bool m_recording_active = false;
};
}  // namespace Core
