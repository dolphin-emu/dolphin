// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <array>
#include <string>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace
{
// Determines whether or not the given instruction is one where its execution
// validity is determined by whether or not HID2's LSQE bit is set.
// In other words, if the instruction is psq_l, psq_lu, psq_st, or psq_stu
bool IsPairedSingleQuantizedNonIndexedInstruction(UGeckoInstruction inst)
{
  const u32 opcode = inst.OPCD;
  return opcode == 0x38 || opcode == 0x39 || opcode == 0x3C || opcode == 0x3D;
}

bool IsPairedSingleInstruction(UGeckoInstruction inst)
{
  return inst.OPCD == 4 || IsPairedSingleQuantizedNonIndexedInstruction(inst);
}
}  // namespace

// Checks if a given instruction would be illegal to execute if it's a paired single instruction.
//
// Paired single instructions are illegal to execute if HID2.PSE is not set.
// It's also illegal to execute psq_l, psq_lu, psq_st, and psq_stu if HID2.PSE is enabled,
// but HID2.LSQE is not set.
bool Interpreter::IsInvalidPairedSingleExecution(UGeckoInstruction inst)
{
  if (!HID2(m_ppc_state).PSE && IsPairedSingleInstruction(inst))
    return true;

  return HID2(m_ppc_state).PSE && !HID2(m_ppc_state).LSQE &&
         IsPairedSingleQuantizedNonIndexedInstruction(inst);
}

void Interpreter::UpdatePC()
{
  m_last_pc = m_ppc_state.pc;
  m_ppc_state.pc = m_ppc_state.npc;
}

Interpreter::Interpreter(Core::System& system, PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu)
    : m_system(system), m_ppc_state(ppc_state), m_mmu(mmu)
{
}

Interpreter::~Interpreter() = default;

void Interpreter::Init()
{
  m_end_block = false;
}

void Interpreter::Shutdown()
{
}

void Interpreter::Trace(const UGeckoInstruction& inst)
{
  std::string regs;
  for (size_t i = 0; i < std::size(m_ppc_state.gpr); i++)
  {
    regs += fmt::format("r{:02d}: {:08x} ", i, m_ppc_state.gpr[i]);
  }

  std::string fregs;
  for (size_t i = 0; i < std::size(m_ppc_state.ps); i++)
  {
    const auto& ps = m_ppc_state.ps[i];
    fregs += fmt::format("f{:02d}: {:08x} {:08x} ", i, ps.PS0AsU64(), ps.PS1AsU64());
  }

  const std::string ppc_inst = Common::GekkoDisassembler::Disassemble(inst.hex, m_ppc_state.pc);
  DEBUG_LOG_FMT(POWERPC,
                "INTER PC: {:08x} SRR0: {:08x} SRR1: {:08x} CRval: {:016x} "
                "FPSCR: {:08x} MSR: {:08x} LR: {:08x} {} {:08x} {}",
                m_ppc_state.pc, SRR0(m_ppc_state), SRR1(m_ppc_state), m_ppc_state.cr.fields[0],
                m_ppc_state.fpscr.Hex, m_ppc_state.msr.Hex, m_ppc_state.spr[8], regs, inst.hex,
                ppc_inst);
}

bool Interpreter::HandleFunctionHooking(u32 address)
{
  return HLE::ReplaceFunctionIfPossible(address, [this](u32 hook_index, HLE::HookType type) {
    HLEFunction(*this, hook_index);
    return type != HLE::HookType::Start;
  });
}

