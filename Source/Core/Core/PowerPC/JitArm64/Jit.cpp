// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <cstdio>

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "Common/PerformanceCounter.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/System.h"

using namespace Arm64Gen;

constexpr size_t CODE_SIZE = 1024 * 1024 * 32;
// We use a bigger farcode size for JitArm64 than Jit64, because JitArm64 always emits farcode
// for the slow path of each loadstore instruction. Jit64 postpones emitting farcode until the
// farcode actually is needed, saving it from having to emit farcode for most instructions.
// TODO: Perhaps implement something similar to Jit64. But using more RAM isn't much of a problem.
constexpr size_t FARCODE_SIZE = 1024 * 1024 * 64;
constexpr size_t FARCODE_SIZE_MMU = 1024 * 1024 * 64;

JitArm64::JitArm64(Core::System& system) : JitBase(system), m_float_emit(this)
{
}

JitArm64::~JitArm64() = default;

void JitArm64::Init()
{
  const size_t child_code_size = m_mmu_enabled ? FARCODE_SIZE_MMU : FARCODE_SIZE;
  AllocCodeSpace(CODE_SIZE + child_code_size);
  AddChildCodeSpace(&m_far_code, child_code_size);

  auto& memory = m_system.GetMemory();

  jo.fastmem_arena = m_fastmem_enabled && memory.InitFastmemArena();
  jo.optimizeGatherPipe = true;
  UpdateMemoryAndExceptionOptions();
  SetBlockLinkingEnabled(true);
  SetOptimizationEnabled(true);
  gpr.Init(this);
  fpr.Init(this);
  blocks.Init();

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;

  InitBLROptimization();

  GenerateAsm();

  ResetFreeMemoryRanges();
}

void JitArm64::SetBlockLinkingEnabled(bool enabled)
{
  jo.enableBlocklink = enabled && !SConfig::GetInstance().bJITNoBlockLinking;
}

void JitArm64::SetOptimizationEnabled(bool enabled)
{
  if (enabled)
  {
    analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
    analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
    analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
  }
  else
  {
    analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
    analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
    analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
  }
}

bool JitArm64::HandleFault(uintptr_t access_address, SContext* ctx)
{
  // Ifdef this since the exception handler runs on a separate thread on macOS (ARM)
#if !(defined(__APPLE__) && defined(_M_ARM_64))
  // We can't handle any fault from other threads.
  if (!Core::IsCPUThread())
  {
    ERROR_LOG_FMT(DYNA_REC, "Exception handler - Not on CPU thread");
    DoBacktrace(access_address, ctx);
    return false;
  }
#endif

  bool success = false;

  // Handle BLR stack faults, may happen in C++ code.
  const uintptr_t stack_guard = reinterpret_cast<uintptr_t>(m_stack_guard);
  if (access_address >= stack_guard && access_address < stack_guard + GUARD_SIZE)
    success = HandleStackFault();

  // If the fault is in JIT code space, look for fastmem areas.
  if (!success && IsInSpace(reinterpret_cast<u8*>(ctx->CTX_PC)))
  {
    auto& memory = m_system.GetMemory();
    if (memory.IsAddressInFastmemArea(reinterpret_cast<u8*>(access_address)))
    {
      const uintptr_t memory_base = reinterpret_cast<uintptr_t>(
          m_ppc_state.msr.DR ? memory.GetLogicalBase() : memory.GetPhysicalBase());

      if (access_address < memory_base || access_address >= memory_base + 0x1'0000'0000)
      {
        ERROR_LOG_FMT(DYNA_REC,
                      "JitArm64 address calculation overflowed. This should never happen! "
                      "PC {:#018x}, access address {:#018x}, memory base {:#018x}, MSR.DR {}",
                      ctx->CTX_PC, access_address, memory_base, m_ppc_state.msr.DR);
      }
      else
      {
        success = HandleFastmemFault(ctx);
      }
    }
  }

  if (!success)
  {
    ERROR_LOG_FMT(DYNA_REC, "Exception handler - Unhandled fault");
    DoBacktrace(access_address, ctx);
  }
  return success;
}

void JitArm64::ClearCache()
{
  m_fault_to_handler.clear();

  blocks.Clear();
  blocks.ClearRangesToFree();
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ClearCodeSpace();
  m_far_code.ClearCodeSpace();
  UpdateMemoryAndExceptionOptions();

  GenerateAsm();

  ResetFreeMemoryRanges();
}

void JitArm64::ResetFreeMemoryRanges()
{
  // Set the near and far code regions as unused.
  m_free_ranges_near.clear();
  m_free_ranges_near.insert(GetWritableCodePtr(), GetWritableCodeEnd());
  m_free_ranges_far.clear();
  m_free_ranges_far.insert(m_far_code.GetWritableCodePtr(), m_far_code.GetWritableCodeEnd());
}

void JitArm64::Shutdown()
{
  auto& memory = m_system.GetMemory();
  memory.ShutdownFastmemArena();
  FreeCodeSpace();
  blocks.Shutdown();
}

