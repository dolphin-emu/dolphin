// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/Jit.h"

#include <cstring>

#include <algorithm>

#include <cstdlib>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/HostDisassembler.h"
#include "Common/JitRegister.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/MemTools.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace PPC64Gen;

JitPPC64::JitPPC64(Core::System& system)
    : JitBase(system),
      m_disassembler(HostDisassembler::Factory(HostDisassembler::Platform::ppc64le))
{
}

JitPPC64::~JitPPC64() = default;

bool JitPPC64::Init()
{
  if (!CheckValidity())
    return false;

  InitFastmemArena();
  // For the slow accesses, which translate addresses themselves in MMU mode.
  m_system.GetMemory().EnablePageTranslations();

  RefreshConfig();

  m_isa_3_0 = cpu_info.bISA_3_0;
  m_isa_3_1 = cpu_info.bISA_3_1;
  if (const char* isa = std::getenv("DOLPHIN_PPC64_ISA"))
  {
    const std::string_view level = isa;
    if (level == "2.07")
      m_isa_3_0 = false;
    if (level == "2.07" || level == "3.0")
      m_isa_3_1 = false;
  }
  SetPrefixedInstructionsEnabled(m_isa_3_1);
  if (const char* limit = std::getenv("DOLPHIN_PPC64_CODE_CHUNK_LIMIT"))
    m_code_chunk_limit = std::strtoull(limit, nullptr, 0);
  NOTICE_LOG_FMT(DYNA_REC, "JitPPC64: Power ISA {}",
                 m_isa_3_1 ? "3.1" :
                 m_isa_3_0 ? "3.0" :
                             "2.07");

  AllocCodeSpace(CODE_CHUNK_SIZE * CODE_CHUNKS);

  jo.enableBlocklink = !SConfig::GetInstance().bJITNoBlockLinking;
  // Stores to the gather pipe fault, and take the slow path, which writes it through the MMU.
  jo.optimizeGatherPipe = false;
  InitBLROptimization();
  // For tests, which run in debug mode, where the guest's reports are logged.
  if (std::getenv("DOLPHIN_PPC64_BLR_OPTIMIZATION"))
    m_enable_blr_optimization = jo.enableBlocklink && EMM::IsExceptionHandlerSupported();
  SetOptimizationEnabled(true);
  gpr.Init(this);
  fpr.Init(this);

  if (!blocks.Init())
    return false;

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;

  GenerateAsmAndResetFreeMemoryRanges();

  return true;
}

void JitPPC64::SetOptimizationEnabled(bool enabled)
{
  if (enabled)
  {
    analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
    analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
  }
  else
  {
    analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
    analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
  }
}

void JitPPC64::Shutdown()
{
  m_system.GetMemory().ShutdownFastmemArena();
  FreeCodeSpace();
  blocks.Shutdown();
}

bool JitPPC64::HandleFault(uintptr_t access_address, SContext* ctx)
{
  // We can't handle any fault from other threads.
  if (!Core::IsCPUThread())
  {
    ERROR_LOG_FMT(DYNA_REC, "Exception handler - Not on CPU thread");
    return false;
  }

  // Handle stack faults, which may happen in C++ code. HandleStackFault lowers the downcount, which
  // JIT code keeps in DOWNCOUNT_REG.
  const uintptr_t stack_guard = reinterpret_cast<uintptr_t>(m_stack_guard);
  if (access_address >= stack_guard && access_address < stack_guard + GUARD_SIZE)
  {
    const bool in_jit_code = IsInSpace(reinterpret_cast<u8*>(ctx->CTX_PC));
    auto& downcount = ctx->CTX_REG(DecodeReg(DOWNCOUNT_REG));
    if (in_jit_code)
      m_ppc_state.downcount = static_cast<s32>(downcount);
    const bool handled = HandleStackFault();
    if (in_jit_code)
      downcount = m_ppc_state.downcount;
    return handled;
  }

  // Handle fastmem accesses to addresses that aren't mapped in the arena.
  if (IsInSpace(reinterpret_cast<u8*>(ctx->CTX_PC)) &&
      m_system.GetMemory().IsAddressInFastmemArea(reinterpret_cast<u8*>(access_address)) &&
      HandleFastmemFault(ctx))
  {
    return true;
  }

  ERROR_LOG_FMT(DYNA_REC, "Exception handler - Unhandled fault at {:#018x}, PC {:#018x}",
                access_address, ctx->CTX_PC);
  return false;
}

void JitPPC64::ClearCache()
{
  m_fault_to_handler.clear();
  blocks.Clear();
  blocks.ClearRangesToFree();
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    ClearCodeSpace();
  }
  RefreshConfig();

  GenerateAsmAndResetFreeMemoryRanges();
}

