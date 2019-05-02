// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
class DebugInterface;
}

struct TBreakPoint
{
  u32 address = 0;
  bool is_enabled = false;
  bool is_temporary = false;
};

struct TMemCheck
{
  u32 start_address = 0;
  u32 end_address = 0;

  bool is_ranged = false;

  bool is_break_on_read = false;
  bool is_break_on_write = false;

  bool log_on_hit = false;
  bool break_on_hit = false;

  u32 num_hits = 0;

  // returns whether to break
  bool Action(Common::DebugInterface* dbg_interface, u32 value, u32 addr, bool write, size_t size,
              u32 pc);
};

// Code breakpoints.
class BreakPoints
{
public:
  using TBreakPoints = std::vector<TBreakPoint>;
  using TBreakPointsStr = std::vector<std::string>;

  const TBreakPoints& GetBreakPoints() const { return m_breakpoints; }
  TBreakPointsStr GetStrings() const;
  void AddFromStrings(const TBreakPointsStr& bp_strings);

  // is address breakpoint
  bool IsAddressBreakPoint(u32 address) const;
  bool IsTempBreakPoint(u32 address) const;

  // Add BreakPoint
  void Add(u32 address, bool temp = false);
  void Add(const TBreakPoint& bp);

  // Remove Breakpoint
  void Remove(u32 address);
  void Clear();
  void ClearAllTemporary();

private:
  TBreakPoints m_breakpoints;
};

// Memory breakpoints
class MemChecks
{
public:
  using TMemChecks = std::vector<TMemCheck>;
  using TMemChecksStr = std::vector<std::string>;

  const TMemChecks& GetMemChecks() const { return m_mem_checks; }
  TMemChecksStr GetStrings() const;
  void AddFromStrings(const TMemChecksStr& mc_strings);

  void Add(const TMemCheck& memory_check);

  // memory breakpoint
  TMemCheck* GetMemCheck(u32 address, size_t size = 1);
  bool OverlapsMemcheck(u32 address, u32 length) const;
  void Remove(u32 address);

  void Clear() { m_mem_checks.clear(); }
  bool HasAny() const { return !m_mem_checks.empty(); }

private:
  TMemChecks m_mem_checks;
};
