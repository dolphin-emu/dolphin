// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64Common/EmuCodeBlock.h"

#include <functional>
#include <limits>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/FloatUtils.h"
#include "Common/Intrinsics.h"
#include "Common/Swap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Gen;

namespace
{
OpArg SwapImmediate(int access_size, const OpArg& reg_value)
{
  if (access_size == 32)
    return Imm32(Common::swap32(reg_value.Imm32()));

  if (access_size == 16)
    return Imm16(Common::swap16(reg_value.Imm16()));

  return Imm8(reg_value.Imm8());
}

OpArg FixImmediate(int access_size, OpArg arg)
{
  if (arg.IsImm())
  {
    arg = access_size == 8 ? arg.AsImm8() : access_size == 16 ? arg.AsImm16() : arg.AsImm32();
  }
  return arg;
}
}  // Anonymous namespace

void EmuCodeBlock::MemoryExceptionCheck()
{
  // TODO: We really should untangle the trampolines, exception handlers and
  // memory checks.

  // If we are currently generating a trampoline for a failed fastmem
  // load/store, the trampoline generator will have stashed the exception
  // handler (that we previously generated after the fastmem instruction) in
  // trampolineExceptionHandler.
  auto& js = m_jit.js;
  if (js.generatingTrampoline)
  {
    if (js.trampolineExceptionHandler)
    {
      TEST(32, PPCSTATE(Exceptions), Gen::Imm32(EXCEPTION_DSI));
      J_CC(CC_NZ, js.trampolineExceptionHandler);
    }
    return;
  }

  // If memcheck (ie: MMU) mode is enabled and we haven't generated an
  // exception handler for this instruction yet, we will generate an
  // exception check.
  if (m_jit.jo.memcheck && !js.fastmemLoadStore && !js.fixupExceptionHandler)
  {
    TEST(32, PPCSTATE(Exceptions), Gen::Imm32(EXCEPTION_DSI));
    js.exceptionHandler = J_CC(Gen::CC_NZ, Jump::Near);
    js.fixupExceptionHandler = true;
  }
}

void EmuCodeBlock::SwitchToFarCode()
{
  m_near_code = GetWritableCodePtr();
  m_near_code_end = GetWritableCodeEnd();
  m_near_code_write_failed = HasWriteFailed();
  SetCodePtr(m_far_code.GetWritableCodePtr(), m_far_code.GetWritableCodeEnd(),
             m_far_code.HasWriteFailed());
}

void EmuCodeBlock::SwitchToNearCode()
{
  m_far_code.SetCodePtr(GetWritableCodePtr(), GetWritableCodeEnd(), HasWriteFailed());
  SetCodePtr(m_near_code, m_near_code_end, m_near_code_write_failed);
}

FixupBranch EmuCodeBlock::BATAddressLookup(X64Reg addr, X64Reg tmp, const void* bat_table)
{
  MOV(64, R(tmp), ImmPtr(bat_table));
  SHR(32, R(addr), Imm8(PowerPC::BAT_INDEX_SHIFT));
  MOV(32, R(addr), MComplex(tmp, addr, SCALE_4, 0));
  BT(32, R(addr), Imm8(MathUtil::IntLog2(PowerPC::BAT_MAPPED_BIT)));

  return J_CC(CC_NC, m_far_code.Enabled() ? Jump::Near : Jump::Short);
}

FixupBranch EmuCodeBlock::CheckIfSafeAddress(const OpArg& reg_value, X64Reg reg_addr,
                                             BitSet32 registers_in_use)
{
  registers_in_use[reg_addr] = true;
  if (reg_value.IsSimpleReg())
    registers_in_use[reg_value.GetSimpleReg()] = true;

  // Get ourselves two free registers
  if (registers_in_use[RSCRATCH])
    PUSH(RSCRATCH);
  if (registers_in_use[RSCRATCH_EXTRA])
    PUSH(RSCRATCH_EXTRA);

  if (reg_addr != RSCRATCH_EXTRA)
    MOV(32, R(RSCRATCH_EXTRA), R(reg_addr));

  // Perform lookup to see if we can use fast path.
  MOV(64, R(RSCRATCH), ImmPtr(m_jit.m_mmu.GetDBATTable().data()));
  SHR(32, R(RSCRATCH_EXTRA), Imm8(PowerPC::BAT_INDEX_SHIFT));
  TEST(32, MComplex(RSCRATCH, RSCRATCH_EXTRA, SCALE_4, 0), Imm32(PowerPC::BAT_PHYSICAL_BIT));

  if (registers_in_use[RSCRATCH_EXTRA])
    POP(RSCRATCH_EXTRA);
  if (registers_in_use[RSCRATCH])
    POP(RSCRATCH);

  return J_CC(CC_Z, m_far_code.Enabled() ? Jump::Near : Jump::Short);
}

