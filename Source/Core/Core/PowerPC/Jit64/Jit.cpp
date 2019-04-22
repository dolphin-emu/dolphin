// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"

#include <disasm.h>
#include <map>
#include <sstream>
#include <string>

// for the PROFILER stuff
#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/PerformanceCounter.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/x64ABI.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/MachineContext.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/FarCodeCache.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/Jit64Common/TrampolineCache.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/Profiler.h"
#if defined(_DEBUG) || defined(DEBUGFAST)
#include "Common/GekkoDisassembler.h"
#endif

using namespace Gen;
using namespace PowerPC;

// Dolphin's PowerPC->x86_64 JIT dynamic recompiler
// Written mostly by ector (hrydgard)
// Features:
// * Basic block linking
// * Fast dispatcher

// Unfeatures:
// * Does not recompile all instructions - sometimes falls back to inserting a CALL to the
// corresponding Interpreter function.

// Open questions
// * Should there be any statically allocated registers? r3, r4, r5, r8, r0 come to mind.. maybe sp
// * Does it make sense to finish off the remaining non-jitted instructions? Seems we are hitting
// diminishing returns.

// Other considerations
//
// We support block linking. Reserve space at the exits of every block for a full 5-byte jmp. Save
// 16-bit offsets
// from the starts of each block, marking the exits so that they can be nicely patched at any time.
//
// Blocks do NOT use call/ret, they only jmp to each other and to the dispatcher when necessary.
//
// All blocks that can be precompiled will be precompiled. Code will be memory protected - any write
// will mark
// the region as non-compilable, and all links to the page will be torn out and replaced with
// dispatcher jmps.
//
// Alternatively, icbi instruction SHOULD mark where we can't compile
//
// Seldom-happening events is handled by adding a decrement of a counter to all blr instructions
// (which are
// expensive anyway since we need to return to dispatcher, except when they can be predicted).

// TODO: SERIOUS synchronization problem with the video backend setting tokens and breakpoints in
// dual core mode!!!
//       Somewhat fixed by disabling idle skipping when certain interrupts are enabled
//       This is no permanent reliable fix
// TODO: Zeldas go whacko when you hang the gfx thread

// Idea - Accurate exception handling
// Compute register state at a certain instruction by running the JIT in "dry mode", and stopping at
// the right place.
// Not likely to be done :P

// Optimization Ideas -
/*
  * Assume SP is in main RAM (in Wii mode too?) - partly done
  * Assume all floating point loads and double precision loads+stores are to/from main ram
    (single precision stores can be used in write gather pipe, specialized fast check added)
  * AMD only - use movaps instead of movapd when loading ps from memory?
  * HLE functions like floorf, sin, memcpy, etc - they can be much faster
  * ABI optimizations - drop F0-F13 on blr, for example. Watch out for context switching.
    CR2-CR4 are non-volatile, rest of CR is volatile -> dropped on blr.
  R5-R12 are volatile -> dropped on blr.
  * classic inlining across calls.
  * Track which registers a block clobbers without using, then take advantage of this knowledge
    when compiling a block that links to that block.
  * Track more dependencies between instructions, e.g. avoiding PPC_FP code, single/double
    conversion, movddup on non-paired singles, etc where possible.
  * Support loads/stores directly from xmm registers in jit_util and the backpatcher; this might
    help AMD a lot since gpr/xmm transfers are slower there.
  * Smarter register allocation in general; maybe learn to drop values once we know they won't be
    used again before being overwritten?
  * More flexible reordering; there's limits to how far we can go because of exception handling
    and such, but it's currently limited to integer ops only. This can definitely be made better.
*/

// The BLR optimization is nice, but it means that JITted code can overflow the
// native stack by repeatedly running BL.  (The chance of this happening in any
// retail game is close to 0, but correctness is correctness...) Also, the
// overflow might not happen directly in the JITted code but in a C++ function
// called from it, so we can't just adjust RSP in the case of a fault.
// Instead, we have to have extra stack space preallocated under the fault
// point which allows the code to continue, after wiping the JIT cache so we
// can reset things at a safe point.  Once this condition trips, the
// optimization is permanently disabled, under the assumption this will never
// happen in practice.

// On Unix, we just mark an appropriate region of the stack as PROT_NONE and
// handle it the same way as fastmem faults.  It's safe to take a fault with a
// bad RSP, because on Linux we can use sigaltstack and on OS X we're already
// on a separate thread.

// Windows is... under-documented.
// It already puts guard pages so it can automatically grow the stack and it
// doesn't look like there is a way to hook into a guard page fault and implement
// our own logic.
// But when windows reaches the last guard page, it raises a "Stack Overflow"
// exception which we can hook into, however by default it leaves you with less
// than 4kb of stack. So we use SetThreadStackGuarantee to trigger the Stack
// Overflow early while we still have 512kb of stack remaining.
// After resetting the stack to the top, we call _resetstkoflw() to restore
// the guard page at the 512kb mark.

enum
{
  STACK_SIZE = 2 * 1024 * 1024,
  SAFE_STACK_SIZE = 512 * 1024,
  GUARD_SIZE = 0x10000,  // two guards - bottom (permanent) and middle (see above)
  GUARD_OFFSET = STACK_SIZE - SAFE_STACK_SIZE - GUARD_SIZE,
};

