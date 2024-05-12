// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

struct CachedInterpreter::Instruction
{
  using CommonCallback = void (*)(UGeckoInstruction);
  using ConditionalCallback = bool (*)(u32);
  using InterpreterCallback = void (*)(Interpreter&, UGeckoInstruction);
  using CachedInterpreterCallback = void (*)(CachedInterpreter&, UGeckoInstruction);
  using ConditionalCachedInterpreterCallback = bool (*)(CachedInterpreter&, u32);

  Instruction() {}
  Instruction(const CommonCallback c, UGeckoInstruction i)
      : common_callback(c), data(i.hex), type(Type::Common)
  {
  }

  Instruction(const ConditionalCallback c, u32 d)
      : conditional_callback(c), data(d), type(Type::Conditional)
  {
  }

  Instruction(const InterpreterCallback c, UGeckoInstruction i)
      : interpreter_callback(c), data(i.hex), type(Type::Interpreter)
  {
  }

  Instruction(const CachedInterpreterCallback c, UGeckoInstruction i)
      : cached_interpreter_callback(c), data(i.hex), type(Type::CachedInterpreter)
  {
  }

  Instruction(const ConditionalCachedInterpreterCallback c, u32 d)
      : conditional_cached_interpreter_callback(c), data(d),
        type(Type::ConditionalCachedInterpreter)
  {
  }

  enum class Type
  {
    Abort,
    Common,
    Conditional,
    Interpreter,
    CachedInterpreter,
    ConditionalCachedInterpreter,
  };

  union
  {
    const CommonCallback common_callback = nullptr;
    const ConditionalCallback conditional_callback;
    const InterpreterCallback interpreter_callback;
    const CachedInterpreterCallback cached_interpreter_callback;
    const ConditionalCachedInterpreterCallback conditional_cached_interpreter_callback;
  };

  u32 data = 0;
  Type type = Type::Abort;
};

CachedInterpreter::CachedInterpreter(Core::System& system) : JitBase(system)
{
}

CachedInterpreter::~CachedInterpreter() = default;

void CachedInterpreter::Init()
{
  RefreshConfig();

  m_code.reserve(CODE_SIZE / sizeof(Instruction));

  jo.enableBlocklink = false;

  m_block_cache.Init();

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;
}

void CachedInterpreter::Shutdown()
{
  m_block_cache.Shutdown();
}

u8* CachedInterpreter::GetCodePtr()
{
  return reinterpret_cast<u8*>(m_code.data() + m_code.size());
}

void CachedInterpreter::ExecuteOneBlock()
{
  const u8* normal_entry = m_block_cache.Dispatch();
  if (!normal_entry)
  {
    Jit(m_ppc_state.pc);
    return;
  }

  const Instruction* code = reinterpret_cast<const Instruction*>(normal_entry);
  auto& interpreter = m_system.GetInterpreter();

  for (; code->type != Instruction::Type::Abort; ++code)
  {
    switch (code->type)
    {
    case Instruction::Type::Common:
      code->common_callback(UGeckoInstruction(code->data));
      break;

    case Instruction::Type::Conditional:
      if (code->conditional_callback(code->data))
        return;
      break;

    case Instruction::Type::Interpreter:
      code->interpreter_callback(interpreter, UGeckoInstruction(code->data));
      break;

    case Instruction::Type::CachedInterpreter:
      code->cached_interpreter_callback(*this, UGeckoInstruction(code->data));
      break;

    case Instruction::Type::ConditionalCachedInterpreter:
      if (code->conditional_cached_interpreter_callback(*this, code->data))
        return;
      break;

    default:
      ERROR_LOG_FMT(POWERPC, "Unknown CachedInterpreter Instruction: {}",
                    static_cast<int>(code->type));
      break;
    }
  }
}

void CachedInterpreter::Run()
{
  auto& core_timing = m_system.GetCoreTiming();
  auto& cpu = m_system.GetCPU();

  const CPU::State* state_ptr = cpu.GetStatePtr();
  while (cpu.GetState() == CPU::State::Running)
  {
    // Start new timing slice
    // NOTE: Exceptions may change PC
    core_timing.Advance();

    do
    {
      ExecuteOneBlock();
    } while (m_ppc_state.downcount > 0 && *state_ptr == CPU::State::Running);
  }
}