void EmuCodeBlock::UnsafeWriteRegToReg(OpArg reg_value, X64Reg reg_addr, int accessSize, s32 offset,
                                       bool swap, MovInfo* info)
{
  if (info)
  {
    info->address = GetWritableCodePtr();
    info->nonAtomicSwapStore = false;
  }

  OpArg dest = MComplex(RMEM, reg_addr, SCALE_1, offset);
  if (reg_value.IsImm())
  {
    if (swap)
      reg_value = SwapImmediate(accessSize, reg_value);
    MOV(accessSize, dest, reg_value);
  }
  else if (swap)
  {
    SwapAndStore(accessSize, dest, reg_value.GetSimpleReg(), info);
  }
  else
  {
    MOV(accessSize, dest, reg_value);
  }
}

void EmuCodeBlock::UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize,
                                       s32 offset, bool swap, Gen::MovInfo* info)
{
  UnsafeWriteRegToReg(R(reg_value), reg_addr, accessSize, offset, swap, info);
}

bool EmuCodeBlock::UnsafeLoadToReg(X64Reg reg_value, OpArg opAddress, int accessSize, s32 offset,
                                   bool signExtend, MovInfo* info)
{
  bool offsetAddedToAddress = false;
  OpArg memOperand;
  if (opAddress.IsSimpleReg())
  {
    // Deal with potential wraparound.  (This is just a heuristic, and it would
    // be more correct to actually mirror the first page at the end, but the
    // only case where it probably actually matters is JitIL turning adds into
    // offsets with the wrong sign, so whatever.  Since the original code
    // *could* try to wrap an address around, however, this is the correct
    // place to address the issue.)
    if ((u32)offset >= 0x1000)
    {
      // This method can potentially clobber the address if it shares a register
      // with the load target. In this case we can just subtract offset from the
      // register (see Jit64 for this implementation).
      offsetAddedToAddress = (reg_value == opAddress.GetSimpleReg());

      LEA(32, reg_value, MDisp(opAddress.GetSimpleReg(), offset));
      opAddress = R(reg_value);
      offset = 0;
    }
    memOperand = MComplex(RMEM, opAddress.GetSimpleReg(), SCALE_1, offset);
  }
  else if (opAddress.IsImm())
  {
    MOV(32, R(reg_value), Imm32((u32)(opAddress.Imm32() + offset)));
    memOperand = MRegSum(RMEM, reg_value);
  }
  else
  {
    MOV(32, R(reg_value), opAddress);
    memOperand = MComplex(RMEM, reg_value, SCALE_1, offset);
  }

  LoadAndSwap(accessSize, reg_value, memOperand, signExtend, info);
  return offsetAddedToAddress;
}

// Visitor that generates code to read a MMIO value.
template <typename T>
class MMIOReadCodeGenerator : public MMIO::ReadHandlingMethodVisitor<T>
{
public:
  MMIOReadCodeGenerator(Core::System* system, Gen::X64CodeBlock* code, BitSet32 registers_in_use,
                        Gen::X64Reg dst_reg, u32 address, bool sign_extend)
      : m_system(system), m_code(code), m_registers_in_use(registers_in_use), m_dst_reg(dst_reg),
        m_address(address), m_sign_extend(sign_extend)
  {
  }

  void VisitConstant(T value) override { LoadConstantToReg(8 * sizeof(T), value); }
  void VisitDirect(const T* addr, u32 mask) override
  {
    LoadAddrMaskToReg(8 * sizeof(T), addr, mask);
  }
  void VisitComplex(const std::function<T(Core::System&, u32)>* lambda) override
  {
    CallLambda(8 * sizeof(T), lambda);
  }

private:
  // Generates code to load a constant to the destination register. In
  // practice it would be better to avoid using a register for this, but it
  // would require refactoring a lot of JIT code.
  void LoadConstantToReg(int sbits, u32 value)
  {
    if (m_sign_extend)
    {
      u32 sign = !!(value & (1 << (sbits - 1)));
      value |= sign * ((0xFFFFFFFF >> sbits) << sbits);
    }
    m_code->MOV(32, R(m_dst_reg), Gen::Imm32(value));
  }

  // Generate the proper MOV instruction depending on whether the read should
  // be sign extended or zero extended.
  void MoveOpArgToReg(int sbits, const Gen::OpArg& arg)
  {
    if (m_sign_extend)
      m_code->MOVSX(32, sbits, m_dst_reg, arg);
    else
      m_code->MOVZX(32, sbits, m_dst_reg, arg);
  }

  void LoadAddrMaskToReg(int sbits, const void* ptr, u32 mask)
  {
    m_code->MOV(64, R(RSCRATCH), ImmPtr(ptr));
    // If we do not need to mask, we can do the sign extend while loading
    // from memory. If masking is required, we have to first zero extend,
    // then mask, then sign extend if needed (1 instr vs. 2/3).
    u32 all_ones = (1ULL << sbits) - 1;
    if ((all_ones & mask) == all_ones)
    {
      MoveOpArgToReg(sbits, MatR(RSCRATCH));
    }
    else
    {
      m_code->MOVZX(32, sbits, m_dst_reg, MatR(RSCRATCH));
      m_code->AND(32, R(m_dst_reg), Imm32(mask));
      if (m_sign_extend)
        m_code->MOVSX(32, sbits, m_dst_reg, R(m_dst_reg));
    }
  }

