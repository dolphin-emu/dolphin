// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Debug/CodeTrace.h"

#include <chrono>
#include <regex>

#include "Common/Event.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"

bool CodeTrace::CompareInstruction(std::string instruction,
                                   std::vector<std::string> type_compare) const
{
  for (auto& s : type_compare)
  {
    if (instruction.compare(0, s.length(), s) == 0)
      return true;
  }

  return false;
}

bool CodeTrace::IsInstructionLoadStore(std::string instruction) const
{
  return ((instruction.compare(0, 2, "st") == 0 || instruction.compare(0, 1, "l") == 0 ||
           instruction.compare(0, 5, "psq_l") == 0 || instruction.compare(0, 5, "psq_s") == 0) &&
          instruction.compare(0, 2, "li") != 0);
}

const InstructionAttributes CodeTrace::GetInstructionAttributes(TraceOutput instruction) const
{
  InstructionAttributes tmp_attributes;
  tmp_attributes.instruction = instruction.instruction;
  tmp_attributes.address = PC;

  std::string instr = instruction.instruction;
  std::regex replace_sp("(\\W)sp");
  std::regex replace_rtoc("rtoc");
  std::regex replace_ps("(\\W)p(\\d+)");
  instr = std::regex_replace(instr, replace_sp, "$1r1");
  instr = std::regex_replace(instr, replace_rtoc, "r2");
  instr = std::regex_replace(instr, replace_ps, "$1f$2");

  // Pull all register numbers out and store them. Limited to Reg0 if ps operation, as ps get
  // too complicated to track easily.
  std::regex regis("\\W([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?");
  std::smatch match;

  // ex: add r4, r5, r6 -> Reg0, Reg1, Reg2. Reg0 is always the target register.
  if (std::regex_search(instr, match, regis))
  {
    tmp_attributes.reg0 = match.str(1);
    if (match[2].matched)
      tmp_attributes.reg1 = match.str(2);
    if (match[3].matched)
      tmp_attributes.reg2 = match.str(3);

    if (instruction.memory_target != 0)
    {
      // Get Memory Destination if load/store. The only instructions that start with L are load and
      // load immediate li/lis (excluded).
      tmp_attributes.memory_target = instruction.memory_target;

      if (instr.compare(0, 2, "st") == 0 || instr.compare(0, 5, "psq_s") == 0)
      {
        tmp_attributes.is_store = true;
      }
      else if ((instr.compare(0, 1, "l") == 0 && instr.compare(1, 1, "i") != 0) ||
               instr.compare(0, 5, "psq_l") == 0)
      {
        tmp_attributes.is_load = true;
      }
    }
  }

  return tmp_attributes;
}

void CodeTrace::SaveInstruction(std::vector<TraceOutput>* output_trace) const
{
  TraceOutput tmp_output;
  std::string tmp = PowerPC::debug_interface.Disassemble(PC);
  tmp_output.instruction = tmp;
  tmp_output.address = PC;

  // Should never fail, but if it does it's hard to pass out an error message from here. Memory
  // targets should never be 0, so setting to 0 suggests an error happened.
  if (IsInstructionLoadStore(tmp_output.instruction))
    tmp_output.memory_target =
        PowerPC::debug_interface.GetMemoryAddressFromInstruction(tmp).value_or(0);

  output_trace->push_back(tmp_output);
}

bool CodeTrace::RecordCodeTrace(std::vector<TraceOutput>* output_trace, size_t record_limit,
                                u32 time_limit, u32 end_bp, bool clear_on_loop)
{
  if (time_limit > 30)
    time_limit = 30;

  if (!CPU::IsStepping() || m_recording)
    return true;

  m_recording = true;

  // Using swap instead of clear incase the reserve size is smaller than before.
  std::vector<TraceOutput>().swap(*output_trace);
  output_trace->reserve(record_limit);
  u32 start_bp = PC;
  bool timed_out = false;

  CPU::PauseAndLock(true, false);
  PowerPC::breakpoints.ClearAllTemporary();

  // Keep stepping until the end_bp or timeout
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(5);
  PowerPC::CoreMode old_mode = PowerPC::GetMode();
  PowerPC::SetMode(PowerPC::CoreMode::Interpreter);
  Common::Event sync_event;

  SaveInstruction(output_trace);

  do
  {
    CPU::StepOpcode(&sync_event);
    PowerPC::SingleStep();

    if (PC == start_bp && clear_on_loop)
      output_trace->clear();

    SaveInstruction(output_trace);

  } while (clock::now() < timeout && PC != end_bp && output_trace->size() < record_limit);

  if (clock::now() >= timeout)
    timed_out = true;

  sync_event.WaitFor(std::chrono::milliseconds(20));
  PowerPC::SetMode(old_mode);
  CPU::PauseAndLock(false, false);

  m_recording = false;

  return timed_out;
}

