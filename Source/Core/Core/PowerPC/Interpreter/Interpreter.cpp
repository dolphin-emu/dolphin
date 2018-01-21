// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <array>
#include <cassert>
#include <cinttypes>
#include <string>

#include "Core/ARBruteForcer.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

namespace
{
u32 last_pc;
}

bool Interpreter::m_end_block;

// function tables
std::array<Interpreter::Instruction, 64> Interpreter::m_op_table;
std::array<Interpreter::Instruction, 1024> Interpreter::m_op_table4;
std::array<Interpreter::Instruction, 1024> Interpreter::m_op_table19;
std::array<Interpreter::Instruction, 1024> Interpreter::m_op_table31;
std::array<Interpreter::Instruction, 32> Interpreter::m_op_table59;
std::array<Interpreter::Instruction, 1024> Interpreter::m_op_table63;

void Interpreter::RunTable4(UGeckoInstruction inst)
{
  m_op_table4[inst.SUBOP10](inst);
}
void Interpreter::RunTable19(UGeckoInstruction inst)
{
  m_op_table19[inst.SUBOP10](inst);
}
void Interpreter::RunTable31(UGeckoInstruction inst)
{
  m_op_table31[inst.SUBOP10](inst);
}
void Interpreter::RunTable59(UGeckoInstruction inst)
{
  m_op_table59[inst.SUBOP5](inst);
}
void Interpreter::RunTable63(UGeckoInstruction inst)
{
  m_op_table63[inst.SUBOP10](inst);
}

void Interpreter::Init()
{
  InitializeInstructionTables();
  m_reserve = false;
  m_end_block = false;
}

void Interpreter::Shutdown()
{
}

static int startTrace = 0;