  void CallLambda(int sbits, const std::function<T(Core::System&, u32)>* lambda)
  {
    m_code->ABI_PushRegistersAndAdjustStack(m_registers_in_use, 0);
    m_code->ABI_CallLambdaPC(lambda, m_system, m_address);
    m_code->ABI_PopRegistersAndAdjustStack(m_registers_in_use, 0);
    MoveOpArgToReg(sbits, R(ABI_RETURN));
  }

  Core::System* m_system;
  Gen::X64CodeBlock* m_code;
  BitSet32 m_registers_in_use;
  Gen::X64Reg m_dst_reg;
  u32 m_address;
  bool m_sign_extend;
};

void EmuCodeBlock::MMIOLoadToReg(MMIO::Mapping* mmio, Gen::X64Reg reg_value,
                                 BitSet32 registers_in_use, u32 address, int access_size,
                                 bool sign_extend)
{
  switch (access_size)
  {
  case 8:
  {
    MMIOReadCodeGenerator<u8> gen(&m_jit.m_system, this, registers_in_use, reg_value, address,
                                  sign_extend);
    mmio->GetHandlerForRead<u8>(address).Visit(gen);
    break;
  }
  case 16:
  {
    MMIOReadCodeGenerator<u16> gen(&m_jit.m_system, this, registers_in_use, reg_value, address,
                                   sign_extend);
    mmio->GetHandlerForRead<u16>(address).Visit(gen);
    break;
  }
  case 32:
  {
    MMIOReadCodeGenerator<u32> gen(&m_jit.m_system, this, registers_in_use, reg_value, address,
                                   sign_extend);
    mmio->GetHandlerForRead<u32>(address).Visit(gen);
    break;
  }
  }
}

void EmuCodeBlock::SafeLoadToReg(X64Reg reg_value, const Gen::OpArg& opAddress, int accessSize,
                                 s32 offset, BitSet32 registersInUse, bool signExtend, int flags)
{
  bool force_slow_access = (flags & SAFE_LOADSTORE_FORCE_SLOW_ACCESS) != 0;

  auto& js = m_jit.js;
  registersInUse[reg_value] = false;
  if (m_jit.jo.fastmem && !(flags & (SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_UPDATE_PC)) &&
      !force_slow_access)
  {
    u8* backpatchStart = GetWritableCodePtr();
    MovInfo mov;
    bool offsetAddedToAddress =
        UnsafeLoadToReg(reg_value, opAddress, accessSize, offset, signExtend, &mov);
    TrampolineInfo& info = m_back_patch_info[mov.address];
    info.pc = js.compilerPC;
    info.nonAtomicSwapStoreSrc = mov.nonAtomicSwapStore ? mov.nonAtomicSwapStoreSrc : INVALID_REG;
    info.start = backpatchStart;
    info.read = true;
    info.op_reg = reg_value;
    info.op_arg = opAddress;
    info.offsetAddedToAddress = offsetAddedToAddress;
    info.accessSize = accessSize >> 3;
    info.offset = offset;
    info.registersInUse = registersInUse;
    info.flags = flags;
    info.signExtend = signExtend;
    ptrdiff_t padding = BACKPATCH_SIZE - (GetCodePtr() - backpatchStart);
    if (padding > 0)
    {
      NOP(padding);
    }
    info.len = static_cast<u32>(GetCodePtr() - info.start);

    js.fastmemLoadStore = mov.address;
    return;
  }

  if (opAddress.IsImm())
  {
    u32 address = opAddress.Imm32() + offset;
    SafeLoadToRegImmediate(reg_value, address, accessSize, registersInUse, signExtend);
    return;
  }

  ASSERT_MSG(DYNA_REC, opAddress.IsSimpleReg(),
             "Incorrect use of SafeLoadToReg (address isn't register or immediate)");
  X64Reg reg_addr = opAddress.GetSimpleReg();
  if (offset)
  {
    reg_addr = RSCRATCH;
    LEA(32, RSCRATCH, MDisp(opAddress.GetSimpleReg(), offset));
  }

  FixupBranch exit;
  const bool dr_set =
      (flags & SAFE_LOADSTORE_DR_ON) || (m_jit.m_ppc_state.feature_flags & FEATURE_FLAG_MSR_DR);
  const bool fast_check_address =
      !force_slow_access && dr_set && m_jit.jo.fastmem_arena && !m_jit.m_ppc_state.m_enable_dcache;
  if (fast_check_address)
  {
    FixupBranch slow = CheckIfSafeAddress(R(reg_value), reg_addr, registersInUse);
    UnsafeLoadToReg(reg_value, R(reg_addr), accessSize, 0, signExtend);
    if (m_far_code.Enabled())
      SwitchToFarCode();
    else
      exit = J(Jump::Near);
    SetJumpTarget(slow);
  }

  // PC is used by memory watchpoints (if enabled), profiling where to insert gather pipe
  // interrupt checks, and printing accurate PC locations in debug logs.
  //
  // In the case of Jit64AsmCommon routines, we don't know the PC here,
  // so the caller has to store the PC themselves.
  if (!(flags & SAFE_LOADSTORE_NO_UPDATE_PC))
  {
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
  }

  size_t rsp_alignment = (flags & SAFE_LOADSTORE_NO_PROLOG) ? 8 : 0;
  ABI_PushRegistersAndAdjustStack(registersInUse, rsp_alignment);
  switch (accessSize)
  {
  case 64:
    ABI_CallFunctionPR(PowerPC::ReadU64FromJit, &m_jit.m_mmu, reg_addr);
    break;
  case 32:
    ABI_CallFunctionPR(PowerPC::ReadU32FromJit, &m_jit.m_mmu, reg_addr);
    break;
  case 16:
    ABI_CallFunctionPR(PowerPC::ReadU16FromJit, &m_jit.m_mmu, reg_addr);
    break;
  case 8:
    ABI_CallFunctionPR(PowerPC::ReadU8FromJit, &m_jit.m_mmu, reg_addr);
    break;
  }
  ABI_PopRegistersAndAdjustStack(registersInUse, rsp_alignment);

  MemoryExceptionCheck();
  if (signExtend && accessSize < 32)
  {
    // Need to sign extend values coming from the Read_U* functions.
    MOVSX(32, accessSize, reg_value, R(ABI_RETURN));
  }
  else if (reg_value != ABI_RETURN)
  {
    MOVZX(64, accessSize, reg_value, R(ABI_RETURN));
  }

  if (fast_check_address)
  {
    if (m_far_code.Enabled())
    {
      exit = J(Jump::Near);
      SwitchToNearCode();
    }
    SetJumpTarget(exit);
  }
}

