// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Expression.h"

namespace Core
{
class System;
}

struct TBreakPoint
{
  u32 address = 0;
  bool is_enabled = false;
  bool log_on_hit = false;
  bool break_on_hit = false;
  std::optional<Expression> condition;
};

struct TMemCheck
{
  u32 start_address = 0;
  u32 end_address = 0;

  bool is_enabled = true;
  bool is_ranged = false;

  bool is_break_on_read = false;
  bool is_break_on_write = false;

  bool log_on_hit = false;
  bool break_on_hit = false;

  u32 num_hits = 0;

  std::optional<Expression> condition;

  // returns whether to break
  bool Action(Core::System& system, u64 value, u32 addr, bool write, size_t size, u32 pc);
};

// Code breakpoints.
class BreakPoints
{
public:
  explicit BreakPoints(Core::System& system);
  BreakPoints(const BreakPoints& other) = delete;
  BreakPoints(BreakPoints&& other) = delete;
  BreakPoints& operator=(const BreakPoints& other) = delete;
  BreakPoints& operator=(BreakPoints&& other) = delete;
  ~BreakPoints();

  using TBreakPoints = std::vector<TBreakPoint>;
  using TBreakPointsStr = std::vector<std::string>;

  const TBreakPoints& GetBreakPoints() const { return m_breakpoints; }
  TBreakPointsStr GetStrings() const;
  void AddFromStrings(const TBreakPointsStr& bp_strings);

  bool IsAddressBreakPoint(u32 address) const;
  bool IsBreakPointEnable(u32 adresss) const;
  // Get the breakpoint in this address (for most purposes)
  const TBreakPoint* GetBreakpoint(u32 address) const;
  // Get the breakpoint in this address (ignore temporary breakpoint, e.g. for editing purposes)
  const TBreakPoint* GetRegularBreakpoint(u32 address) const;

  // Add BreakPoint. If one already exists on the same address, replace it.
  void Add(u32 address, bool break_on_hit, bool log_on_hit, std::optional<Expression> condition);
  void Add(u32 address);
  void Add(TBreakPoint bp);
  // Add temporary breakpoint (e.g., Step Over, Run to Here)
  // It can be on the same address of a regular breakpoint (it will have priority in this case)
  // It's cleared whenever the emulation is paused for any reason
  // (CPUManager::SetStateLocked(State::Paused))
  // TODO: Should it somehow force to resume emulation when called?
  void SetTemporary(u32 address);

  bool ToggleBreakPoint(u32 address);
  bool ToggleEnable(u32 address);

  // Remove Breakpoint. Returns whether it was removed.
  bool Remove(u32 address);
  void Clear();
  void ClearTemporary();

private:
  TBreakPoints m_breakpoints;
  std::optional<TBreakPoint> m_temp_breakpoint;
  Core::System& m_system;
};

class DelayedMemCheckUpdate;

// Memory breakpoints
class MemChecks
{
public:
  explicit MemChecks(Core::System& system);
  MemChecks(const MemChecks& other) = delete;
  MemChecks(MemChecks&& other) = delete;
  MemChecks& operator=(const MemChecks& other) = delete;
  MemChecks& operator=(MemChecks&& other) = delete;
  ~MemChecks();

  using TMemChecks = std::vector<TMemCheck>;
  using TMemChecksStr = std::vector<std::string>;

  const TMemChecks& GetMemChecks() const { return m_mem_checks; }
  TMemChecksStr GetStrings() const;
  void AddFromStrings(const TMemChecksStr& mc_strings);

  DelayedMemCheckUpdate Add(TMemCheck memory_check);

  bool ToggleEnable(u32 address);

  TMemCheck* GetMemCheck(u32 address, size_t size = 1);
  bool OverlapsMemcheck(u32 address, u32 length) const;
  DelayedMemCheckUpdate Remove(u32 address);

  void Update();
  void Clear();
  bool HasAny() const { return !m_mem_checks.empty(); }

  BitSet32 GetGPRsUsedInConditions() { return m_gprs_used_in_conditions; }
  BitSet32 GetFPRsUsedInConditions() { return m_fprs_used_in_conditions; }

private:
  // Returns whether any change was made
  bool UpdateRegistersUsedInConditions();

  TMemChecks m_mem_checks;
  Core::System& m_system;
  BitSet32 m_gprs_used_in_conditions;
  BitSet32 m_fprs_used_in_conditions;
  bool m_mem_breakpoints_set = false;
};

class DelayedMemCheckUpdate final
{
public:
  DelayedMemCheckUpdate(MemChecks* memchecks, bool update_needed = false)
      : m_memchecks(memchecks), m_update_needed(update_needed)
  {
  }

  DelayedMemCheckUpdate(const DelayedMemCheckUpdate&) = delete;
  DelayedMemCheckUpdate(DelayedMemCheckUpdate&& other) = delete;
  DelayedMemCheckUpdate& operator=(const DelayedMemCheckUpdate&) = delete;
  DelayedMemCheckUpdate& operator=(DelayedMemCheckUpdate&& other) = delete;

  ~DelayedMemCheckUpdate()
  {
    if (m_update_needed)
      m_memchecks->Update();
  }

  DelayedMemCheckUpdate& operator|=(DelayedMemCheckUpdate&& other)
  {
    if (m_memchecks == other.m_memchecks)
    {
      m_update_needed |= other.m_update_needed;
      other.m_update_needed = false;
    }
    return *this;
  }

  operator bool() const { return m_update_needed; }

private:
  MemChecks* m_memchecks;
  bool m_update_needed;
};