Jit64::Jit64() : QuantizedMemoryRoutines(*this)
{
}

Jit64::~Jit64() = default;

void Jit64::AllocStack()
{
#ifndef _WIN32
  m_stack = static_cast<u8*>(Common::AllocateMemoryPages(STACK_SIZE));
  Common::ReadProtectMemory(m_stack, GUARD_SIZE);
  Common::ReadProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
#else
  // For windows we just keep using the system stack and reserve a large amount of memory at the end
  // of the stack.
  ULONG reserveSize = SAFE_STACK_SIZE;
  SetThreadStackGuarantee(&reserveSize);
#endif
}

void Jit64::FreeStack()
{
#ifndef _WIN32
  if (m_stack)
  {
    Common::FreeMemoryPages(m_stack, STACK_SIZE);
    m_stack = nullptr;
  }
#endif
}

bool Jit64::HandleStackFault()
{
  // It's possible the stack fault might have been caused by something other than
  // the BLR optimization. If the fault was triggered from another thread, or
  // when BLR optimization isn't enabled then there is nothing we can do about the fault.
  // Return false so the regular stack overflow handler can trigger (which crashes)
  if (!m_enable_blr_optimization || !Core::IsCPUThread())
    return false;

  WARN_LOG(POWERPC, "BLR cache disabled due to excessive BL in the emulated program.");
  m_enable_blr_optimization = false;
#ifndef _WIN32
  // Windows does this automatically.
  Common::UnWriteProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
#endif
  // We're going to need to clear the whole cache to get rid of the bad
  // CALLs, but we can't yet.  Fake the downcount so we're forced to the
  // dispatcher (no block linking), and clear the cache so we're sent to
  // Jit. In the case of Windows, we will also need to call _resetstkoflw()
  // to reset the guard page.
  // Yeah, it's kind of gross.
  GetBlockCache()->InvalidateICache(0, 0xffffffff, true);
  CoreTiming::ForceExceptionCheck(0);
  m_cleanup_after_stackfault = true;

  return true;
}

bool Jit64::HandleFault(uintptr_t access_address, SContext* ctx)
{
  uintptr_t stack = (uintptr_t)m_stack;
  uintptr_t diff = access_address - stack;
  // In the trap region?
  if (m_enable_blr_optimization && diff >= GUARD_OFFSET && diff < GUARD_OFFSET + GUARD_SIZE)
    return HandleStackFault();

  // This generates some fairly heavy trampolines, but it doesn't really hurt.
  // Only instructions that access I/O will get these, and there won't be that
  // many of them in a typical program/game.

  // TODO: do we properly handle off-the-end?
  const auto base_ptr = reinterpret_cast<uintptr_t>(Memory::physical_base);
  if (access_address >= base_ptr && access_address < base_ptr + 0x100010000)
    return BackPatch(static_cast<u32>(access_address - base_ptr), ctx);

  const auto logical_base_ptr = reinterpret_cast<uintptr_t>(Memory::logical_base);
  if (access_address >= logical_base_ptr && access_address < logical_base_ptr + 0x100010000)
    return BackPatch(static_cast<u32>(access_address - logical_base_ptr), ctx);

  return false;
}

bool Jit64::BackPatch(u32 emAddress, SContext* ctx)
{
  u8* codePtr = reinterpret_cast<u8*>(ctx->CTX_PC);

  if (!IsInSpace(codePtr))
    return false;  // this will become a regular crash real soon after this

  auto it = m_back_patch_info.find(codePtr);
  if (it == m_back_patch_info.end())
  {
    PanicAlert("BackPatch: no register use entry for address %p", codePtr);
    return false;
  }

  TrampolineInfo& info = it->second;

  u8* exceptionHandler = nullptr;
  if (jo.memcheck)
  {
    auto it2 = m_exception_handler_at_loc.find(codePtr);
    if (it2 != m_exception_handler_at_loc.end())
      exceptionHandler = it2->second;
  }

  // In the trampoline code, we jump back into the block at the beginning
  // of the next instruction. The next instruction comes immediately
  // after the backpatched operation, or BACKPATCH_SIZE bytes after the start
  // of the backpatched operation, whichever comes last. (The JIT inserts NOPs
  // into the original code if necessary to ensure there is enough space
  // to insert the backpatch jump.)

  js.generatingTrampoline = true;
  js.trampolineExceptionHandler = exceptionHandler;
  js.compilerPC = info.pc;

  // Generate the trampoline.
  const u8* trampoline = trampolines.GenerateTrampoline(info);
  js.generatingTrampoline = false;
  js.trampolineExceptionHandler = nullptr;

  u8* start = info.start;

  // Patch the original memory operation.
  XEmitter emitter(start);
  emitter.JMP(trampoline, true);
  // NOPs become dead code
  const u8* end = info.start + info.len;
  for (const u8* i = emitter.GetCodePtr(); i < end; ++i)
    emitter.INT3();

  // Rewind time to just before the start of the write block. If we swapped memory
  // before faulting (eg: the store+swap was not an atomic op like MOVBE), let's
  // swap it back so that the swap can happen again (this double swap isn't ideal but
  // only happens the first time we fault).
  if (info.nonAtomicSwapStoreSrc != Gen::INVALID_REG)
  {
    u64* ptr = ContextRN(ctx, info.nonAtomicSwapStoreSrc);
    switch (info.accessSize << 3)
    {
    case 8:
      // No need to swap a byte
      break;
    case 16:
      *ptr = Common::swap16(static_cast<u16>(*ptr));
      break;
    case 32:
      *ptr = Common::swap32(static_cast<u32>(*ptr));
      break;
    case 64:
      *ptr = Common::swap64(static_cast<u64>(*ptr));
      break;
    default:
      DEBUG_ASSERT(0);
      break;
    }
  }

  // This is special code to undo the LEA in SafeLoadToReg if it clobbered the address
  // register in the case where reg_value shared the same location as opAddress.
  if (info.offsetAddedToAddress)
  {
    u64* ptr = ContextRN(ctx, info.op_arg.GetSimpleReg());
    *ptr -= static_cast<u32>(info.offset);
  }

  ctx->CTX_PC = reinterpret_cast<u64>(trampoline);

  return true;
}

