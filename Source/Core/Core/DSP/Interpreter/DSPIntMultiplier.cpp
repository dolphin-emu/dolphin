// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional copyrights go to Duddie and Tratax (c) 2004

// Multiplier and product register control

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"

namespace DSP::Interpreter
{
// CLRP
// 1000 0100 xxxx xxxx
// Clears product register $prod.
// Magic numbers taken from duddie's doc
//
// 00ff_(fff0 + 0010)_0000 = 0100_0000_0000, conveniently, lower 40bits = 0
//
// It's not ok, to just zero all of them, correct values should be set because of
// direct use of prod regs by AX/AXWII (look @that part of ucode).
void Interpreter::clrp(const UDSPInstruction)
{
  ZeroWriteBackLog();

  auto& state = m_dsp_core.DSPState();
  state.r.prod.l = 0x0000;
  state.r.prod.m = 0xfff0;
  state.r.prod.h = 0x00ff;
  state.r.prod.m2 = 0x0010;
}

// TSTPROD
// 1000 0101 xxxx xxxx
// Test prod regs value.
//
// flags out: --xx xx0x
void Interpreter::tstprod(const UDSPInstruction)
{
  const s64 prod = GetLongProduct();
  UpdateSR64(prod);
  ZeroWriteBackLog();
}

//----

// MOVP $acD
// 0110 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD register.
//
// flags out: --xx xx0x
void Interpreter::movp(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 8) & 0x1;
  const s64 acc = GetLongProduct();

  ZeroWriteBackLog();

  SetLongAcc(dreg, acc);
  UpdateSR64(acc);
}

// MOVNP $acD
// 0111 111d xxxx xxxx
// Moves negative of multiply product from $prod register to accumulator
// $acD register.
//
// flags out: --xx xx0x
void Interpreter::movnp(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 8) & 0x1;
  const s64 acc = -GetLongProduct();

  ZeroWriteBackLog();

  SetLongAcc(dreg, acc);
  UpdateSR64(acc);
}

// MOVPZ $acD
// 1111 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD
// register and sets (rounds) $acD.l to 0
//
// flags out: --xx xx0x
void Interpreter::movpz(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 8) & 0x01;
  const s64 acc = GetLongProductRounded();

  ZeroWriteBackLog();

  SetLongAcc(dreg, acc);
  UpdateSR64(acc);
}

// ADDPAXZ $acD, $axS
// 1111 10sd xxxx xxxx
// Adds secondary accumulator $axS to product register and stores result
// in accumulator register. Low 16-bits of $acD ($acD.l) are set (round) to 0.
//
// TODO: ugly code and still small error here (+/- 1 in .m - randomly)
// flags out: --xx xx0x
void Interpreter::addpaxz(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 9) & 0x1;

  const s64 oldprod = GetLongProduct();
  const s64 prod = GetLongProductRounded();
  const s64 ax = GetLongACX(sreg);
  s64 res = prod + (ax & ~0xffff);

  ZeroWriteBackLog();

  SetLongAcc(dreg, res);
  res = GetLongAcc(dreg);
  UpdateSR64(res, isCarryAdd(oldprod, res), false);  // TODO: Why doesn't this set the overflow bit?
}

//----

