// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/CodeTrace.h"

#include <algorithm>
#include <chrono>
#include <regex>

#include "Common/Event.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace
{
bool IsInstructionLoadStore(std::string_view ins)
{
  // Easier version, but doesn't work for lwzx etc:
  // return ins.find("(r") != std::string::npos;
  return (ins.starts_with('l') && !ins.starts_with("li")) || ins.starts_with("st") ||
         ins.starts_with("psq_l") || ins.starts_with("psq_s");
}

bool CompareInstruction(std::string_view instr, const std::vector<std::string>& compare)
{
  return std::any_of(compare.begin(), compare.end(),
                     [&instr](std::string_view s) { return instr.starts_with(s); });
}

u32 GetMemoryTargetSize(std::string_view instr)
{
  // Word-size operations are taken as the default, check the others.
  auto op = instr.substr(0, 4);

  constexpr char BYTE_TAG = 'b';
  constexpr char HALF_TAG = 'h';
  constexpr char DOUBLE_WORD_TAG = 'd';
  constexpr char PAIRED_TAG = 'p';

  // Actual range is 0 to size - 1;
  if (op.find(BYTE_TAG) != std::string::npos)
  {
    return 1;
  }
  else if (op.find(HALF_TAG) != std::string::npos)
  {
    return 2;
  }
  else if (op.find(DOUBLE_WORD_TAG) != std::string::npos ||
           op.find(PAIRED_TAG) != std::string::npos)
  {
    return 8;
  }

  return 4;
}

double RegisterValue(const Core::CPUThreadGuard& guard, std::string_view reg)
{
  auto& ppc_state = guard.GetSystem().GetPowerPC().GetPPCState();

  if (reg.starts_with("f"))
    return ppc_state.ps[stoi(std::string{reg.begin() + 1, reg.end()})].PS0AsDouble();
  else
    return static_cast<double>(ppc_state.gpr[std::stoi(std::string{reg.begin() + 1, reg.end()})]);
}

bool CompareMemoryTargetToTracked(const std::string& instr, const u32 mem_target,
                                  const std::set<u32>& mem_tracked)
{
  // This function is hit often and should be optimized.
  auto it_lower = std::lower_bound(mem_tracked.begin(), mem_tracked.end(), mem_target);

  if (it_lower == mem_tracked.end())
    return false;
  else if (*it_lower == mem_target)
    return true;

  // If the base value doesn't hit, still need to check if longer values overlap.
  return *it_lower < mem_target + GetMemoryTargetSize(instr);
}
}  // namespace

void CodeTrace::SetRegTracked(const std::string& reg)
{
  m_reg_tracked.insert(reg);
}

void CodeTrace::SetMemTracked(const u32 mem)
{
  m_mem_tracked.insert(mem);
}

InstructionAttributes CodeTrace::GetInstructionAttributes(const TraceOutput& instruction) const
{
  // Slower process of breaking down saved instruction.
  InstructionAttributes tmp_attributes;
  tmp_attributes.instruction = instruction.instruction;
  tmp_attributes.address = instruction.address;
  std::string instr = instruction.instruction;
  std::smatch match;

  // Convert sp, rtoc, and ps to r1, r2, and F#. ps is handled like a float operation.
  static const std::regex replace_sp("(\\W)sp");
  instr = std::regex_replace(instr, replace_sp, "$1r1");
  static const std::regex replace_rtoc("rtoc");
  instr = std::regex_replace(instr, replace_rtoc, "r2");
  static const std::regex replace_ps("(\\W)p(\\d+)");
  instr = std::regex_replace(instr, replace_ps, "$1f$2");

  // Pull all register numbers out and store them. Limited to Reg0 if ps operation, as ps get
  // too complicated to track easily.
  // ex: add r4, r5, r6 -> r4 = Reg0, r5 = Reg1, r6 = Reg2. Reg0 is always the target register.
  // Small bug: If exactly two registers it may fill reg0 and reg2, but it affects nothing.
  static const std::regex regis(
      "\\W([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?",
      std::regex::optimize);

  if (std::regex_search(instr, match, regis))
  {
    for (int i = 0; i <= 3; i++)
    {
      if (match[i + 1].matched)
      {
        if (i == 0)
          tmp_attributes.target_reg = match.str(i + 1);
        else
          tmp_attributes.regs.insert(match.str(i + 1));
      }
    }

    if (instruction.memory_target)
    {
      tmp_attributes.memory_target = instruction.memory_target;
      tmp_attributes.memory_target_size = GetMemoryTargetSize(instr);

      if (instr.starts_with("st") || instr.starts_with("psq_s"))
        tmp_attributes.is_store = true;
      else
        tmp_attributes.is_load = true;
    }
  }

  return tmp_attributes;
}