void Jit64::Init()
{
  InitializeInstructionTables();
  EnableBlockLink();

  jo.optimizeGatherPipe = true;
  jo.accurateSinglePrecision = true;
  UpdateMemoryOptions();
  js.fastmemLoadStore = nullptr;
  js.compilerPC = 0;

  gpr.SetEmitter(this);
  fpr.SetEmitter(this);

  const size_t routines_size = asm_routines.CODE_SIZE;
  const size_t trampolines_size = jo.memcheck ? TRAMPOLINE_CODE_SIZE_MMU : TRAMPOLINE_CODE_SIZE;
  const size_t farcode_size = jo.memcheck ? FARCODE_SIZE_MMU : FARCODE_SIZE;
  const size_t constpool_size = m_const_pool.CONST_POOL_SIZE;
  AllocCodeSpace(CODE_SIZE + routines_size + trampolines_size + farcode_size + constpool_size);
  AddChildCodeSpace(&asm_routines, routines_size);
  AddChildCodeSpace(&trampolines, trampolines_size);
  AddChildCodeSpace(&m_far_code, farcode_size);
  m_const_pool.Init(AllocChildCodeSpace(constpool_size), constpool_size);

  // BLR optimization has the same consequences as block linking, as well as
  // depending on the fault handler to be safe in the event of excessive BL.
  m_enable_blr_optimization = jo.enableBlocklink && SConfig::GetInstance().bFastmem &&
                              !SConfig::GetInstance().bEnableDebugging;
  m_cleanup_after_stackfault = false;

  m_stack = nullptr;
  if (m_enable_blr_optimization)
    AllocStack();

  blocks.Init();
  asm_routines.Init(m_stack ? (m_stack + STACK_SIZE) : nullptr);

  // important: do this *after* generating the global asm routines, because we can't use farcode in
  // them.
  // it'll crash because the farcode functions get cleared on JIT clears.
  m_far_code.Init();
  Clear();

  code_block.m_stats = &js.st;
  code_block.m_gpa = &js.gpa;
  code_block.m_fpa = &js.fpa;
  EnableOptimization();
}

void Jit64::ClearCache()
{
  blocks.Clear();
  trampolines.ClearCodeSpace();
  m_far_code.ClearCodeSpace();
  m_const_pool.Clear();
  ClearCodeSpace();
  Clear();
  UpdateMemoryOptions();
}

void Jit64::Shutdown()
{
  FreeStack();
  FreeCodeSpace();

  blocks.Shutdown();
  m_far_code.Shutdown();
  m_const_pool.Shutdown();
}

void Jit64::FallBackToInterpreter(UGeckoInstruction inst)
{
  gpr.Flush();
  fpr.Flush();
  if (js.op->opinfo->flags & FL_ENDBLOCK)
  {
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
    MOV(32, PPCSTATE(npc), Imm32(js.compilerPC + 4));
  }
  Interpreter::Instruction instr = PPCTables::GetInterpreterOp(inst);
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunctionC(instr, inst.hex);
  ABI_PopRegistersAndAdjustStack({}, 0);
  if (js.op->opinfo->flags & FL_ENDBLOCK)
  {
    if (js.isLastInstruction)
    {
      MOV(32, R(RSCRATCH), PPCSTATE(npc));
      MOV(32, PPCSTATE(pc), R(RSCRATCH));
      WriteExceptionExit();
    }
    else
    {
      MOV(32, R(RSCRATCH), PPCSTATE(npc));
      CMP(32, R(RSCRATCH), Imm32(js.compilerPC + 4));
      FixupBranch c = J_CC(CC_Z);
      MOV(32, PPCSTATE(pc), R(RSCRATCH));
      WriteExceptionExit();
      SetJumpTarget(c);
    }
  }
}

void Jit64::HLEFunction(UGeckoInstruction _inst)
{
  gpr.Flush();
  fpr.Flush();
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunctionCC(HLE::Execute, js.compilerPC, _inst.hex);
  ABI_PopRegistersAndAdjustStack({}, 0);
}

