// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Expression.h"

namespace Common
{
class DebugInterface;
}
namespace Core
{
class System;
}

struct TBreakPoint
{
  u32 address = 0;
  bool is_enabled = false;
  bool is_temporary = false;
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
  bool Action(Core::System& system, Common::DebugInterface* debug_interface, u64 value, u32 addr,
              bool write, size_t size, u32 pc);
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

  // is address breakpoint
  bool IsAddressBreakPoint(u32 address) const;
  bool IsBreakPointEnable(u32 adresss) const;
  bool IsTempBreakPoint(u32 address) const;
  const TBreakPoint* GetBreakpoint(u32 address) const;

  // Add BreakPoint
  void Add(u32 address, bool temp, bool break_on_hit, bool log_on_hit,
           std::optional<Expression> condition);
  void Add(u32 address, bool temp = false);
  void Add(TBreakPoint bp);

  // Modify Breakpoint
  bool ToggleBreakPoint(u32 address);

  // Remove Breakpoint
  void Remove(u32 address);
  void Clear();
  void ClearAllTemporary();

private:
  TBreakPoints m_breakpoints;
  Core::System& m_system;
};

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

  void Add(TMemCheck memory_check);

  bool ToggleBreakPoint(u32 address);

  // memory breakpoint
  TMemCheck* GetMemCheck(u32 address, size_t size = 1);
  bool OverlapsMemcheck(u32 address, u32 length) const;
  void Remove(u32 address);

  void Clear();
  bool HasAny() const { return !m_mem_checks.empty(); }

private:
  TMemChecks m_mem_checks;
  Core::System& m_system;
};