TraceOutput CodeTrace::SaveCurrentInstruction(const Core::CPUThreadGuard& guard) const
{
  auto& power_pc = guard.GetSystem().GetPowerPC();
  auto& ppc_state = power_pc.GetPPCState();
  auto& debug_interface = power_pc.GetDebugInterface();

  // Quickly save instruction and memory target for fast logging.
  TraceOutput output;
  const std::string instr = debug_interface.Disassemble(&guard, ppc_state.pc);
  output.instruction = instr;
  output.address = ppc_state.pc;

  if (IsInstructionLoadStore(output.instruction))
    output.memory_target = debug_interface.GetMemoryAddressFromInstruction(instr);

  return output;
}

AutoStepResults CodeTrace::AutoStepping(const Core::CPUThreadGuard& guard,
                                        std::vector<TraceOutput>* output_trace,
                                        bool continue_previous, AutoStop stop_on)
{
  auto& system = guard.GetSystem();
  auto& power_pc = system.GetPowerPC();

  AutoStepResults results;

  if (m_recording || !system.GetCPU().IsStepping())
    return results;

  m_recording = true;
  TraceOutput pc_instr = SaveCurrentInstruction(guard);
  const InstructionAttributes instr = GetInstructionAttributes(pc_instr);
  HitType hit = HitType::SKIP;
  HitType stop_condition = HitType::LOADSTORE;

  // Don't let autostep write to an existing trace log, unless we are continuing. Trace widget
  // should be used to clear the log when the user wants to.
  const bool log_trace = output_trace != nullptr;
  bool first_hit = false;

  // Once autostep stops, it can be told to continue running without resetting the tracked
  // registers and memory.
  if (!continue_previous)
  {
    // It wouldn't necessarily be wrong to also record the memory of a load operation, as the
    // value exists there too. May or may not be desirable depending on task. Leaving it out.
    if (instr.is_store)
    {
      const u32 size = GetMemoryTargetSize(instr.instruction);
      for (u32 i = 0; i < size; i++)
        m_mem_tracked.insert(instr.memory_target.value() + i);
    }

    // contains() is always true when initiated from CodeViewWidget. RegisterWidget needs to use
    // first hit logic when we need to Step() before we can find the register. Code and Register
    // autostep are identical if contains() is true.
    if (!m_reg_tracked.contains(instr.target_reg))
    {
      first_hit = true;

      // If the Trace is starting and RegisterWidget's target is referenced in the PC instruction's
      // arguments, update m_reg_tracked by calling TraceLogic.
      for (auto& reg : instr.regs)
      {
        if (m_reg_tracked.contains(reg))
        {
          hit = TraceLogic(pc_instr, first_hit);
          first_hit = false;
          break;
        }
      }
    }
  }

  // Could use bit flags, but I organized it to have decreasing levels of verbosity, so the
  // less-than comparison ignores what is needed for the current usage.
  if (stop_on == AutoStop::Always)
    stop_condition = HitType::LOADSTORE;
  else if (stop_on == AutoStop::Used)
    stop_condition = HitType::PASSIVE;
  else if (stop_on == AutoStop::Changed)
    stop_condition = HitType::ACTIVE;

  power_pc.GetBreakPoints().ClearAllTemporary();
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(5);

  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);

  TraceOutput output = SaveCurrentInstruction(guard);

  // Step and Trace
  // If logging: Need the target register's value before saving to log, which requires a Step() to
  // execute the instruction and update the register. Therefore the saved instruction needs to be
  // one Step behind. The final log update can be done after the loop.
  do
  {
    InstructionAttributes attrib;

    if (log_trace)
    {
      attrib = GetInstructionAttributes(output);

      for (auto& reg : attrib.regs)
        output.regdata.emplace_back(RegisterData{reg, RegisterValue(guard, reg)});
    }

    power_pc.SingleStep();

    // Check log limit?
    if (log_trace)
    {
      // Must be done after the SingleStep for correct value.
      if (!attrib.target_reg.empty())
        output.regdata.emplace_back(
            RegisterData{attrib.target_reg, RegisterValue(guard, attrib.target_reg)});

      output_trace->emplace_back(output);
    }

    output = SaveCurrentInstruction(guard);
    hit = TraceLogic(output, first_hit);

    if (first_hit && hit != HitType::SKIP)
      first_hit = false;

    results.count += 1;

  } while (clock::now() < timeout && hit < stop_condition &&
           !(m_reg_tracked.empty() && m_mem_tracked.empty()));

  // Just leave the register values out for the final PC instr.
  if (log_trace)
    output_trace->emplace_back(output);

  // Report the timeout to the caller.
  if (clock::now() >= timeout)
    results.timed_out = true;

  power_pc.SetMode(old_mode);

  m_recording = false;

  results.reg_tracked = m_reg_tracked;
  results.mem_tracked = m_mem_tracked;

  // Doesn't currently need to report the hit type to the caller. Denoting when the reg and mem
  // trackers are both empty is important, as it means our target was overwritten and can no longer
  // be tracked. Different actions can be taken on a timeout vs empty trackers, so they are reported
  // individually.
  return results;
}

