// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Core
{
class CPUThreadGuard;
}

struct InstructionAttributes
{
  u32 address = 0;
  std::string instruction = "";
  std::string reg0 = "";
  std::string reg1 = "";
  std::string reg2 = "";
  std::string reg3 = "";
  std::optional<u32> memory_target = std::nullopt;
  u32 memory_target_size = 4;
  bool is_store = false;
  bool is_load = false;
};

struct TraceOutput
{
  u32 address = 0;
  std::optional<u32> memory_target = std::nullopt;
  std::string instruction;
};

struct AutoStepResults
{
  std::vector<std::string> reg_tracked;
  std::set<u32> mem_tracked;
  u32 count = 0;
  bool timed_out = false;
  bool trackers_empty = false;
};

enum class HitType : u32
{
  SKIP = (1 << 0),       // Not a hit
  OVERWRITE = (1 << 1),  // Tracked value gets overwritten by untracked. Typically skipped.
  MOVED = (1 << 2),      // Target duplicated to another register, unchanged.
  SAVELOAD = (1 << 3),   // Target saved or loaded. Priority over Pointer.
  POINTER = (1 << 4),    // Target used as pointer/offset for save or load
  PASSIVE = (1 << 5),    // Conditional, etc, but not pointer. Unchanged
  ACTIVE = (1 << 6),     // Math, etc. Changed.
  UPDATED = (1 << 7),    // Masked or math without changing register.
};

class CodeTrace
{
public:
  enum class AutoStop
  {
    Always,
    Used,
    Changed
  };

  void SetRegTracked(const std::string& reg);
  AutoStepResults AutoStepping(const Core::CPUThreadGuard& guard, bool continue_previous = false,
                               AutoStop stop_on = AutoStop::Always);

private:
  InstructionAttributes GetInstructionAttributes(const TraceOutput& line) const;
  TraceOutput SaveCurrentInstruction(const Core::CPUThreadGuard& guard) const;
  HitType TraceLogic(const TraceOutput& current_instr, bool first_hit = false);

  bool m_recording = false;
  std::vector<std::string> m_reg_autotrack;
  std::set<u32> m_mem_autotrack;
};