void JitArm64::FallBackToInterpreter(UGeckoInstruction inst)
{
  FlushCarry();
  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  if (js.op->opinfo->flags & FL_ENDBLOCK)
  {
    // also flush the program counter
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, js.compilerPC);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(pc));
    ADD(WA, WA, 4);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(npc));
    gpr.Unlock(WA);
  }

  Interpreter::Instruction instr = Interpreter::GetInterpreterOp(inst);
  MOVP2R(ARM64Reg::X8, instr);
  MOVP2R(ARM64Reg::X0, &m_system.GetInterpreter());
  MOVI2R(ARM64Reg::W1, inst.hex);
  BLR(ARM64Reg::X8);

  // If the instruction wrote to any registers which were marked as discarded,
  // we must mark them as no longer discarded
  gpr.ResetRegisters(js.op->regsOut);
  fpr.ResetRegisters(js.op->GetFregsOut());

  if (js.op->opinfo->flags & FL_ENDBLOCK)
  {
    if (js.isLastInstruction)
    {
      ARM64Reg WA = gpr.GetReg();
      LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(npc));
      WriteExceptionExit(WA);
      gpr.Unlock(WA);
    }
    else
    {
      // only exit if ppcstate.npc was changed
      ARM64Reg WA = gpr.GetReg();
      LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(npc));
      ARM64Reg WB = gpr.GetReg();
      MOVI2R(WB, js.compilerPC + 4);
      CMP(WB, WA);
      gpr.Unlock(WB);
      FixupBranch c = B(CC_EQ);
      WriteExceptionExit(WA);
      SetJumpTarget(c);
      gpr.Unlock(WA);
    }
  }
  else if (ShouldHandleFPExceptionForInstruction(js.op))
  {
    WriteConditionalExceptionExit(EXCEPTION_PROGRAM);
  }

  if (jo.memcheck && (js.op->opinfo->flags & FL_LOADSTORE))
  {
    WriteConditionalExceptionExit(EXCEPTION_DSI);
  }
}

void JitArm64::HLEFunction(u32 hook_index)
{
  FlushCarry();
  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  MOVP2R(ARM64Reg::X8, &HLE::ExecuteFromJIT);
  MOVI2R(ARM64Reg::W0, js.compilerPC);
  MOVI2R(ARM64Reg::W1, hook_index);
  BLR(ARM64Reg::X8);
}

void JitArm64::DoNothing(UGeckoInstruction inst)
{
  // Yup, just don't do anything.
}

void JitArm64::Break(UGeckoInstruction inst)
{
  WARN_LOG_FMT(DYNA_REC, "Breaking! {:08x} - Fix me ;)", inst.hex);
  std::exit(0);
}

void JitArm64::Cleanup()
{
  if (jo.optimizeGatherPipe && js.fifoBytesSinceCheck > 0)
  {
    static_assert(PPCSTATE_OFF(gather_pipe_ptr) <= 504);
    static_assert(PPCSTATE_OFF(gather_pipe_ptr) + 8 == PPCSTATE_OFF(gather_pipe_base_ptr));
    LDP(IndexType::Signed, ARM64Reg::X0, ARM64Reg::X1, PPC_REG, PPCSTATE_OFF(gather_pipe_ptr));
    SUB(ARM64Reg::X0, ARM64Reg::X0, ARM64Reg::X1);
    CMP(ARM64Reg::X0, GPFifo::GATHER_PIPE_SIZE);
    FixupBranch exit = B(CC_LT);
    MOVP2R(ARM64Reg::X1, &GPFifo::UpdateGatherPipe);
    MOVP2R(ARM64Reg::X0, &m_system.GetGPFifo());
    BLR(ARM64Reg::X1);
    SetJumpTarget(exit);
  }

  // SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
  if (MMCR0(m_ppc_state).Hex || MMCR1(m_ppc_state).Hex)
  {
    MOVP2R(ARM64Reg::X8, &PowerPC::UpdatePerformanceMonitor);
    MOVI2R(ARM64Reg::X0, js.downcountAmount);
    MOVI2R(ARM64Reg::X1, js.numLoadStoreInst);
    MOVI2R(ARM64Reg::X2, js.numFloatingPointInst);
    MOVP2R(ARM64Reg::X3, &m_ppc_state);
    BLR(ARM64Reg::X8);
  }
}

void JitArm64::DoDownCount()
{
  LDR(IndexType::Unsigned, ARM64Reg::W0, PPC_REG, PPCSTATE_OFF(downcount));
  SUBSI2R(ARM64Reg::W0, ARM64Reg::W0, js.downcountAmount, ARM64Reg::W1);
  STR(IndexType::Unsigned, ARM64Reg::W0, PPC_REG, PPCSTATE_OFF(downcount));
}

void JitArm64::ResetStack()
{
  if (!m_enable_blr_optimization)
    return;

  LDR(IndexType::Unsigned, ARM64Reg::X0, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));
  ADD(ARM64Reg::SP, ARM64Reg::X0, 0);
}