bool CodeTrace::RecordTrace(const Core::CPUThreadGuard& guard,
                            std::vector<TraceOutput>* output_trace, u32 time_limit, u32 end_bp,
                            bool clear_on_loop)
{
  auto& system = guard.GetSystem();
  auto& power_pc = system.GetPowerPC();
  auto& ppc_state = power_pc.GetPPCState();

  if (time_limit > 30)
    time_limit = 30;

  if (!system.GetCPU().IsStepping() || m_recording)
    return true;

  m_recording = true;

  // Using swap instead of clear in case the reserve size is smaller than before.
  std::vector<TraceOutput>().swap(*output_trace);
  output_trace->reserve(40000 * time_limit);

  u32 start_bp = ppc_state.pc;
  bool timed_out = false;

  // Keep stepping until the end_bp or timeout
  power_pc.GetBreakPoints().ClearAllTemporary();
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(time_limit);

  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);
  do
  {
    if (ppc_state.pc == start_bp && clear_on_loop)
      output_trace->clear();

    TraceOutput output = SaveCurrentInstruction(guard);
    InstructionAttributes attrib = GetInstructionAttributes(output);

    for (auto& reg : attrib.regs)
      output.regdata.emplace_back(RegisterData{reg, RegisterValue(guard, reg)});

    power_pc.SingleStep();

    if (!attrib.target_reg.empty())
      output.regdata.emplace_back(
          RegisterData{attrib.target_reg, RegisterValue(guard, attrib.target_reg)});

    output_trace->emplace_back(output);

  } while (clock::now() < timeout && ppc_state.pc != end_bp);

  // Saved instructions are one step behind when loop ends.
  output_trace->emplace_back(SaveCurrentInstruction(guard));

  if (clock::now() >= timeout)
    timed_out = true;

  power_pc.SetMode(old_mode);

  m_recording = false;

  return timed_out;
}