void EmuCodeBlock::SafeLoadToRegImmediate(X64Reg reg_value, u32 address, int accessSize,
                                          BitSet32 registersInUse, bool signExtend)
{
  // If the address is known to be RAM, just load it directly.
  if (m_jit.jo.fastmem_arena && m_jit.m_mmu.IsOptimizableRAMAddress(address))
  {
    UnsafeLoadToReg(reg_value, Imm32(address), accessSize, 0, signExtend);
    return;
  }

  // If the address maps to an MMIO register, inline MMIO read code.
  u32 mmioAddress = m_jit.m_mmu.IsOptimizableMMIOAccess(address, accessSize);
  if (accessSize != 64 && mmioAddress)
  {
    auto& memory = m_jit.m_system.GetMemory();
    MMIOLoadToReg(memory.GetMMIOMapping(), reg_value, registersInUse, mmioAddress, accessSize,
                  signExtend);
    return;
  }

  // Helps external systems know which instruction triggered the read.
  MOV(32, PPCSTATE(pc), Imm32(m_jit.js.compilerPC));

  // Fall back to general-case code.
  ABI_PushRegistersAndAdjustStack(registersInUse, 0);
  switch (accessSize)
  {
  case 64:
    ABI_CallFunctionPC(PowerPC::ReadU64FromJit, &m_jit.m_mmu, address);
    break;
  case 32:
    ABI_CallFunctionPC(PowerPC::ReadU32FromJit, &m_jit.m_mmu, address);
    break;
  case 16:
    ABI_CallFunctionPC(PowerPC::ReadU16FromJit, &m_jit.m_mmu, address);
    break;
  case 8:
    ABI_CallFunctionPC(PowerPC::ReadU8FromJit, &m_jit.m_mmu, address);
    break;
  }
  ABI_PopRegistersAndAdjustStack(registersInUse, 0);

  MemoryExceptionCheck();
  if (signExtend && accessSize < 32)
  {
    // Need to sign extend values coming from the Read_U* functions.
    MOVSX(32, accessSize, reg_value, R(ABI_RETURN));
  }
  else if (reg_value != ABI_RETURN)
  {
    MOVZX(64, accessSize, reg_value, R(ABI_RETURN));
  }
}