void Jit64::DoNothing(UGeckoInstruction _inst)
{
  // Yup, just don't do anything.
}

static const bool ImHereDebug = false;
static const bool ImHereLog = false;
static std::map<u32, int> been_here;

static void ImHere()
{
  static File::IOFile f;
  if (ImHereLog)
  {
    if (!f)
      f.Open("log64.txt", "w");

    fprintf(f.GetHandle(), "%08x\n", PC);
  }
  if (been_here.find(PC) != been_here.end())
  {
    been_here.find(PC)->second++;
    if ((been_here.find(PC)->second) & 1023)
      return;
  }
  INFO_LOG(DYNA_REC, "I'm here - PC = %08x , LR = %08x", PC, LR);
  been_here[PC] = 1;
}

bool Jit64::Cleanup()
{
  bool did_something = false;

  if (jo.optimizeGatherPipe && js.fifoBytesSinceCheck > 0)
  {
    MOV(64, R(RSCRATCH), PPCSTATE(gather_pipe_ptr));
    SUB(64, R(RSCRATCH), PPCSTATE(gather_pipe_base_ptr));
    CMP(64, R(RSCRATCH), Imm32(GPFifo::GATHER_PIPE_SIZE));
    FixupBranch exit = J_CC(CC_L);
    ABI_PushRegistersAndAdjustStack({}, 0);
    ABI_CallFunction(GPFifo::UpdateGatherPipe);
    ABI_PopRegistersAndAdjustStack({}, 0);
    SetJumpTarget(exit);
    did_something = true;
  }

  // SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
  if (MMCR0.Hex || MMCR1.Hex)
  {
    ABI_PushRegistersAndAdjustStack({}, 0);
    ABI_CallFunctionCCC(PowerPC::UpdatePerformanceMonitor, js.downcountAmount, js.numLoadStoreInst,
                        js.numFloatingPointInst);
    ABI_PopRegistersAndAdjustStack({}, 0);
    did_something = true;
  }

  if (jo.profile_blocks)
  {
    ABI_PushRegistersAndAdjustStack({}, 0);
    // get end tic
    MOV(64, R(ABI_PARAM1), ImmPtr(&js.curBlock->profile_data.ticStop));
    ABI_CallFunction(QueryPerformanceCounter);
    // tic counter += (end tic - start tic)
    MOV(64, R(RSCRATCH2), ImmPtr(&js.curBlock->profile_data));
    MOV(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(JitBlock::ProfileData, ticStop)));
    SUB(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(JitBlock::ProfileData, ticStart)));
    ADD(64, R(RSCRATCH), MDisp(RSCRATCH2, offsetof(JitBlock::ProfileData, ticCounter)));
    ADD(64, MDisp(RSCRATCH2, offsetof(JitBlock::ProfileData, downcountCounter)),
        Imm32(js.downcountAmount));
    MOV(64, MDisp(RSCRATCH2, offsetof(JitBlock::ProfileData, ticCounter)), R(RSCRATCH));
    ABI_PopRegistersAndAdjustStack({}, 0);
  }

  return did_something;
}

void Jit64::FakeBLCall(u32 after)
{
  if (!m_enable_blr_optimization)
    return;

  // We may need to fake the BLR stack on inlined CALL instructions.
  // Else we can't return to this location any more.
  MOV(32, R(RSCRATCH2), Imm32(after));
  PUSH(RSCRATCH2);
  FixupBranch skip_exit = CALL();
  POP(RSCRATCH2);
  JustWriteExit(after, false, 0);
  SetJumpTarget(skip_exit);
}

void Jit64::WriteExit(u32 destination, bool bl, u32 after)
{
  if (!m_enable_blr_optimization)
    bl = false;

  Cleanup();

  if (bl)
  {
    MOV(32, R(RSCRATCH2), Imm32(after));
    PUSH(RSCRATCH2);
  }

  SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));

  JustWriteExit(destination, bl, after);
}

void Jit64::JustWriteExit(u32 destination, bool bl, u32 after)
{
  // If nobody has taken care of this yet (this can be removed when all branches are done)
  JitBlock* b = js.curBlock;
  JitBlock::LinkData linkData;
  linkData.exitAddress = destination;
  linkData.linkStatus = false;
  linkData.call = bl;

  MOV(32, PPCSTATE(pc), Imm32(destination));

  // Do not skip breakpoint check if debugging.
  const u8* dispatcher = SConfig::GetInstance().bEnableDebugging ? asm_routines.dispatcher :
                                                                   asm_routines.dispatcher_no_check;

  // Perform downcount flag check, followed by the requested exit
  if (bl)
  {
    FixupBranch do_timing = J_CC(CC_LE, true);
    SwitchToFarCode();
    SetJumpTarget(do_timing);
    CALL(asm_routines.do_timing);
    FixupBranch after_fixup = J(true);
    SwitchToNearCode();

    linkData.exitPtrs = GetWritableCodePtr();
    CALL(dispatcher);

    SetJumpTarget(after_fixup);
    POP(RSCRATCH);
    JustWriteExit(after, false, 0);
  }
  else
  {
    J_CC(CC_LE, asm_routines.do_timing);

    linkData.exitPtrs = GetWritableCodePtr();
    JMP(dispatcher, true);
  }

  b->linkData.push_back(linkData);
}