std::optional<HitType> CodeTrace::SpecialInstruction(const InstructionAttributes& instr,
                                                     const Matches& match) const
{
  // Checks if the instruction is a type that needs special handling.
  // Warning: If anything is of the form "instr rNUM" and instr does not start with mf or mt, then
  // it needs to be added here.
  const auto CompareInstr = [](std::string_view instruction, const auto& type_compare) {
    return std::any_of(type_compare.begin(), type_compare.end(),
                       [&instruction](std::string_view s) { return instruction.starts_with(s); });
  };

  // Exclusions from updating tracking logic. mt operations are too complex and specialized.
  static const std::array<std::string_view, 2> exclude{"dc", "ic"};
  static const std::array<std::string_view, 2> compare{"c", "fc"};

  // Move to / Move from special registers.
  if (instr.instruction.starts_with("mf") && match.target_reg)
    return HitType::OVERWRITE;

  if (instr.instruction.starts_with("mt") && match.target_reg)
    return HitType::MOVED;

  if (CompareInstr(instr.instruction, exclude))
    return HitType::SKIP;

  if (CompareInstr(instr.instruction, compare))
    return HitType::PASSIVE;

  if (match.passive_reg && !match.target_reg && (instr.is_store || instr.is_load))
    return HitType::POINTER;

  return std::nullopt;
}

HitType CodeTrace::TraceLogic(const TraceOutput& current_instr, bool first_hit,
                              std::set<std::string>* regs, bool backtrace)
{
  // Tracks the original value that is in the targeted register or memory through loads, stores,
  // register moves, and value changes. Also finds when it is used. ps operations are not fully
  // supported. -ux and -m memory instructions may need special cases.
  // Should not be called if reg and mem tracked are empty.

  // Using a std::set because it can easily insert the items being accessed without causing
  // duplicates, and quickly erases elements without caring if the element actually exists.

  Matches match;

  if (m_reg_tracked.empty() && m_mem_tracked.empty())
    return HitType::STOP;

  if (current_instr.memory_target && !m_mem_tracked.empty())
  {
    match.mem = CompareMemoryTargetToTracked(current_instr.instruction,
                                             *current_instr.memory_target, m_mem_tracked);
  }

  // Optimization for tracking a memory target when no registers are being tracked.
  if (m_reg_tracked.empty() && !match.mem)
    return HitType::SKIP;

  // Break instruction down into parts to be analyzed.
  const InstructionAttributes instr = GetInstructionAttributes(current_instr);

  // Not an instruction we care about (branches).
  if (instr.target_reg.empty())
    return HitType::SKIP;

  for (auto& reg : instr.regs)
  {
    if (m_reg_tracked.contains(reg))
      match.regs.insert(reg);
  }

  match.target_reg = m_reg_tracked.contains(instr.target_reg);
  match.passive_reg = !match.regs.empty();

  if (!match.target_reg && !match.passive_reg && !match.mem)
    return HitType::SKIP;

  const std::optional<HitType> type = SpecialInstruction(instr, match);
  HitType return_type;

  if (type)
  {
    if (type.value() == HitType::OVERWRITE && !first_hit)
      m_reg_tracked.erase(instr.target_reg);

    return_type = type.value();
  }
  else
  {
    // Update tracking logic. At this point a memory or register hit happened.
    if (backtrace)
      return_type = Backtrace(instr, match);
    else
      return_type = ForwardTrace(instr, match, first_hit);
  }

  // Save targeted registers for tagging in the table. Duplicates don't matter. Could skip matching
  // entirely and save m_regs_tracked, but this will be a shorter list.
  if (regs == nullptr)
    return return_type;

  // Combining for easy output if target_reg was hit or just now added.
  if (match.target_reg || m_reg_tracked.contains(instr.target_reg))
    match.regs.insert(instr.target_reg);

  for (auto& reg : match.regs)
  {
    if (reg == "r1")
      regs->insert("sp");
    else if (reg == "r2")
      regs->insert("rtoc");
    else
      regs->insert(reg);
  }

  return return_type;
}

