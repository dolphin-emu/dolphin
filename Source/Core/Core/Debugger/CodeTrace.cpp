// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/CodeTrace.h"

#include <algorithm>
#include <chrono>
#include <regex>

#include "Core/Core.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace
{
bool IsInstructionLoadStore(const std::string_view ins)
{
  return (ins.starts_with('l') && !ins.starts_with("li")) || ins.starts_with("st") ||
         ins.starts_with("psq_l") || ins.starts_with("psq_s");
}

u32 GetMemoryTargetSize(const std::string_view instr)
{
  // Word-size operations are taken as the default, check the others.
  const auto op = instr.substr(0, 4);

  constexpr char BYTE_TAG = 'b';
  constexpr char HALF_TAG = 'h';
  constexpr char DOUBLE_WORD_TAG = 'd';
  constexpr char PAIRED_TAG = 'p';

  // Actual range is 0 to size - 1;
  if (op.find(BYTE_TAG) != std::string::npos)
  {
    return 1;
  }
  if (op.find(HALF_TAG) != std::string::npos)
  {
    return 2;
  }
  if (op.find(DOUBLE_WORD_TAG) != std::string::npos || op.find(PAIRED_TAG) != std::string::npos)
  {
    return 8;
  }

  return 4;
}

bool CompareMemoryTargetToTracked(const std::string& instr, const u32 mem_target,
                                  const std::set<u32>& mem_tracked)
{
  const auto it_lower = mem_tracked.lower_bound(mem_target);

  if (it_lower == mem_tracked.end())
    return false;
  if (*it_lower == mem_target)
    return true;

  // If the base value doesn't hit, still need to check if longer values overlap.
  return *it_lower < mem_target + GetMemoryTargetSize(instr);
}
}  // namespace

void CodeTrace::SetRegTracked(const std::string& reg)
{
  m_reg_autotrack.push_back(reg);
}