void Jit64::WriteExitDestInRSCRATCH(bool bl, u32 after)
{
  if (!m_enable_blr_optimization)
    bl = false;
  MOV(32, PPCSTATE(pc), R(RSCRATCH));
  Cleanup();

  if (bl)
  {
    MOV(32, R(RSCRATCH2), Imm32(after));
    PUSH(RSCRATCH2);
  }

  SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
  if (bl)
  {
    CALL(asm_routines.dispatcher);
    POP(RSCRATCH);
    JustWriteExit(after, false, 0);
  }
  else
  {
    JMP(asm_routines.dispatcher, true);
  }
}

void Jit64::WriteBLRExit()
{
  if (!m_enable_blr_optimization)
  {
    WriteExitDestInRSCRATCH();
    return;
  }
  MOV(32, PPCSTATE(pc), R(RSCRATCH));
  bool disturbed = Cleanup();
  if (disturbed)
    MOV(32, R(RSCRATCH), PPCSTATE(pc));
  MOV(32, R(RSCRATCH2), Imm32(js.downcountAmount));
  CMP(64, R(RSCRATCH), MDisp(RSP, 8));
  J_CC(CC_NE, asm_routines.dispatcher_mispredicted_blr);
  SUB(32, PPCSTATE(downcount), R(RSCRATCH2));
  RET();
}