void CachedInterpreter::SingleStep()
{
  // Enter new timing slice
  m_system.GetCoreTiming().Advance();
  ExecuteOneBlock();
}

void CachedInterpreter::EndBlock(CachedInterpreter& cached_interpreter, UGeckoInstruction data)
{
  auto& ppc_state = cached_interpreter.m_ppc_state;
  ppc_state.pc = ppc_state.npc;
  ppc_state.downcount -= data.hex;
  PowerPC::UpdatePerformanceMonitor(data.hex, 0, 0, ppc_state);
}

void CachedInterpreter::UpdateNumLoadStoreInstructions(CachedInterpreter& cached_interpreter,
                                                       UGeckoInstruction data)
{
  PowerPC::UpdatePerformanceMonitor(0, data.hex, 0, cached_interpreter.m_ppc_state);
}

void CachedInterpreter::UpdateNumFloatingPointInstructions(CachedInterpreter& cached_interpreter,
                                                           UGeckoInstruction data)
{
  PowerPC::UpdatePerformanceMonitor(0, 0, data.hex, cached_interpreter.m_ppc_state);
}

void CachedInterpreter::WritePC(CachedInterpreter& cached_interpreter, UGeckoInstruction data)
{
  auto& ppc_state = cached_interpreter.m_ppc_state;
  ppc_state.pc = data.hex;
  ppc_state.npc = data.hex + 4;
}

void CachedInterpreter::WriteBrokenBlockNPC(CachedInterpreter& cached_interpreter,
                                            UGeckoInstruction data)
{
  cached_interpreter.m_ppc_state.npc = data.hex;
}

bool CachedInterpreter::CheckFPU(CachedInterpreter& cached_interpreter, u32 data)
{
  auto& ppc_state = cached_interpreter.m_ppc_state;
  if (!ppc_state.msr.FP)
  {
    ppc_state.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
    cached_interpreter.m_system.GetPowerPC().CheckExceptions();
    ppc_state.downcount -= data;
    return true;
  }
  return false;
}

bool CachedInterpreter::CheckDSI(CachedInterpreter& cached_interpreter, u32 data)
{
  auto& ppc_state = cached_interpreter.m_ppc_state;
  if (ppc_state.Exceptions & EXCEPTION_DSI)
  {
    cached_interpreter.m_system.GetPowerPC().CheckExceptions();
    ppc_state.downcount -= data;
    return true;
  }
  return false;
}

bool CachedInterpreter::CheckProgramException(CachedInterpreter& cached_interpreter, u32 data)
{
  auto& ppc_state = cached_interpreter.m_ppc_state;
  if (ppc_state.Exceptions & EXCEPTION_PROGRAM)
  {
    cached_interpreter.m_system.GetPowerPC().CheckExceptions();
    ppc_state.downcount -= data;
    return true;
  }
  return false;
}

bool CachedInterpreter::CheckBreakpoint(CachedInterpreter& cached_interpreter, u32 data)
{
  cached_interpreter.m_system.GetPowerPC().CheckBreakPoints();
  if (cached_interpreter.m_system.GetCPU().GetState() != CPU::State::Running)
  {
    cached_interpreter.m_ppc_state.downcount -= data;
    return true;
  }
  return false;
}

bool CachedInterpreter::CheckIdle(CachedInterpreter& cached_interpreter, u32 idle_pc)
{
  if (cached_interpreter.m_ppc_state.npc == idle_pc)
  {
    cached_interpreter.m_system.GetCoreTiming().Idle();
  }
  return false;
}

bool CachedInterpreter::HandleFunctionHooking(u32 address)
{
  // CachedInterpreter inherits from JitBase and is considered a JIT by relevant code.
  // (see JitInterface and how m_mode is set within PowerPC.cpp)
  const auto result = HLE::TryReplaceFunction(m_ppc_symbol_db, address, PowerPC::CoreMode::JIT);
  if (!result)
    return false;

  m_code.emplace_back(WritePC, address);
  m_code.emplace_back(Interpreter::HLEFunction, result.hook_index);

  if (result.type != HLE::HookType::Replace)
    return false;

  m_code.emplace_back(EndBlock, js.downcountAmount);
  m_code.emplace_back();
  return true;
}