void EmuCodeBlock::SafeWriteRegToReg(OpArg reg_value, X64Reg reg_addr, int accessSize, s32 offset,
                                     BitSet32 registersInUse, int flags)
{
  bool swap = !(flags & SAFE_LOADSTORE_NO_SWAP);
  bool force_slow_access = (flags & SAFE_LOADSTORE_FORCE_SLOW_ACCESS) != 0;

  // set the correct immediate format
  reg_value = FixImmediate(accessSize, reg_value);

  auto& js = m_jit.js;
  if (m_jit.jo.fastmem && !(flags & (SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_UPDATE_PC)) &&
      !force_slow_access)
  {
    u8* backpatchStart = GetWritableCodePtr();
    MovInfo mov;
    UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, offset, swap, &mov);
    TrampolineInfo& info = m_back_patch_info[mov.address];
    info.pc = js.compilerPC;
    info.nonAtomicSwapStoreSrc = mov.nonAtomicSwapStore ? mov.nonAtomicSwapStoreSrc : INVALID_REG;
    info.start = backpatchStart;
    info.read = false;
    info.op_arg = reg_value;
    info.op_reg = reg_addr;
    info.offsetAddedToAddress = false;
    info.accessSize = accessSize >> 3;
    info.offset = offset;
    info.registersInUse = registersInUse;
    info.flags = flags;
    ptrdiff_t padding = BACKPATCH_SIZE - (GetCodePtr() - backpatchStart);
    if (padding > 0)
    {
      NOP(padding);
    }
    info.len = static_cast<u32>(GetCodePtr() - info.start);

    js.fastmemLoadStore = mov.address;

    return;
  }

  if (offset)
  {
    if (flags & SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR)
    {
      LEA(32, RSCRATCH, MDisp(reg_addr, (u32)offset));
      reg_addr = RSCRATCH;
    }
    else
    {
      ADD(32, R(reg_addr), Imm32((u32)offset));
    }
  }

  FixupBranch exit;
  const bool dr_set =
      (flags & SAFE_LOADSTORE_DR_ON) || (m_jit.m_ppc_state.feature_flags & FEATURE_FLAG_MSR_DR);
  const bool fast_check_address =
      !force_slow_access && dr_set && m_jit.jo.fastmem_arena && !m_jit.m_ppc_state.m_enable_dcache;
  if (fast_check_address)
  {
    FixupBranch slow = CheckIfSafeAddress(reg_value, reg_addr, registersInUse);
    UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
    if (m_far_code.Enabled())
      SwitchToFarCode();
    else
      exit = J(Jump::Near);
    SetJumpTarget(slow);
  }

  // PC is used by memory watchpoints (if enabled), profiling where to insert gather pipe
  // interrupt checks, and printing accurate PC locations in debug logs.
  //
  // In the case of Jit64AsmCommon routines, we don't know the PC here,
  // so the caller has to store the PC themselves.
  if (!(flags & SAFE_LOADSTORE_NO_UPDATE_PC))
  {
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
  }

  size_t rsp_alignment = (flags & SAFE_LOADSTORE_NO_PROLOG) ? 8 : 0;
  ABI_PushRegistersAndAdjustStack(registersInUse, rsp_alignment);

  // If the input is an immediate, we need to put it in a register.
  X64Reg reg;
  if (reg_value.IsImm())
  {
    reg = reg_addr == ABI_PARAM1 ? RSCRATCH : ABI_PARAM1;
    MOV(accessSize, R(reg), reg_value);
  }
  else
  {
    reg = reg_value.GetSimpleReg();
  }

  switch (accessSize)
  {
  case 64:
    ABI_CallFunctionPRR(swap ? PowerPC::WriteU64FromJit : PowerPC::WriteU64SwapFromJit,
                        &m_jit.m_mmu, reg, reg_addr);
    break;
  case 32:
    ABI_CallFunctionPRR(swap ? PowerPC::WriteU32FromJit : PowerPC::WriteU32SwapFromJit,
                        &m_jit.m_mmu, reg, reg_addr);
    break;
  case 16:
    ABI_CallFunctionPRR(swap ? PowerPC::WriteU16FromJit : PowerPC::WriteU16SwapFromJit,
                        &m_jit.m_mmu, reg, reg_addr);
    break;
  case 8:
    ABI_CallFunctionPRR(PowerPC::WriteU8FromJit, &m_jit.m_mmu, reg, reg_addr);
    break;
  }
  ABI_PopRegistersAndAdjustStack(registersInUse, rsp_alignment);

  MemoryExceptionCheck();

  if (fast_check_address)
  {
    if (m_far_code.Enabled())
    {
      exit = J(Jump::Near);
      SwitchToNearCode();
    }
    SetJumpTarget(exit);
  }
}

void EmuCodeBlock::SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize,
                                     s32 offset, BitSet32 registersInUse, int flags)
{
  SafeWriteRegToReg(R(reg_value), reg_addr, accessSize, offset, registersInUse, flags);
}

bool EmuCodeBlock::WriteClobbersRegValue(int accessSize, bool swap)
{
  return swap && !cpu_info.bMOVBE && accessSize > 8;
}