static void Trace(UGeckoInstruction& inst)
{
  std::string regs = "";
  for (int i = 0; i < 32; i++)
  {
    regs += StringFromFormat("r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
  }

  std::string fregs = "";
  for (int i = 0; i < 32; i++)
  {
    fregs += StringFromFormat("f%02d: %08" PRIx64 " %08" PRIx64 " ", i, PowerPC::ppcState.ps[i][0],
                              PowerPC::ppcState.ps[i][1]);
  }

  std::string ppc_inst = GekkoDisassembler::Disassemble(inst.hex, PC);
  DEBUG_LOG(POWERPC, "INTER PC: %08x SRR0: %08x SRR1: %08x CRval: %016lx FPSCR: %08x MSR: %08x LR: "
                     "%08x %s %08x %s",
            PC, SRR0, SRR1, (unsigned long)PowerPC::ppcState.cr_val[0], PowerPC::ppcState.fpscr,
            PowerPC::ppcState.msr, PowerPC::ppcState.spr[8], regs.c_str(), inst.hex,
            ppc_inst.c_str());
}

int Interpreter::SingleStepInner()
{
  static UGeckoInstruction instCode;
  u32 function = HLE::GetFirstFunctionIndex(PC);
  if (function != 0)
  {
    int type = HLE::GetFunctionTypeByIndex(function);
    if (type == HLE::HLE_HOOK_START || type == HLE::HLE_HOOK_REPLACE)
    {
      int flags = HLE::GetFunctionFlagsByIndex(function);
      if (HLE::IsEnabled(flags))
      {
        HLEFunction(function);
        if (type == HLE::HLE_HOOK_START)
        {
          // Run the original.
          function = 0;
        }
      }
      else
      {
        function = 0;
      }
    }
  }

  if (function == 0)
  {
#ifdef USE_GDBSTUB
    if (gdb_active() && gdb_bp_x(PC))
    {
      Host_UpdateDisasmDialog();

      gdb_signal(SIGTRAP);
      gdb_handle_exception();
    }
#endif

    NPC = PC + sizeof(UGeckoInstruction);
    instCode.hex = PowerPC::Read_Opcode(PC);

    // Uncomment to trace the interpreter
    // if ((PC & 0xffffff)>=0x0ab54c && (PC & 0xffffff)<=0x0ab624)
    //	startTrace = 1;
    // else
    //	startTrace = 0;

    if (startTrace)
    {
      Trace(instCode);
    }

    if (instCode.hex != 0)
    {
      UReg_MSR& msr = (UReg_MSR&)MSR;
      if (msr.FP)  // If FPU is enabled, just execute
      {
        m_op_table[instCode.OPCD](instCode);
        if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
        {
          PowerPC::CheckExceptions();
          m_end_block = true;
        }
      }
      else
      {
        // check if we have to generate a FPU unavailable exception
        if (!PPCTables::UsesFPU(instCode))
        {
          m_op_table[instCode.OPCD](instCode);
          if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
          {
            PowerPC::CheckExceptions();
            m_end_block = true;
          }
        }
        else
        {
          PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
          PowerPC::CheckExceptions();
          m_end_block = true;
        }
      }
    }
    else
    {
      // Memory exception on instruction fetch
      PowerPC::CheckExceptions();
      m_end_block = true;
    }
  }
  last_pc = PC;
  PC = NPC;

  GekkoOPInfo* opinfo = GetOpInfo(instCode);
  return opinfo->numCycles;
}

void Interpreter::SingleStep()
{
  // Declare start of new slice
  CoreTiming::Advance();

  SingleStepInner();

  // The interpreter ignores instruction timing information outside the 'fast runloop'.
  CoreTiming::g.slice_length = 1;
  PowerPC::ppcState.downcount = 0;

  if (PowerPC::ppcState.Exceptions)
  {
    PowerPC::CheckExceptions();
    PC = NPC;
  }
}

//#define SHOW_HISTORY
#ifdef SHOW_HISTORY
std::vector<int> PCVec;
std::vector<int> PCBlockVec;
int ShowBlocks = 30;
int ShowSteps = 300;
#endif

// FastRun - inspired by GCemu (to imitate the JIT so that they can be compared).
void Interpreter::Run()
{
  while (CPU::GetState() == CPU::State::Running)
  {
    // CoreTiming Advance() ends the previous slice and declares the start of the next
    // one so it must always be called at the start. At boot, we are in slice -1 and must
    // advance into slice 0 to get a correct slice length before executing any cycles.
    CoreTiming::Advance();

    // we have to check exceptions at branches apparently (or maybe just rfi?)
    if (SConfig::GetInstance().bEnableDebugging)
    {
#ifdef SHOW_HISTORY
      PCBlockVec.push_back(PC);
      if (PCBlockVec.size() > ShowBlocks)
        PCBlockVec.erase(PCBlockVec.begin());
#endif

      // Debugging friendly version of inner loop. Tries to do the timing as similarly to the
      // JIT as possible. Does not take into account that some instructions take multiple cycles.
      while (PowerPC::ppcState.downcount > 0)
      {
        m_end_block = false;
        int i;
        for (i = 0; !m_end_block; i++)
        {
#ifdef SHOW_HISTORY
          PCVec.push_back(PC);
          if (PCVec.size() > ShowSteps)
            PCVec.erase(PCVec.begin());
#endif

          // 2: check for breakpoint
          if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
          {
#ifdef SHOW_HISTORY
            NOTICE_LOG(POWERPC, "----------------------------");
            NOTICE_LOG(POWERPC, "Blocks:");
            for (int j = 0; j < PCBlockVec.size(); j++)
              NOTICE_LOG(POWERPC, "PC: 0x%08x", PCBlockVec.at(j));
            NOTICE_LOG(POWERPC, "----------------------------");
            NOTICE_LOG(POWERPC, "Steps:");
            for (int j = 0; j < PCVec.size(); j++)
            {
              // Write space
              if (j > 0)
              {
                if (PCVec.at(j) != PCVec.at(j - 1) + 4)
                  NOTICE_LOG(POWERPC, "");
              }

              NOTICE_LOG(POWERPC, "PC: 0x%08x", PCVec.at(j));
            }
#endif
            INFO_LOG(POWERPC, "Hit Breakpoint - %08x", PC);
            CPU::Break();
            if (PowerPC::breakpoints.IsTempBreakPoint(PC))
              PowerPC::breakpoints.Remove(PC);

            Host_UpdateDisasmDialog();
            return;
          }
          SingleStepInner();
        }
        PowerPC::ppcState.downcount -= i;
      }
    }
    else
    {
      // "fast" version of inner loop. well, it's not so fast.
      while (PowerPC::ppcState.downcount > 0)
      {
        m_end_block = false;

        int cycles = 0;
        while (!m_end_block)
        {
          cycles += SingleStepInner();
        }
        PowerPC::ppcState.downcount -= cycles;
      }
    }
  }
}

void Interpreter::unknown_instruction(UGeckoInstruction inst)
{
  if (ARBruteForcer::ch_bruteforce)
    Core::KillDolphinAndRestart();

  std::string disasm = GekkoDisassembler::Disassemble(PowerPC::HostRead_U32(last_pc), last_pc);
  NOTICE_LOG(POWERPC, "Last PC = %08x : %s", last_pc, disasm.c_str());
  Dolphin_Debugger::PrintCallstack();
  NOTICE_LOG(POWERPC,
             "\nIntCPU: Unknown instruction %08x at PC = %08x  last_PC = %08x  LR = %08x\n",
             inst.hex, PC, last_pc, LR);
  for (int i = 0; i < 32; i += 4)
    NOTICE_LOG(POWERPC, "r%d: 0x%08x r%d: 0x%08x r%d:0x%08x r%d: 0x%08x", i, rGPR[i], i + 1,
               rGPR[i + 1], i + 2, rGPR[i + 2], i + 3, rGPR[i + 3]);
  _assert_msg_(POWERPC, 0,
               "\nIntCPU: Unknown instruction %08x at PC = %08x  last_PC = %08x  LR = %08x\n",
               inst.hex, PC, last_pc, LR);
}

void Interpreter::ClearCache()
{
  // Do nothing.
}

const char* Interpreter::GetName()
{
#ifdef _ARCH_64
  return "Interpreter64";
#else
  return "Interpreter32";
#endif
}

Interpreter* Interpreter::getInstance()
{
  static Interpreter instance;
  return &instance;
}