void JitPPC64::GenerateAsmAndResetFreeMemoryRanges()
{
  ResetCodePtr();
  const u8* routines_start = GetCodePtr();
  m_generating_routines = true;
  GenerateAsm();
  m_generating_routines = false;
  const size_t routines_size = GetCodePtr() - routines_start;
  ASSERT(routines_start == region);

  // The constant pool follows the routines in the first chunk.
  m_constant_pool = reinterpret_cast<u64*>(region + Common::AlignUp(routines_size, 64));
  m_constant_pool_count = 0;
  m_constant_pool_entries.clear();

  // The routines only branch to themselves, relatively, so a copy works anywhere.
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    for (size_t i = 1; i < CODE_CHUNKS; i++)
    {
      u8* const copy = region + CODE_CHUNK_SIZE * i;
      std::memcpy(copy, region, routines_size);
      FlushIcacheSection(copy, copy + routines_size);
      Common::JitRegister::Register(copy, copy + routines_size, "JIT_Routines_{}", i);
    }
  }
  ResetFreeMemoryRanges(routines_size);

  Host_JitCacheInvalidation();
}

bool JitPPC64::LoadConstantFromPool(GPR rd, u64 imm)
{
  if (m_generating_routines || m_constant_pool == nullptr)
    return false;

  auto entry = m_constant_pool_entries.find(imm);
  if (entry == m_constant_pool_entries.end())
  {
    if (m_constant_pool_count == CONSTANT_POOL_SIZE / sizeof(u64))
      return false;
    u64* const slot = m_constant_pool + m_constant_pool_count++;
    std::memcpy(slot, &imm, sizeof(imm));
    entry = m_constant_pool_entries.emplace(imm, slot).first;
  }
  if (!IsInPrefixedPCRange(entry->second))
    return false;
  PLD(rd, entry->second);
  return true;
}

size_t JitPPC64::CodeChunk(const u8* code) const
{
  return static_cast<size_t>(code - region) / CODE_CHUNK_SIZE;
}

const u8* JitPPC64::Routine(const u8* routine, const u8* code) const
{
  return routine + CODE_CHUNK_SIZE * CodeChunk(code);
}

void JitPPC64::FreeRanges()
{
  // Check if any code blocks have been freed in the block cache and transfer this information to
  // the local rangeset to allow overwriting them with new code.
  for (const auto& [from, to] : blocks.GetRangesToFree())
  {
    const size_t chunk = CodeChunk(from);
    if (from < region + CODE_CHUNK_SIZE * chunk + NEAR_CODE_SIZE)
    {
      m_fault_to_handler.erase(m_fault_to_handler.lower_bound(from),
                               m_fault_to_handler.lower_bound(to));
      m_free_ranges_near[chunk].insert(from, to);
    }
    else
    {
      m_free_ranges_far[chunk].insert(from, to);
    }
  }
  blocks.ClearRangesToFree();
}

void JitPPC64::ResetFreeMemoryRanges(size_t routines_size)
{
  for (size_t i = 0; i < CODE_CHUNKS; i++)
  {
    u8* const chunk = region + CODE_CHUNK_SIZE * i;
    // The first chunk has the constant pool after its routines.
    const size_t reserved =
        i == 0 ? Common::AlignUp(routines_size, 64) + CONSTANT_POOL_SIZE : routines_size;
    size_t near_end = NEAR_CODE_SIZE;
    size_t far_end = CODE_CHUNK_SIZE;
    if (m_code_chunk_limit != 0)
    {
      near_end = std::min(near_end, reserved + m_code_chunk_limit);
      far_end = std::min(far_end, NEAR_CODE_SIZE + m_code_chunk_limit);
    }
    m_free_ranges_near[i].clear();
    m_free_ranges_near[i].insert(chunk + reserved, chunk + near_end);
    m_free_ranges_far[i].clear();
    m_free_ranges_far[i].insert(chunk + NEAR_CODE_SIZE, chunk + far_end);
  }
}