const std::vector<TraceOutput>
CodeTrace::ForwardTrace(std::vector<TraceOutput>* full_trace, std::optional<std::string> track_reg,
                        std::optional<u32> track_mem, u32 begin_address = 0, u32 end_address = 0,
                        u32 results_limit = 1000, bool verbose = false) const
{
  std::vector<TraceOutput> trace_output;
  std::vector<std::string> reg_tracked;
  std::vector<u32> mem_tracked;

  if (track_reg)
    reg_tracked.push_back(track_reg.value());
  else if (track_mem)
    mem_tracked.push_back(track_mem.value());

  const std::vector<std::string> exclude{"dc", "ic", "mt", "c", "fc"};
  const std::vector<std::string> combiner{"ins", "rlwi"};

  // If the first instance of a tracked target is it being destroyed, we probably wanted to track
  // it from that point onwards. Make the first hit a special exclusion case.
  bool first_hit = true;

  // m_error_msg = tr("Change Range using invalid addresses.");
  // return;

  bool trace_running = false;
  if (begin_address == 0)
    trace_running = true;

  for (TraceOutput& current : *full_trace)
  {
    if (current.address != begin_address && !trace_running)
      continue;
    else if (current.address == begin_address && !trace_running)
      trace_running = true;

    // Optimization for tracking a memory target when no registers are being tracked.
    auto itM = std::find(mem_tracked.begin(), mem_tracked.end(), current.memory_target);
    if (reg_tracked.empty() && itM == mem_tracked.end())
      continue;

    // Break instruction down into parts to be analyzed.
    const InstructionAttributes instr = GetInstructionAttributes(current);

    // Not an instruction we care about (branches).
    if (instr.reg0.empty())
      continue;

    auto itR = std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg0);
    const bool match_reg12 =
        (std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg1) != reg_tracked.end() &&
         !instr.reg1.empty()) ||
        (std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg2) != reg_tracked.end() &&
         !instr.reg2.empty());
    const bool match_reg0 = (itR != reg_tracked.end());
    bool hold_continue = false;

    // Exclude a few instruction types, such as compares
    if (CompareInstruction(instr.instruction, exclude))
      hold_continue = true;

    // Exclude hits where the match is a memory pointer
    if (match_reg12 && !match_reg0 && (instr.is_store || instr.is_load))
      hold_continue = true;

    if (!verbose)
    {
      if (hold_continue)
        continue;

      // Output only where tracked items move to.
      if ((match_reg0 && instr.is_store) || (itM != mem_tracked.end() && instr.is_load) ||
          match_reg12 || (match_reg0 && first_hit) || (itM != mem_tracked.end() && first_hit))
      {
        trace_output.push_back(current);
      }
    }
    else if (match_reg12 || match_reg0 || itM != mem_tracked.end())
    {
      // Output all uses of tracked item.
      trace_output.push_back(current);

      if (hold_continue)
        continue;
    }

    // Update tracking logic.
    // Save/Load
    if (instr.memory_target)
    {
      // If using tracked memory. Add register to tracked if Load. Remove tracked memory if
      // overwritten with a store.
      if (itM != mem_tracked.end())
      {
        if (instr.is_load && !match_reg0)
          reg_tracked.push_back(instr.reg0);
        else if (instr.is_store && !match_reg0 && !first_hit)
          mem_tracked.erase(itM);
      }
      else if (instr.is_store && match_reg0)
      {
        // If store but not using tracked memory, then track memory location.
        mem_tracked.push_back(instr.memory_target);
      }
      else if (instr.is_load && match_reg0 && !first_hit)
      {
        reg_tracked.erase(itR);
      }
    }
    else
    {
      // Other instructions
      // Skip if no matches. Happens most often.
      if (!match_reg0 && !match_reg12)
        continue;
      // If tracked register data is being stored in a new register, save new register.
      else if (match_reg12 && !match_reg0)
        reg_tracked.push_back(instr.reg0);
      // If tracked register is overwritten, stop tracking.
      else if (match_reg0 && !match_reg12 && !first_hit &&
               !CompareInstruction(instr.instruction, combiner))
        reg_tracked.erase(itR);
    }

    // First hit will likely be start of value we want to track - not the end. So don't remove itR.
    if (match_reg0 || match_reg12 || (itM != mem_tracked.end()))
      first_hit = false;

    if ((reg_tracked.empty() && mem_tracked.empty()) || trace_output.size() >= results_limit)
      break;

    if (current.address == end_address && end_address != 0)
      break;
  }

  return trace_output;
}

