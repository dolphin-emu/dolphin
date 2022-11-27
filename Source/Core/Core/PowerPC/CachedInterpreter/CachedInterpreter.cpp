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
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

struct CachedInterpreter::Instruction
{
  using CommonCallback = void (*)(UGeckoInstruction);
  using ConditionalCallback = bool (*)(u32);

  Instruction() {}
  Instruction(const CommonCallback c, UGeckoInstruction i)
      : common_callback(c), data(i.hex), type(Type::Common)
  {
  }

  Instruction(const ConditionalCallback c, u32 d)
      : conditional_callback(c), data(d), type(Type::Conditional)
  {
  }

  enum class Type
  {
    Abort,
    Common,
    Conditional,
  };

  union
  {
    const CommonCallback common_callback;
    const ConditionalCallback conditional_callback;
  };

  u32 data = 0;
  Type type = Type::Abort;
};

CachedInterpreter::CachedInterpreter() = default;

CachedInterpreter::~CachedInterpreter() = default;

void CachedInterpreter::Init()
{
  m_code.reserve(CODE_SIZE / sizeof(Instruction));

  jo.enableBlocklink = false;

  m_block_cache.Init();
  UpdateMemoryAndExceptionOptions();

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
    Jit(PC);
    return;
  }

  const Instruction* code = reinterpret_cast<const Instruction*>(normal_entry);

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

    default:
      ERROR_LOG_FMT(POWERPC, "Unknown CachedInterpreter Instruction: {}",
                    static_cast<int>(code->type));
      break;
    }
  }
}

void CachedInterpreter::Run()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();

  const CPU::State* state_ptr = CPU::GetStatePtr();
  while (CPU::GetState() == CPU::State::Running)
  {
    // Start new timing slice
    // NOTE: Exceptions may change PC
    core_timing.Advance();

    do
    {
      ExecuteOneBlock();
    } while (PowerPC::ppcState.downcount > 0 && *state_ptr == CPU::State::Running);
  }
}

void CachedInterpreter::SingleStep()
{
  // Enter new timing slice
  Core::System::GetInstance().GetCoreTiming().Advance();
  ExecuteOneBlock();
}

static void EndBlock(UGeckoInstruction data)
{
  PC = NPC;
  PowerPC::ppcState.downcount -= data.hex;
  PowerPC::UpdatePerformanceMonitor(data.hex, 0, 0);
}

static void UpdateNumLoadStoreInstructions(UGeckoInstruction data)
{
  PowerPC::UpdatePerformanceMonitor(0, data.hex, 0);
}

static void UpdateNumFloatingPointInstructions(UGeckoInstruction data)
{
  PowerPC::UpdatePerformanceMonitor(0, 0, data.hex);
}

static void WritePC(UGeckoInstruction data)
{
  PC = data.hex;
  NPC = data.hex + 4;
}

static void WriteBrokenBlockNPC(UGeckoInstruction data)
{
  NPC = data.hex;
}

static bool CheckFPU(u32 data)
{
  if (!MSR.FP)
  {
    PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
    PowerPC::CheckExceptions();
    PowerPC::ppcState.downcount -= data;
    return true;
  }
  return false;
}

static bool CheckDSI(u32 data)
{
  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    PowerPC::CheckExceptions();
    PowerPC::ppcState.downcount -= data;
    return true;
  }
  return false;
}

static bool CheckProgramException(u32 data)
{
  if (PowerPC::ppcState.Exceptions & EXCEPTION_PROGRAM)
  {
    PowerPC::CheckExceptions();
    PowerPC::ppcState.downcount -= data;
    return true;
  }
  return false;
}

static bool CheckBreakpoint(u32 data)
{
  PowerPC::CheckBreakPoints();
  if (CPU::GetState() != CPU::State::Running)
  {
    PowerPC::ppcState.downcount -= data;
    return true;
  }
  return false;
}

static bool CheckIdle(u32 idle_pc)
{
  if (PowerPC::ppcState.npc == idle_pc)
  {
    Core::System::GetInstance().GetCoreTiming().Idle();
  }
  return false;
}

bool CachedInterpreter::HandleFunctionHooking(u32 address)
{
  return HLE::ReplaceFunctionIfPossible(address, [&](u32 hook_index, HLE::HookType type) {
    m_code.emplace_back(WritePC, address);
    m_code.emplace_back(Interpreter::HLEFunction, hook_index);

    if (type != HLE::HookType::Replace)
      return false;

    m_code.emplace_back(EndBlock, js.downcountAmount);
    m_code.emplace_back();
    return true;
  });
}

void CachedInterpreter::Jit(u32 address)
{
  if (m_code.size() >= CODE_SIZE / sizeof(Instruction) - 0x1000 ||
      SConfig::GetInstance().bJITNoBlockCache)
  {
    ClearCache();
  }

  const u32 nextPC = analyzer.Analyze(PC, &code_block, &m_code_buffer, m_code_buffer.size());
  if (code_block.m_memory_exception)
  {
    // Address of instruction could not be translated
    NPC = nextPC;
    PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
    PowerPC::CheckExceptions();
    WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", nextPC);
    return;
  }

  JitBlock* b = m_block_cache.AllocateBlock(PC);

  js.blockStart = PC;
  js.firstFPInstructionFound = false;
  js.fifoBytesSinceCheck = 0;
  js.downcountAmount = 0;
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;
  js.curBlock = b;

  b->checkedEntry = GetCodePtr();
  b->normalEntry = GetCodePtr();

  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    PPCAnalyst::CodeOp& op = m_code_buffer[i];

    js.downcountAmount += op.opinfo->numCycles;
    if (op.opinfo->flags & FL_LOADSTORE)
      ++js.numLoadStoreInst;
    if (op.opinfo->flags & FL_USE_FPU)
      ++js.numFloatingPointInst;

    if (HandleFunctionHooking(op.address))
      break;

    if (!op.skip)
    {
      const bool breakpoint =
          m_enable_debugging && PowerPC::breakpoints.IsAddressBreakPoint(op.address);
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

      m_code.emplace_back(PPCTables::GetInterpreterOp(op.inst), op.inst);
      if (memcheck)
        m_code.emplace_back(CheckDSI, js.downcountAmount);
      if (check_program_exception)
        m_code.emplace_back(CheckProgramException, js.downcountAmount);
      if (idle_loop)
        m_code.emplace_back(CheckIdle, js.blockStart);
      if (endblock)
      {
        m_code.emplace_back(EndBlock, js.downcountAmount);
        m_code.emplace_back(UpdateNumLoadStoreInstructions, js.numLoadStoreInst);
        m_code.emplace_back(UpdateNumFloatingPointInstructions, js.numFloatingPointInst);
      }
    }
  }
  if (code_block.m_broken)
  {
    m_code.emplace_back(WriteBrokenBlockNPC, nextPC);
    m_code.emplace_back(EndBlock, js.downcountAmount);
    m_code.emplace_back(UpdateNumLoadStoreInstructions, js.numLoadStoreInst);
    m_code.emplace_back(UpdateNumFloatingPointInstructions, js.numFloatingPointInst);
  }
  m_code.emplace_back();

  b->codeSize = (u32)(GetCodePtr() - b->checkedEntry);
  b->originalSize = code_block.m_num_instructions;

  m_block_cache.FinalizeBlock(*b, jo.enableBlocklink, code_block.m_physical_addresses);
}

void CachedInterpreter::ClearCache()
{
  m_code.clear();
  m_block_cache.Clear();
  UpdateMemoryAndExceptionOptions();
}