bool JitPPC64::SetEmitterStateToFreeCodeRegion()
{
  // The first chunk with room for any block, so that blocks are together and link directly, or
  // failing that, the one whose largest free ranges of near and far code are the largest, the
  // smaller of them compared, with far code scaled as their sizes are.
  constexpr size_t ROOM = 1024 * 1024;
  size_t best_chunk = CODE_CHUNKS;
  size_t best_size = 0;
  for (size_t i = 0; i < CODE_CHUNKS && best_size < ROOM; i++)
  {
    const auto free_near = m_free_ranges_near[i].by_size_begin();
    const auto free_far = m_free_ranges_far[i].by_size_begin();
    if (free_near == m_free_ranges_near[i].by_size_end() ||
        free_far == m_free_ranges_far[i].by_size_end())
    {
      continue;
    }
    const size_t far_size = static_cast<size_t>(free_far.to() - free_far.from());
    const size_t size = std::min(static_cast<size_t>(free_near.to() - free_near.from()),
                                 far_size * NEAR_CODE_SIZE / (CODE_CHUNK_SIZE - NEAR_CODE_SIZE));
    if (size > best_size)
    {
      best_chunk = i;
      best_size = size;
    }
  }
  if (best_chunk == CODE_CHUNKS)
  {
    WARN_LOG_FMT(DYNA_REC, "Failed to find free memory region in code region.");
    return false;
  }
  const auto free_near = m_free_ranges_near[best_chunk].by_size_begin();
  const auto free_far = m_free_ranges_far[best_chunk].by_size_begin();
  SetCodePtr(free_near.from(), free_near.to());
  m_other_code = free_far.from();
  m_other_code_end = free_far.to();
  m_other_code_write_failed = false;
  m_in_far_code = false;
  return true;
}

void JitPPC64::SwitchToFarCode()
{
  ASSERT(!m_in_far_code);
  m_in_far_code = true;
  u8* const near_code = GetWritableCodePtr();
  u8* const near_code_end = const_cast<u8*>(GetCodeEnd());
  const bool near_code_write_failed = HasWriteFailed();
  SetCodePtrUnsafe(m_other_code, m_other_code_end, m_other_code_write_failed);
  m_other_code = near_code;
  m_other_code_end = near_code_end;
  m_other_code_write_failed = near_code_write_failed;
}

FixupBranch JitPPC64::BCFar(CCFlags cond, CRF field)
{
  const FixupBranch skip = BC(Invert(cond), field);
  const FixupBranch far = B();
  SetJumpTarget(skip);
  return far;
}

void JitPPC64::FlushRegisters(FlushMode mode)
{
  gpr.Flush(mode);
  fpr.Flush(mode);
}

void JitPPC64::SwitchToNearCode()
{
  ASSERT(m_in_far_code);
  m_in_far_code = false;
  u8* const far_code = GetWritableCodePtr();
  u8* const far_code_end = const_cast<u8*>(GetCodeEnd());
  const bool far_code_write_failed = HasWriteFailed();
  SetCodePtrUnsafe(m_other_code, m_other_code_end, m_other_code_write_failed);
  m_other_code = far_code;
  m_other_code_end = far_code_end;
  m_other_code_write_failed = far_code_write_failed;
}

void JitPPC64::Run()
{
  ProtectStack();
  m_system.GetJitInterface().UpdateMembase();

  const auto enter = reinterpret_cast<void (*)()>(const_cast<u8*>(enter_code));
  enter();

  UnprotectStack();
}

void JitPPC64::SingleStep()
{
  // Jit() compiles a single instruction at a time while stepping, so this is the same as Run().
  Run();
}

void JitPPC64::InterpretInstruction(UGeckoInstruction inst)
{
  // For instructions that only use the registers and CR fields the analyzer lists, with XER,
  // FPSCR, SPRs and memory, and don't change MSR or end the block: the interpreter works on
  // PowerPCState, but only these have to be written back first, and the ones it writes dropped
  // after, which spares flushing the others. After a DSI exception, those hold what they held
  // before.
  ASSERT(!js.op->canEndBlock);
  BitSet32 fprs = js.op->fregsIn;
  if (js.op->fregOut >= 0)
    fprs[js.op->fregOut] = true;
  for (const int preg : js.op->regsIn | js.op->regsOut)
    gpr.StoreIfDirty(preg);
  for (const int preg : fprs)
    fpr.StoreIfDirty(preg);
  for (const int field : js.op->crIn | js.op->crOut)
    gpr.StoreCRIfDirty(field);

  // The PC is used by memory checks, by GPFifo to find stores to the gather pipe, and in logs.
  MOVI2R(GPR::R12, js.compilerPC);
  STW(GPR::R12, PPC_REG, PPCSTATE_OFF(pc));
  const BitSet32 saved = SaveScratchRegs();
  ABI_CallFunction(Interpreter::GetInterpreterOp(inst), &m_system.GetInterpreter(), inst.hex);
  RestoreScratchRegs(saved);

  for (const int preg : js.op->regsOut)
    gpr.Discard(preg);
  if (js.op->fregOut >= 0)
    fpr.Discard(js.op->fregOut);
  for (const int field : js.op->crOut)
    gpr.DiscardCR(field);
  // FPSCR[NI] may have changed.
  m_ni_loaded = false;

  if (ShouldHandleFPExceptionForInstruction(js.op))
    WriteConditionalExceptionExit(EXCEPTION_PROGRAM);
  if (jo.memcheck && (js.op->opinfo->flags & FL_LOADSTORE))
    WriteConditionalExceptionExit(EXCEPTION_DSI);
}