int Interpreter::SingleStepInner()
{
  if (HandleFunctionHooking(m_ppc_state.pc))
  {
    UpdatePC();
    // TODO: Does it make sense to use m_prev_inst here?
    // It seems like we should use the num_cycles for the instruction at PC instead
    // (m_prev_inst has not yet been updated)
    return PPCTables::GetOpInfo(m_prev_inst, m_ppc_state.pc)->num_cycles;
  }

  m_ppc_state.npc = m_ppc_state.pc + sizeof(UGeckoInstruction);
  m_prev_inst.hex = m_mmu.Read_Opcode(m_ppc_state.pc);

  const GekkoOPInfo* opinfo = PPCTables::GetOpInfo(m_prev_inst, m_ppc_state.pc);

  // Uncomment to trace the interpreter
  // if ((m_ppc_state.pc & 0x00FFFFFF) >= 0x000AB54C &&
  //     (m_ppc_state.pc & 0x00FFFFFF) <= 0x000AB624)
  // {
  //   m_start_trace = true;
  // }
  // else
  // {
  //   m_start_trace = false;
  // }

  if (m_start_trace)
  {
    Trace(m_prev_inst);
  }

  if (m_prev_inst.hex != 0)
  {
    if (IsInvalidPairedSingleExecution(m_prev_inst))
    {
      GenerateProgramException(m_ppc_state, ProgramExceptionCause::IllegalInstruction);
      CheckExceptions();
    }
    else if (m_ppc_state.msr.FP)
    {
      RunInterpreterOp(*this, m_prev_inst);
      if ((m_ppc_state.Exceptions & EXCEPTION_DSI) != 0)
      {
        CheckExceptions();
      }
    }
    else
    {
      // check if we have to generate a FPU unavailable exception or a program exception.
      if ((opinfo->flags & FL_USE_FPU) != 0)
      {
        m_ppc_state.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
        CheckExceptions();
      }
      else
      {
        RunInterpreterOp(*this, m_prev_inst);
        if ((m_ppc_state.Exceptions & EXCEPTION_DSI) != 0)
        {
          CheckExceptions();
        }
      }
    }
  }
  else
  {
    // Memory exception on instruction fetch
    CheckExceptions();
  }

  UpdatePC();

  PowerPC::UpdatePerformanceMonitor(opinfo->num_cycles, (opinfo->flags & FL_LOADSTORE) != 0,
                                    (opinfo->flags & FL_USE_FPU) != 0, m_ppc_state);
  return opinfo->num_cycles;
}

void Interpreter::SingleStep()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& core_timing_globals = core_timing.GetGlobals();

  // Declare start of new slice
  core_timing.Advance();

  SingleStepInner();

  // The interpreter ignores instruction timing information outside the 'fast runloop'.
  core_timing_globals.slice_length = 1;
  m_ppc_state.downcount = 0;

  if (m_ppc_state.Exceptions != 0)
  {
    m_system.GetPowerPC().CheckExceptions();
    m_ppc_state.pc = m_ppc_state.npc;
  }
}

//#define SHOW_HISTORY
#ifdef SHOW_HISTORY
static std::vector<u32> s_pc_vec;
static std::vector<u32> s_pc_block_vec;
constexpr u32 s_show_blocks = 30;
constexpr u32 s_show_steps = 300;
#endif