void Jit64::WriteRfiExitDestInRSCRATCH()
{
  MOV(32, PPCSTATE(pc), R(RSCRATCH));
  MOV(32, PPCSTATE(npc), R(RSCRATCH));
  Cleanup();
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(PowerPC::CheckExceptions);
  ABI_PopRegistersAndAdjustStack({}, 0);
  SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
  JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteIdleExit(u32 destination)
{
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(CoreTiming::Idle);
  ABI_PopRegistersAndAdjustStack({}, 0);
  MOV(32, PPCSTATE(pc), Imm32(destination));
  WriteExceptionExit();
}

void Jit64::WriteExceptionExit()
{
  Cleanup();
  MOV(32, R(RSCRATCH), PPCSTATE(pc));
  MOV(32, PPCSTATE(npc), R(RSCRATCH));
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(PowerPC::CheckExceptions);
  ABI_PopRegistersAndAdjustStack({}, 0);
  SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
  JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExternalExceptionExit()
{
  Cleanup();
  MOV(32, R(RSCRATCH), PPCSTATE(pc));
  MOV(32, PPCSTATE(npc), R(RSCRATCH));
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(PowerPC::CheckExternalExceptions);
  ABI_PopRegistersAndAdjustStack({}, 0);
  SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
  JMP(asm_routines.dispatcher, true);
}

void Jit64::Run()
{
  CompiledCode pExecAddr = (CompiledCode)asm_routines.enter_code;
  pExecAddr();
}

void Jit64::SingleStep()
{
  CompiledCode pExecAddr = (CompiledCode)asm_routines.enter_code;
  pExecAddr();
}

void Jit64::Trace()
{
  std::string regs;
  std::string fregs;

#ifdef JIT_LOG_GPR
  for (int i = 0; i < 32; i++)
  {
    regs += StringFromFormat("r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
  }
#endif

#ifdef JIT_LOG_FPR
  for (int i = 0; i < 32; i++)
  {
    fregs += StringFromFormat("f%02d: %016x ", i, riPS0(i));
  }
#endif

  DEBUG_LOG(DYNA_REC, "JIT64 PC: %08x SRR0: %08x SRR1: %08x FPSCR: %08x MSR: %08x LR: %08x %s %s",
            PC, SRR0, SRR1, FPSCR.Hex, MSR.Hex, PowerPC::ppcState.spr[8], regs.c_str(),
            fregs.c_str());
}

void Jit64::Jit(u32 em_address)
{
  if (m_cleanup_after_stackfault)
  {
    ClearCache();
    m_cleanup_after_stackfault = false;
#ifdef _WIN32
    // The stack is in an invalid state with no guard page, reset it.
    _resetstkoflw();
#endif
  }

  if (IsAlmostFull() || m_far_code.IsAlmostFull() || trampolines.IsAlmostFull() ||
      SConfig::GetInstance().bJITNoBlockCache)
  {
    if (!SConfig::GetInstance().bJITNoBlockCache)
    {
      const auto reason =
          IsAlmostFull() ? "main" : m_far_code.IsAlmostFull() ? "far" : "trampoline";
      WARN_LOG(POWERPC, "flushing %s code cache, please report if this happens a lot", reason);
    }
    ClearCache();
  }

  std::size_t block_size = m_code_buffer.size();

  if (SConfig::GetInstance().bEnableDebugging)
  {
    // We can link blocks as long as we are not single stepping and there are no breakpoints here
    EnableBlockLink();
    EnableOptimization();

    // Comment out the following to disable breakpoints (speed-up)
    if (!jo.profile_blocks)
    {
      if (CPU::IsStepping())
      {
        block_size = 1;

        // Do not link this block to other blocks While single stepping
        jo.enableBlocklink = false;
        analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
        analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE);
        analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE);
        analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
        analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
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
    NPC = nextPC;
    PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
    PowerPC::CheckExceptions();
    WARN_LOG(POWERPC, "ISI exception at 0x%08x", nextPC);
    return;
  }

  JitBlock* b = blocks.AllocateBlock(em_address);
  DoJit(em_address, b, nextPC);
  blocks.FinalizeBlock(*b, jo.enableBlocklink, code_block.m_physical_addresses);
}

u8* Jit64::DoJit(u32 em_address, JitBlock* b, u32 nextPC)
{
  js.firstFPInstructionFound = false;
  js.isLastInstruction = false;
  js.blockStart = em_address;
  js.fifoBytesSinceCheck = 0;
  js.mustCheckFifo = false;
  js.curBlock = b;
  js.numLoadStoreInst = 0;
  js.numFloatingPointInst = 0;

  // TODO: Test if this or AlignCode16 make a difference from GetCodePtr
  u8* const start = AlignCode4();
  b->checkedEntry = start;
  b->normalEntry = start;

  // Used to get a trace of the last few blocks before a crash, sometimes VERY useful
  if (ImHereDebug)
  {
    ABI_PushRegistersAndAdjustStack({}, 0);
    ABI_CallFunction(ImHere);
    ABI_PopRegistersAndAdjustStack({}, 0);
  }

  // Conditionally add profiling code.
  if (jo.profile_blocks)
  {
    // get start tic
    MOV(64, R(ABI_PARAM1), ImmPtr(&b->profile_data.ticStart));
    int offset = static_cast<int>(offsetof(JitBlock::ProfileData, runCount)) -
                 static_cast<int>(offsetof(JitBlock::ProfileData, ticStart));
    ADD(64, MDisp(ABI_PARAM1, offset), Imm8(1));
    ABI_CallFunction(QueryPerformanceCounter);
  }
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(NAN_CHECK)
  // should help logged stack-traces become more accurate
  MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
#endif

  // Start up the register allocators
  // They use the information in gpa/fpa to preload commonly used registers.
  gpr.Start();
  fpr.Start();

  js.downcountAmount = 0;
  js.skipInstructions = 0;
  js.carryFlagSet = false;
  js.carryFlagInverted = false;
  js.constantGqr.clear();

  // Assume that GQR values don't change often at runtime. Many paired-heavy games use largely float
  // loads and stores,
  // which are significantly faster when inlined (especially in MMU mode, where this lets them use
  // fastmem).
  if (js.pairedQuantizeAddresses.find(js.blockStart) == js.pairedQuantizeAddresses.end())
  {
    // If there are GQRs used but not set, we'll treat those as constant and optimize them
    BitSet8 gqr_static = ComputeStaticGQRs(code_block);
    if (gqr_static)
    {
      SwitchToFarCode();
      const u8* target = GetCodePtr();
      MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
      ABI_PushRegistersAndAdjustStack({}, 0);
      ABI_CallFunctionC(JitInterface::CompileExceptionCheck,
                        static_cast<u32>(JitInterface::ExceptionType::PairedQuantize));
      ABI_PopRegistersAndAdjustStack({}, 0);
      JMP(asm_routines.dispatcher_no_check, true);
      SwitchToNearCode();

      // Insert a check that the GQRs are still the value we expect at
      // the start of the block in case our guess turns out wrong.
      for (int gqr : gqr_static)
      {
        u32 value = GQR(gqr);
        js.constantGqr[gqr] = value;
        CMP_or_TEST(32, PPCSTATE(spr[SPR_GQR0 + gqr]), Imm32(value));
        J_CC(CC_NZ, target);
      }
    }
  }

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
    js.instructionNumber = i;
    js.instructionsLeft = (code_block.m_num_instructions - 1) - i;
    const GekkoOPInfo* opinfo = op.opinfo;
    js.downcountAmount += opinfo->numCycles;
    js.fastmemLoadStore = nullptr;
    js.fixupExceptionHandler = false;

    if (!SConfig::GetInstance().bEnableDebugging)
      js.downcountAmount += PatchEngine::GetSpeedhackCycles(js.compilerPC);

    if (i == (code_block.m_num_instructions - 1))
    {
      js.isLastInstruction = true;
    }

    // Gather pipe writes using a non-immediate address are discovered by profiling.
    bool gatherPipeIntCheck = js.fifoWriteAddresses.find(op.address) != js.fifoWriteAddresses.end();

    // Gather pipe writes using an immediate address are explicitly tracked.
    if (jo.optimizeGatherPipe && (js.fifoBytesSinceCheck >= 32 || js.mustCheckFifo))
    {
      js.fifoBytesSinceCheck = 0;
      js.mustCheckFifo = false;
      BitSet32 registersInUse = CallerSavedRegistersInUse();
      ABI_PushRegistersAndAdjustStack(registersInUse, 0);
      ABI_CallFunction(GPFifo::FastCheckGatherPipe);
      ABI_PopRegistersAndAdjustStack(registersInUse, 0);
      gatherPipeIntCheck = true;
    }

    // Gather pipe writes can generate an exception; add an exception check.
    // TODO: This doesn't really match hardware; the CP interrupt is
    // asynchronous.
    if (gatherPipeIntCheck)
    {
      TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT));
      FixupBranch extException = J_CC(CC_NZ, true);

      SwitchToFarCode();
      SetJumpTarget(extException);
      TEST(32, PPCSTATE(msr), Imm32(0x0008000));
      FixupBranch noExtIntEnable = J_CC(CC_Z, true);
      MOV(64, R(RSCRATCH), ImmPtr(&ProcessorInterface::m_InterruptCause));
      TEST(32, MatR(RSCRATCH),
           Imm32(ProcessorInterface::INT_CAUSE_CP | ProcessorInterface::INT_CAUSE_PE_TOKEN |
                 ProcessorInterface::INT_CAUSE_PE_FINISH));
      FixupBranch noCPInt = J_CC(CC_Z, true);

      {
        RCForkGuard gpr_guard = gpr.Fork();
        RCForkGuard fpr_guard = fpr.Fork();

        gpr.Flush();
        fpr.Flush();

        MOV(32, PPCSTATE(pc), Imm32(op.address));
        WriteExternalExceptionExit();
      }
      SwitchToNearCode();
      SetJumpTarget(noCPInt);
      SetJumpTarget(noExtIntEnable);
    }

    if (HandleFunctionHooking(op.address))
      break;

    if (!op.skip)
    {
      if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
      {
        // This instruction uses FPU - needs to add FP exception bailout
        TEST(32, PPCSTATE(msr), Imm32(1 << 13));  // Test FP enabled bit
        FixupBranch b1 = J_CC(CC_Z, true);

        SwitchToFarCode();
        SetJumpTarget(b1);
        {
          RCForkGuard gpr_guard = gpr.Fork();
          RCForkGuard fpr_guard = fpr.Fork();

          gpr.Flush();
          fpr.Flush();

          // If a FPU exception occurs, the exception handler will read
          // from PC.  Update PC with the latest value in case that happens.
          MOV(32, PPCSTATE(pc), Imm32(op.address));
          OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
          WriteExceptionExit();
        }
        SwitchToNearCode();

        js.firstFPInstructionFound = true;
      }

      if (SConfig::GetInstance().bEnableDebugging && breakpoints.IsAddressBreakPoint(op.address) &&
          !CPU::IsStepping())
      {
        // Turn off block linking if there are breakpoints so that the Step Over command does not
        // link this block.
        jo.enableBlocklink = false;

        gpr.Flush();
        fpr.Flush();

        MOV(32, PPCSTATE(pc), Imm32(op.address));
        ABI_PushRegistersAndAdjustStack({}, 0);
        ABI_CallFunction(PowerPC::CheckBreakPoints);
        ABI_PopRegistersAndAdjustStack({}, 0);
        MOV(64, R(RSCRATCH), ImmPtr(CPU::GetStatePtr()));
        TEST(32, MatR(RSCRATCH), Imm32(0xFFFFFFFF));
        FixupBranch noBreakpoint = J_CC(CC_Z);

        WriteExit(op.address);
        SetJumpTarget(noBreakpoint);
      }

      // If we have an input register that is going to be used again, load it pre-emptively,
      // even if the instruction doesn't strictly need it in a register, to avoid redundant
      // loads later. Of course, don't do this if we're already out of registers.
      // As a bit of a heuristic, make sure we have at least one register left over for the
      // output, which needs to be bound in the actual instruction compilation.
      // TODO: make this smarter in the case that we're actually register-starved, i.e.
      // prioritize the more important registers.
      gpr.PreloadRegisters(op.regsIn & op.gprInReg);
      fpr.PreloadRegisters(op.fregsIn & op.fprInXmm);

      CompileInstruction(op);

      if (jo.memcheck && (opinfo->flags & FL_LOADSTORE))
      {
        // If we have a fastmem loadstore, we can omit the exception check and let fastmem handle
        // it.
        FixupBranch memException;
        ASSERT_MSG(DYNA_REC, !(js.fastmemLoadStore && js.fixupExceptionHandler),
                   "Fastmem loadstores shouldn't have exception handler fixups (PC=%x)!",
                   op.address);
        if (!js.fastmemLoadStore && !js.fixupExceptionHandler)
        {
          TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_DSI));
          memException = J_CC(CC_NZ, true);
        }

        SwitchToFarCode();
        if (!js.fastmemLoadStore)
        {
          m_exception_handler_at_loc[js.fastmemLoadStore] = nullptr;
          SetJumpTarget(js.fixupExceptionHandler ? js.exceptionHandler : memException);
        }
        else
        {
          m_exception_handler_at_loc[js.fastmemLoadStore] = GetWritableCodePtr();
        }

        RCForkGuard gpr_guard = gpr.Fork();
        RCForkGuard fpr_guard = fpr.Fork();

        gpr.Revert();
        fpr.Revert();
        gpr.Flush();
        fpr.Flush();

        MOV(32, PPCSTATE(pc), Imm32(op.address));
        WriteExceptionExit();
        SwitchToNearCode();
      }

      gpr.Commit();
      fpr.Commit();

      // If we have a register that will never be used again, flush it.
      gpr.Flush(~op.gprInUse);
      fpr.Flush(~op.fprInUse);

      if (opinfo->flags & FL_LOADSTORE)
        ++js.numLoadStoreInst;

      if (opinfo->flags & FL_USE_FPU)
        ++js.numFloatingPointInst;
    }