void JitPPC64::FallBackToInterpreter(UGeckoInstruction inst)
{
  // The interpreter works on PowerPCState.
  FlushRegisters(FlushMode::All);

  if (js.op->canEndBlock)
  {
    // The interpreter decides where to go next by writing npc.
    MOVI2R(GPR::R3, js.compilerPC);
    STW(GPR::R3, PPC_REG, PPCSTATE_OFF(pc));
    ADDI(GPR::R3, GPR::R3, 4);
    STW(GPR::R3, PPC_REG, PPCSTATE_OFF(npc));
  }

  const Interpreter::Instruction instr = Interpreter::GetInterpreterOp(inst);
  ABI_CallFunction(instr, &m_system.GetInterpreter(), inst.hex);
  // mtmsr and rfi change MSR[DR], and with it the arena, and mtfsf changes FPSCR[NI].
  LD(MEM_REG, PPC_REG, PPCSTATE_OFF(mem_ptr));
  m_ni_loaded = false;

  if (js.op->branchAction == PPCAnalyst::BranchAction::IdleLoop)
  {
    // A branch back to the start of a block which does nothing but wait for an interrupt.
    LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(npc));
    MOVI2R(GPR::R4, js.blockStart);
    CMPLW(CRF::CR1, GPR::R3, GPR::R4);
    const FixupBranch not_idle = BC(CCFlags::NE, CRF::CR1);
    ABI_CallFunction(&CoreTiming::GlobalIdle);
    SetJumpTarget(not_idle);
  }

  if (js.op->canEndBlock)
  {
    LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
    if (js.isLastInstruction)
    {
      WriteExceptionExit(DISPATCHER_PC);
    }
    else
    {
      // Only exit if the instruction branched.
      MOVI2R(GPR::R3, js.compilerPC + 4);
      CMPLW(CRF::CR1, DISPATCHER_PC, GPR::R3);
      const FixupBranch not_taken = BC(CCFlags::EQ, CRF::CR1);
      WriteExceptionExit(DISPATCHER_PC);
      SetJumpTarget(not_taken);
    }
  }
  else if (ShouldHandleFPExceptionForInstruction(js.op))
  {
    WriteConditionalExceptionExit(EXCEPTION_PROGRAM);
  }

  if (jo.memcheck && (js.op->opinfo->flags & FL_LOADSTORE))
    WriteConditionalExceptionExit(EXCEPTION_DSI);
}

void JitPPC64::HLEFunction(u32 hook_index)
{
  ABI_CallFunction(&HLE::ExecuteFromJIT, js.compilerPC, hook_index, &m_system);
  LD(MEM_REG, PPC_REG, PPCSTATE_OFF(mem_ptr));
  m_ni_loaded = false;
  // The call clobbers CR7, which a compare before may have set for this instruction's branch.
  m_cr7_field.reset();
}

bool JitPPC64::HandleFunctionHooking(u32 address)
{
  const auto result = HLE::TryReplaceFunction(m_ppc_symbol_db, address, PowerPC::CoreMode::JIT);
  if (!result)
    return false;

  FlushRegisters(FlushMode::All);
  HLEFunction(result.hook_index);

  if (result.type != HLE::HookType::Replace)
    return false;

  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
  js.downcountAmount += js.st.numCycles;
  WriteExit(DISPATCHER_PC);
  return true;
}

void JitPPC64::DoNothing(UGeckoInstruction inst)
{
  // Yup, just don't do anything.
}

void JitPPC64::Cleanup()
{
  if (m_ppc_state.feature_flags & FEATURE_FLAG_PERFMON)
  {
    ABI_CallFunction(&PowerPC::UpdatePerformanceMonitor, js.downcountAmount, js.numLoadStoreInst,
                     js.numFloatingPointInst, &m_ppc_state);
  }
}

void JitPPC64::EndProfiling()
{
  if (IsProfilingEnabled())
  {
    ABI_CallFunction(&JitBlock::ProfileData::EndProfiling, js.curBlock->profile_data.get(),
                     js.downcountAmount);
  }
}

