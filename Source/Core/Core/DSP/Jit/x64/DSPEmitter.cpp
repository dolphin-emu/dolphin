// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/Jit/x64/DSPEmitter.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntTables.h"
#include "Core/DSP/Jit/x64/DSPJitTables.h"

using namespace Gen;

namespace DSP::JIT::x64
{
constexpr size_t COMPILED_CODE_SIZE = 2097152;
constexpr size_t MAX_BLOCK_SIZE = 250;
constexpr u16 DSP_IDLE_SKIP_CYCLES = 0x1000;

DSPEmitter::DSPEmitter()
    : m_compile_status_register{SR_INT_ENABLE | SR_EXT_INT_ENABLE}, m_blocks(MAX_BLOCKS),
      m_block_size(MAX_BLOCKS), m_block_links(MAX_BLOCKS)
{
  x64::InitInstructionTables();
  AllocCodeSpace(COMPILED_CODE_SIZE);

  CompileDispatcher();
  m_stub_entry_point = CompileStub();

  // Clear all of the block references
  std::fill(m_blocks.begin(), m_blocks.end(), (DSPCompiledCode)m_stub_entry_point);
}

DSPEmitter::~DSPEmitter()
{
  FreeCodeSpace();
}

u16 DSPEmitter::RunCycles(u16 cycles)
{
  if (g_dsp.external_interrupt_waiting)
  {
    DSPCore_CheckExternalInterrupt();
    DSPCore_CheckExceptions();
    DSPCore_SetExternalInterrupt(false);
  }

  m_cycles_left = cycles;
  auto exec_addr = (DSPCompiledCode)m_enter_dispatcher;
  exec_addr();

  if (g_dsp.reset_dspjit_codespace)
    ClearIRAMandDSPJITCodespaceReset();

  return m_cycles_left;
}

void DSPEmitter::DoState(PointerWrap& p)
{
  p.Do(m_cycles_left);
}

void DSPEmitter::ClearIRAM()
{
  for (size_t i = 0; i < DSP_IRAM_SIZE; i++)
  {
    m_blocks[i] = (DSPCompiledCode)m_stub_entry_point;
    m_block_links[i] = nullptr;
    m_block_size[i] = 0;
    m_unresolved_jumps[i].clear();
  }
  g_dsp.reset_dspjit_codespace = true;
}

void DSPEmitter::ClearIRAMandDSPJITCodespaceReset()
{
  ClearCodeSpace();
  CompileDispatcher();
  m_stub_entry_point = CompileStub();

  for (size_t i = 0; i < MAX_BLOCKS; i++)
  {
    m_blocks[i] = (DSPCompiledCode)m_stub_entry_point;
    m_block_links[i] = nullptr;
    m_block_size[i] = 0;
    m_unresolved_jumps[i].clear();
  }
  g_dsp.reset_dspjit_codespace = false;
}

// Must go out of block if exception is detected
void DSPEmitter::checkExceptions(u32 retval)
{
  // Check for interrupts and exceptions
  TEST(8, M_SDSP_exceptions(), Imm8(0xff));
  FixupBranch skipCheck = J_CC(CC_Z, true);

  MOV(16, M_SDSP_pc(), Imm16(m_compile_pc));

  DSPJitRegCache c(m_gpr);
  m_gpr.SaveRegs();
  ABI_CallFunction(DSPCore_CheckExceptions);
  MOV(32, R(EAX), Imm32(retval));
  JMP(m_return_dispatcher, true);
  m_gpr.LoadRegs(false);
  m_gpr.FlushRegs(c, false);

  SetJumpTarget(skipCheck);
}

bool DSPEmitter::FlagsNeeded() const
{
  const u8 flags = Analyzer::GetCodeFlags(m_compile_pc);

  return !(flags & Analyzer::CODE_START_OF_INST) || (flags & Analyzer::CODE_UPDATE_SR);
}

void DSPEmitter::FallBackToInterpreter(UDSPInstruction inst)
{
  const DSPOPCTemplate* const op_template = GetOpTemplate(inst);

  if (op_template->reads_pc)
  {
    // Increment PC - we shouldn't need to do this for every instruction. only for branches and end
    // of block.
    // Fallbacks to interpreter need this for fetching immediate values

    MOV(16, M_SDSP_pc(), Imm16(m_compile_pc + 1));
  }

  // Fall back to interpreter
  const auto interpreter_function = Interpreter::GetOp(inst);

  m_gpr.PushRegs();
  ASSERT_MSG(DSPLLE, interpreter_function != nullptr, "No function for %04x", inst);
  ABI_CallFunctionC16(interpreter_function, inst);
  m_gpr.PopRegs();
}

void DSPEmitter::EmitInstruction(UDSPInstruction inst)
{
  const DSPOPCTemplate* const op_template = GetOpTemplate(inst);
  bool ext_is_jit = false;

  // Call extended
  if (op_template->extended)
  {
    const auto jit_function = GetExtOp(inst);

    if (jit_function)
    {
      (this->*jit_function)(inst);
      ext_is_jit = true;
    }
    else
    {
      // Fall back to interpreter
      const auto interpreter_function = Interpreter::GetExtOp(inst);

      m_gpr.PushRegs();
      ABI_CallFunctionC16(interpreter_function, inst);
      m_gpr.PopRegs();
      INFO_LOG(DSPLLE, "Instruction not JITed(ext part): %04x", inst);
      ext_is_jit = false;
    }
  }

  // Main instruction
  const auto jit_function = GetOp(inst);
  if (jit_function)
  {
    (this->*jit_function)(inst);
  }
  else
  {
    FallBackToInterpreter(inst);
    INFO_LOG(DSPLLE, "Instruction not JITed(main part): %04x", inst);
  }

  // Backlog
  if (op_template->extended)
  {
    if (!ext_is_jit)
    {
      // need to call the online cleanup function because
      // the writeBackLog gets populated at runtime
      m_gpr.PushRegs();
      ABI_CallFunction(ApplyWriteBackLog);
      m_gpr.PopRegs();
    }
    else
    {
      popExtValueToReg();
    }
  }
}

void DSPEmitter::Compile(u16 start_addr)
{
  // Remember the current block address for later
  m_start_address = start_addr;
  m_unresolved_jumps[start_addr].clear();

  const u8* entryPoint = AlignCode16();

  m_gpr.LoadRegs();

  m_block_link_entry = GetCodePtr();

  m_compile_pc = start_addr;
  bool fixup_pc = false;
  m_block_size[start_addr] = 0;

  while (m_compile_pc < start_addr + MAX_BLOCK_SIZE)
  {
    if (Analyzer::GetCodeFlags(m_compile_pc) & Analyzer::CODE_CHECK_INT)
      checkExceptions(m_block_size[start_addr]);

    UDSPInstruction inst = dsp_imem_read(m_compile_pc);
    const DSPOPCTemplate* opcode = GetOpTemplate(inst);

    EmitInstruction(inst);

    m_block_size[start_addr]++;
    m_compile_pc += opcode->size;

    // If the block was trying to link into itself, remove the link
    m_unresolved_jumps[start_addr].remove(m_compile_pc);

    fixup_pc = true;

    // Handle loop condition, only if current instruction was flagged as a loop destination
    // by the analyzer.
    if (Analyzer::GetCodeFlags(static_cast<u16>(m_compile_pc - 1u)) & Analyzer::CODE_LOOP_END)
    {
      MOVZX(32, 16, EAX, M_SDSP_r_st(2));
      TEST(32, R(EAX), R(EAX));
      FixupBranch rLoopAddressExit = J_CC(CC_LE, true);

      MOVZX(32, 16, EAX, M_SDSP_r_st(3));
      TEST(32, R(EAX), R(EAX));
      FixupBranch rLoopCounterExit = J_CC(CC_LE, true);

      if (!opcode->branch)
      {
        // branch insns update the g_dsp.pc
        MOV(16, M_SDSP_pc(), Imm16(m_compile_pc));
      }

      // These functions branch and therefore only need to be called in the
      // end of each block and in this order
      DSPJitRegCache c(m_gpr);
      HandleLoop();
      m_gpr.SaveRegs();
      if (!Host::OnThread() && Analyzer::GetCodeFlags(start_addr) & Analyzer::CODE_IDLE_SKIP)
      {
        MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
      }
      else
      {
        MOV(16, R(EAX), Imm16(m_block_size[start_addr]));
      }
      JMP(m_return_dispatcher, true);
      m_gpr.LoadRegs(false);
      m_gpr.FlushRegs(c, false);

      SetJumpTarget(rLoopAddressExit);
      SetJumpTarget(rLoopCounterExit);
    }

    if (opcode->branch)
    {
      // don't update g_dsp.pc -- the branch insn already did
      fixup_pc = false;
      if (opcode->uncond_branch)
      {
        break;
      }

      const auto jit_function = GetOp(inst);
      if (!jit_function)
      {
        // look at g_dsp.pc if we actually branched
        MOV(16, R(AX), M_SDSP_pc());
        CMP(16, R(AX), Imm16(m_compile_pc));
        FixupBranch rNoBranch = J_CC(CC_Z, true);

        DSPJitRegCache c(m_gpr);
        // don't update g_dsp.pc -- the branch insn already did
        m_gpr.SaveRegs();
        if (!Host::OnThread() && Analyzer::GetCodeFlags(start_addr) & Analyzer::CODE_IDLE_SKIP)
        {
          MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
        }
        else
        {
          MOV(16, R(EAX), Imm16(m_block_size[start_addr]));
        }
        JMP(m_return_dispatcher, true);
        m_gpr.LoadRegs(false);
        m_gpr.FlushRegs(c, false);

        SetJumpTarget(rNoBranch);
      }
    }

    // End the block if we're before an idle skip address
    if (Analyzer::GetCodeFlags(m_compile_pc) & Analyzer::CODE_IDLE_SKIP)
    {
      break;
    }
  }

  if (fixup_pc)
  {
    MOV(16, M_SDSP_pc(), Imm16(m_compile_pc));
  }

  m_blocks[start_addr] = (DSPCompiledCode)entryPoint;

  // Mark this block as a linkable destination if it does not contain
  // any unresolved CALL's
  if (m_unresolved_jumps[start_addr].empty())
  {
    m_block_links[start_addr] = m_block_link_entry;

    for (size_t i = 0; i < 0xffff; ++i)
    {
      if (!m_unresolved_jumps[i].empty())
      {
        // Check if there were any blocks waiting for this block to be linkable
        size_t size = m_unresolved_jumps[i].size();
        m_unresolved_jumps[i].remove(start_addr);
        if (m_unresolved_jumps[i].size() < size)
        {
          // Mark the block to be recompiled again
          m_blocks[i] = (DSPCompiledCode)m_stub_entry_point;
          m_block_links[i] = nullptr;
          m_block_size[i] = 0;
        }
      }
    }
  }

  if (m_block_size[start_addr] == 0)
  {
    // just a safeguard, should never happen anymore.
    // if it does we might get stuck over in RunForCycles.
    ERROR_LOG(DSPLLE, "Block at 0x%04x has zero size", start_addr);
    m_block_size[start_addr] = 1;
  }

  m_gpr.SaveRegs();
  if (!Host::OnThread() && Analyzer::GetCodeFlags(start_addr) & Analyzer::CODE_IDLE_SKIP)
  {
    MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
  }
  else
  {
    MOV(16, R(EAX), Imm16(m_block_size[start_addr]));
  }
  JMP(m_return_dispatcher, true);
}

void DSPEmitter::CompileCurrent(DSPEmitter& emitter)
{
  emitter.Compile(g_dsp.pc);

  bool retry = true;

  while (retry)
  {
    retry = false;
    for (size_t i = 0; i < 0xffff; ++i)
    {
      if (!emitter.m_unresolved_jumps[i].empty())
      {
        const u16 address_to_compile = emitter.m_unresolved_jumps[i].front();
        emitter.Compile(address_to_compile);
        if (!emitter.m_unresolved_jumps[i].empty())
          retry = true;
      }
    }
  }
}

const u8* DSPEmitter::CompileStub()
{
  const u8* entryPoint = AlignCode16();
  MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(this)));
  ABI_CallFunction(CompileCurrent);
  XOR(32, R(EAX), R(EAX));  // Return 0 cycles executed
  JMP(m_return_dispatcher);
  return entryPoint;
}

