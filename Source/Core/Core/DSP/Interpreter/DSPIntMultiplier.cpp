// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

// Multiplier and product register control

#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

namespace DSP::Interpreter
{
namespace
{
// Only MULX family instructions have unsigned/mixed support.
s64 dsp_get_multiply_prod(u16 a, u16 b, u8 sign)
{
  s64 prod;

  if ((sign == 1) && (g_dsp.r.sr & SR_MUL_UNSIGNED))  // unsigned
    prod = (u32)(a * b);
  else if ((sign == 2) && (g_dsp.r.sr & SR_MUL_UNSIGNED))  // mixed
    prod = a * (s16)b;
  else
    prod = (s16)a * (s16)b;  // signed

  // Conditionally multiply by 2.
  if ((g_dsp.r.sr & SR_MUL_MODIFY) == 0)
    prod <<= 1;

  return prod;
}

s64 dsp_multiply(u16 a, u16 b, u8 sign = 0)
{
  s64 prod = dsp_get_multiply_prod(a, b, sign);
  return prod;
}

s64 dsp_multiply_add(u16 a, u16 b, u8 sign = 0)
{
  s64 prod = dsp_get_long_prod() + dsp_get_multiply_prod(a, b, sign);
  return prod;
}

s64 dsp_multiply_sub(u16 a, u16 b, u8 sign = 0)
{
  s64 prod = dsp_get_long_prod() - dsp_get_multiply_prod(a, b, sign);
  return prod;
}

s64 dsp_multiply_mulx(u8 axh0, u8 axh1, u16 val1, u16 val2)
{
  s64 result;

  if ((axh0 == 0) && (axh1 == 0))
    result = dsp_multiply(val1, val2, 1);  // unsigned support ON if both ax?.l regs are used
  else if ((axh0 == 0) && (axh1 == 1))
    result = dsp_multiply(val1, val2, 2);  // mixed support ON (u16)axl.0  * (s16)axh.1
  else if ((axh0 == 1) && (axh1 == 0))
    result = dsp_multiply(val2, val1, 2);  // mixed support ON (u16)axl.1  * (s16)axh.0
  else
    result = dsp_multiply(val1, val2, 0);  // unsigned support OFF if both ax?.h regs are used

  return result;
}
}  // Anonymous namespace

// CLRP
// 1000 0100 xxxx xxxx
// Clears product register $prod.
// Magic numbers taken from duddie's doc
//
// 00ff_(fff0 + 0010)_0000 = 0100_0000_0000, conveniently, lower 40bits = 0
//
// It's not ok, to just zero all of them, correct values should be set because of
// direct use of prod regs by AX/AXWII (look @that part of ucode).
void clrp(const UDSPInstruction opc)
{
  ZeroWriteBackLog();

  g_dsp.r.prod.l = 0x0000;
  g_dsp.r.prod.m = 0xfff0;
  g_dsp.r.prod.h = 0x00ff;
  g_dsp.r.prod.m2 = 0x0010;
}

// TSTPROD
// 1000 0101 xxxx xxxx
// Test prod regs value.
//
// flags out: --xx xx0x
void tstprod(const UDSPInstruction opc)
{
  s64 prod = dsp_get_long_prod();
  Update_SR_Register64(prod);
  ZeroWriteBackLog();
}

//----

// MOVP $acD
// 0110 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD register.
//
// flags out: --xx xx0x
void movp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_prod();

  ZeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(acc);
}

// MOVNP $acD
// 0111 111d xxxx xxxx
// Moves negative of multiply product from $prod register to accumulator
// $acD register.
//
// flags out: --xx xx0x
void movnp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = -dsp_get_long_prod();

  ZeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(acc);
}

// MOVPZ $acD
// 1111 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD
// register and sets (rounds) $acD.l to 0
//
// flags out: --xx xx0x
void movpz(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;

  s64 acc = dsp_get_long_prod_round_prodl();

  ZeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(acc);
}

// ADDPAXZ $acD, $axS
// 1111 10sd xxxx xxxx
// Adds secondary accumulator $axS to product register and stores result
// in accumulator register. Low 16-bits of $acD ($acD.l) are set (round) to 0.
//
// TODO: ugly code and still small error here (+/- 1 in .m - randomly)
// flags out: --xx xx0x
void addpaxz(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s64 oldprod = dsp_get_long_prod();
  s64 prod = dsp_get_long_prod_round_prodl();
  s64 ax = dsp_get_long_acx(sreg);
  s64 res = prod + (ax & ~0xffff);

  ZeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(oldprod, res), false);
}

//----