void JitPPC64::DoDownCount()
{
  // The downcount is sign-extended, so the 64-bit result, which sets CR0, has the right sign.
  if (js.downcountAmount <= 0x8000)
  {
    ADDIC(DOWNCOUNT_REG, DOWNCOUNT_REG, static_cast<s16>(-static_cast<s32>(js.downcountAmount)),
          true);
  }
  else
  {
    MOVI2R(GPR::R3, js.downcountAmount);
    SUBF(DOWNCOUNT_REG, GPR::R3, DOWNCOUNT_REG, true);
  }
}

void JitPPC64::WriteExit(u32 destination, bool LK, u32 exit_address_after_return)
{
  Cleanup();
  EndProfiling();
  DoDownCount();

  LK &= m_enable_blr_optimization;
  if (LK)
    PushReturnFrame(exit_address_after_return, JitPPC64BlockCache::CALL_LINK_RETURN);

  JitBlock::LinkData link_data{};
  link_data.exitAddress = destination;
  link_data.exitPtrs = GetWritableCodePtr();
  link_data.linkStatus = false;
  link_data.call = LK;
  js.curBlock->linkData.push_back(link_data);

  blocks.WriteLinkBlock(*this, link_data);

  if (LK)
    WriteReturnExit(exit_address_after_return);
}

void JitPPC64::WriteExit(GPR dest, bool LK, u32 exit_address_after_return)
{
  if (dest != DISPATCHER_PC)
    MR(DISPATCHER_PC, dest);

  Cleanup();
  EndProfiling();
  DoDownCount();

  LK &= m_enable_blr_optimization;
  if (!LK)
  {
    B(Routine(dispatcher));
    return;
  }

  // A host call to the dispatcher, which the guest's return comes back after.
  PushReturnFrame(exit_address_after_return, sizeof(u32));
  BL(Routine(dispatcher));
  WriteReturnExit(exit_address_after_return);
}

void JitPPC64::WriteBLRExit(GPR dest)
{
  if (!m_enable_blr_optimization)
  {
    WriteExit(dest);
    return;
  }

  if (dest != DISPATCHER_PC)
    MR(DISPATCHER_PC, dest);

  Cleanup();
  EndProfiling();

  // Return to the host code after the call, if the frame on top is for this return address and
  // the same feature flags.
  LD(GPR::R4, GPR::SP, BLR_GUEST_RETURN);
  const u32 feature_flags = m_ppc_state.feature_flags;
  if (feature_flags == 0)
  {
    CMPLD(CRF::CR1, GPR::R4, DISPATCHER_PC);
  }
  else
  {
    MOVI2R(GPR::R3, u64{feature_flags} << 32);
    OR(GPR::R3, GPR::R3, DISPATCHER_PC);
    CMPLD(CRF::CR1, GPR::R4, GPR::R3);
  }
  const FixupBranch no_match = BC(CCFlags::NE, CRF::CR1);
  LD(GPR::R4, GPR::SP, BLR_HOST_RETURN);
  ADDI(GPR::SP, GPR::SP, BLR_FRAME_SIZE);
  MTLR(GPR::R4);
  DoDownCount();
  BLR();

  // Otherwise the frames are of no use.
  SetJumpTarget(no_match);
  ResetStack();
  DoDownCount();
  B(Routine(dispatcher));
}

void JitPPC64::FakeLKExit(u32 exit_address_after_return)
{
  if (!m_enable_blr_optimization)
    return;

  // A host call over the exit, so that the host predicts the guest's return to it.
  PushReturnFrame(exit_address_after_return, sizeof(u32));
  BL(GetCodePtr() + sizeof(u32) + JitPPC64BlockCache::BLOCK_LINK_SIZE);
  WriteReturnExit(exit_address_after_return);
}

void JitPPC64::PushReturnFrame(u32 exit_address_after_return, s32 host_offset)
{
  MOVI2R(GPR::R4, (u64{m_ppc_state.feature_flags} << 32) | exit_address_after_return);
  STDU(GPR::SP, GPR::SP, -BLR_FRAME_SIZE);
  STD(GPR::R4, GPR::SP, BLR_GUEST_RETURN);
  // The host address, from that of the instruction after addpcis or bcl, which is followed by
  // (mflr,) addi and std.
  s32 offset = host_offset;
  if (m_isa_3_0)
  {
    ADDPCIS(GPR::R4, 0);
    offset += 2 * sizeof(u32);
  }
  else
  {
    BCL_NEXT();
    MFLR(GPR::R4);
    offset += 3 * sizeof(u32);
  }
  ADDI(GPR::R4, GPR::R4, static_cast<s16>(offset));
  STD(GPR::R4, GPR::SP, BLR_HOST_RETURN);
}

