// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
    js.exceptionHandler = J_CC(Gen::CC_NZ, true);
    js.fixupExceptionHandler = true;
  }
}

void EmuCodeBlock::SwitchToFarCode()
{
  m_near_code = GetWritableCodePtr();
  SetCodePtr(m_far_code.GetWritableCodePtr());
}

void EmuCodeBlock::SwitchToNearCode()
{
  m_far_code.SetCodePtr(GetWritableCodePtr());
  SetCodePtr(m_near_code);
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
  MOV(64, R(RSCRATCH), ImmPtr(&PowerPC::dbat_table[0]));
  SHR(32, R(RSCRATCH_EXTRA), Imm8(PowerPC::BAT_INDEX_SHIFT));
  TEST(32, MComplex(RSCRATCH, RSCRATCH_EXTRA, SCALE_4, 0), Imm32(PowerPC::BAT_PHYSICAL_BIT));

  if (registers_in_use[RSCRATCH_EXTRA])
    POP(RSCRATCH_EXTRA);
  if (registers_in_use[RSCRATCH])
    POP(RSCRATCH);

  return J_CC(CC_Z, m_far_code.Enabled());
}

void EmuCodeBlock::UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset,
                                      bool signExtend)
{
  OpArg src = MComplex(RMEM, reg_addr, SCALE_1, offset);
  LoadAndSwap(accessSize, reg_value, src, signExtend);
}

void EmuCodeBlock::UnsafeLoadRegToRegNoSwap(X64Reg reg_addr, X64Reg reg_value, int accessSize,
                                            s32 offset, bool signExtend)
{
  if (signExtend)
    MOVSX(32, accessSize, reg_value, MComplex(RMEM, reg_addr, SCALE_1, offset));
  else
    MOVZX(32, accessSize, reg_value, MComplex(RMEM, reg_addr, SCALE_1, offset));
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
class MMIOReadCodeGenerator : public MMIO::ReadHandlerVisitor<T>
{
public:
  MMIOReadCodeGenerator(Gen::X64CodeBlock* code, BitSet32 registers_in_use, Gen::X64Reg dst_reg,
                        u32 address, bool sign_extend)
      : m_code(code), m_registers_in_use(registers_in_use), m_dst_reg(dst_reg), m_address(address),
        m_sign_extend(sign_extend)
  {
  }

  void VisitConstant(T value) override { LoadConstantToReg(8 * sizeof(T), value); }
  void VisitDirect(const T* addr, u32 mask) override
  {
    LoadAddrMaskToReg(8 * sizeof(T), addr, mask);
  }
  void VisitComplex(const std::function<T(u32)>* lambda) override
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

  void CallLambda(int sbits, const std::function<T(u32)>* lambda)
  {
    m_code->ABI_PushRegistersAndAdjustStack(m_registers_in_use, 0);
    m_code->ABI_CallLambdaC(lambda, m_address);
    m_code->ABI_PopRegistersAndAdjustStack(m_registers_in_use, 0);
    MoveOpArgToReg(sbits, R(ABI_RETURN));
  }

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
    MMIOReadCodeGenerator<u8> gen(this, registers_in_use, reg_value, address, sign_extend);
    mmio->GetHandlerForRead<u8>(address)->Visit(gen);
    break;
  }
  case 16:
  {
    MMIOReadCodeGenerator<u16> gen(this, registers_in_use, reg_value, address, sign_extend);
    mmio->GetHandlerForRead<u16>(address)->Visit(gen);
    break;
  }
  case 32:
  {
    MMIOReadCodeGenerator<u32> gen(this, registers_in_use, reg_value, address, sign_extend);
    mmio->GetHandlerForRead<u32>(address)->Visit(gen);
    break;
  }
  }
}