bool EmuCodeBlock::WriteToConstAddress(int accessSize, OpArg arg, u32 address,
                                       BitSet32 registersInUse)
{
  arg = FixImmediate(accessSize, arg);

  // If we already know the address through constant folding, we can do some
  // fun tricks...
  if (m_jit.jo.optimizeGatherPipe && m_jit.m_mmu.IsOptimizableGatherPipeWrite(address))
  {
    X64Reg arg_reg = RSCRATCH;

    // With movbe, we can store inplace without temporary register
    if (arg.IsSimpleReg() && cpu_info.bMOVBE)
      arg_reg = arg.GetSimpleReg();

    if (!arg.IsSimpleReg(arg_reg))
      MOV(accessSize, R(arg_reg), arg);

    // And store it in the gather pipe
    MOV(64, R(RSCRATCH2), PPCSTATE(gather_pipe_ptr));
    SwapAndStore(accessSize, MatR(RSCRATCH2), arg_reg);
    ADD(64, R(RSCRATCH2), Imm8(accessSize >> 3));
    MOV(64, PPCSTATE(gather_pipe_ptr), R(RSCRATCH2));

    m_jit.js.fifoBytesSinceCheck += accessSize >> 3;
    return false;
  }
  else if (m_jit.jo.fastmem_arena && m_jit.m_mmu.IsOptimizableRAMAddress(address))
  {
    WriteToConstRamAddress(accessSize, arg, address);
    return false;
  }
  else
  {
    // Helps external systems know which instruction triggered the write
    MOV(32, PPCSTATE(pc), Imm32(m_jit.js.compilerPC));

    ABI_PushRegistersAndAdjustStack(registersInUse, 0);
    switch (accessSize)
    {
    case 64:
      ABI_CallFunctionPAC(64, PowerPC::WriteU64FromJit, &m_jit.m_mmu, arg, address);
      break;
    case 32:
      ABI_CallFunctionPAC(32, PowerPC::WriteU32FromJit, &m_jit.m_mmu, arg, address);
      break;
    case 16:
      ABI_CallFunctionPAC(16, PowerPC::WriteU16FromJit, &m_jit.m_mmu, arg, address);
      break;
    case 8:
      ABI_CallFunctionPAC(8, PowerPC::WriteU8FromJit, &m_jit.m_mmu, arg, address);
      break;
    }
    ABI_PopRegistersAndAdjustStack(registersInUse, 0);
    return true;
  }
}

void EmuCodeBlock::WriteToConstRamAddress(int accessSize, OpArg arg, u32 address, bool swap)
{
  X64Reg reg;
  if (arg.IsImm())
  {
    arg = SwapImmediate(accessSize, arg);
    MOV(32, R(RSCRATCH), Imm32(address));
    MOV(accessSize, MRegSum(RMEM, RSCRATCH), arg);
    return;
  }

  if (!arg.IsSimpleReg() || (!cpu_info.bMOVBE && swap && arg.GetSimpleReg() != RSCRATCH))
  {
    MOV(accessSize, R(RSCRATCH), arg);
    reg = RSCRATCH;
  }
  else
  {
    reg = arg.GetSimpleReg();
  }

  MOV(32, R(RSCRATCH2), Imm32(address));
  if (swap)
    SwapAndStore(accessSize, MRegSum(RMEM, RSCRATCH2), reg);
  else
    MOV(accessSize, MRegSum(RMEM, RSCRATCH2), R(reg));
}

void EmuCodeBlock::JitGetAndClearCAOV(bool oe)
{
  if (oe)
    AND(8, PPCSTATE(xer_so_ov), Imm8(~XER_OV_MASK));  // XER.OV = 0
  SHR(8, PPCSTATE(xer_ca), Imm8(1));                  // carry = XER.CA, XER.CA = 0
}

void EmuCodeBlock::JitSetCA()
{
  MOV(8, PPCSTATE(xer_ca), Imm8(1));  // XER.CA = 1
}

// Some testing shows CA is set roughly ~1/3 of the time (relative to clears), so
// branchless calculation of CA is probably faster in general.
void EmuCodeBlock::JitSetCAIf(CCFlags conditionCode)
{
  SETcc(conditionCode, PPCSTATE(xer_ca));
}

void EmuCodeBlock::JitClearCA()
{
  MOV(8, PPCSTATE(xer_ca), Imm8(0));
}

// Abstract between AVX and SSE: automatically handle 3-operand instructions
void EmuCodeBlock::avx_op(void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&),
                          void (XEmitter::*sseOp)(X64Reg, const OpArg&), X64Reg regOp,
                          const OpArg& arg1, const OpArg& arg2, bool packed, bool reversible)
{
  if (arg1.IsSimpleReg(regOp))
  {
    (this->*sseOp)(regOp, arg2);
  }
  else if (arg1.IsSimpleReg() && cpu_info.bAVX)
  {
    (this->*avxOp)(regOp, arg1.GetSimpleReg(), arg2);
  }
  else if (arg2.IsSimpleReg(regOp))
  {
    if (reversible)
    {
      (this->*sseOp)(regOp, arg1);
    }
    else
    {
      // The ugly case: regOp == arg2 without AVX, or with arg1 == memory
      if (!arg1.IsSimpleReg(XMM0))
        MOVAPD(XMM0, arg1);
      if (cpu_info.bAVX)
      {
        (this->*avxOp)(regOp, XMM0, arg2);
      }
      else
      {
        (this->*sseOp)(XMM0, arg2);
        if (packed)
          MOVAPD(regOp, R(XMM0));
        else
          MOVSD(regOp, R(XMM0));
      }
    }
  }
  else
  {
    if (packed)
      MOVAPD(regOp, arg1);
    else
      MOVSD(regOp, arg1);
    (this->*sseOp)(regOp, arg1 == arg2 ? R(regOp) : arg2);
  }
}