const std::vector<TraceOutput>
CodeTrace::Backtrace(std::vector<TraceOutput>* full_trace, std::optional<std::string> track_reg,
                     std::optional<u32> track_mem, u32 start_address = 0, u32 end_address = 0,
                     u32 results_limit = 1000, bool verbose = false) const
{
  std::vector<TraceOutput> trace_output;
  const std::vector<std::string> exclude{"dc", "ic", "mt", "c", "fc"};
  const std::vector<std::string> combiner{"ins", "rlwi"};

  std::vector<std::string> reg_tracked;
  std::vector<u32> mem_tracked;

  if (track_reg)
    reg_tracked.push_back(track_reg.value());
  else if (track_mem)
    mem_tracked.push_back(track_mem.value());

  // Update iterator. C++20 will eventually have a std::views::reverse option for
  // ranged-based loops.
  auto begin_itr = full_trace->rbegin();
  auto end_itr = full_trace->rend();

  // start_address counts from the oldest instruction executed, but backtrace needs it to count from
  // the most recent, which is the end_address
  std::swap(start_address, end_address);

  if (start_address != 0)
  {
    begin_itr =
        find_if(full_trace->rbegin(), full_trace->rend(),
                [start_address](const TraceOutput& t) { return t.address == start_address; });
  }
  if (end_address != 0)
  {
    auto temp_itr =
        find_if(full_trace->begin(), full_trace->end(),
                [end_address](const TraceOutput& t) { return t.address == end_address; });
    end_itr = std::reverse_iterator(temp_itr);
  }

  for (auto& current = begin_itr; current != end_itr; begin_itr++)
  {
    // Optimization for tracking a memory target when no registers are being tracked.
    auto itM = std::find(mem_tracked.begin(), mem_tracked.end(), current->memory_target);
    if (reg_tracked.empty() && itM == mem_tracked.end())
      continue;

    // Break instruction down into parts to be analyzed.
    const InstructionAttributes instr = GetInstructionAttributes(*current);

    // Not an instruction we care about
    if (instr.reg0.empty())
      continue;

    auto itR = std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg0);
    const bool match_reg1 =
        (std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg1) != reg_tracked.end() &&
         !instr.reg1.empty());
    const bool match_reg2 =
        (std::find(reg_tracked.begin(), reg_tracked.end(), instr.reg2) != reg_tracked.end() &&
         !instr.reg2.empty());
    const bool match_reg0 = (itR != reg_tracked.end());
    bool hold_continue = false;

    // Exclude a few instruction types, such as compares
    if (CompareInstruction(instr.instruction, exclude))
      hold_continue = true;

    // Exclude hits where the match is a memory pointer
    if ((match_reg1 || match_reg2) && !match_reg0 && (instr.is_store || instr.is_load))
      hold_continue = true;

    // Write instructions to output.
    if (!verbose)
    {
      if (hold_continue)
        continue;

      // Output only where tracked items came from.
      if ((match_reg0 && !instr.is_store) || (itM != mem_tracked.end() && instr.is_store))
      {
        trace_output.push_back(*current);
      }
    }
    else if ((match_reg1 || match_reg2 || match_reg0 || itM != mem_tracked.end()))
    {
      // Output stuff like compares if they contain a tracked register
      trace_output.push_back(*current);

      if (hold_continue)
        continue;
    }

    // Update trace logic.
    // Store/Load
    if (instr.memory_target)
    {
      // Backtrace: what wrote to tracked memory & remove memory track. Else if: what loaded to
      // tracked register & remove register from track.
      if (itM != mem_tracked.end())
      {
        if (instr.is_store && !match_reg0)
        {
          reg_tracked.push_back(instr.reg0);
          mem_tracked.erase(itM);
        }
      }
      else if (instr.is_load && match_reg0)
      {
        mem_tracked.push_back(instr.memory_target);
        reg_tracked.erase(itR);
      }
    }
    else
    {
      // Other instructions
      // Skip if we aren't watching output register. Happens most often.
      // Else: Erase tracked register and save what wrote to it.
      if (!match_reg0)
        continue;
      else if (instr.reg0 != instr.reg1 && instr.reg0 != instr.reg2 &&
               !CompareInstruction(instr.instruction, combiner))
        reg_tracked.erase(itR);

      // If tracked register is written, track r1 / r2.
      if (!match_reg1 && !instr.reg1.empty())
        reg_tracked.push_back(instr.reg1);
      if (!match_reg2 && !instr.reg2.empty())
        reg_tracked.push_back(instr.reg2);
    }

    // Stop if we run out of things to track
    if ((reg_tracked.empty() && mem_tracked.empty()) || trace_output.size() >= results_limit)
      break;
  }
  return trace_output;
}