void JitPPC64::WriteReturnExit(u32 exit_address_after_return)
{
  // The guest's return set CR0 from the downcount, as an exit does.
  JitBlock::LinkData link_data{};
  link_data.exitAddress = exit_address_after_return;
  link_data.exitPtrs = GetWritableCodePtr();
  link_data.linkStatus = false;
  link_data.call = false;
  js.curBlock->linkData.push_back(link_data);

  blocks.WriteLinkBlock(*this, link_data);
}

void JitPPC64::ResetStack()
{
  if (m_enable_blr_optimization)
    LD(GPR::SP, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));
}

void JitPPC64::WriteExceptionExit(u32 destination, bool only_external, bool always_exception)
{
  MOVI2R(DISPATCHER_PC, destination);
  WriteExceptionExit(DISPATCHER_PC, only_external, always_exception);
}

void JitPPC64::WriteExceptionExit(GPR dest, bool only_external, bool always_exception)
{
  if (dest != DISPATCHER_PC)
    MR(DISPATCHER_PC, dest);

  Cleanup();

  FixupBranch no_exceptions;
  if (!always_exception)
  {
    LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(Exceptions));
    CMPWI(CRF::CR1, GPR::R3, 0);
    no_exceptions = BC(CCFlags::EQ, CRF::CR1);
  }

  STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
  if (only_external)
    ABI_CallFunction(&PowerPC::CheckExternalExceptionsFromJIT, &m_system.GetPowerPC());
  else
    ABI_CallFunction(&PowerPC::CheckExceptionsFromJIT, &m_system.GetPowerPC());
  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));

  if (!always_exception)
    SetJumpTarget(no_exceptions);

  EndProfiling();
  DoDownCount();

  B(Routine(dispatcher));
}

void JitPPC64::WriteConditionalExceptionExit(u32 exception, GPR tmp)
{
  // The exit is in far code, unless this is far code already.
  ASSERT(exception <= 0xFFFF);
  LWZ(tmp, PPC_REG, PPCSTATE_OFF(Exceptions));
  ANDI_(tmp, tmp, static_cast<u16>(exception));
  if (m_in_far_code)
  {
    const FixupBranch no_exception = BC(CCFlags::EQ, CRF::CR0);
    FlushRegisters(FlushMode::MaintainState);
    WriteTakeExceptionExit();
    SetJumpTarget(no_exception);
    return;
  }

  const FixupBranch raised = BCFar(CCFlags::NE, CRF::CR0);
  SwitchToFarCode();
  SetJumpTarget(raised);
  FlushRegisters(FlushMode::MaintainState);
  WriteTakeExceptionExit();
  SwitchToNearCode();
}

void JitPPC64::WriteTakeExceptionExit()
{
  // Profiling and the performance monitor need this block's data at the exit.
  if (IsProfilingEnabled() || (m_ppc_state.feature_flags & FEATURE_FLAG_PERFMON))
  {
    WriteExceptionExit(js.compilerPC, false, true);
    return;
  }

  MOVI2R(GPR::R3, js.compilerPC);
  MOVI2R(GPR::R4, js.downcountAmount);
  B(Routine(m_exception_exit_routine));
}

void JitPPC64::WriteProgramExceptionExit(u32 cause)
{
  // As GenerateProgramException, then the exception exit.
  FlushRegisters(FlushMode::MaintainState);
  {
    const auto tmp = gpr.GetScopedReg();
    LWZ(tmp, PPC_REG, PPCSTATE_OFF(Exceptions));
    ORI(tmp, tmp, EXCEPTION_PROGRAM);
    STW(tmp, PPC_REG, PPCSTATE_OFF(Exceptions));
    MOVI2R(tmp, cause);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR1));
  }
  WriteTakeExceptionExit();
}

void JitPPC64::Jit(u32 em_address)
{
  Jit(em_address, true);
}