void JitArm64::IntializeSpeculativeConstants()
{
  // If the block depends on an input register which looks like a gather pipe or MMIO related
  // constant, guess that it is actually a constant input, and specialize the block based on this
  // assumption. This happens when there are branches in code writing to the gather pipe, but only
  // the first block loads the constant.
  // Insert a check at the start of the block to verify that the value is actually constant.
  // This can save a lot of backpatching and optimize gather pipe writes in more places.
  const u8* fail = nullptr;
  for (auto i : code_block.m_gpr_inputs)
  {
    u32 compile_time_value = m_ppc_state.gpr[i];
    if (m_mmu.IsOptimizableGatherPipeWrite(compile_time_value) ||
        m_mmu.IsOptimizableGatherPipeWrite(compile_time_value - 0x8000) ||
        compile_time_value == 0xCC000000)
    {
      if (!fail)
      {
        SwitchToFarCode();
        fail = GetCodePtr();
        MOVI2R(DISPATCHER_PC, js.blockStart);
        STR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
        MOVP2R(ARM64Reg::X8, &JitInterface::CompileExceptionCheckFromJIT);
        MOVP2R(ARM64Reg::X0, &m_system.GetJitInterface());
        MOVI2R(ARM64Reg::W1, static_cast<u32>(JitInterface::ExceptionType::SpeculativeConstants));
        BLR(ARM64Reg::X8);
        B(dispatcher_no_check);
        SwitchToNearCode();
      }

      ARM64Reg tmp = gpr.GetReg();
      ARM64Reg value = gpr.R(i);
      MOVI2R(tmp, compile_time_value);
      CMP(value, tmp);
      gpr.Unlock(tmp);

      FixupBranch no_fail = B(CCFlags::CC_EQ);
      B(fail);
      SetJumpTarget(no_fail);

      gpr.SetImmediate(i, compile_time_value, true);
    }
  }
}

void JitArm64::WriteExit(u32 destination, bool LK, u32 exit_address_after_return)
{
  Cleanup();
  EndTimeProfile(js.curBlock);
  DoDownCount();

  LK &= m_enable_blr_optimization;

  if (LK)
  {
    // Push {ARM_PC+20; PPC_PC} on the stack
    MOVI2R(ARM64Reg::X1, exit_address_after_return);
    ADR(ARM64Reg::X0, 20);
    STP(IndexType::Pre, ARM64Reg::X0, ARM64Reg::X1, ARM64Reg::SP, -16);
  }

  JitBlock* b = js.curBlock;
  JitBlock::LinkData linkData;
  linkData.exitAddress = destination;
  linkData.exitPtrs = GetWritableCodePtr();
  linkData.linkStatus = false;
  linkData.call = LK;
  b->linkData.push_back(linkData);

  blocks.WriteLinkBlock(*this, linkData);

  if (LK)
  {
    // Write the regular exit node after the return.
    linkData.exitAddress = exit_address_after_return;
    linkData.exitPtrs = GetWritableCodePtr();
    linkData.linkStatus = false;
    linkData.call = false;
    b->linkData.push_back(linkData);

    blocks.WriteLinkBlock(*this, linkData);
  }
}

void JitArm64::WriteExit(Arm64Gen::ARM64Reg dest, bool LK, u32 exit_address_after_return)
{
  if (dest != DISPATCHER_PC)
    MOV(DISPATCHER_PC, dest);

  Cleanup();
  EndTimeProfile(js.curBlock);
  DoDownCount();

  LK &= m_enable_blr_optimization;

  if (!LK)
  {
    B(dispatcher);
  }
  else
  {
    // Push {ARM_PC, PPC_PC} on the stack
    MOVI2R(ARM64Reg::X1, exit_address_after_return);
    ADR(ARM64Reg::X0, 12);
    STP(IndexType::Pre, ARM64Reg::X0, ARM64Reg::X1, ARM64Reg::SP, -16);

    BL(dispatcher);

    // Write the regular exit node after the return.
    JitBlock* b = js.curBlock;
    JitBlock::LinkData linkData;
    linkData.exitAddress = exit_address_after_return;
    linkData.exitPtrs = GetWritableCodePtr();
    linkData.linkStatus = false;
    linkData.call = false;
    b->linkData.push_back(linkData);

    blocks.WriteLinkBlock(*this, linkData);
  }
}

void JitArm64::FakeLKExit(u32 exit_address_after_return)
{
  if (!m_enable_blr_optimization)
    return;

  // We may need to fake the BLR stack on inlined CALL instructions.
  // Else we can't return to this location any more.
  gpr.Lock(ARM64Reg::W30);
  ARM64Reg after_reg = gpr.GetReg();
  ARM64Reg code_reg = gpr.GetReg();
  MOVI2R(after_reg, exit_address_after_return);
  ADR(EncodeRegTo64(code_reg), 12);
  STP(IndexType::Pre, EncodeRegTo64(code_reg), EncodeRegTo64(after_reg), ARM64Reg::SP, -16);
  gpr.Unlock(after_reg, code_reg);

  FixupBranch skip_exit = BL();
  gpr.Unlock(ARM64Reg::W30);

  // Write the regular exit node after the return.
  JitBlock* b = js.curBlock;
  JitBlock::LinkData linkData;
  linkData.exitAddress = exit_address_after_return;
  linkData.exitPtrs = GetWritableCodePtr();
  linkData.linkStatus = false;
  linkData.call = false;
  b->linkData.push_back(linkData);

  blocks.WriteLinkBlock(*this, linkData);

  SetJumpTarget(skip_exit);
}