#if defined(_DEBUG) || defined(DEBUGFAST)
    if (!gpr.SanityCheck() || !fpr.SanityCheck())
    {
      std::string ppc_inst = Common::GekkoDisassembler::Disassemble(op.inst.hex, em_address);
      NOTICE_LOG(DYNA_REC, "Unflushed register: %s", ppc_inst.c_str());
    }
#endif
    i += js.skipInstructions;
    js.skipInstructions = 0;
  }

  if (code_block.m_broken)
  {
    gpr.Flush();
    fpr.Flush();
    WriteExit(nextPC);
  }

  b->codeSize = (u32)(GetCodePtr() - start);
  b->originalSize = code_block.m_num_instructions;

#ifdef JIT_LOG_GENERATED_CODE
  LogGeneratedX86(code_block.m_num_instructions, m_code_buffer, start, b);
#endif

  return start;
}

BitSet8 Jit64::ComputeStaticGQRs(const PPCAnalyst::CodeBlock& cb) const
{
  return cb.m_gqr_used & ~cb.m_gqr_modified;
}

BitSet32 Jit64::CallerSavedRegistersInUse() const
{
  BitSet32 in_use = gpr.RegistersInUse() | (fpr.RegistersInUse() << 16);
  return in_use & ABI_ALL_CALLER_SAVED;
}