// FastRun - inspired by GCemu (to imitate the JIT so that they can be compared).
void Interpreter::Run()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& cpu = m_system.GetCPU();
  auto& power_pc = m_system.GetPowerPC();
  while (cpu.GetState() == CPU::State::Running)
  {
    // CoreTiming Advance() ends the previous slice and declares the start of the next
    // one so it must always be called at the start. At boot, we are in slice -1 and must
    // advance into slice 0 to get a correct slice length before executing any cycles.
    core_timing.Advance();

    // we have to check exceptions at branches apparently (or maybe just rfi?)
    if (Config::Get(Config::MAIN_ENABLE_DEBUGGING))
    {
#ifdef SHOW_HISTORY
      s_pc_block_vec.push_back(m_ppc_state.pc);
      if (s_pc_block_vec.size() > s_show_blocks)
        s_pc_block_vec.erase(s_pc_block_vec.begin());
#endif

      // Debugging friendly version of inner loop. Tries to do the timing as similarly to the
      // JIT as possible. Does not take into account that some instructions take multiple cycles.
      while (m_ppc_state.downcount > 0)
      {
        m_end_block = false;
        int cycles = 0;
        while (!m_end_block)
        {
#ifdef SHOW_HISTORY
          s_pc_vec.push_back(m_ppc_state.pc);
          if (s_pc_vec.size() > s_show_steps)
            s_pc_vec.erase(s_pc_vec.begin());
#endif

          // 2: check for breakpoint
          if (power_pc.GetBreakPoints().IsAddressBreakPoint(m_ppc_state.pc))
          {
#ifdef SHOW_HISTORY
            NOTICE_LOG_FMT(POWERPC, "----------------------------");
            NOTICE_LOG_FMT(POWERPC, "Blocks:");
            for (const u32 entry : s_pc_block_vec)
              NOTICE_LOG_FMT(POWERPC, "PC: {:#010x}", entry);
            NOTICE_LOG_FMT(POWERPC, "----------------------------");
            NOTICE_LOG_FMT(POWERPC, "Steps:");
            for (size_t j = 0; j < s_pc_vec.size(); j++)
            {
              // Write space
              if (j > 0)
              {
                if (s_pc_vec[j] != s_pc_vec[(j - 1) + 4]
                  NOTICE_LOG_FMT(POWERPC, "");
              }

              NOTICE_LOG_FMT(POWERPC, "PC: {:#010x}", s_pc_vec[j]);
            }
#endif
            INFO_LOG_FMT(POWERPC, "Hit Breakpoint - {:08x}", m_ppc_state.pc);
            cpu.Break();
            if (GDBStub::IsActive())
              GDBStub::TakeControl();
            if (power_pc.GetBreakPoints().IsTempBreakPoint(m_ppc_state.pc))
              power_pc.GetBreakPoints().Remove(m_ppc_state.pc);

            Host_UpdateDisasmDialog();
            return;
          }
          cycles += SingleStepInner();
        }
        m_ppc_state.downcount -= cycles;
      }
    }
    else
    {
      // "fast" version of inner loop. well, it's not so fast.
      while (m_ppc_state.downcount > 0)
      {
        m_end_block = false;

        int cycles = 0;
        while (!m_end_block)
        {
          cycles += SingleStepInner();
        }
        m_ppc_state.downcount -= cycles;
      }
    }
  }
}

void Interpreter::unknown_instruction(Interpreter& interpreter, UGeckoInstruction inst)
{
  ASSERT(Core::IsCPUThread());
  auto& system = interpreter.m_system;
  Core::CPUThreadGuard guard(system);

  const u32 last_pc = interpreter.m_last_pc;
  const u32 opcode = PowerPC::MMU::HostRead_U32(guard, last_pc);
  const std::string disasm = Common::GekkoDisassembler::Disassemble(opcode, last_pc);
  NOTICE_LOG_FMT(POWERPC, "Last PC = {:08x} : {}", last_pc, disasm);
  Dolphin_Debugger::PrintCallstack(system, guard, Common::Log::LogType::POWERPC,
                                   Common::Log::LogLevel::LNOTICE);

  const auto& ppc_state = interpreter.m_ppc_state;
  NOTICE_LOG_FMT(
      POWERPC,
      "\nIntCPU: Unknown instruction {:08x} at PC = {:08x}  last_PC = {:08x}  LR = {:08x}\n",
      inst.hex, ppc_state.pc, last_pc, LR(ppc_state));
  for (int i = 0; i < 32; i += 4)
  {
    NOTICE_LOG_FMT(POWERPC, "r{}: {:#010x} r{}: {:#010x} r{}: {:#010x} r{}: {:#010x}", i,
                   ppc_state.gpr[i], i + 1, ppc_state.gpr[i + 1], i + 2, ppc_state.gpr[i + 2],
                   i + 3, ppc_state.gpr[i + 3]);
  }
  ASSERT_MSG(POWERPC, 0,
             "\nIntCPU: Unknown instruction {:08x} at PC = {:08x}  last_PC = {:08x}  LR = {:08x}\n",
             inst.hex, ppc_state.pc, last_pc, LR(ppc_state));
  if (system.IsPauseOnPanicMode())
    system.GetCPU().Break();
}

void Interpreter::ClearCache()
{
  // Do nothing.
}

void Interpreter::CheckExceptions()
{
  m_system.GetPowerPC().CheckExceptions();
  m_end_block = true;
}

const char* Interpreter::GetName() const
{
#ifdef _ARCH_64
  return "Interpreter64";
#else
  return "Interpreter32";
#endif
}