// Abstract between AVX and SSE: automatically handle 3-operand instructions
void EmuCodeBlock::avx_op(void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&, u8),
                          void (XEmitter::*sseOp)(X64Reg, const OpArg&, u8), X64Reg regOp,
                          const OpArg& arg1, const OpArg& arg2, u8 imm)
{
  if (arg1.IsSimpleReg(regOp))
  {
    (this->*sseOp)(regOp, arg2, imm);
  }
  else if (arg1.IsSimpleReg() && cpu_info.bAVX)
  {
    (this->*avxOp)(regOp, arg1.GetSimpleReg(), arg2, imm);
  }
  else if (arg2.IsSimpleReg(regOp))
  {
    // The ugly case: regOp == arg2 without AVX, or with arg1 == memory
    if (!arg1.IsSimpleReg(XMM0))
      MOVAPD(XMM0, arg1);
    if (cpu_info.bAVX)
    {
      (this->*avxOp)(regOp, XMM0, arg2, imm);
    }
    else
    {
      (this->*sseOp)(XMM0, arg2, imm);
      if (regOp != XMM0)
        MOVAPD(regOp, R(XMM0));
    }
  }
  else
  {
    MOVAPD(regOp, arg1);
    (this->*sseOp)(regOp, arg1 == arg2 ? R(regOp) : arg2, imm);
  }
}

alignas(16) static const u64 psMantissaTruncate[2] = {0xFFFFFFFFF8000000ULL, 0xFFFFFFFFF8000000ULL};
alignas(16) static const u64 psRoundBit[2] = {0x8000000, 0x8000000};

// Emulate the odd truncation/rounding that the PowerPC does on the RHS operand before
// a single precision multiply. To be precise, it drops the low 28 bits of the mantissa,
// rounding to nearest as it does.
// It needs a temp, so let the caller pass that in.
void EmuCodeBlock::Force25BitPrecision(X64Reg output, const OpArg& input, X64Reg tmp)
{
  if (m_jit.jo.accurateSinglePrecision)
  {
    // mantissa = (mantissa & ~0xFFFFFFF) + ((mantissa & (1ULL << 27)) << 1);
    if (input.IsSimpleReg() && cpu_info.bAVX)
    {
      VPAND(tmp, input.GetSimpleReg(), MConst(psRoundBit));
      VPAND(output, input.GetSimpleReg(), MConst(psMantissaTruncate));
      PADDQ(output, R(tmp));
    }
    else
    {
      if (!input.IsSimpleReg(output))
        MOVAPD(output, input);
      avx_op(&XEmitter::VPAND, &XEmitter::PAND, tmp, R(output), MConst(psRoundBit), true, true);
      PAND(output, MConst(psMantissaTruncate));
      PADDQ(output, R(tmp));
    }
  }
  else if (!input.IsSimpleReg(output))
  {
    MOVAPD(output, input);
  }
}

alignas(16) static const __m128i double_qnan_bit = _mm_set_epi64x(0xffffffffffffffff,
                                                                  0xfff7ffffffffffff);

// Converting single->double is a bit easier because all single denormals are double normals.
void EmuCodeBlock::ConvertSingleToDouble(X64Reg dst, X64Reg src, bool src_is_gpr)
{
  X64Reg gprsrc = src_is_gpr ? src : RSCRATCH;
  if (src_is_gpr)
  {
    MOVD_xmm(dst, R(src));
  }
  else
  {
    if (dst != src)
      MOVAPS(dst, R(src));
    MOVD_xmm(R(RSCRATCH), src);
  }

  UCOMISS(dst, R(dst));
  CVTSS2SD(dst, R(dst));
  FixupBranch nanConversion = J_CC(CC_P, Jump::Near);

  SwitchToFarCode();
  SetJumpTarget(nanConversion);
  TEST(32, R(gprsrc), Imm32(0x00400000));
  FixupBranch continue1 = J_CC(CC_NZ, Jump::Near);
  ANDPD(dst, MConst(double_qnan_bit));
  FixupBranch continue2 = J(Jump::Near);
  SwitchToNearCode();

  SetJumpTarget(continue1);
  SetJumpTarget(continue2);
  MOVDDUP(dst, R(dst));
}

alignas(16) static const u64 psDoubleExp[2] = {Common::DOUBLE_EXP, 0};
alignas(16) static const u64 psDoubleFrac[2] = {Common::DOUBLE_FRAC, 0};
alignas(16) static const u64 psDoubleNoSign[2] = {~Common::DOUBLE_SIGN, 0};

alignas(16) static const u32 psFloatExp[4] = {Common::FLOAT_EXP, 0, 0, 0};
alignas(16) static const u32 psFloatFrac[4] = {Common::FLOAT_FRAC, 0, 0, 0};
alignas(16) static const u32 psFloatNoSign[4] = {~Common::FLOAT_SIGN, 0, 0, 0};

