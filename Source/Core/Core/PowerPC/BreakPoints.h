// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class DebugInterface;

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
  bool Action(DebugInterface* dbg_interface, u32 value, u32 addr, bool write, size_t size, u32 pc);
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