void JitPPC64::Jit(u32 em_address, bool clear_cache_and_retry_on_failure)
{
  CleanUpAfterStackFault();

  if (SConfig::GetInstance().bJITNoBlockCache)
    ClearCache();
  FreeRanges();

  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

  std::size_t block_size = m_code_buffer.size();

  auto& cpu = m_system.GetCPU();

  if (IsDebuggingEnabled())
  {
    // We can link blocks as long as we are not single stepping
    jo.enableBlocklink = !SConfig::GetInstance().bJITNoBlockLinking;
    SetOptimizationEnabled(true);

    if (!IsProfilingEnabled() && cpu.IsStepping())
    {
      block_size = 1;

      // Do not link this block to other blocks while single stepping
      jo.enableBlocklink = false;
      SetOptimizationEnabled(false);
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
    m_system.GetJitInterface().UpdateMembase();
    WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", nextPC);
    return;
  }

  if (SetEmitterStateToFreeCodeRegion())
  {
    u8* near_start = GetWritableCodePtr();
    u8* far_start = m_other_code;

    JitBlock* b = blocks.AllocateBlock(em_address);
    if (DoJit(em_address, b, nextPC))
    {
      // Code generation succeeded.

      // Mark the memory regions that this code block uses as used in the local rangesets.
      const size_t chunk = CodeChunk(near_start);
      u8* near_end = GetWritableCodePtr();
      if (near_start != near_end)
        m_free_ranges_near[chunk].erase(near_start, near_end);
      u8* far_end = m_other_code;
      if (far_start != far_end)
        m_free_ranges_far[chunk].erase(far_start, far_end);
      FlushIcacheSection(far_start, far_end);

      // Store the used memory regions in the block so we know what to mark as unused when the
      // block gets invalidated.
      b->near_begin = near_start;
      b->near_end = near_end;
      b->far_begin = far_start;
      b->far_end = far_end;

      blocks.FinalizeBlock(*b, jo.enableBlocklink, code_block, m_code_buffer);

#ifdef JIT_LOG_GENERATED_CODE
      LogGeneratedCode();
#endif
      return;
    }
  }

  if (clear_cache_and_retry_on_failure)
  {
    // Code generation failed due to not enough free space in the code region.
    // Clear the entire JIT cache and retry.
    WARN_LOG_FMT(DYNA_REC, "flushing code caches, please report if this happens a lot");
    ClearCache();
    Jit(em_address, false);
    return;
  }

  PanicAlertFmtT("JIT failed to find code space after a cache clear. This should never happen. "
                 "Please report this incident on the bug tracker. Dolphin will now exit.");
  std::exit(-1);
}

bool JitPPC64::DoJit(u32 em_address, JitBlock* b, u32 nextPC)
{
  auto& cpu = m_system.GetCPU();

  js.isLastInstruction = false;
  js.firstFPInstructionFound = false;
  js.blockStart = em_address;
  js.fifoBytesSinceCheck = 0;
  js.mustCheckFifo = false;
  js.downcountAmount = 0;
  js.skipInstructions = 0;
  js.curBlock = b;
  m_next_cr7_field.reset();
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;

  b->normalEntry = GetWritableCodePtr();

  gpr.Start();
  fpr.Start();
  m_ni_loaded = false;

  // Take the GQRs that the block uses but doesn't set to keep the values they have now, as Jit64
  // does, unless that turned out wrong for this block before. If they've changed when the block
  // runs, have it compiled again without that.
  js.constantGqrValid = BitSet8();
  if (!js.pairedQuantizeAddresses.TestBit(js.blockStart / sizeof(UGeckoInstruction)))
  {
    const BitSet8 gqr_static = code_block.m_gqr_used & ~code_block.m_gqr_modified;
    if (gqr_static)
    {
      std::vector<FixupBranch> changed;
      for (const int gqr : gqr_static)
      {
        const u32 value = GQR(m_ppc_state, gqr);
        js.constantGqr[gqr] = value;
        LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF_SPR(SPR_GQR0 + gqr));
        MOVI2R(GPR::R4, value);
        CMPLW(CRF::CR1, GPR::R3, GPR::R4);
        changed.push_back(BCFar(CCFlags::NE, CRF::CR1));
      }

      SwitchToFarCode();
      for (const FixupBranch& branch : changed)
        SetJumpTarget(branch);
      MOVI2R(DISPATCHER_PC, js.blockStart);
      STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
      ABI_CallFunction(&JitInterface::CompileExceptionCheckFromJIT, &m_system.GetJitInterface(),
                       JitInterface::ExceptionType::PairedQuantize);
      B(Routine(dispatcher_no_check));
      SwitchToNearCode();

      js.constantGqrValid = gqr_static;
    }
  }

  // Conditionally add profiling code.
  if (IsProfilingEnabled())
    ABI_CallFunction(&JitBlock::ProfileData::BeginProfiling, b->profile_data.get());

  // Translate instructions
  for (u32 i = 0; i < code_block.m_num_instructions; i++)
  {
    PPCAnalyst::CodeOp& op = m_code_buffer[i];

    js.compilerPC = op.address;
    js.op = &op;
    js.instructionsLeft = (code_block.m_num_instructions - 1) - i;
    const GekkoOPInfo* opinfo = op.opinfo;
    js.downcountAmount += opinfo->num_cycles;
    js.isLastInstruction = i == (code_block.m_num_instructions - 1);
    gpr.NextInstruction();
    fpr.NextInstruction();
    // Only the instruction right after a compare can branch on host CR7.
    m_cr7_field = std::exchange(m_next_cr7_field, std::nullopt);

    if (HandleFunctionHooking(op.address))
      break;

    if (!op.skip)
    {
      // Count the instruction first, as it may exit the block.
      if (opinfo->flags & FL_LOADSTORE)
        ++js.numLoadStoreInst;

      if (opinfo->flags & FL_USE_FPU)
        ++js.numFloatingPointInst;

      if (IsDebuggingEnabled() && !cpu.IsStepping() &&
          m_system.GetPowerPC().GetBreakPoints().IsAddressBreakPoint(op.address))
      {
        FlushRegisters(FlushMode::All);
        MOVI2R(DISPATCHER_PC, op.address);
        STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
        STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
        ABI_CallFunction(&PowerPC::CheckAndHandleBreakPointsFromJIT, &m_system.GetPowerPC());
        m_cr7_field.reset();

        MOVP2R(GPR::R3, cpu.GetStatePtr());
        LWZ(GPR::R3, GPR::R3, 0);
        static_assert(std::to_underlying(CPU::State::Running) == 0);
        CMPWI(CRF::CR1, GPR::R3, 0);
        const FixupBranch no_breakpoint = BC(CCFlags::EQ, CRF::CR1);

        Cleanup();
        EndProfiling();
        DoDownCount();
        // dispatcher_exit is past the dispatcher's store of the downcount.
        STW(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));
        B(Routine(dispatcher_exit));

        SetJumpTarget(no_breakpoint);
      }

      if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
      {
        // This instruction uses the FPU: raise an exception if it is disabled.
        LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(msr));
        ANDI_(GPR::R3, GPR::R3, 0x2000);  // MSR.FP
        const FixupBranch fpu_enabled = BC(CCFlags::NE, CRF::CR0);

        FlushRegisters(FlushMode::MaintainState);
        LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(Exceptions));
        ORI(GPR::R3, GPR::R3, EXCEPTION_FPU_UNAVAILABLE);
        STW(GPR::R3, PPC_REG, PPCSTATE_OFF(Exceptions));
        WriteExceptionExit(js.compilerPC, false, true);

        SetJumpTarget(fpu_enabled);

        js.firstFPInstructionFound = true;
      }

      CompileInstruction(op);
    }

    i += js.skipInstructions;
    js.skipInstructions = 0;
  }

  if (code_block.m_broken)
  {
    FlushRegisters(FlushMode::All);
    WriteExit(nextPC);
  }

  if (HasWriteFailed() || m_other_code_write_failed)
  {
    WARN_LOG_FMT(DYNA_REC, "JIT ran out of space in code region during code generation.");
    return false;
  }

  FlushIcache();

  return true;
}