void JitArm64::WriteBLRExit(Arm64Gen::ARM64Reg dest)
{
  if (!m_enable_blr_optimization)
  {
    WriteExit(dest);
    return;
  }

  if (dest != DISPATCHER_PC)
    MOV(DISPATCHER_PC, dest);

  Cleanup();
  EndTimeProfile(js.curBlock);

  // Check if {ARM_PC, PPC_PC} matches the current state.
  LDP(IndexType::Post, ARM64Reg::X2, ARM64Reg::X1, ARM64Reg::SP, 16);
  CMP(ARM64Reg::W1, DISPATCHER_PC);
  FixupBranch no_match = B(CC_NEQ);

  DoDownCount();  // overwrites X0 + X1

  RET(ARM64Reg::X2);

  SetJumpTarget(no_match);

  ResetStack();

  DoDownCount();

  B(dispatcher);
}

void JitArm64::WriteExceptionExit(u32 destination, bool only_external, bool always_exception)
{
  MOVI2R(DISPATCHER_PC, destination);
  WriteExceptionExit(DISPATCHER_PC, only_external, always_exception);
}

void JitArm64::WriteExceptionExit(ARM64Reg dest, bool only_external, bool always_exception)
{
  if (dest != DISPATCHER_PC)
    MOV(DISPATCHER_PC, dest);

  Cleanup();

  FixupBranch no_exceptions;
  if (!always_exception)
  {
    LDR(IndexType::Unsigned, ARM64Reg::W30, PPC_REG, PPCSTATE_OFF(Exceptions));
    no_exceptions = CBZ(ARM64Reg::W30);
  }

  static_assert(PPCSTATE_OFF(pc) <= 252);
  static_assert(PPCSTATE_OFF(pc) + 4 == PPCSTATE_OFF(npc));
  STP(IndexType::Signed, DISPATCHER_PC, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  MOVP2R(ARM64Reg::X0, &m_system.GetPowerPC());
  if (only_external)
    MOVP2R(EncodeRegTo64(DISPATCHER_PC), &PowerPC::CheckExternalExceptionsFromJIT);
  else
    MOVP2R(EncodeRegTo64(DISPATCHER_PC), &PowerPC::CheckExceptionsFromJIT);
  BLR(EncodeRegTo64(DISPATCHER_PC));

  LDR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));

  if (!always_exception)
    SetJumpTarget(no_exceptions);

  EndTimeProfile(js.curBlock);
  DoDownCount();

  B(dispatcher);
}

void JitArm64::WriteConditionalExceptionExit(int exception, u64 increment_sp_on_exit)
{
  ARM64Reg WA = gpr.GetReg();
  WriteConditionalExceptionExit(exception, WA, Arm64Gen::ARM64Reg::INVALID_REG,
                                increment_sp_on_exit);
  gpr.Unlock(WA);
}

void JitArm64::WriteConditionalExceptionExit(int exception, ARM64Reg temp_gpr, ARM64Reg temp_fpr,
                                             u64 increment_sp_on_exit)
{
  LDR(IndexType::Unsigned, temp_gpr, PPC_REG, PPCSTATE_OFF(Exceptions));
  FixupBranch no_exception = TBZ(temp_gpr, MathUtil::IntLog2(exception));

  const bool switch_to_far_code = !IsInFarCode();

  if (switch_to_far_code)
  {
    FixupBranch handle_exception = B();
    SwitchToFarCode();
    SetJumpTarget(handle_exception);
  }

  if (increment_sp_on_exit != 0)
    ADDI2R(ARM64Reg::SP, ARM64Reg::SP, increment_sp_on_exit, temp_gpr);

  gpr.Flush(FlushMode::MaintainState, temp_gpr);
  fpr.Flush(FlushMode::MaintainState, temp_fpr);

  WriteExceptionExit(js.compilerPC, false, true);

  if (switch_to_far_code)
    SwitchToNearCode();

  SetJumpTarget(no_exception);
}

bool JitArm64::HandleFunctionHooking(u32 address)
{
  return HLE::ReplaceFunctionIfPossible(address, [&](u32 hook_index, HLE::HookType type) {
    HLEFunction(hook_index);

    if (type != HLE::HookType::Replace)
      return false;

    LDR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
    js.downcountAmount += js.st.numCycles;
    WriteExit(DISPATCHER_PC);
    return true;
  });
}