// MULAXH
// 1000 0011 xxxx xxxx
// Multiply $ax0.h by $ax0.h
void mulaxh(const UDSPInstruction opc)
{
  s64 prod = dsp_multiply(dsp_get_ax_h(0), dsp_get_ax_h(0));

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

//----

// MUL $axS.l, $axS.h
// 1001 s000 xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed).
void mul(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 11) & 0x1;

  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply(axh, axl);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MULAC $axS.l, $axS.h, $acR
// 1001 s10r xxxx xxxx
// Add product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
//
// flags out: --xx xx0x
void mulac(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 11) & 0x1;

  s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply(axl, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULMV $axS.l, $axS.h, $acR
// 1001 s11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
//
// flags out: --xx xx0x
void mulmv(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 11) & 0x1);

  s64 acc = dsp_get_long_prod();
  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply(axl, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULMVZ $axS.l, $axS.h, $acR
// 1001 s01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply low part $axS.l of secondary
// accumulator $axS by high part $axS.h of secondary accumulator $axS (treat
// them both as signed).
//
// flags out: --xx xx0x
void mulmvz(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 11) & 0x1;

  s64 acc = dsp_get_long_prod_round_prodl();
  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply(axl, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

//----

// MULX $ax0.S, $ax1.T
// 101s t000 xxxx xxxx
// Multiply one part $ax0 by one part $ax1.
// Part is selected by S and T bits. Zero selects low part, one selects high part.
void mulx(const UDSPInstruction opc)
{
  u8 treg = ((opc >> 11) & 0x1);
  u8 sreg = ((opc >> 12) & 0x1);

  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MULXAC $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Add product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.
//
// flags out: --xx xx0x
void mulxac(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.
//
// flags out: --xx xx0x
void mulxmv(const UDSPInstruction opc)
{
  u8 rreg = ((opc >> 8) & 0x1);
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_prod();
  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULXMVZ $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply one part $ax0 by one part $ax1
// Part is selected by S and T bits. Zero selects low part,
// one selects high part.
//
// flags out: --xx xx0x
void mulxmvz(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_prod_round_prodl();
  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

//----

// MULC $acS.m, $axT.h
// 110s t000 xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS (treat them both as signed).
void mulc(const UDSPInstruction opc)
{
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MULCAC $acS.m, $axT.h, $acR
// 110s	t10r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS  (treat them both as signed). Add product
// register before multiplication to accumulator $acR.
//
// flags out: --xx xx0x
void mulcac(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULCMV $acS.m, $axT.h, $acR
// 110s t11r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR.
// possible mistake in duddie's doc axT.h rather than axS.h
//
// flags out: --xx xx0x
void mulcmv(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_prod();
  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// MULCMVZ $acS.m, $axT.h, $acR
// 110s	t01r xxxx xxxx
// (fixed possible bug in duddie's description, s->t)
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR, set (round) low part of
// accumulator $acR.l to zero.
//
// flags out: --xx xx0x
void mulcmvz(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x1;
  u8 treg = (opc >> 11) & 0x1;
  u8 sreg = (opc >> 12) & 0x1;

  s64 acc = dsp_get_long_prod_round_prodl();
  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

//----

// MADDX ax0.S ax1.T
// 1110 00st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and add result to product register.
void maddx(const UDSPInstruction opc)
{
  u8 treg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_add(val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MSUBX $(0x18+S*2), $(0x19+T*2)
// 1110 01st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and subtract result from product register.
void msubx(const UDSPInstruction opc)
{
  u8 treg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
  u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
  s64 prod = dsp_multiply_sub(val1, val2);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MADDC $acS.m, $axT.h
// 1110 10st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and add result to product
// register.
void maddc(const UDSPInstruction opc)
{
  u8 treg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply_add(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MSUBC $acS.m, $axT.h
// 1110 11st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and subtract result from
// product register.
void msubc(const UDSPInstruction opc)
{
  u8 treg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  u16 accm = dsp_get_acc_m(sreg);
  u16 axh = dsp_get_ax_h(treg);
  s64 prod = dsp_multiply_sub(accm, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MADD $axS.l, $axS.h
// 1111 001s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and add
// result to product register.
void madd(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 8) & 0x1;

  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply_add(axl, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}

// MSUB $axS.l, $axS.h
// 1111 011s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and
// subtract result from product register.
void msub(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 8) & 0x1;

  u16 axl = dsp_get_ax_l(sreg);
  u16 axh = dsp_get_ax_h(sreg);
  s64 prod = dsp_multiply_sub(axl, axh);

  ZeroWriteBackLog();

  dsp_set_long_prod(prod);
}
}  // namespace DSP::Interpreter