HitType CodeTrace::ForwardTrace(const InstructionAttributes& instr, const Matches& match,
                                const bool& first_hit)
{
  static const std::vector<std::string> mover{"mr", "fmr"};

  if (instr.memory_target)
  {
    if (match.mem)
    {
      // If hit tracked memory. Load -> Add register to tracked.  Store -> Remove tracked memory
      // if overwritten.

      if (instr.is_load && !match.target_reg)
      {
        m_reg_tracked.insert(instr.target_reg);
        return HitType::LOADSTORE;
      }
      else if (instr.is_store && !match.target_reg)
      {
        // On First Hit it wouldn't necessarily be wrong to track the register, which contains the
        // same value. A matter of preference.
        if (first_hit)
          return HitType::LOADSTORE;

        for (u32 i = 0; i < instr.memory_target_size; i++)
          m_mem_tracked.erase(*instr.memory_target + i);

        return HitType::OVERWRITE;
      }
      else
      {
        // If reg0 and store/load are both already tracked, do nothing.
        return HitType::LOADSTORE;
      }
    }
    else if (instr.is_store && match.target_reg)
    {
      // If store to untracked memory, then track memory.
      for (u32 i = 0; i < instr.memory_target_size; i++)
        m_mem_tracked.insert(*instr.memory_target + i);

      return HitType::LOADSTORE;
    }
    else if (instr.is_load && match.target_reg)
    {
      // Not wrong to track load memory_target here. Preference.
      if (first_hit)
        return HitType::LOADSTORE;

      // If untracked load is overwriting tracked register, then remove register
      m_reg_tracked.erase(instr.target_reg);
      return HitType::OVERWRITE;
    }
  }
  else
  {
    // If tracked register data is being stored in a new register, save new register.
    if (match.passive_reg && !match.target_reg)
    {
      m_reg_tracked.insert(instr.target_reg);

      // This should include any instruction that can reach this point and is not ACTIVE. Can only
      // think of mr at this time.
      if (CompareInstruction(instr.instruction, mover))
        return HitType::MOVED;

      return HitType::ACTIVE;
    }
    // If tracked register is overwritten, stop tracking.
    else if (match.target_reg && !match.passive_reg)
    {
      if (instr.instruction.starts_with("rlwimi") || first_hit)
        return HitType::UPDATED;

      m_reg_tracked.erase(instr.target_reg);
      return HitType::OVERWRITE;
    }
    else if (match.target_reg && match.passive_reg)
    {
      // Or moved
      return HitType::UPDATED;
    }
  }

  // Should not reach this
  return HitType::SKIP;
}

HitType CodeTrace::Backtrace(const InstructionAttributes& instr, const Matches& match)
{
  // Can only be used on a Trace Log.

  static const std::vector<std::string> mover{"mr", "fmr"};

  if (instr.memory_target)
  {
    // Backtrace: Memory Store: Erase memory and save register written to memory.
    // Memory Load: Erase register and track the memory it was loaded from.
    if (match.mem)
    {
      if (instr.is_store && !match.target_reg)
      {
        m_reg_tracked.insert(instr.target_reg);
        for (u32 i = 0; i < instr.memory_target_size; i++)
          m_mem_tracked.erase(*instr.memory_target + i);

        return HitType::LOADSTORE;
      }
    }
    else if (instr.is_load && match.target_reg)
    {
      m_reg_tracked.erase(instr.target_reg);
      for (u32 i = 0; i < instr.memory_target_size; i++)
        m_mem_tracked.insert(*instr.memory_target + i);

      return HitType::LOADSTORE;
    }
    else
    {
      // LoadStore but not changing location of target
      return HitType::PASSIVE;
    }
  }
  else
  {
    // Other instructions
    // Skip if we aren't watching output register. Happens most often.
    // Else: Erase tracked register and save what wrote to it.
    if (!match.target_reg)
      return HitType::PASSIVE;

    const bool updater = instr.instruction.starts_with("rlwimi");

    // Tracked reg was created here, so delete it from tracking.
    if (!instr.regs.contains(instr.target_reg) && !updater)
      m_reg_tracked.erase(instr.target_reg);

    // If tracked register is written, track other regs.
    for (auto& reg : instr.regs)
      m_reg_tracked.insert(reg);

    if (CompareInstruction(instr.instruction, mover))
      return HitType::MOVED;
    else if (updater)
      return HitType::UPDATED;
    else
      return HitType::ACTIVE;
  }

  // Should never reach this.
  return HitType::SKIP;
}