void JitArm64::DumpCode(const u8* start, const u8* end)
{
  std::string output;
  for (const u8* code = start; code < end; code += sizeof(u32))
    output += fmt::format("{:08x}", Common::swap32(code));
  WARN_LOG_FMT(DYNA_REC, "Code dump from {} to {}:\n{}", fmt::ptr(start), fmt::ptr(end), output);
}

void JitArm64::BeginTimeProfile(JitBlock* b)
{
  MOVP2R(ARM64Reg::X0, &b->profile_data);
  LDR(IndexType::Unsigned, ARM64Reg::X1, ARM64Reg::X0, offsetof(JitBlock::ProfileData, runCount));
  ADD(ARM64Reg::X1, ARM64Reg::X1, 1);

  // Fetch the current counter register
  CNTVCT(ARM64Reg::X2);

  // stores runCount and ticStart
  STP(IndexType::Signed, ARM64Reg::X1, ARM64Reg::X2, ARM64Reg::X0,
      offsetof(JitBlock::ProfileData, runCount));
}

void JitArm64::EndTimeProfile(JitBlock* b)
{
  if (!jo.profile_blocks)
    return;

  // Fetch the current counter register
  CNTVCT(ARM64Reg::X1);

  MOVP2R(ARM64Reg::X0, &b->profile_data);

  LDR(IndexType::Unsigned, ARM64Reg::X2, ARM64Reg::X0, offsetof(JitBlock::ProfileData, ticStart));
  SUB(ARM64Reg::X1, ARM64Reg::X1, ARM64Reg::X2);

  // loads ticCounter and downcountCounter
  LDP(IndexType::Signed, ARM64Reg::X2, ARM64Reg::X3, ARM64Reg::X0,
      offsetof(JitBlock::ProfileData, ticCounter));
  ADD(ARM64Reg::X2, ARM64Reg::X2, ARM64Reg::X1);
  ADDI2R(ARM64Reg::X3, ARM64Reg::X3, js.downcountAmount, ARM64Reg::X1);

  // stores ticCounter and downcountCounter
  STP(IndexType::Signed, ARM64Reg::X2, ARM64Reg::X3, ARM64Reg::X0,
      offsetof(JitBlock::ProfileData, ticCounter));
}

void JitArm64::Run()
{
  ProtectStack();

  CompiledCode pExecAddr = (CompiledCode)enter_code;
  pExecAddr();

  UnprotectStack();
}

void JitArm64::SingleStep()
{
  ProtectStack();

  CompiledCode pExecAddr = (CompiledCode)enter_code;
  pExecAddr();

  UnprotectStack();
}

void JitArm64::Trace()
{
  std::string regs;
  std::string fregs;

#ifdef JIT_LOG_GPR
  for (size_t i = 0; i < std::size(m_ppc_state.gpr); i++)
  {
    regs += fmt::format("r{:02d}: {:08x} ", i, m_ppc_state.gpr[i]);
  }
#endif

#ifdef JIT_LOG_FPR
  for (size_t i = 0; i < std::size(m_ppc_state.ps); i++)
  {
    fregs += fmt::format("f{:02d}: {:016x} ", i, m_ppc_state.ps[i].PS0AsU64());
  }
#endif

  DEBUG_LOG_FMT(DYNA_REC,
                "JitArm64 PC: {:08x} SRR0: {:08x} SRR1: {:08x} FPSCR: {:08x} "
                "MSR: {:08x} LR: {:08x} {} {}",
                m_ppc_state.pc, SRR0(m_ppc_state), SRR1(m_ppc_state), m_ppc_state.fpscr.Hex,
                m_ppc_state.msr.Hex, m_ppc_state.spr[8], regs, fregs);
}

void JitArm64::Jit(u32 em_address)
{
  Jit(em_address, true);
}