InstructionAttributes CodeTrace::GetInstructionAttributes(const TraceOutput& instruction)
{
  // Slower process of breaking down saved instruction. Only used when stepping through code if a
  // decision has to be made, otherwise used afterwards on a log file.
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
  static const std::regex regis(
      "\\W([rfp]\\d+)[^r^f]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?[^r^f\\d]*(?:([rf]\\d+))?",
      std::regex::optimize);

  if (std::regex_search(instr, match, regis))
  {
    tmp_attributes.reg0 = match.str(1);
    if (match[2].matched)
      tmp_attributes.reg1 = match.str(2);
    if (match[3].matched)
      tmp_attributes.reg2 = match.str(3);
    if (match[4].matched)
      tmp_attributes.reg3 = match.str(4);

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

TraceOutput CodeTrace::SaveCurrentInstruction(const Core::CPUThreadGuard& guard)
{
  const auto& system = guard.GetSystem();
  auto& power_pc = system.GetPowerPC();
  const auto& [pc, _npc, _gather_pipe_ptr, _gather_pipe_base_ptr, _gpr, _cr, _msr, _fpscr,
    _feature_flags, _Exceptions, _downcount, _xer_ca, _xer_so_ov, _xer_stringctrl,
    _above_fits_in_first_0x100, _ps, _sr, _spr, _stored_stack_pointer, _mem_ptr, _tlb,
    _pagetable_base, _pagetable_hashmask, _iCache, _m_enable_dcache, _dCache, _reserve,
    _reserve_address] = power_pc.GetPPCState();
  const auto& debug_interface = power_pc.GetDebugInterface();

  // Quickly save instruction and memory target for fast logging.
  TraceOutput output;
  const std::string instr = debug_interface.Disassemble(&guard, pc);
  output.instruction = instr;
  output.address = pc;

  if (IsInstructionLoadStore(output.instruction))
    output.memory_target = debug_interface.GetMemoryAddressFromInstruction(instr);

  return output;
}

AutoStepResults CodeTrace::AutoStepping(const Core::CPUThreadGuard& guard, bool continue_previous,
                                        AutoStop stop_on)
{
  AutoStepResults results;

  if (m_recording)
    return results;

  TraceOutput pc_instr = SaveCurrentInstruction(guard);
  const auto [_address, instruction, reg0, _reg1, _reg2, _reg3, memory_target, _memory_target_size,
    is_store, _is_load] = GetInstructionAttributes(pc_instr);

  // Not an instruction we should start autostepping from (ie branches).
  if (reg0.empty() && !continue_previous)
    return results;

  m_recording = true;

  // Once autostep stops, it can be told to continue running without resetting the tracked
  // registers and memory.
  if (!continue_previous)
  {
    m_reg_autotrack.clear();
    m_mem_autotrack.clear();
    m_reg_autotrack.push_back(reg0);

    // It wouldn't necessarily be wrong to also record the memory of a load operation, as the
    // value exists there too. May or may not be desirable depending on task. Leaving it out.
    if (is_store)
    {
      const u32 size = GetMemoryTargetSize(instruction);
      for (u32 i = 0; i < size; i++)
        m_mem_autotrack.insert(memory_target.value() + i);
    }
  }

  // Count is important for feedback on how much work was done.

  HitType hit = HitType::SKIP;
  HitType stop_condition = HitType::SAVELOAD;

  // Could use bit flags, but I organized it to have decreasing levels of verbosity, so the
  // less-than comparison ignores what is needed for the current usage.
  if (stop_on == AutoStop::Always)
    stop_condition = HitType::SAVELOAD;
  else if (stop_on == AutoStop::Used)
    stop_condition = HitType::PASSIVE;
  else if (stop_on == AutoStop::Changed)
    stop_condition = HitType::ACTIVE;

  auto& power_pc = guard.GetSystem().GetPowerPC();
  using clock = std::chrono::steady_clock;
  clock::time_point timeout = clock::now() + std::chrono::seconds(4);

  PowerPC::CoreMode old_mode = power_pc.GetMode();
  power_pc.SetMode(PowerPC::CoreMode::Interpreter);

  do
  {
    power_pc.SingleStep();

    pc_instr = SaveCurrentInstruction(guard);
    hit = TraceLogic(pc_instr);
    results.count += 1;
  } while (clock::now() < timeout && hit < stop_condition &&
           !(m_reg_autotrack.empty() && m_mem_autotrack.empty()));

  // Report the timeout to the caller.
  if (clock::now() >= timeout)
    results.timed_out = true;

  power_pc.SetMode(old_mode);
  m_recording = false;

  results.reg_tracked = m_reg_autotrack;
  results.mem_tracked = m_mem_autotrack;

  // Doesn't currently need to report the hit type to the caller. Denoting when the reg and mem
  // trackers are both empty is important, as it means our target was overwritten and can no longer
  // be tracked. Different actions can be taken on a timeout vs empty trackers, so they are reported
  // individually.
  return results;
}

HitType CodeTrace::TraceLogic(const TraceOutput& current_instr, const bool first_hit)
{
  // Tracks the original value that is in the targeted register or memory through loads, stores,
  // register moves, and value changes. Also finds when it is used. ps operations are not fully
  // supported. -ux memory instructions may need special cases.
  // Should not be called if reg and mem tracked are empty.

  // Using a std::set because it can easily insert the memory range being accessed without
  // causing duplicates, and quickly erases all members of the memory range without caring if the
  // element actually exists.

  bool mem_hit = false;
  if (current_instr.memory_target && !m_mem_autotrack.empty())
  {
    mem_hit = CompareMemoryTargetToTracked(current_instr.instruction, *current_instr.memory_target,
                                           m_mem_autotrack);
  }

  // Optimization for tracking a memory target when no registers are being tracked.
  if (m_reg_autotrack.empty() && !mem_hit)
    return HitType::SKIP;

  // Break instruction down into parts to be analyzed.
  const auto [_address, instruction, reg0, reg1, reg2, reg3, memory_target, memory_target_size,
    is_store, is_load] = GetInstructionAttributes(current_instr);

  // Not an instruction we care about (branches).
  if (reg0.empty())
    return HitType::SKIP;

  // The reg_itr will be used later for erasing.
  const auto reg_itr = std::ranges::find(m_reg_autotrack, reg0);
  const bool match_reg123 =
      (!reg1.empty() &&
       std::ranges::find(m_reg_autotrack, reg1) != m_reg_autotrack.end()) ||
      (!reg2.empty() &&
       std::ranges::find(m_reg_autotrack, reg2) != m_reg_autotrack.end()) ||
      (!reg3.empty() &&
       std::ranges::find(m_reg_autotrack, reg3) != m_reg_autotrack.end());
  const bool match_reg0 = reg_itr != m_reg_autotrack.end();

  if (!match_reg0 && !match_reg123 && !mem_hit)
    return HitType::SKIP;

  // Checks if the intstruction is a type that needs special handling.
  const auto CompareInstruction = [](std::string_view instruction, const auto& type_compare) {
    return std::any_of(type_compare.begin(), type_compare.end(),
                       [&instruction](const std::string_view s) { return instruction.starts_with(s); });
  };

  // Exclusions from updating tracking logic. mt operations are too complex and specialized.
  // Combiner used later.
  static constexpr std::array<std::string_view, 3> exclude{"dc", "ic", "mt"};
  static constexpr std::array<std::string_view, 2> compare{"c", "fc"};

  // rlwimi, at least, can preserve parts of the target register. Not sure if rldimi can too or if
  // there are any others like this.
  static constexpr std::array<std::string_view, 1> combiner{"rlwimi"};

  static constexpr std::array<std::string_view, 2> mover{"mr", "fmr"};

  // Link register for when r0 gets overwritten
  if (instruction.starts_with("mflr") && match_reg0)
  {
    m_reg_autotrack.erase(reg_itr);
    return HitType::OVERWRITE;
  }
  if (instruction.starts_with("mtlr") && match_reg0)
  {
    // LR is not something tracked
    return HitType::MOVED;
  }

  if (CompareInstruction(instruction, exclude))
    return HitType::SKIP;
  if (CompareInstruction(instruction, compare))
    return HitType::PASSIVE;
  if (match_reg123 && !match_reg0 && (is_store || is_load))
    return HitType::POINTER;

  // Update tracking logic. At this point a memory or register hit happened.
  // Save/Load
  if (memory_target)
  {
    if (mem_hit)
    {
      // If hit a tracked memory. Load -> Add register to tracked.  Store -> Remove tracked memory
      // if overwritten.

      if (is_load && !match_reg0)
      {
        m_reg_autotrack.push_back(reg0);
        return HitType::SAVELOAD;
      }

      if (is_store && !match_reg0)
      {
        // On First Hit it wouldn't necessarily be wrong to track the register, which contains the
        // same value. A matter of preference.
        if (first_hit)
          return HitType::SAVELOAD;

        for (u32 i = 0; i < memory_target_size; i++)
          m_mem_autotrack.erase(*memory_target + i);

        return HitType::OVERWRITE;
      }
      // If reg0 and store/load are both already tracked, do nothing.
      return HitType::SAVELOAD;
    }
    if (is_store && match_reg0)
    {
      // If store to untracked memory, then track memory.
      for (u32 i = 0; i < memory_target_size; i++)
        m_mem_autotrack.insert(*memory_target + i);

      return HitType::SAVELOAD;
    }
    if (is_load && match_reg0)
    {
      // Not wrong to track load memory_target here. Preference.
      if (first_hit)
        return HitType::SAVELOAD;

      // If untracked load is overwriting tracked register, then remove register
      m_reg_autotrack.erase(reg_itr);
      return HitType::OVERWRITE;
    }
  }
  else if (!match_reg0 && !match_reg123)
  {
    // Skip if no matches. Happens most often.
    return HitType::SKIP;
  }
  else
  {
    // If tracked register data is being stored in a new register, save new register.
    if (match_reg123 && !match_reg0)
    {
      m_reg_autotrack.push_back(reg0);

      // This should include any instruction that can reach this point and is not ACTIVE. Can only
      // think of mr at this time.
      if (CompareInstruction(instruction, mover))
        return HitType::MOVED;

      return HitType::ACTIVE;
    }
    // If tracked register is overwritten, stop tracking.
    if (match_reg0 && !match_reg123)
    {
      if (CompareInstruction(instruction, combiner) || first_hit)
        return HitType::UPDATED;

      m_reg_autotrack.erase(reg_itr);
      return HitType::OVERWRITE;
    }
    if (match_reg0 && match_reg123)
    {
      // Or moved
      return HitType::UPDATED;
    }
  }

  // Should not reach this
  return HitType::SKIP;
}
