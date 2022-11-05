// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
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
  std::array<std::string, 4> reg{"", "", "", ""};
  std::optional<u32> memory_target = std::nullopt;
  u32 memory_target_size = 4;
  bool is_store = false;
  bool is_load = false;
};

struct TraceOutput
{
  u32 address;
  std::optional<u32> memory_target = std::nullopt;
  std::string instruction;
  std::array<double, 4> value{10, 10, 10, 10};
  std::array<std::string, 4> regs{"", "", "", ""};
};

struct AutoStepResults
{
  std::set<std::string> reg_tracked;
  std::set<u32> mem_tracked;
  u32 count = 0;
  bool timed_out = false;
  bool trackers_empty = false;
};

struct Matches
{
  std::array<bool, 4> reg;
  bool reg123 = false;
  bool loadstore = false;
  bool mem = false;
};

enum class HitType : u32
{
  STOP = 0,              // Nothing to trace
  SKIP = (1 << 0),       // Not a hit
  OVERWRITE = (1 << 1),  // Tracked value gets overwritten by untracked. Typically skipped.
  MOVED = (1 << 2),      // Target duplicated to another register, unchanged.
  LOADSTORE = (1 << 3),  // Target saved or loaded. Priority over Pointer.
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
  void SetMemTracked(const u32 mem);
  AutoStepResults AutoStepping(const Core::CPUThreadGuard& guard,
                               std::vector<TraceOutput>* output_trace, bool continue_previous,
                               AutoStop stop_on);
  bool RecordTrace(const Core::CPUThreadGuard& guard, std::vector<TraceOutput>* output_trace,
                   size_t record_limit, u32 time_limit, u32 end_bp, bool clear_on_loop);
  HitType TraceLogic(const TraceOutput& current_instr, bool first_hit = false,
                     std::set<std::string>* regs = nullptr, bool backtrace = false);

private:
  InstructionAttributes GetInstructionAttributes(const TraceOutput& line) const;
  TraceOutput SaveCurrentInstruction(const Core::CPUThreadGuard& guard) const;
  std::optional<HitType> SpecialInstruction(const InstructionAttributes& instr,
                                            const Matches& match) const;
  HitType Backtrace(const InstructionAttributes& instr, const Matches& match);
  HitType ForwardTrace(const InstructionAttributes& instr, const Matches& match,
                       const bool& first_hit);

  bool m_recording = false;
  std::set<std::string> m_reg_tracked;
  std::set<u32> m_mem_tracked;
};