void JitArm64::Jit(u32 em_address, bool clear_cache_and_retry_on_failure)
{
  CleanUpAfterStackFault();

  if (SConfig::GetInstance().bJITNoBlockCache)
    ClearCache();

  // Check if any code blocks have been freed in the block cache and transfer this information to
  // the local rangesets to allow overwriting them with new code.
  for (auto range : blocks.GetRangesToFreeNear())
  {
    auto first_fastmem_area = m_fault_to_handler.upper_bound(range.first);
    auto last_fastmem_area = first_fastmem_area;
    auto end = m_fault_to_handler.end();
    while (last_fastmem_area != end && last_fastmem_area->first <= range.second)
      ++last_fastmem_area;
    m_fault_to_handler.erase(first_fastmem_area, last_fastmem_area);

    m_free_ranges_near.insert(range.first, range.second);
  }
  for (auto range : blocks.GetRangesToFreeFar())
  {
    m_free_ranges_far.insert(range.first, range.second);
  }
  blocks.ClearRangesToFree();

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

  std::size_t block_size = m_code_buffer.size();

  auto& cpu = m_system.GetCPU();

  if (m_enable_debugging)
  {
    // We can link blocks as long as we are not single stepping
    SetBlockLinkingEnabled(true);
    SetOptimizationEnabled(true);

    if (!jo.profile_blocks)
    {
      if (cpu.IsStepping())
      {
        block_size = 1;

        // Do not link this block to other blocks while single stepping
        SetBlockLinkingEnabled(false);
        SetOptimizationEnabled(false);
      }
      Trace();
    }
  }

  // Analyze the block, collect all instructions it is made of (including inlining,
  // if that is enabled), reorder instructions for optimal performance, and join joinable
  // instructions.
  const u32 nextPC = analyzer.Analyze(em_address, &code_block, &m_code_buffer, block_size);

  if (code_block.m_memory_exception)
  {
    // Address of instruction could not be translated
    m_ppc_state.npc = nextPC;
    m_ppc_state.Exceptions |= EXCEPTION_ISI;
    m_system.GetPowerPC().CheckExceptions();
    WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", nextPC);
    return;
  }

  if (SetEmitterStateToFreeCodeRegion())
  {
    u8* near_start = GetWritableCodePtr();
    u8* far_start = m_far_code.GetWritableCodePtr();

    JitBlock* b = blocks.AllocateBlock(em_address);
    if (DoJit(em_address, b, nextPC))
    {
      // Code generation succeeded.

      // Mark the memory regions that this code block uses as used in the local rangesets.
      u8* near_end = GetWritableCodePtr();
      if (near_start != near_end)
        m_free_ranges_near.erase(near_start, near_end);
      u8* far_end = m_far_code.GetWritableCodePtr();
      if (far_start != far_end)
        m_free_ranges_far.erase(far_start, far_end);

      // Store the used memory regions in the block so we know what to mark as unused when the
      // block gets invalidated.
      b->near_begin = near_start;
      b->near_end = near_end;
      b->far_begin = far_start;
      b->far_end = far_end;

      blocks.FinalizeBlock(*b, jo.enableBlocklink, code_block.m_physical_addresses);
      return;
    }
  }

  if (clear_cache_and_retry_on_failure)
  {
    // Code generation failed due to not enough free space in either the near or far code regions.
    // Clear the entire JIT cache and retry.
    WARN_LOG_FMT(POWERPC, "flushing code caches, please report if this happens a lot");
    ClearCache();
    Jit(em_address, false);
    return;
  }

  PanicAlertFmtT(
      "JIT failed to find code space after a cache clear. This should never happen. Please "
      "report this incident on the bug tracker. Dolphin will now exit.");
  exit(-1);
}

bool JitArm64::SetEmitterStateToFreeCodeRegion()
{
  // Find the largest free memory blocks and set code emitters to point at them.
  // If we can't find a free block return false instead, which will trigger a JIT cache clear.
  auto free_near = m_free_ranges_near.by_size_begin();
  if (free_near == m_free_ranges_near.by_size_end())
  {
    WARN_LOG_FMT(POWERPC, "Failed to find free memory region in near code region.");
    return false;
  }
  SetCodePtr(free_near.from(), free_near.to());

  auto free_far = m_free_ranges_far.by_size_begin();
  if (free_far == m_free_ranges_far.by_size_end())
  {
    WARN_LOG_FMT(POWERPC, "Failed to find free memory region in far code region.");
    return false;
  }
  m_far_code.SetCodePtr(free_far.from(), free_far.to());

  return true;
}

