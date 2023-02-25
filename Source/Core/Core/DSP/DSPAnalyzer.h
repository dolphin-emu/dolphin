// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "Common/CommonTypes.h"

// The update SR analysis is not perfect: it does not properly handle modified SR values if SR is
// only read within a function call, and it's possible that a previous instruction sets SR (e.g. the
// logical zero bit, or the sticky overflow bit) but is marked as not changing SR as a later
// instruction sets it.  When this flag is set, we always treat instructions as updating SR, and
// disable the analysis for if SR needs to be set.
#define DISABLE_UPDATE_SR_ANALYSIS

namespace DSP
{
struct SDSP;
}

namespace DSP
{
// Useful things to detect:
// * Loop endpoints - so that we can avoid checking for loops every cycle.

class Analyzer
{
public:
  explicit Analyzer();
  ~Analyzer();

  Analyzer(const Analyzer&) = default;
  Analyzer& operator=(const Analyzer&) = default;

  Analyzer(Analyzer&&) = default;
  Analyzer& operator=(Analyzer&&) = default;

  // This one should be called every time IRAM changes - which is basically
  // every time that a new ucode gets uploaded, and never else. At that point,
  // we can do as much static analysis as we want - but we should always throw
  // all old analysis away. Luckily the entire address space is only 64K code
  // words and the actual code space 8K instructions in total, so we can do
  // some pretty expensive analysis if necessary.
  void Analyze(const SDSP& dsp);

  // Whether or not the given address indicates the start of an instruction.
  [[nodiscard]] bool IsStartOfInstruction(u16 address) const
  {
    return (GetCodeFlags(address) & CODE_START_OF_INST) != 0;
  }

  // Whether or not the address indicates an idle skip location.
  [[nodiscard]] bool IsIdleSkip(u16 address) const
  {
    return (GetCodeFlags(address) & CODE_IDLE_SKIP) != 0;
  }

  // Whether or not the address indicates the start of a loop.
  [[nodiscard]] bool IsLoopStart(u16 address) const
  {
    return (GetCodeFlags(address) & CODE_LOOP_START) != 0;
  }

  // Whether or not the address indicates the end of a loop.
  [[nodiscard]] bool IsLoopEnd(u16 address) const
  {
    return (GetCodeFlags(address) & CODE_LOOP_END) != 0;
  }

  // Whether or not the address describes an instruction that requires updating the SR register.
  [[nodiscard]] bool IsUpdateSR(u16 address) const
  {
#ifdef DISABLE_UPDATE_SR_ANALYSIS
    return true;
#else
    return (GetCodeFlags(address) & CODE_UPDATE_SR) != 0;
#endif
  }

  // Whether or not the address describes instructions that potentially raise exceptions.
  [[nodiscard]] bool IsCheckExceptions(u16 address) const
  {
    return (GetCodeFlags(address) & CODE_CHECK_EXC) != 0;
  }

private:
  enum CodeFlags : u8
  {
    CODE_NONE = 0,
    CODE_START_OF_INST = 1,
    CODE_IDLE_SKIP = 2,
    CODE_LOOP_START = 4,
    CODE_LOOP_END = 8,
    CODE_UPDATE_SR = 16,
    CODE_CHECK_EXC = 32,
  };

  // Flushes all analyzed state.
  void Reset();

  // Analyzes a region of DSP memory.
  // Note: start is inclusive, end is exclusive.
  void AnalyzeRange(const SDSP& dsp, u16 start_addr, u16 end_addr);

  // Finds addresses in the range [start_addr, end_addr) that are the start of an
  // instruction. During this process other attributes may be detected as well
  // for relevant instructions (loop start/end, etc).
  void FindInstructionStarts(const SDSP& dsp, u16 start_addr, u16 end_addr);

  // Finds locations within the range [start_addr, end_addr) that may contain idle skips.
  void FindIdleSkips(const SDSP& dsp, u16 start_addr, u16 end_addr);

  // Retrieves the flags set during analysis for code in memory.
  [[nodiscard]] u8 GetCodeFlags(u16 address) const { return m_code_flags[address]; }

  // Holds data about all instructions in RAM.
  std::array<u8, 65536> m_code_flags{};
};
}  // namespace DSP
