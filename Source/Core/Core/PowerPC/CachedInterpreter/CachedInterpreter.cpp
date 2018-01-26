// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/CachedInterpreter/CachedInterpreter.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64Common/Jit64Base.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

struct CachedInterpreter::Instruction
{
  typedef void (*CommonCallback)(UGeckoInstruction);
  typedef bool (*ConditionalCallback)(u32 data);

  Instruction() : type(INSTRUCTION_ABORT) {}
  Instruction(const CommonCallback c, UGeckoInstruction i)
      : common_callback(c), data(i.hex), type(INSTRUCTION_TYPE_COMMON)
  {
  }

  Instruction(const ConditionalCallback c, u32 d)
      : conditional_callback(c), data(d), type(INSTRUCTION_TYPE_CONDITIONAL)
  {
  }

  union
  {
    const CommonCallback common_callback;
    const ConditionalCallback conditional_callback;
  };
  u32 data;
  enum
  {
    INSTRUCTION_ABORT,
    INSTRUCTION_TYPE_COMMON,
    INSTRUCTION_TYPE_CONDITIONAL,
  } type;
};

CachedInterpreter::CachedInterpreter() : code_buffer(32000)
{
}

CachedInterpreter::~CachedInterpreter() = default;

void CachedInterpreter::Init()
{
  m_code.reserve(CODE_SIZE / sizeof(Instruction));

  jo.enableBlocklink = false;

  m_block_cache.Init();
  UpdateMemoryOptions();

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;
}

void CachedInterpreter::Shutdown()
{
  m_block_cache.Shutdown();
}

const u8* CachedInterpreter::GetCodePtr() const
{
  return reinterpret_cast<const u8*>(m_code.data() + m_code.size());
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

  for (; code->type != Instruction::INSTRUCTION_ABORT; ++code)
  {
    switch (code->type)
    {
    case Instruction::INSTRUCTION_TYPE_COMMON:
      code->common_callback(UGeckoInstruction(code->data));
      break;

    case Instruction::INSTRUCTION_TYPE_CONDITIONAL:
      if (code->conditional_callback(code->data))
        return;
      break;

    default:
      ERROR_LOG(POWERPC, "Unknown CachedInterpreter Instruction: %d", code->type);
      break;
    }
  }
}

void CachedInterpreter::Run()
{
  while (CPU::GetState() == CPU::State::Running)
  {
    // Start new timing slice
    // NOTE: Exceptions may change PC
    CoreTiming::Advance();

    do
    {
      ExecuteOneBlock();
    } while (PowerPC::ppcState.downcount > 0);
  }
}

void CachedInterpreter::SingleStep()
{
  // Enter new timing slice
  CoreTiming::Advance();
  ExecuteOneBlock();
}

static void EndBlock(UGeckoInstruction data)
{
  PC = NPC;
  PowerPC::ppcState.downcount -= data.hex;
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
  UReg_MSR msr{MSR};
  if (!msr.FP)
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

void CachedInterpreter::Jit(u32 address)
{
  if (m_code.size() >= CODE_SIZE / sizeof(Instruction) - 0x1000 ||
      SConfig::GetInstance().bJITNoBlockCache)
  {
    ClearCache();
  }

  u32 nextPC = analyzer.Analyze(PC, &code_block, &code_buffer, code_buffer.GetSize());
  if (code_block.m_memory_exception)
  {
    // Address of instruction could not be translated
    NPC = nextPC;
    PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
    PowerPC::CheckExceptions();
    WARN_LOG(POWERPC, "ISI exception at 0x%08x", nextPC);
    return;
  }

  JitBlock* b = m_block_cache.AllocateBlock(PC);

  js.blockStart = PC;
  js.firstFPInstructionFound = false;
  js.fifoBytesSinceCheck = 0;
  js.downcountAmount = 0;
  js.curBlock = b;

  PPCAnalyst::CodeOp* ops = code_buffer.codebuffer;

  b->checkedEntry = GetCodePtr();
  b->normalEntry = GetCodePtr();

  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    js.downcountAmount += ops[i].opinfo->numCycles;

    u32 function = HLE::GetFirstFunctionIndex(ops[i].address);
    if (function != 0)
    {
      int type = HLE::GetFunctionTypeByIndex(function);
      if (type == HLE::HLE_HOOK_START || type == HLE::HLE_HOOK_REPLACE)
      {
        int flags = HLE::GetFunctionFlagsByIndex(function);
        if (HLE::IsEnabled(flags))
        {
          m_code.emplace_back(WritePC, ops[i].address);
          m_code.emplace_back(Interpreter::HLEFunction, function);
          if (type == HLE::HLE_HOOK_REPLACE)
          {
            m_code.emplace_back(EndBlock, js.downcountAmount);
            m_code.emplace_back();
            break;
          }
        }
      }
    }

    if (!ops[i].skip)
    {
      bool check_fpu = (ops[i].opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound;
      bool endblock = (ops[i].opinfo->flags & FL_ENDBLOCK) != 0;
      bool memcheck = (ops[i].opinfo->flags & FL_LOADSTORE) && jo.memcheck;

      if (check_fpu)
      {
        m_code.emplace_back(WritePC, ops[i].address);
        m_code.emplace_back(CheckFPU, js.downcountAmount);
        js.firstFPInstructionFound = true;
      }

      if (endblock || memcheck)
        m_code.emplace_back(WritePC, ops[i].address);
      m_code.emplace_back(GetInterpreterOp(ops[i].inst), ops[i].inst);
      if (memcheck)
        m_code.emplace_back(CheckDSI, js.downcountAmount);
      if (endblock)
        m_code.emplace_back(EndBlock, js.downcountAmount);
    }
  }
  if (code_block.m_broken)
  {
    m_code.emplace_back(WriteBrokenBlockNPC, nextPC);
    m_code.emplace_back(EndBlock, js.downcountAmount);
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
  UpdateMemoryOptions();
}