bool JitArm64::DoJit(u32 em_address, JitBlock* b, u32 nextPC)
{
  auto& cpu = m_system.GetCPU();

  js.isLastInstruction = false;
  js.firstFPInstructionFound = false;
  js.assumeNoPairedQuantize = false;
  js.blockStart = em_address;
  js.fifoBytesSinceCheck = 0;
  js.mustCheckFifo = false;
  js.downcountAmount = 0;
  js.skipInstructions = 0;
  js.curBlock = b;
  js.carryFlag = CarryFlag::InPPCState;
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;

  u8* const start = GetWritableCodePtr();
  b->checkedEntry = start;

  // Downcount flag check, Only valid for linked blocks
  {
    FixupBranch bail = B(CC_PL);
    MOVI2R(DISPATCHER_PC, js.blockStart);
    B(do_timing);
    SetJumpTarget(bail);
  }

  // Normal entry doesn't need to check for downcount.
  b->normalEntry = GetWritableCodePtr();

  // Conditionally add profiling code.
  if (jo.profile_blocks)
  {
    // get start tic
    BeginTimeProfile(b);
  }

  if (code_block.m_gqr_used.Count() == 1 &&
      js.pairedQuantizeAddresses.find(js.blockStart) == js.pairedQuantizeAddresses.end())
  {
    int gqr = *code_block.m_gqr_used.begin();
    if (!code_block.m_gqr_modified[gqr] && !GQR(m_ppc_state, gqr))
    {
      LDR(IndexType::Unsigned, ARM64Reg::W0, PPC_REG, PPCSTATE_OFF_SPR(SPR_GQR0 + gqr));
      FixupBranch no_fail = CBZ(ARM64Reg::W0);
      FixupBranch fail = B();
      SwitchToFarCode();
      SetJumpTarget(fail);
      MOVI2R(DISPATCHER_PC, js.blockStart);
      STR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
      MOVP2R(ARM64Reg::X0, &m_system.GetJitInterface());
      MOVI2R(ARM64Reg::W1, static_cast<u32>(JitInterface::ExceptionType::PairedQuantize));
      MOVP2R(ARM64Reg::X2, &JitInterface::CompileExceptionCheckFromJIT);
      BLR(ARM64Reg::X2);
      B(dispatcher_no_check);
      SwitchToNearCode();
      SetJumpTarget(no_fail);
      js.assumeNoPairedQuantize = true;
    }
  }

  gpr.Start(js.gpa);
  fpr.Start(js.fpa);

  if (js.noSpeculativeConstantsAddresses.find(js.blockStart) ==
      js.noSpeculativeConstantsAddresses.end())
  {
    IntializeSpeculativeConstants();
  }

  // Translate instructions
  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    PPCAnalyst::CodeOp& op = m_code_buffer[i];

    js.compilerPC = op.address;
    js.op = &op;
    js.fpr_is_store_safe = op.fprIsStoreSafeBeforeInst;
    js.instructionNumber = i;
    js.instructionsLeft = (code_block.m_num_instructions - 1) - i;
    const GekkoOPInfo* opinfo = op.opinfo;
    js.downcountAmount += opinfo->num_cycles;
    js.isLastInstruction = i == (code_block.m_num_instructions - 1);

    if (!m_enable_debugging)
      js.downcountAmount += PatchEngine::GetSpeedhackCycles(js.compilerPC);

    // Skip calling UpdateLastUsed for lmw/stmw - it usually hurts more than it helps
    if (op.inst.OPCD != 46 && op.inst.OPCD != 47)
      gpr.UpdateLastUsed(op.regsIn | op.regsOut);

    BitSet32 fpr_used = op.fregsIn;
    if (op.fregOut >= 0)
      fpr_used[op.fregOut] = true;
    fpr.UpdateLastUsed(fpr_used);

    // Gather pipe writes using a non-immediate address are discovered by profiling.
    bool gatherPipeIntCheck = js.fifoWriteAddresses.find(op.address) != js.fifoWriteAddresses.end();

    if (jo.optimizeGatherPipe &&
        (js.fifoBytesSinceCheck >= GPFifo::GATHER_PIPE_SIZE || js.mustCheckFifo))
    {
      js.fifoBytesSinceCheck = 0;
      js.mustCheckFifo = false;

      gpr.Lock(ARM64Reg::W30);
      BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
      BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
      regs_in_use[DecodeReg(ARM64Reg::W30)] = 0;

      ABI_PushRegisters(regs_in_use);
      m_float_emit.ABI_PushRegisters(fprs_in_use, ARM64Reg::X30);
      MOVP2R(ARM64Reg::X8, &GPFifo::FastCheckGatherPipe);
      MOVP2R(ARM64Reg::X0, &m_system.GetGPFifo());
      BLR(ARM64Reg::X8);
      m_float_emit.ABI_PopRegisters(fprs_in_use, ARM64Reg::X30);
      ABI_PopRegisters(regs_in_use);

      // Inline exception check
      LDR(IndexType::Unsigned, ARM64Reg::W30, PPC_REG, PPCSTATE_OFF(Exceptions));
      FixupBranch no_ext_exception = TBZ(ARM64Reg::W30, MathUtil::IntLog2(EXCEPTION_EXTERNAL_INT));
      FixupBranch exception = B();
      SwitchToFarCode();
      const u8* done_here = GetCodePtr();
      FixupBranch exit = B();
      SetJumpTarget(exception);
      LDR(IndexType::Unsigned, ARM64Reg::W30, PPC_REG, PPCSTATE_OFF(msr));
      TBZ(ARM64Reg::W30, 15, done_here);  // MSR.EE
      LDR(IndexType::Unsigned, ARM64Reg::W30, ARM64Reg::X30,
          MOVPage2R(ARM64Reg::X30, &m_system.GetProcessorInterface().m_interrupt_cause));
      constexpr u32 cause_mask = ProcessorInterface::INT_CAUSE_CP |
                                 ProcessorInterface::INT_CAUSE_PE_TOKEN |
                                 ProcessorInterface::INT_CAUSE_PE_FINISH;
      TST(ARM64Reg::W30, LogicalImm(cause_mask, 32));
      B(CC_EQ, done_here);

      gpr.Flush(FlushMode::MaintainState, ARM64Reg::W30);
      fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);
      WriteExceptionExit(js.compilerPC, true, true);
      SwitchToNearCode();
      SetJumpTarget(no_ext_exception);
      SetJumpTarget(exit);
      gpr.Unlock(ARM64Reg::W30);

      // So we don't check exceptions twice
      gatherPipeIntCheck = false;
    }
    // Gather pipe writes can generate an exception; add an exception check.
    // TODO: This doesn't really match hardware; the CP interrupt is
    // asynchronous.
    if (jo.optimizeGatherPipe && gatherPipeIntCheck)
    {
      ARM64Reg WA = gpr.GetReg();
      ARM64Reg XA = EncodeRegTo64(WA);

      LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
      FixupBranch no_ext_exception = TBZ(WA, MathUtil::IntLog2(EXCEPTION_EXTERNAL_INT));
      FixupBranch exception = B();
      SwitchToFarCode();
      const u8* done_here = GetCodePtr();
      FixupBranch exit = B();
      SetJumpTarget(exception);
      LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(msr));
      TBZ(WA, 15, done_here);  // MSR.EE
      LDR(IndexType::Unsigned, WA, XA,
          MOVPage2R(XA, &m_system.GetProcessorInterface().m_interrupt_cause));
      constexpr u32 cause_mask = ProcessorInterface::INT_CAUSE_CP |
                                 ProcessorInterface::INT_CAUSE_PE_TOKEN |
                                 ProcessorInterface::INT_CAUSE_PE_FINISH;
      TST(WA, LogicalImm(cause_mask, 32));
      B(CC_EQ, done_here);

      gpr.Flush(FlushMode::MaintainState, WA);
      fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);
      WriteExceptionExit(js.compilerPC, true, true);
      SwitchToNearCode();
      SetJumpTarget(no_ext_exception);
      SetJumpTarget(exit);

      gpr.Unlock(WA);
    }

    if (HandleFunctionHooking(op.address))
      break;

    if (!op.skip)
    {
      if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
      {
        // This instruction uses FPU - needs to add FP exception bailout
        ARM64Reg WA = gpr.GetReg();
        LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(msr));
        FixupBranch b1 = TBNZ(WA, 13);  // Test FP enabled bit

        FixupBranch far_addr = B();
        SwitchToFarCode();
        SetJumpTarget(far_addr);

        gpr.Flush(FlushMode::MaintainState, WA);
        fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);

        LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
        ORR(WA, WA, LogicalImm(EXCEPTION_FPU_UNAVAILABLE, 32));
        STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));

        gpr.Unlock(WA);

        WriteExceptionExit(js.compilerPC, false, true);

        SwitchToNearCode();

        SetJumpTarget(b1);

        js.firstFPInstructionFound = true;
      }

      if (m_enable_debugging && !cpu.IsStepping() &&
          m_system.GetPowerPC().GetBreakPoints().IsAddressBreakPoint(op.address))
      {
        FlushCarry();
        gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
        fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

        static_assert(PPCSTATE_OFF(pc) <= 252);
        static_assert(PPCSTATE_OFF(pc) + 4 == PPCSTATE_OFF(npc));

        MOVI2R(DISPATCHER_PC, op.address);
        STP(IndexType::Signed, DISPATCHER_PC, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
        MOVP2R(ARM64Reg::X0, &m_system.GetPowerPC());
        MOVP2R(ARM64Reg::X1, &PowerPC::CheckBreakPointsFromJIT);
        BLR(ARM64Reg::X1);

        LDR(IndexType::Unsigned, ARM64Reg::W0, ARM64Reg::X0,
            MOVPage2R(ARM64Reg::X0, cpu.GetStatePtr()));
        FixupBranch no_breakpoint = CBZ(ARM64Reg::W0);

        Cleanup();
        EndTimeProfile(js.curBlock);
        DoDownCount();
        B(dispatcher_exit);

        SetJumpTarget(no_breakpoint);
      }

      if (bJITRegisterCacheOff)
      {
        FlushCarry();
        gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
        fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
      }

      CompileInstruction(op);

      js.fpr_is_store_safe = op.fprIsStoreSafeAfterInst;

      if (!CanMergeNextInstructions(1) || js.op[1].opinfo->type != ::OpType::Integer)
        FlushCarry();

      // If we have a register that will never be used again, discard or flush it.
      if (!bJITRegisterCacheOff)
      {
        gpr.DiscardRegisters(op.gprDiscardable);
        fpr.DiscardRegisters(op.fprDiscardable);
      }
      gpr.StoreRegisters(~op.gprInUse & (op.regsIn | op.regsOut));
      fpr.StoreRegisters(~op.fprInUse & (op.fregsIn | op.GetFregsOut()));

      if (opinfo->flags & FL_LOADSTORE)
        ++js.numLoadStoreInst;

      if (opinfo->flags & FL_USE_FPU)
        ++js.numFloatingPointInst;
    }

    i += js.skipInstructions;
    js.skipInstructions = 0;
  }

  if (code_block.m_broken)
  {
    gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    WriteExit(nextPC);
  }

  if (HasWriteFailed() || m_far_code.HasWriteFailed())
  {
    if (HasWriteFailed())
      WARN_LOG_FMT(POWERPC, "JIT ran out of space in near code region during code generation.");
    if (m_far_code.HasWriteFailed())
      WARN_LOG_FMT(POWERPC, "JIT ran out of space in far code region during code generation.");

    return false;
  }

  b->codeSize = (u32)(GetCodePtr() - start);
  b->originalSize = code_block.m_num_instructions;

  FlushIcache();
  m_far_code.FlushIcache();

  return true;
}