// MULAXH
// 1000 0011 xxxx xxxx
// Multiply $ax0.h by $ax0.h
void Interpreter::mulaxh(const UDSPInstruction)
{
  const s16 value = GetAXHigh(0);
  const s64 prod = Multiply(value, value);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

//----

// MUL $axS.l, $axS.h
// 1001 s000 xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed).
void Interpreter::mul(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 11) & 0x1;
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = Multiply(axh, axl);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MULAC $axS.l, $axS.h, $acR
// 1001 s10r xxxx xxxx
// Add product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
//
// flags out: --xx xx0x
void Interpreter::mulac(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 11) & 0x1;

  const s64 acc = GetLongAcc(rreg) + GetLongProduct();
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = Multiply(axl, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULMV $axS.l, $axS.h, $acR
// 1001 s11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
//
// flags out: --xx xx0x
void Interpreter::mulmv(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 sreg = ((opc >> 11) & 0x1);

  const s64 acc = GetLongProduct();
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = Multiply(axl, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULMVZ $axS.l, $axS.h, $acR
// 1001 s01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply low part $axS.l of secondary
// accumulator $axS by high part $axS.h of secondary accumulator $axS (treat
// them both as signed).
//
// flags out: --xx xx0x
void Interpreter::mulmvz(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 11) & 0x1;

  const s64 acc = GetLongProductRounded();
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = Multiply(axl, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

//----

// MULX $ax0.S, $ax1.T
// 101s t000 xxxx xxxx
// Multiply one part $ax0 by one part $ax1.
// Part is selected by S and T bits. Zero selects low part, one selects high part.
void Interpreter::mulx(const UDSPInstruction opc)
{
  const u8 treg = ((opc >> 11) & 0x1);
  const u8 sreg = ((opc >> 12) & 0x1);

  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplyMulX(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MULXAC $ax0.S, $ax1.T, $acR
// 101s t10r xxxx xxxx
// Add product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.
//
// flags out: --xx xx0x
void Interpreter::mulxac(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongAcc(rreg) + GetLongProduct();
  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplyMulX(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.
//
// flags out: --xx xx0x
void Interpreter::mulxmv(const UDSPInstruction opc)
{
  const u8 rreg = ((opc >> 8) & 0x1);
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongProduct();
  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplyMulX(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULXMVZ $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply one part $ax0 by one part $ax1
// Part is selected by S and T bits. Zero selects low part,
// one selects high part.
//
// flags out: --xx xx0x
void Interpreter::mulxmvz(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongProductRounded();
  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplyMulX(sreg, treg, val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

//----

// MULC $acS.m, $axT.h
// 110s t000 xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS (treat them both as signed).
void Interpreter::mulc(const UDSPInstruction opc)
{
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = Multiply(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MULCAC $acS.m, $axT.h, $acR
// 110s t10r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS  (treat them both as signed). Add product
// register before multiplication to accumulator $acR.
//
// flags out: --xx xx0x
void Interpreter::mulcac(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongAcc(rreg) + GetLongProduct();
  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = Multiply(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULCMV $acS.m, $axT.h, $acR
// 110s t11r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR.
//
// flags out: --xx xx0x
void Interpreter::mulcmv(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongProduct();
  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = Multiply(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

// MULCMVZ $acS.m, $axT.h, $acR
// 110s t01r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR, set (round) low part of
// accumulator $acR.l to zero.
//
// flags out: --xx xx0x
void Interpreter::mulcmvz(const UDSPInstruction opc)
{
  const u8 rreg = (opc >> 8) & 0x1;
  const u8 treg = (opc >> 11) & 0x1;
  const u8 sreg = (opc >> 12) & 0x1;

  const s64 acc = GetLongProductRounded();
  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = Multiply(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
  SetLongAcc(rreg, acc);
  UpdateSR64(GetLongAcc(rreg));
}

//----

// MADDX ax0.S ax1.T
// 1110 00st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and add result to product register.
void Interpreter::maddx(const UDSPInstruction opc)
{
  const u8 treg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 9) & 0x1;

  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplyAdd(val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MSUBX $(0x18+S*2), $(0x19+T*2)
// 1110 01st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and subtract result from product register.
void Interpreter::msubx(const UDSPInstruction opc)
{
  const u8 treg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 9) & 0x1;

  const u16 val1 = (sreg == 0) ? GetAXLow(0) : GetAXHigh(0);
  const u16 val2 = (treg == 0) ? GetAXLow(1) : GetAXHigh(1);
  const s64 prod = MultiplySub(val1, val2);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MADDC $acS.m, $axT.h
// 1110 10st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and add result to product
// register.
void Interpreter::maddc(const UDSPInstruction opc)
{
  const u8 treg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 9) & 0x1;

  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = MultiplyAdd(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MSUBC $acS.m, $axT.h
// 1110 11st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and subtract result from
// product register.
void Interpreter::msubc(const UDSPInstruction opc)
{
  const u8 treg = (opc >> 8) & 0x1;
  const u8 sreg = (opc >> 9) & 0x1;

  const u16 accm = GetAccMid(sreg);
  const u16 axh = GetAXHigh(treg);
  const s64 prod = MultiplySub(accm, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MADD $axS.l, $axS.h
// 1111 001s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and add
// result to product register.
void Interpreter::madd(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 8) & 0x1;
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = MultiplyAdd(axl, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}

// MSUB $axS.l, $axS.h
// 1111 011s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and
// subtract result from product register.
void Interpreter::msub(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 8) & 0x1;
  const u16 axl = GetAXLow(sreg);
  const u16 axh = GetAXHigh(sreg);
  const s64 prod = MultiplySub(axl, axh);

  ZeroWriteBackLog();

  SetLongProduct(prod);
}
}  // namespace DSP::Interpreter