// TODO: it might be faster to handle FPRF in the same way as CR is currently handled for integer,
// storing the result of each floating point op and calculating it when needed. This is trickier
// than for integers though, because there's 32 possible FPRF bit combinations but only 9 categories
// of floating point values. Fortunately, PPCAnalyzer can optimize out a large portion of FPRF
// calculations, so maybe this isn't quite that necessary.
void EmuCodeBlock::SetFPRF(Gen::X64Reg xmm, bool single)
{
  const int input_size = single ? 32 : 64;

  AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));

  FixupBranch continue1, continue2, continue3, continue4;
  if (cpu_info.bSSE4_1)
  {
    MOVQ_xmm(R(RSCRATCH), xmm);
    // Get the sign bit; almost all the branches need it.
    SHR(input_size, R(RSCRATCH), Imm8(input_size - 1));
    if (single)
      PTEST(xmm, MConst(psFloatExp));
    else
      PTEST(xmm, MConst(psDoubleExp));
    FixupBranch maxExponent = J_CC(CC_C);
    FixupBranch zeroExponent = J_CC(CC_Z);

    // Nice normalized number: sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NN - Common::PPC_FPCLASS_PN, Common::PPC_FPCLASS_PN));
    continue1 = J();

    SetJumpTarget(maxExponent);
    if (single)
      PTEST(xmm, MConst(psFloatFrac));
    else
      PTEST(xmm, MConst(psDoubleFrac));
    FixupBranch notNAN = J_CC(CC_Z);

    // Max exponent + mantissa: PPC_FPCLASS_QNAN
    MOV(32, R(RSCRATCH), Imm32(Common::PPC_FPCLASS_QNAN));
    continue2 = J();

    // Max exponent + no mantissa: sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
    SetJumpTarget(notNAN);
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NINF - Common::PPC_FPCLASS_PINF,
                Common::PPC_FPCLASS_PINF));
    continue3 = J();

    SetJumpTarget(zeroExponent);
    if (single)
      PTEST(xmm, MConst(psFloatNoSign));
    else
      PTEST(xmm, MConst(psDoubleNoSign));
    FixupBranch zero = J_CC(CC_Z);

    // No exponent + mantissa: sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_ND - Common::PPC_FPCLASS_PD, Common::PPC_FPCLASS_PD));
    continue4 = J();

    // Zero: sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
    SetJumpTarget(zero);
    SHL(32, R(RSCRATCH), Imm8(4));
    ADD(32, R(RSCRATCH), Imm8(Common::PPC_FPCLASS_PZ));
  }
  else
  {
    MOVQ_xmm(R(RSCRATCH), xmm);
    if (single)
      TEST(32, R(RSCRATCH), Imm32(Common::FLOAT_EXP));
    else
      TEST(64, R(RSCRATCH), MConst(psDoubleExp));
    FixupBranch zeroExponent = J_CC(CC_Z);

    if (single)
    {
      AND(32, R(RSCRATCH), Imm32(~Common::FLOAT_SIGN));
      CMP(32, R(RSCRATCH), Imm32(Common::FLOAT_EXP));
    }
    else
    {
      AND(64, R(RSCRATCH), MConst(psDoubleNoSign));
      CMP(64, R(RSCRATCH), MConst(psDoubleExp));
    }
    FixupBranch nan =
        J_CC(CC_G);  // This works because if the sign bit is set, RSCRATCH is negative
    FixupBranch infinity = J_CC(CC_E);

    MOVQ_xmm(R(RSCRATCH), xmm);
    SHR(input_size, R(RSCRATCH), Imm8(input_size - 1));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NN - Common::PPC_FPCLASS_PN, Common::PPC_FPCLASS_PN));
    continue1 = J();

    SetJumpTarget(nan);
    MOV(32, R(RSCRATCH), Imm32(Common::PPC_FPCLASS_QNAN));
    continue2 = J();

    SetJumpTarget(infinity);
    MOVQ_xmm(R(RSCRATCH), xmm);
    SHR(input_size, R(RSCRATCH), Imm8(input_size - 1));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NINF - Common::PPC_FPCLASS_PINF,
                Common::PPC_FPCLASS_PINF));
    continue3 = J();

    SetJumpTarget(zeroExponent);
    if (single)
      TEST(input_size, R(RSCRATCH), Imm32(~Common::FLOAT_SIGN));
    else
      TEST(input_size, R(RSCRATCH), MConst(psDoubleNoSign));
    FixupBranch zero = J_CC(CC_Z);

    SHR(input_size, R(RSCRATCH), Imm8(input_size - 1));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_ND - Common::PPC_FPCLASS_PD, Common::PPC_FPCLASS_PD));
    continue4 = J();

    SetJumpTarget(zero);
    SHR(input_size, R(RSCRATCH), Imm8(input_size - 1));
    SHL(32, R(RSCRATCH), Imm8(4));
    ADD(32, R(RSCRATCH), Imm8(Common::PPC_FPCLASS_PZ));
  }

  SetJumpTarget(continue1);
  SetJumpTarget(continue2);
  SetJumpTarget(continue3);
  SetJumpTarget(continue4);
  SHL(32, R(RSCRATCH), Imm8(FPRF_SHIFT));
  OR(32, PPCSTATE(fpscr), R(RSCRATCH));
}

void EmuCodeBlock::Clear()
{
  m_back_patch_info.clear();
  m_exception_handler_at_loc.clear();
}