void DSPEmitter::CompileDispatcher()
{
  m_enter_dispatcher = AlignCode16();
  // We don't use floating point (high 16 bits).
  BitSet32 registers_used = ABI_ALL_CALLEE_SAVED & BitSet32(0xffff);
  ABI_PushRegistersAndAdjustStack(registers_used, 8);

  MOV(64, R(R15), ImmPtr(&g_dsp));

  const u8* dispatcherLoop = GetCodePtr();

  FixupBranch exceptionExit;
  if (Host::OnThread())
  {
    CMP(8, M_SDSP_external_interrupt_waiting(), Imm8(0));
    exceptionExit = J_CC(CC_NE);
  }

  // Check for DSP halt
  TEST(8, M_SDSP_cr(), Imm8(CR_HALT));
  FixupBranch _halt = J_CC(CC_NE);

  // Execute block. Cycles executed returned in EAX.
  MOVZX(64, 16, ECX, M_SDSP_pc());
  MOV(64, R(RBX), ImmPtr(m_blocks.data()));
  JMPptr(MComplex(RBX, RCX, SCALE_8, 0));

  m_return_dispatcher = GetCodePtr();

  // Decrement cyclesLeft
  MOV(64, R(RCX), ImmPtr(&m_cycles_left));
  SUB(16, MatR(RCX), R(EAX));

  J_CC(CC_A, dispatcherLoop);

  // DSP gave up the remaining cycles.
  SetJumpTarget(_halt);
  if (Host::OnThread())
  {
    SetJumpTarget(exceptionExit);
  }
  // MOV(32, M(&cyclesLeft), Imm32(0));
  ABI_PopRegistersAndAdjustStack(registers_used, 8);
  RET();
}

Gen::OpArg DSPEmitter::M_SDSP_pc()
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, pc)));
}

Gen::OpArg DSPEmitter::M_SDSP_exceptions()
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, exceptions)));
}

Gen::OpArg DSPEmitter::M_SDSP_cr()
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, cr)));
}

Gen::OpArg DSPEmitter::M_SDSP_external_interrupt_waiting()
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, external_interrupt_waiting)));
}

Gen::OpArg DSPEmitter::M_SDSP_r_st(size_t index)
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, r.st[index])));
}

Gen::OpArg DSPEmitter::M_SDSP_reg_stack_ptr(size_t index)
{
  return MDisp(R15, static_cast<int>(offsetof(SDSP, reg_stack_ptr[index])));
}

}  // namespace DSP::JIT::x64