void CachedInterpreter::Jit(u32 address)
{
  if (m_code.size() >= CODE_SIZE / sizeof(Instruction) - 0x1000 ||
      SConfig::GetInstance().bJITNoBlockCache)
  {
    ClearCache();
  }

  const u32 nextPC =
      analyzer.Analyze(m_ppc_state.pc, &code_block, &m_code_buffer, m_code_buffer.size());
  if (code_block.m_memory_exception)
  {
    // Address of instruction could not be translated
    m_ppc_state.npc = nextPC;
    m_ppc_state.Exceptions |= EXCEPTION_ISI;
    m_system.GetPowerPC().CheckExceptions();
    WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", nextPC);
    return;
  }

  JitBlock* b = m_block_cache.AllocateBlock(m_ppc_state.pc);

  js.blockStart = m_ppc_state.pc;
  js.firstFPInstructionFound = false;
  js.fifoBytesSinceCheck = 0;
  js.downcountAmount = 0;
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;
  js.curBlock = b;

  b->normalEntry = b->near_begin = GetCodePtr();

  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    PPCAnalyst::CodeOp& op = m_code_buffer[i];

    js.downcountAmount += op.opinfo->num_cycles;
    if (op.opinfo->flags & FL_LOADSTORE)
      ++js.numLoadStoreInst;
    if (op.opinfo->flags & FL_USE_FPU)
      ++js.numFloatingPointInst;

    if (HandleFunctionHooking(op.address))
      break;

    if (!op.skip)
    {
      const bool breakpoint =
          m_enable_debugging &&
          m_system.GetPowerPC().GetBreakPoints().IsAddressBreakPoint(op.address);
      const bool check_fpu = (op.opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound;
      const bool endblock = (op.opinfo->flags & FL_ENDBLOCK) != 0;
      const bool memcheck = (op.opinfo->flags & FL_LOADSTORE) && jo.memcheck;
      const bool check_program_exception = !endblock && ShouldHandleFPExceptionForInstruction(&op);
      const bool idle_loop = op.branchIsIdleLoop;

      if (breakpoint || check_fpu || endblock || memcheck || check_program_exception)
        m_code.emplace_back(WritePC, op.address);

      if (breakpoint)
        m_code.emplace_back(CheckBreakpoint, js.downcountAmount);

      if (check_fpu)
      {
        m_code.emplace_back(CheckFPU, js.downcountAmount);
        js.firstFPInstructionFound = true;
      }

      m_code.emplace_back(Interpreter::GetInterpreterOp(op.inst), op.inst);
      if (memcheck)
        m_code.emplace_back(CheckDSI, js.downcountAmount);
      if (check_program_exception)
        m_code.emplace_back(CheckProgramException, js.downcountAmount);
      if (idle_loop)
        m_code.emplace_back(CheckIdle, js.blockStart);
      if (endblock)
      {
        m_code.emplace_back(EndBlock, js.downcountAmount);
        if (js.numLoadStoreInst != 0)
          m_code.emplace_back(UpdateNumLoadStoreInstructions, js.numLoadStoreInst);
        if (js.numFloatingPointInst != 0)
          m_code.emplace_back(UpdateNumFloatingPointInstructions, js.numFloatingPointInst);
      }
    }
  }
  if (code_block.m_broken)
  {
    m_code.emplace_back(WriteBrokenBlockNPC, nextPC);
    m_code.emplace_back(EndBlock, js.downcountAmount);
    if (js.numLoadStoreInst != 0)
      m_code.emplace_back(UpdateNumLoadStoreInstructions, js.numLoadStoreInst);
    if (js.numFloatingPointInst != 0)
      m_code.emplace_back(UpdateNumFloatingPointInstructions, js.numFloatingPointInst);
  }
  m_code.emplace_back();

  b->near_end = GetCodePtr();
  b->far_begin = nullptr;
  b->far_end = nullptr;

  b->codeSize = static_cast<u32>(GetCodePtr() - b->normalEntry);
  b->originalSize = code_block.m_num_instructions;

  m_block_cache.FinalizeBlock(*b, jo.enableBlocklink, code_block.m_physical_addresses);
}

void CachedInterpreter::ClearCache()
{
  m_code.clear();
  m_block_cache.Clear();
  RefreshConfig();
}