void Jit64::EnableBlockLink()
{
  jo.enableBlocklink = true;
  if (SConfig::GetInstance().bJITNoBlockLinking)
    jo.enableBlocklink = false;
}

void Jit64::EnableOptimization()
{
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
  analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_FOLLOW);
}

void Jit64::IntializeSpeculativeConstants()
{
  // If the block depends on an input register which looks like a gather pipe or MMIO related
  // constant, guess that it is actually a constant input, and specialize the block based on this
  // assumption. This happens when there are branches in code writing to the gather pipe, but only
  // the first block loads the constant.
  // Insert a check at the start of the block to verify that the value is actually constant.
  // This can save a lot of backpatching and optimize gather pipe writes in more places.
  const u8* target = nullptr;
  for (auto i : code_block.m_gpr_inputs)
  {
    u32 compileTimeValue = PowerPC::ppcState.gpr[i];
    if (PowerPC::IsOptimizableGatherPipeWrite(compileTimeValue) ||
        PowerPC::IsOptimizableGatherPipeWrite(compileTimeValue - 0x8000) ||
        compileTimeValue == 0xCC000000)
    {
      if (!target)
      {
        SwitchToFarCode();
        target = GetCodePtr();
        MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
        ABI_PushRegistersAndAdjustStack({}, 0);
        ABI_CallFunctionC(JitInterface::CompileExceptionCheck,
                          static_cast<u32>(JitInterface::ExceptionType::SpeculativeConstants));
        ABI_PopRegistersAndAdjustStack({}, 0);
        JMP(asm_routines.dispatcher, true);
        SwitchToNearCode();
      }
      CMP(32, PPCSTATE(gpr[i]), Imm32(compileTimeValue));
      J_CC(CC_NZ, target);
      gpr.SetImmediate32(i, compileTimeValue, false);
    }
  }
}

bool Jit64::HandleFunctionHooking(u32 address)
{
  return HLE::ReplaceFunctionIfPossible(address, [&](u32 function, HLE::HookType type) {
    HLEFunction(function);

    if (type != HLE::HookType::Replace)
      return false;

    MOV(32, R(RSCRATCH), PPCSTATE(npc));
    js.downcountAmount += js.st.numCycles;
    WriteExitDestInRSCRATCH();
    return true;
  });
}

void LogGeneratedX86(size_t size, const PPCAnalyst::CodeBuffer& code_buffer, const u8* normalEntry,
                     const JitBlock* b)
{
  for (size_t i = 0; i < size; i++)
  {
    const PPCAnalyst::CodeOp& op = code_buffer[i];
    const std::string disasm = Common::GekkoDisassembler::Disassemble(op.inst.hex, op.address);
    DEBUG_LOG(DYNA_REC, "IR_X86 PPC: %08x %s\n", op.address, disasm.c_str());
  }

  disassembler x64disasm;
  x64disasm.set_syntax_intel();

  u64 disasmPtr = reinterpret_cast<u64>(normalEntry);
  const u8* end = normalEntry + b->codeSize;

  while (reinterpret_cast<u8*>(disasmPtr) < end)
  {
    char sptr[1000] = "";
    disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, reinterpret_cast<u8*>(disasmPtr), sptr);
    DEBUG_LOG(DYNA_REC, "IR_X86 x86: %s", sptr);
  }

  if (b->codeSize <= 250)
  {
    std::stringstream ss;
    ss << std::hex;
    for (u8 i = 0; i <= b->codeSize; i++)
    {
      ss.width(2);
      ss.fill('0');
      ss << static_cast<u32>(*(normalEntry + i));
    }
    DEBUG_LOG(DYNA_REC, "IR_X86 bin: %s\n\n\n", ss.str().c_str());
  }
}