void EmuCodeBlock::SafeLoadToReg(X64Reg reg_value, const Gen::OpArg& opAddress, int accessSize,
                                 s32 offset, BitSet32 registersInUse, bool signExtend, int flags)
{
  bool slowmem = (flags & SAFE_LOADSTORE_FORCE_SLOWMEM) != 0;

  auto& js = m_jit.js;
  registersInUse[reg_value] = false;
  if (m_jit.jo.fastmem && !(flags & (SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_UPDATE_PC)) &&
      !slowmem)
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
  const bool dr_set = (flags & SAFE_LOADSTORE_DR_ON) || MSR.DR;
  const bool fast_check_address = !slowmem && dr_set;
  if (fast_check_address)
  {
    FixupBranch slow = CheckIfSafeAddress(R(reg_value), reg_addr, registersInUse);
    UnsafeLoadToReg(reg_value, R(reg_addr), accessSize, 0, signExtend);
    if (m_far_code.Enabled())
      SwitchToFarCode();
    else
      exit = J(true);
    SetJumpTarget(slow);
  }

  // Helps external systems know which instruction triggered the read.
  // Invalid for calls from Jit64AsmCommon routines
  if (!(flags & SAFE_LOADSTORE_NO_UPDATE_PC))
  {
    MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
  }

  size_t rsp_alignment = (flags & SAFE_LOADSTORE_NO_PROLOG) ? 8 : 0;
  ABI_PushRegistersAndAdjustStack(registersInUse, rsp_alignment);
  switch (accessSize)
  {
  case 64:
    ABI_CallFunctionR(PowerPC::Read_U64, reg_addr);
    break;
  case 32:
    ABI_CallFunctionR(PowerPC::Read_U32, reg_addr);
    break;
  case 16:
    ABI_CallFunctionR(PowerPC::Read_U16_ZX, reg_addr);
    break;
  case 8:
    ABI_CallFunctionR(PowerPC::Read_U8_ZX, reg_addr);
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
      exit = J(true);
      SwitchToNearCode();
    }
    SetJumpTarget(exit);
  }
}

