// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHWInterface.h"
#include "Core/DSP/Jit/DSPEmitter.h"

using namespace Gen;

namespace DSP
{
namespace JIT
{
namespace x86
{
// clobbers:
// EAX = (s8)g_dsp.reg_stack_ptr[reg_index]
// expects:
void DSPEmitter::dsp_reg_stack_push(StackRegister stack_reg)
{
  const auto reg_index = static_cast<size_t>(stack_reg);

  // g_dsp.reg_stack_ptr[reg_index]++;
  // g_dsp.reg_stack_ptr[reg_index] &= DSP_STACK_MASK;
  MOV(8, R(AL), M_SDSP_reg_stack_ptr(reg_index));
  ADD(8, R(AL), Imm8(1));
  AND(8, R(AL), Imm8(DSP_STACK_MASK));
  MOV(8, M_SDSP_reg_stack_ptr(reg_index), R(AL));

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  X64Reg tmp2 = m_gpr.GetFreeXReg();
  // g_dsp.reg_stack[reg_index][g_dsp.reg_stack_ptr[reg_index]] = g_dsp.r[DSP_REG_ST0 + reg_index];
  MOV(16, R(tmp1), M_SDSP_r_st(reg_index));
  MOVZX(64, 8, RAX, R(AL));
  MOV(64, R(tmp2), ImmPtr(g_dsp.reg_stack[reg_index]));
  MOV(16, MComplex(tmp2, EAX, SCALE_2, 0), R(tmp1));
  m_gpr.PutXReg(tmp1);
  m_gpr.PutXReg(tmp2);
}

// clobbers:
// EAX = (s8)g_dsp.reg_stack_ptr[reg_index]
// expects:
void DSPEmitter::dsp_reg_stack_pop(StackRegister stack_reg)
{
  const auto reg_index = static_cast<size_t>(stack_reg);

  // g_dsp.r[DSP_REG_ST0 + reg_index] = g_dsp.reg_stack[reg_index][g_dsp.reg_stack_ptr[reg_index]];
  MOV(8, R(AL), M_SDSP_reg_stack_ptr(reg_index));
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  X64Reg tmp2 = m_gpr.GetFreeXReg();
  MOVZX(64, 8, RAX, R(AL));
  MOV(64, R(tmp2), ImmPtr(g_dsp.reg_stack[reg_index]));
  MOV(16, R(tmp1), MComplex(tmp2, EAX, SCALE_2, 0));
  MOV(16, M_SDSP_r_st(reg_index), R(tmp1));
  m_gpr.PutXReg(tmp1);
  m_gpr.PutXReg(tmp2);

  // g_dsp.reg_stack_ptr[reg_index]--;
  // g_dsp.reg_stack_ptr[reg_index] &= DSP_STACK_MASK;
  SUB(8, R(AL), Imm8(1));
  AND(8, R(AL), Imm8(DSP_STACK_MASK));
  MOV(8, M_SDSP_reg_stack_ptr(reg_index), R(AL));
}

void DSPEmitter::dsp_reg_store_stack(StackRegister stack_reg, Gen::X64Reg host_sreg)
{
  if (host_sreg != EDX)
  {
    MOV(16, R(EDX), R(host_sreg));
  }

  dsp_reg_stack_push(stack_reg);

  // g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
  MOV(16, M_SDSP_r_st(static_cast<size_t>(stack_reg)), R(EDX));
}

void DSPEmitter::dsp_reg_load_stack(StackRegister stack_reg, Gen::X64Reg host_dreg)
{
  // u16 val = g_dsp.r[DSP_REG_ST0 + stack_reg];
  MOV(16, R(EDX), M_SDSP_r_st(static_cast<size_t>(stack_reg)));

  dsp_reg_stack_pop(stack_reg);

  if (host_dreg != EDX)
  {
    MOV(16, R(host_dreg), R(EDX));
  }
}

void DSPEmitter::dsp_reg_store_stack_imm(StackRegister stack_reg, u16 val)
{
  dsp_reg_stack_push(stack_reg);

  // g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
  MOV(16, M_SDSP_r_st(static_cast<size_t>(stack_reg)), Imm16(val));
}

void DSPEmitter::dsp_op_write_reg(int reg, Gen::X64Reg host_sreg)
{
  switch (reg & 0x1f)
  {
  // 8-bit sign extended registers.
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    m_gpr.WriteReg(reg, R(host_sreg));
    break;

  // Stack registers.
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    dsp_reg_store_stack(static_cast<StackRegister>(reg - DSP_REG_ST0), host_sreg);
    break;

  default:
    m_gpr.WriteReg(reg, R(host_sreg));
    break;
  }
}

void DSPEmitter::dsp_op_write_reg_imm(int reg, u16 val)
{
  switch (reg & 0x1f)
  {
  // 8-bit sign extended registers. Should look at prod.h too...
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    m_gpr.WriteReg(reg, Imm16((u16)(s16)(s8)(u8)val));
    break;
  // Stack registers.
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    dsp_reg_store_stack_imm(static_cast<StackRegister>(reg - DSP_REG_ST0), val);
    break;

  default:
    m_gpr.WriteReg(reg, Imm16(val));
    break;
  }
}

void DSPEmitter::dsp_conditional_extend_accum(int reg)
{
  switch (reg)
  {
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
  {
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    DSPJitRegCache c(m_gpr);
    TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
    FixupBranch not_40bit = J_CC(CC_Z, true);
    // if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
    //{
    // Sign extend into whole accum.
    // u16 val = g_dsp.r[reg];
    get_acc_m(reg - DSP_REG_ACM0, EAX);
    SHR(32, R(EAX), Imm8(16));
    // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
    // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
    set_acc_h(reg - DSP_REG_ACM0, R(RAX));
    set_acc_l(reg - DSP_REG_ACM0, Imm16(0));
    //}
    m_gpr.FlushRegs(c);
    SetJumpTarget(not_40bit);
    m_gpr.PutReg(DSP_REG_SR, false);
  }
  }
}

void DSPEmitter::dsp_conditional_extend_accum_imm(int reg, u16 val)
{
  switch (reg)
  {
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
  {
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    DSPJitRegCache c(m_gpr);
    TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
    FixupBranch not_40bit = J_CC(CC_Z, true);
    // if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
    //{
    // Sign extend into whole accum.
    // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
    // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
    set_acc_h(reg - DSP_REG_ACM0, Imm16((val & 0x8000) ? 0xffff : 0x0000));
    set_acc_l(reg - DSP_REG_ACM0, Imm16(0));
    //}
    m_gpr.FlushRegs(c);
    SetJumpTarget(not_40bit);
    m_gpr.PutReg(DSP_REG_SR, false);
  }
  }
}

void DSPEmitter::dsp_op_read_reg_dont_saturate(int reg, Gen::X64Reg host_dreg,
                                               RegisterExtension extend)
{
  switch (reg & 0x1f)
  {
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    dsp_reg_load_stack(static_cast<StackRegister>(reg - DSP_REG_ST0), host_dreg);
    switch (extend)
    {
    case RegisterExtension::Sign:
      MOVSX(64, 16, host_dreg, R(host_dreg));
      break;
    case RegisterExtension::Zero:
      MOVZX(64, 16, host_dreg, R(host_dreg));
      break;
    case RegisterExtension::None:
    default:
      break;
    }
    return;
  default:
    m_gpr.ReadReg(reg, host_dreg, extend);
    return;
  }
}

void DSPEmitter::dsp_op_read_reg(int reg, Gen::X64Reg host_dreg, RegisterExtension extend)
{
  switch (reg & 0x1f)
  {
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    dsp_reg_load_stack(static_cast<StackRegister>(reg - DSP_REG_ST0), host_dreg);
    switch (extend)
    {
    case RegisterExtension::Sign:
      MOVSX(64, 16, host_dreg, R(host_dreg));
      break;
    case RegisterExtension::Zero:
      MOVZX(64, 16, host_dreg, R(host_dreg));
      break;
    case RegisterExtension::None:
    default:
      break;
    }
    return;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
  {
    // we already know this is ACCM0 or ACCM1
    const OpArg acc_reg = m_gpr.GetReg(reg - DSP_REG_ACM0 + DSP_REG_ACC0_64);
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);

    DSPJitRegCache c(m_gpr);
    TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
    FixupBranch not_40bit = J_CC(CC_Z, true);

    MOVSX(64, 32, host_dreg, acc_reg);
    CMP(64, R(host_dreg), acc_reg);
    FixupBranch no_saturate = J_CC(CC_Z);

    CMP(64, acc_reg, Imm32(0));
    FixupBranch negative = J_CC(CC_LE);

    MOV(64, R(host_dreg), Imm32(0x7fff));  // this works for all extend modes
    FixupBranch done_positive = J();

    SetJumpTarget(negative);
    if (extend == RegisterExtension::None || extend == RegisterExtension::Zero)
      MOV(64, R(host_dreg), Imm32(0x00008000));
    else
      MOV(64, R(host_dreg), Imm32(0xffff8000));
    FixupBranch done_negative = J();

    SetJumpTarget(no_saturate);
    SetJumpTarget(not_40bit);

    MOV(64, R(host_dreg), acc_reg);
    if (extend == RegisterExtension::None || extend == RegisterExtension::Zero)
      SHR(64, R(host_dreg), Imm8(16));
    else
      SAR(64, R(host_dreg), Imm8(16));
    SetJumpTarget(done_positive);
    SetJumpTarget(done_negative);
    m_gpr.FlushRegs(c);
    m_gpr.PutReg(reg - DSP_REG_ACM0 + DSP_REG_ACC0_64, false);

    m_gpr.PutReg(DSP_REG_SR, false);
  }
    return;
  default:
    m_gpr.ReadReg(reg, host_dreg, extend);
    return;
  }
}

// addr math
//
// These functions detect overflow by checking if
// the bit past the top of the mask(WR) has changed in AR.
// They detect values under the minimum for a mask by adding wr + 1
// and checking if the bit past the top of the mask doesn't change.
// Both are done while ignoring changes due to values/holes in IX
// above the mask.

// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
void DSPEmitter::increment_addr_reg(int reg)
{
  const OpArg wr_reg = m_gpr.GetReg(DSP_REG_WR0 + reg);
  MOVZX(32, 16, EDX, wr_reg);
  m_gpr.PutReg(DSP_REG_WR0 + reg, false);

  const OpArg ar_reg = m_gpr.GetReg(DSP_REG_AR0 + reg);
  MOVZX(32, 16, EAX, ar_reg);

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  // u32 nar = ar + 1;
  MOV(32, R(tmp1), R(EAX));
  ADD(32, R(EAX), Imm8(1));

  // if ((nar ^ ar) > ((wr | 1) << 1))
  //		nar -= wr + 1;
  XOR(32, R(tmp1), R(EAX));
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  CMP(32, R(tmp1), R(ECX));
  FixupBranch nowrap = J_CC(CC_BE);
  SUB(16, R(AX), R(DX));
  SUB(16, R(AX), Imm8(1));
  SetJumpTarget(nowrap);
  m_gpr.PutXReg(tmp1);
  // g_dsp.r.ar[reg] = nar;

  MOV(16, ar_reg, R(AX));
  m_gpr.PutReg(DSP_REG_AR0 + reg);
}

// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
void DSPEmitter::decrement_addr_reg(int reg)
{
  const OpArg wr_reg = m_gpr.GetReg(DSP_REG_WR0 + reg);
  MOVZX(32, 16, EDX, wr_reg);
  m_gpr.PutReg(DSP_REG_WR0 + reg, false);

  const OpArg ar_reg = m_gpr.GetReg(DSP_REG_AR0 + reg);
  MOVZX(32, 16, EAX, ar_reg);

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  // u32 nar = ar + wr;
  // edi = nar
  LEA(32, tmp1, MRegSum(EAX, EDX));

  // if (((nar ^ ar) & ((wr | 1) << 1)) > wr)
  //		nar -= wr + 1;
  XOR(32, R(EAX), R(tmp1));
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  AND(32, R(EAX), R(ECX));
  CMP(32, R(EAX), R(EDX));
  FixupBranch nowrap = J_CC(CC_BE);
  SUB(16, R(tmp1), R(DX));
  SUB(16, R(tmp1), Imm8(1));
  SetJumpTarget(nowrap);
  // g_dsp.r.ar[reg] = nar;

  MOV(16, ar_reg, R(tmp1));
  m_gpr.PutReg(DSP_REG_AR0 + reg);
  m_gpr.PutXReg(tmp1);
}

// Increase addr register according to the correspond ix register
// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// ECX = g_dsp.r.ix[reg]
void DSPEmitter::increase_addr_reg(int reg, int _ix_reg)
{
  const OpArg wr_reg = m_gpr.GetReg(DSP_REG_WR0 + reg);
  MOVZX(32, 16, EDX, wr_reg);
  m_gpr.PutReg(DSP_REG_WR0 + reg, false);

  const OpArg ix_reg = m_gpr.GetReg(DSP_REG_IX0 + _ix_reg);
  MOVSX(32, 16, ECX, ix_reg);
  m_gpr.PutReg(DSP_REG_IX0 + _ix_reg, false);

  const OpArg ar_reg = m_gpr.GetReg(DSP_REG_AR0 + reg);
  MOVZX(32, 16, EAX, ar_reg);

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  // u32 nar = ar + ix;
  // edi = nar
  LEA(32, tmp1, MRegSum(EAX, ECX));

  // u32 dar = (nar ^ ar ^ ix) & ((wr | 1) << 1);
  // eax = dar
  XOR(32, R(EAX), R(ECX));
  XOR(32, R(EAX), R(tmp1));

  // if (ix >= 0)
  TEST(32, R(ECX), R(ECX));
  FixupBranch negative = J_CC(CC_S);
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  AND(32, R(EAX), R(ECX));

  // if (dar > wr)
  CMP(32, R(EAX), R(EDX));
  FixupBranch done = J_CC(CC_BE);
  // nar -= wr + 1;
  SUB(16, R(tmp1), R(DX));
  SUB(16, R(tmp1), Imm8(1));
  FixupBranch done2 = J();

  // else
  SetJumpTarget(negative);
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  AND(32, R(EAX), R(ECX));

  // if ((((nar + wr + 1) ^ nar) & dar) <= wr)
  LEA(32, ECX, MComplex(tmp1, EDX, SCALE_1, 1));
  XOR(32, R(ECX), R(tmp1));
  AND(32, R(ECX), R(EAX));
  CMP(32, R(ECX), R(EDX));
  FixupBranch done3 = J_CC(CC_A);
  // nar += wr + 1;
  LEA(32, tmp1, MComplex(tmp1, EDX, SCALE_1, 1));

  SetJumpTarget(done);
  SetJumpTarget(done2);
  SetJumpTarget(done3);
  // g_dsp.r.ar[reg] = nar;

  MOV(16, ar_reg, R(tmp1));
  m_gpr.PutReg(DSP_REG_AR0 + reg);
  m_gpr.PutXReg(tmp1);
}

// Decrease addr register according to the correspond ix register
// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// ECX = g_dsp.r.ix[reg]
void DSPEmitter::decrease_addr_reg(int reg)
{
  const OpArg wr_reg = m_gpr.GetReg(DSP_REG_WR0 + reg);
  MOVZX(32, 16, EDX, wr_reg);
  m_gpr.PutReg(DSP_REG_WR0 + reg, false);

  const OpArg ix_reg = m_gpr.GetReg(DSP_REG_IX0 + reg);
  MOVSX(32, 16, ECX, ix_reg);
  m_gpr.PutReg(DSP_REG_IX0 + reg, false);

  const OpArg ar_reg = m_gpr.GetReg(DSP_REG_AR0 + reg);
  MOVZX(32, 16, EAX, ar_reg);

  NOT(32, R(ECX));  // esi = ~ix

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  // u32 nar = ar - ix; (ar + ~ix + 1)
  LEA(32, tmp1, MComplex(EAX, ECX, SCALE_1, 1));

  // u32 dar = (nar ^ ar ^ ~ix) & ((wr | 1) << 1);
  // eax = dar
  XOR(32, R(EAX), R(ECX));
  XOR(32, R(EAX), R(tmp1));

  // if ((u32)ix > 0xFFFF8000)  ==> (~ix < 0x00007FFF)
  CMP(32, R(ECX), Imm32(0x00007FFF));
  FixupBranch positive = J_CC(CC_AE);
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  AND(32, R(EAX), R(ECX));

  // if (dar > wr)
  CMP(32, R(EAX), R(EDX));
  FixupBranch done = J_CC(CC_BE);
  // nar -= wr + 1;
  SUB(16, R(tmp1), R(DX));
  SUB(16, R(tmp1), Imm8(1));
  FixupBranch done2 = J();

  // else
  SetJumpTarget(positive);
  LEA(32, ECX, MRegSum(EDX, EDX));
  OR(32, R(ECX), Imm8(2));
  AND(32, R(EAX), R(ECX));

  // if ((((nar + wr + 1) ^ nar) & dar) <= wr)
  LEA(32, ECX, MComplex(tmp1, EDX, SCALE_1, 1));
  XOR(32, R(ECX), R(tmp1));
  AND(32, R(ECX), R(EAX));
  CMP(32, R(ECX), R(EDX));
  FixupBranch done3 = J_CC(CC_A);
  // nar += wr + 1;
  LEA(32, tmp1, MComplex(tmp1, EDX, SCALE_1, 1));

  SetJumpTarget(done);
  SetJumpTarget(done2);
  SetJumpTarget(done3);
  // return nar

  MOV(16, ar_reg, R(tmp1));
  m_gpr.PutReg(DSP_REG_AR0 + reg);
  m_gpr.PutXReg(tmp1);
}

// EAX - destination address
// ECX - Base of DRAM
void DSPEmitter::dmem_write(X64Reg value)
{
  //	if (saddr == 0)
  CMP(16, R(EAX), Imm16(0x0fff));
  FixupBranch ifx = J_CC(CC_A);

  //  g_dsp.dram[addr & DSP_DRAM_MASK] = val;
  AND(16, R(EAX), Imm16(DSP_DRAM_MASK));
  MOV(64, R(ECX), ImmPtr(g_dsp.dram));
  MOV(16, MComplex(ECX, EAX, SCALE_2, 0), R(value));

  FixupBranch end = J(true);
  //	else if (saddr == 0xf)
  SetJumpTarget(ifx);
  DSPJitRegCache c(m_gpr);
  X64Reg abisafereg = m_gpr.MakeABICallSafe(value);
  MOVZX(32, 16, abisafereg, R(abisafereg));
  m_gpr.PushRegs();
  ABI_CallFunctionRR(gdsp_ifx_write, EAX, abisafereg);
  m_gpr.PopRegs();
  m_gpr.FlushRegs(c);
  SetJumpTarget(end);
}

void DSPEmitter::dmem_write_imm(u16 address, X64Reg value)
{
  switch (address >> 12)
  {
  case 0x0:  // 0xxx DRAM
    MOV(64, R(RDX), ImmPtr(g_dsp.dram));
    MOV(16, MDisp(RDX, (address & DSP_DRAM_MASK) * 2), R(value));
    break;

  case 0xf:  // Fxxx HW regs
  {
    MOV(16, R(EAX), Imm16(address));
    X64Reg abisafereg = m_gpr.MakeABICallSafe(value);
    m_gpr.PushRegs();
    ABI_CallFunctionRR(gdsp_ifx_write, EAX, abisafereg);
    m_gpr.PopRegs();
    break;
  }
  default:  // Unmapped/non-existing memory
    ERROR_LOG(DSPLLE, "%04x DSP ERROR: Write to UNKNOWN (%04x) memory", g_dsp.pc, address);
    break;
  }
}

// In:  (address) - the address to read
// Out: EAX - the result of the read (used by caller)
// ECX - Base
void DSPEmitter::imem_read(X64Reg address)
{
  //	if (addr == 0)
  CMP(16, R(address), Imm16(0x0fff));
  FixupBranch irom = J_CC(CC_A);
  //	return g_dsp.iram[addr & DSP_IRAM_MASK];
  AND(16, R(address), Imm16(DSP_IRAM_MASK));
  MOV(64, R(ECX), ImmPtr(g_dsp.iram));
  MOV(16, R(EAX), MComplex(ECX, address, SCALE_2, 0));

  FixupBranch end = J();
  SetJumpTarget(irom);
  //	else if (addr == 0x8)
  //		return g_dsp.irom[addr & DSP_IROM_MASK];
  AND(16, R(address), Imm16(DSP_IROM_MASK));
  MOV(64, R(ECX), ImmPtr(g_dsp.irom));
  MOV(16, R(EAX), MComplex(ECX, address, SCALE_2, 0));

  SetJumpTarget(end);
}

// In:  (address) - the address to read
// Out: EAX - the result of the read (used by caller)
// ECX - Base
void DSPEmitter::dmem_read(X64Reg address)
{
  //	if (saddr == 0)
  CMP(16, R(address), Imm16(0x0fff));
  FixupBranch dram = J_CC(CC_A);
  //	return g_dsp.dram[addr & DSP_DRAM_MASK];
  AND(32, R(address), Imm32(DSP_DRAM_MASK));
  MOVZX(64, 16, address, R(address));
  MOV(64, R(ECX), ImmPtr(g_dsp.dram));
  MOV(16, R(EAX), MComplex(ECX, address, SCALE_2, 0));

  FixupBranch end = J(true);
  SetJumpTarget(dram);
  //	else if (saddr == 0x1)
  CMP(16, R(address), Imm16(0x1fff));
  FixupBranch ifx = J_CC(CC_A);
  //		return g_dsp.coef[addr & DSP_COEF_MASK];
  AND(32, R(address), Imm32(DSP_COEF_MASK));
  MOVZX(64, 16, address, R(address));
  MOV(64, R(ECX), ImmPtr(g_dsp.coef));
  MOV(16, R(EAX), MComplex(ECX, address, SCALE_2, 0));

  FixupBranch end2 = J(true);
  SetJumpTarget(ifx);
  //	else if (saddr == 0xf)
  //		return gdsp_ifx_read(addr);
  DSPJitRegCache c(m_gpr);
  X64Reg abisafereg = m_gpr.MakeABICallSafe(address);
  m_gpr.PushRegs();
  ABI_CallFunctionR(gdsp_ifx_read, abisafereg);
  m_gpr.PopRegs();
  m_gpr.FlushRegs(c);
  SetJumpTarget(end);
  SetJumpTarget(end2);
}

void DSPEmitter::dmem_read_imm(u16 address)
{
  switch (address >> 12)
  {
  case 0x0:  // 0xxx DRAM
    MOV(64, R(RDX), ImmPtr(g_dsp.dram));
    MOV(16, R(EAX), MDisp(RDX, (address & DSP_DRAM_MASK) * 2));
    break;

  case 0x1:  // 1xxx COEF
    MOV(64, R(RDX), ImmPtr(g_dsp.coef));
    MOV(16, R(EAX), MDisp(RDX, (address & DSP_COEF_MASK) * 2));
    break;

  case 0xf:  // Fxxx HW regs
  {
    m_gpr.PushRegs();
    ABI_CallFunctionC16(gdsp_ifx_read, address);
    m_gpr.PopRegs();
    break;
  }
  default:  // Unmapped/non-existing memory
    ERROR_LOG(DSPLLE, "%04x DSP ERROR: Read from UNKNOWN (%04x) memory", g_dsp.pc, address);
  }
}

// Returns s64 in RAX
void DSPEmitter::get_long_prod(X64Reg long_prod)
{
  // s64 val   = (s8)(u8)g_dsp.r[DSP_REG_PRODH];
  const OpArg prod_reg = m_gpr.GetReg(DSP_REG_PROD_64);
  MOV(64, R(long_prod), prod_reg);
  m_gpr.PutReg(DSP_REG_PROD_64, false);
  // no use in keeping prod_reg any longer.
  X64Reg tmp = m_gpr.GetFreeXReg();
  MOV(64, R(tmp), R(long_prod));
  SHL(64, R(long_prod), Imm8(64 - 40));  // sign extend
  SAR(64, R(long_prod), Imm8(64 - 40));
  SHR(64, R(tmp), Imm8(48));
  SHL(64, R(tmp), Imm8(16));
  ADD(64, R(long_prod), R(tmp));
  m_gpr.PutXReg(tmp);
}

// Returns s64 in RAX
// Clobbers RCX
void DSPEmitter::get_long_prod_round_prodl(X64Reg long_prod)
{
  // s64 prod = dsp_get_long_prod();
  get_long_prod(long_prod);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (prod & 0x10000) prod = (prod + 0x8000) & ~0xffff;
  TEST(32, R(long_prod), Imm32(0x10000));
  FixupBranch jump = J_CC(CC_Z);
  ADD(64, R(long_prod), Imm32(0x8000));
  MOV(64, R(tmp), Imm64(~0xffff));
  AND(64, R(long_prod), R(tmp));
  FixupBranch _ret = J();
  // else prod = (prod + 0x7fff) & ~0xffff;
  SetJumpTarget(jump);
  ADD(64, R(long_prod), Imm32(0x7fff));
  MOV(64, R(tmp), Imm64(~0xffff));
  AND(64, R(long_prod), R(tmp));
  SetJumpTarget(_ret);
  // return prod;
  m_gpr.PutXReg(tmp);
}

// For accurate emulation, this is wrong - but the real prod registers behave
// in completely bizarre ways. Probably not meaningful to emulate them accurately.
// In: RAX = s64 val
void DSPEmitter::set_long_prod()
{
  X64Reg tmp = m_gpr.GetFreeXReg();

  MOV(64, R(tmp), Imm64(0x000000ffffffffffULL));
  AND(64, R(RAX), R(tmp));
  m_gpr.PutXReg(tmp);
  const OpArg prod_reg = m_gpr.GetReg(DSP_REG_PROD_64, false);
  //	g_dsp.r[DSP_REG_PRODL] = (u16)val;
  MOV(64, prod_reg, R(RAX));

  m_gpr.PutReg(DSP_REG_PROD_64, true);
}

// Returns s64 in RAX
// Clobbers RCX
void DSPEmitter::round_long_acc(X64Reg long_acc)
{
  // if (prod & 0x10000) prod = (prod + 0x8000) & ~0xffff;
  TEST(32, R(long_acc), Imm32(0x10000));
  FixupBranch jump = J_CC(CC_Z);
  ADD(64, R(long_acc), Imm32(0x8000));
  MOV(64, R(ECX), Imm64(~0xffff));
  AND(64, R(long_acc), R(RCX));
  FixupBranch _ret = J();
  // else prod = (prod + 0x7fff) & ~0xffff;
  SetJumpTarget(jump);
  ADD(64, R(long_acc), Imm32(0x7fff));
  MOV(64, R(RCX), Imm64(~0xffff));
  AND(64, R(long_acc), R(RCX));
  SetJumpTarget(_ret);
  // return prod;
}

// Returns s64 in acc
void DSPEmitter::get_long_acc(int _reg, X64Reg acc)
{
  const OpArg reg = m_gpr.GetReg(DSP_REG_ACC0_64 + _reg);
  MOV(64, R(acc), reg);
  m_gpr.PutReg(DSP_REG_ACC0_64 + _reg, false);
}

// In: acc = s64 val
void DSPEmitter::set_long_acc(int _reg, X64Reg acc)
{
  const OpArg reg = m_gpr.GetReg(DSP_REG_ACC0_64 + _reg, false);
  MOV(64, reg, R(acc));
  m_gpr.PutReg(DSP_REG_ACC0_64 + _reg);
}

// Returns s16 in AX
void DSPEmitter::get_acc_l(int _reg, X64Reg acl, bool sign)
{
  //	return g_dsp.r[DSP_REG_ACM0 + _reg];
  m_gpr.ReadReg(_reg + DSP_REG_ACL0, acl, sign ? RegisterExtension::Sign : RegisterExtension::Zero);
}

void DSPEmitter::set_acc_l(int _reg, const OpArg& arg)
{
  //	return g_dsp.r[DSP_REG_ACM0 + _reg];
  m_gpr.WriteReg(_reg + DSP_REG_ACL0, arg);
}

// Returns s16 in AX
void DSPEmitter::get_acc_m(int _reg, X64Reg acm, bool sign)
{
  //	return g_dsp.r[DSP_REG_ACM0 + _reg];
  m_gpr.ReadReg(_reg + DSP_REG_ACM0, acm, sign ? RegisterExtension::Sign : RegisterExtension::Zero);
}

// In: s16 in AX
void DSPEmitter::set_acc_m(int _reg, const OpArg& arg)
{
  //	return g_dsp.r.ac[_reg].m;
  m_gpr.WriteReg(_reg + DSP_REG_ACM0, arg);
}

// Returns s16 in AX
void DSPEmitter::get_acc_h(int _reg, X64Reg ach, bool sign)
{
  //	return g_dsp.r.ac[_reg].h;
  m_gpr.ReadReg(_reg + DSP_REG_ACH0, ach, sign ? RegisterExtension::Sign : RegisterExtension::Zero);
}

// In: s16 in AX
void DSPEmitter::set_acc_h(int _reg, const OpArg& arg)
{
  //	return g_dsp.r[DSP_REG_ACM0 + _reg];
  m_gpr.WriteReg(_reg + DSP_REG_ACH0, arg);
}

// Returns u32 in EAX
void DSPEmitter::get_long_acx(int _reg, X64Reg acx)
{
  //	return ((u32)g_dsp.r[DSP_REG_AXH0 + _reg] << 16) | g_dsp.r[DSP_REG_AXL0 + _reg];
  m_gpr.ReadReg(_reg + DSP_REG_AX0_32, acx, RegisterExtension::Sign);
}

// Returns s16 in EAX
void DSPEmitter::get_ax_l(int _reg, X64Reg axl)
{
  //	return (s16)g_dsp.r[DSP_REG_AXL0 + _reg];
  m_gpr.ReadReg(_reg + DSP_REG_AXL0, axl, RegisterExtension::Sign);
}

// Returns s16 in EAX
void DSPEmitter::get_ax_h(int _reg, X64Reg axh)
{
  //	return (s16)g_dsp.r[DSP_REG_AXH0 + _reg];
  m_gpr.ReadReg(_reg + DSP_REG_AXH0, axh, RegisterExtension::Sign);
}

}  // namespace x86
}  // namespace JIT
}  // namespace DSP