void JitPPC64::EraseSingleBlock(const JitBlock& block)
{
  blocks.EraseSingleBlock(block);
  FreeRanges();
}

std::vector<JitBase::MemoryStats> JitPPC64::GetMemoryStats() const
{
  static_assert(CODE_CHUNKS == 4);
  return {
      {"near_0", m_free_ranges_near[0].get_stats()}, {"near_1", m_free_ranges_near[1].get_stats()},
      {"near_2", m_free_ranges_near[2].get_stats()}, {"near_3", m_free_ranges_near[3].get_stats()},
      {"far_0", m_free_ranges_far[0].get_stats()},   {"far_1", m_free_ranges_far[1].get_stats()},
      {"far_2", m_free_ranges_far[2].get_stats()},   {"far_3", m_free_ranges_far[3].get_stats()}};
}

std::size_t JitPPC64::DisassembleNearCode(const JitBlock& block, std::ostream& stream) const
{
  return m_disassembler->Disassemble(block.normalEntry, block.near_end, stream);
}

std::size_t JitPPC64::DisassembleFarCode(const JitBlock& block, std::ostream& stream) const
{
  return m_disassembler->Disassemble(block.far_begin, block.far_end, stream);
}

void JitPPC64::LogGeneratedCode() const
{
  std::ostringstream stream;

  stream << "\nPPC Code Buffer:\n";
  for (const PPCAnalyst::CodeOp& op :
       std::span{m_code_buffer.data(), code_block.m_num_instructions})
  {
    fmt::print(stream, "0x{:08x}\t\t{}\n", op.address,
               Common::GekkoDisassembler::Disassemble(op.inst.hex, op.address));
  }

  const JitBlock* const block = js.curBlock;
  stream << "\nHost Code:\n";
  m_disassembler->Disassemble(block->normalEntry, block->near_end, stream);

  DEBUG_LOG_FMT(DYNA_REC, "{}", std::move(stream).str());
}
