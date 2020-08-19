// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"

struct InstructionAttributes
{
  u32 address = 0;
  std::string instruction = "";
  std::string reg0 = "";
  std::string reg1 = "";
  std::string reg2 = "";
  u32 memory_target = 0;
  bool is_store = false;
  bool is_load = false;
};

struct TraceOutput
{
  u32 address;
  u32 memory_target = 0;
  std::string instruction;
};

class CodeTrace
{
public:
  bool RecordCodeTrace(std::vector<TraceOutput>* output_trace, size_t record_limit, u32 time_limit,
                       u32 end_bp, bool clear_on_loop);
  const std::vector<TraceOutput> ForwardTrace(std::vector<TraceOutput>* full_trace,
                                              std::optional<std::string> track_reg,
                                              std::optional<u32> track_mem, u32 begin_address,
                                              u32 end_address, u32 results_limit,
                                              bool verbose) const;
  const std::vector<TraceOutput> Backtrace(std::vector<TraceOutput>* full_trace,
                                           std::optional<std::string> track_reg,
                                           std::optional<u32> track_mem, u32 begin_address,
                                           u32 end_address, u32 results_limit, bool verbose) const;

private:
  bool CompareInstruction(std::string instruction, std::vector<std::string> type_compare) const;
  bool IsInstructionLoadStore(std::string instruction) const;
  const InstructionAttributes GetInstructionAttributes(TraceOutput line) const;
  void SaveInstruction(std::vector<TraceOutput>* output_trace) const;

  bool m_recording = false;
};