void EmuCodeBlock::SafeLoadToRegImmediate(X64Reg reg_value, u32 address, int accessSize,
                                          BitSet32 registersInUse, bool signExtend)
{
  // If the address is known to be RAM, just load it directly.
  if (PowerPC::IsOptimizableRAMAddress(address))
  {
    UnsafeLoadToReg(reg_value, Imm32(address), accessSize, 0, signExtend);
    return;
  }

  // If the address maps to an MMIO register, inline MMIO read code.
  u32 mmioAddress = PowerPC::IsOptimizableMMIOAccess(address, accessSize);
  if (accessSize != 64 && mmioAddress)
  {
    MMIOLoadToReg(Memory::mmio_mapping.get(), reg_value, registersInUse, mmioAddress, accessSize,
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
    ABI_CallFunctionC(PowerPC::Read_U64, address);
    break;
  case 32:
    ABI_CallFunctionC(PowerPC::Read_U32, address);
    break;
  case 16:
    ABI_CallFunctionC(PowerPC::Read_U16_ZX, address);
    break;
  case 8:
    ABI_CallFunctionC(PowerPC::Read_U8_ZX, address);
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
  bool slowmem = (flags & SAFE_LOADSTORE_FORCE_SLOWMEM) != 0;

  // set the correct immediate format
  reg_value = FixImmediate(accessSize, reg_value);

  auto& js = m_jit.js;
  if (m_jit.jo.fastmem && !(flags & (SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_UPDATE_PC)) &&
      !slowmem)
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
  const bool dr_set = (flags & SAFE_LOADSTORE_DR_ON) || MSR.DR;
  const bool fast_check_address = !slowmem && dr_set;
  if (fast_check_address)
  {
    FixupBranch slow = CheckIfSafeAddress(reg_value, reg_addr, registersInUse);
    UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0, swap);
    if (m_far_code.Enabled())
      SwitchToFarCode();
    else
      exit = J(true);
    SetJumpTarget(slow);
  }

  // PC is used by memory watchpoints (if enabled) or to print accurate PC locations in debug logs
  // Invalid for calls from Jit64AsmCommon routines
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
    ABI_CallFunctionRR(swap ? PowerPC::Write_U64 : PowerPC::Write_U64_Swap, reg, reg_addr);
    break;
  case 32:
    ABI_CallFunctionRR(swap ? PowerPC::Write_U32 : PowerPC::Write_U32_Swap, reg, reg_addr);
    break;
  case 16:
    ABI_CallFunctionRR(swap ? PowerPC::Write_U16 : PowerPC::Write_U16_Swap, reg, reg_addr);
    break;
  case 8:
    ABI_CallFunctionRR(PowerPC::Write_U8, reg, reg_addr);
    break;
  }
  ABI_PopRegistersAndAdjustStack(registersInUse, rsp_alignment);

  MemoryExceptionCheck();

  if (fast_check_address)
  {
    if (m_far_code.Enabled())
    {
      exit = J(true);
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
  if (m_jit.jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(address))
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
  else if (PowerPC::IsOptimizableRAMAddress(address))
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
      ABI_CallFunctionAC(64, PowerPC::Write_U64, arg, address);
      break;
    case 32:
      ABI_CallFunctionAC(32, PowerPC::Write_U32, arg, address);
      break;
    case 16:
      ABI_CallFunctionAC(16, PowerPC::Write_U16, arg, address);
      break;
    case 8:
      ABI_CallFunctionAC(8, PowerPC::Write_U8, arg, address);
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

void EmuCodeBlock::ForceSinglePrecision(X64Reg output, const OpArg& input, bool packed,
                                        bool duplicate)
{
  // Most games don't need these. Zelda requires it though - some platforms get stuck without them.
  if (m_jit.jo.accurateSinglePrecision)
  {
    if (packed)
    {
      CVTPD2PS(output, input);
      CVTPS2PD(output, R(output));
    }
    else
    {
      CVTSD2SS(output, input);
      CVTSS2SD(output, R(output));
      if (duplicate)
        MOVDDUP(output, R(output));
    }
  }
  else if (!input.IsSimpleReg(output))
  {
    if (duplicate)
      MOVDDUP(output, input);
    else
      MOVAPD(output, input);
  }
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

// Since the following float conversion functions are used in non-arithmetic PPC float
// instructions, they must convert floats bitexact and never flush denormals to zero or turn SNaNs
// into QNaNs. This means we can't use CVTSS2SD/CVTSD2SS. The x87 FPU doesn't even support
// flush-to-zero so we can use FLD+FSTP even on denormals.
// If the number is a NaN, make sure to set the QNaN bit back to its original value.

// Another problem is that officially, converting doubles to single format results in undefined
// behavior.  Relying on undefined behavior is a bug so no software should ever do this.
// Super Mario 64 (on Wii VC) accidentally relies on this behavior.  See issue #11173

alignas(16) static const __m128i double_exponent = _mm_set_epi64x(0, 0x7ff0000000000000);
alignas(16) static const __m128i double_fraction = _mm_set_epi64x(0, 0x000fffffffffffff);
alignas(16) static const __m128i double_sign_bit = _mm_set_epi64x(0, 0x8000000000000000);
alignas(16) static const __m128i double_explicit_top_bit = _mm_set_epi64x(0, 0x0010000000000000);
alignas(16) static const __m128i double_top_two_bits = _mm_set_epi64x(0, 0xc000000000000000);
alignas(16) static const __m128i double_bottom_bits = _mm_set_epi64x(0, 0x07ffffffe0000000);
alignas(16) static const __m128i double_qnan_bit = _mm_set_epi64x(0xffffffffffffffff,
                                                                  0xfff7ffffffffffff);

// This is the same algorithm used in the interpreter (and actual hardware)
// The documentation states that the conversion of a double with an outside the
// valid range for a single (or a single denormal) is undefined.
// But testing on actual hardware shows it always picks bits 0..1 and 5..34
// unless the exponent is in the range of 874 to 896.
void EmuCodeBlock::ConvertDoubleToSingle(X64Reg dst, X64Reg src)
{
  MOVAPD(XMM1, R(src));

  // Grab Exponent
  PAND(XMM1, MConst(double_exponent));
  PSRLQ(XMM1, 52);
  MOVD_xmm(R(RSCRATCH), XMM1);

  // Check if the double is in the range of valid single subnormal
  SUB(16, R(RSCRATCH), Imm16(874));
  CMP(16, R(RSCRATCH), Imm16(896 - 874));
  FixupBranch NoDenormalize = J_CC(CC_A);

  // Denormalise

  // shift = (905 - Exponent) plus the 21 bit double to single shift
  MOV(16, R(RSCRATCH), Imm16(905 + 21));
  MOVD_xmm(XMM0, R(RSCRATCH));
  PSUBQ(XMM0, R(XMM1));

  // xmm1 = fraction | 0x0010000000000000
  MOVAPD(XMM1, R(src));
  PAND(XMM1, MConst(double_fraction));
  POR(XMM1, MConst(double_explicit_top_bit));

  // fraction >> shift
  PSRLQ(XMM1, R(XMM0));

  // OR the sign bit in.
  MOVAPD(XMM0, R(src));
  PAND(XMM0, MConst(double_sign_bit));
  PSRLQ(XMM0, 32);
  POR(XMM1, R(XMM0));

  FixupBranch end = J(false);  // Goto end

  SetJumpTarget(NoDenormalize);

  // Don't Denormalize

  // We want bits 0, 1
  MOVAPD(XMM1, R(src));
  PAND(XMM1, MConst(double_top_two_bits));
  PSRLQ(XMM1, 32);

  // And 5 through to 34
  MOVAPD(XMM0, R(src));
  PAND(XMM0, MConst(double_bottom_bits));
  PSRLQ(XMM0, 29);

  // OR them togther
  POR(XMM1, R(XMM0));

  // End
  SetJumpTarget(end);
  MOVDDUP(dst, R(XMM1));
}

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
  FixupBranch nanConversion = J_CC(CC_P, true);

  SwitchToFarCode();
  SetJumpTarget(nanConversion);
  TEST(32, R(gprsrc), Imm32(0x00400000));
  FixupBranch continue1 = J_CC(CC_NZ, true);
  ANDPD(dst, MConst(double_qnan_bit));
  FixupBranch continue2 = J(true);
  SwitchToNearCode();

  SetJumpTarget(continue1);
  SetJumpTarget(continue2);
  MOVDDUP(dst, R(dst));
}

alignas(16) static const u64 psDoubleExp[2] = {0x7FF0000000000000ULL, 0};
alignas(16) static const u64 psDoubleFrac[2] = {0x000FFFFFFFFFFFFFULL, 0};
alignas(16) static const u64 psDoubleNoSign[2] = {0x7FFFFFFFFFFFFFFFULL, 0};

// TODO: it might be faster to handle FPRF in the same way as CR is currently handled for integer,
// storing
// the result of each floating point op and calculating it when needed. This is trickier than for
// integers
// though, because there's 32 possible FPRF bit combinations but only 9 categories of floating point
// values,
// which makes the whole thing rather trickier.
// Fortunately, PPCAnalyzer can optimize out a large portion of FPRF calculations, so maybe this
// isn't
// quite that necessary.
void EmuCodeBlock::SetFPRF(Gen::X64Reg xmm)
{
  AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));

  FixupBranch continue1, continue2, continue3, continue4;
  if (cpu_info.bSSE4_1)
  {
    MOVQ_xmm(R(RSCRATCH), xmm);
    SHR(64, R(RSCRATCH), Imm8(63));  // Get the sign bit; almost all the branches need it.
    PTEST(xmm, MConst(psDoubleExp));
    FixupBranch maxExponent = J_CC(CC_C);
    FixupBranch zeroExponent = J_CC(CC_Z);

    // Nice normalized number: sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NN - Common::PPC_FPCLASS_PN, Common::PPC_FPCLASS_PN));
    continue1 = J();

    SetJumpTarget(maxExponent);
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
    TEST(64, R(RSCRATCH), MConst(psDoubleExp));
    FixupBranch zeroExponent = J_CC(CC_Z);
    AND(64, R(RSCRATCH), MConst(psDoubleNoSign));
    CMP(64, R(RSCRATCH), MConst(psDoubleExp));
    FixupBranch nan =
        J_CC(CC_G);  // This works because if the sign bit is set, RSCRATCH is negative
    FixupBranch infinity = J_CC(CC_E);
    MOVQ_xmm(R(RSCRATCH), xmm);
    SHR(64, R(RSCRATCH), Imm8(63));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NN - Common::PPC_FPCLASS_PN, Common::PPC_FPCLASS_PN));
    continue1 = J();
    SetJumpTarget(nan);
    MOV(32, R(RSCRATCH), Imm32(Common::PPC_FPCLASS_QNAN));
    continue2 = J();
    SetJumpTarget(infinity);
    MOVQ_xmm(R(RSCRATCH), xmm);
    SHR(64, R(RSCRATCH), Imm8(63));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_NINF - Common::PPC_FPCLASS_PINF,
                Common::PPC_FPCLASS_PINF));
    continue3 = J();
    SetJumpTarget(zeroExponent);
    TEST(64, R(RSCRATCH), R(RSCRATCH));
    FixupBranch zero = J_CC(CC_Z);
    SHR(64, R(RSCRATCH), Imm8(63));
    LEA(32, RSCRATCH,
        MScaled(RSCRATCH, Common::PPC_FPCLASS_ND - Common::PPC_FPCLASS_PD, Common::PPC_FPCLASS_PD));
    continue4 = J();
    SetJumpTarget(zero);
    SHR(64, R(RSCRATCH), Imm8(63));
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
