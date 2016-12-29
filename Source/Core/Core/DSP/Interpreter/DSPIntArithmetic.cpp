// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

// Arithmetic and accumulator control.

namespace DSP
{
namespace Interpreter
{
// CLR $acR
// 1000 r001 xxxx xxxx
// Clears accumulator $acR
//
// flags out: --10 0100
void clr(const UDSPInstruction opc)
{
  u8 reg = (opc >> 11) & 0x1;

  dsp_set_long_acc(reg, 0);
  Update_SR_Register64(0);
  zeroWriteBackLog();
}

// CLRL $acR.l
// 1111 110r xxxx xxxx
// Clears (and rounds!) $acR.l - low 16 bits of accumulator $acR.
//
// flags out: --xx xx00
void clrl(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  s64 acc = dsp_round_long_acc(dsp_get_long_acc(reg));

  zeroWriteBackLog();

  dsp_set_long_acc(reg, acc);
  Update_SR_Register64(acc);
}

//----

// ANDCF $acD.m, #I
// 0000 001r 1100 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logic AND of
// accumulator mid part $acD.m with immediate value I is equal I.
//
// flags out: -x-- ----
void andcf(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;

  u16 imm = dsp_fetch_code();
  u16 val = dsp_get_acc_m(reg);
  Update_SR_LZ(((val & imm) == imm) ? true : false);
}

// ANDF $acD.m, #I
// 0000 001r 1010 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logical AND
// operation of accumulator mid part $acD.m with immediate value I is equal
// immediate value 0.
//
// flags out: -x-- ----
void andf(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;

  u16 imm = dsp_fetch_code();
  u16 val = dsp_get_acc_m(reg);
  Update_SR_LZ(((val & imm) == 0) ? true : false);
}

//----

// TST
// 1011 r001 xxxx xxxx
// Test accumulator %acR.
//
// flags out: --xx xx00
void tst(const UDSPInstruction opc)
{
  u8 reg = (opc >> 11) & 0x1;

  s64 acc = dsp_get_long_acc(reg);
  Update_SR_Register64(acc);
  zeroWriteBackLog();
}

// TSTAXH $axR.h
// 1000 011r xxxx xxxx
// Test high part of secondary accumulator $axR.h.
//
// flags out: --x0 xx00
void tstaxh(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;

  s16 val = dsp_get_ax_h(reg);
  Update_SR_Register16(val);
  zeroWriteBackLog();
}

//----

// CMP
// 1000 0010 xxxx xxxx
// Compares accumulator $ac0 with accumulator $ac1.
//
// flags out: x-xx xxxx
void cmp(const UDSPInstruction opc)
{
  s64 acc0 = dsp_get_long_acc(0);
  s64 acc1 = dsp_get_long_acc(1);
  s64 res = dsp_convert_long_acc(acc0 - acc1);

  Update_SR_Register64(res, isCarry2(acc0, res),
                       isOverflow(acc0, -acc1, res));  // CF -> influence on ABS/0xa100
  zeroWriteBackLog();
}

// CMPAR $acS axR.h
// 110r s001 xxxx xxxx
// Compares accumulator $acS with accumulator axR.h.
// Not described by Duddie's doc - at least not as a separate instruction.
//
// flags out: x-xx xxxx
void cmpar(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 12) & 0x1;
  u8 sreg = (opc >> 11) & 0x1;

  s64 sr = dsp_get_long_acc(sreg);
  s64 rr = (s16)g_dsp.r.ax[rreg].h;
  rr <<= 16;
  s64 res = dsp_convert_long_acc(sr - rr);

  Update_SR_Register64(res, isCarry2(sr, res), isOverflow(sr, -rr, res));
  zeroWriteBackLog();
}

// CMPI $amD, #I
// 0000 001r 1000 0000
// iiii iiii iiii iiii
// Compares mid accumulator $acD.hm ($amD) with sign extended immediate value I.
// Although flags are being set regarding whole accumulator register.
//
// flags out: x-xx xxxx
void cmpi(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;

  s64 val = dsp_get_long_acc(reg);
  s64 imm = (s64)(s16)dsp_fetch_code()
            << 16;  // Immediate is considered to be at M level in the 40-bit accumulator.
  s64 res = dsp_convert_long_acc(val - imm);

  Update_SR_Register64(res, isCarry2(val, res), isOverflow(val, -imm, res));
}

// CMPIS $acD, #I
// 0000 011d iiii iiii
// Compares accumulator with short immediate. Comaprison is executed
// by subtracting short immediate (8bit sign extended) from mid accumulator
// $acD.hm and computing flags based on whole accumulator $acD.
//
// flags out: x-xx xxxx
void cmpis(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(areg);
  s64 val = (s8)opc;
  val <<= 16;
  s64 res = dsp_convert_long_acc(acc - val);

  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -val, res));
}

//----

// XORR $acD.m, $axS.h
// 0011 00sd 0xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with
// high part of secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void xorr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m ^ g_dsp.r.ax[sreg].h;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// ANDR $acD.m, $axS.h
// 0011 01sd 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void andr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m & g_dsp.r.ax[sreg].h;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// ORR $acD.m, $axS.h
// 0011 10sd 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void orr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m | g_dsp.r.ax[sreg].h;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// ANDC $acD.m, $ac(1-D).m
// 0011 110d 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void andc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m & g_dsp.r.ac[1 - dreg].m;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// ORC $acD.m, $ac(1-D).m
// 0011 111d 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void orc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m | g_dsp.r.ac[1 - dreg].m;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// XORC $acD.m
// 0011 000d 1xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with $ac(1-D).m
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void xorc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m ^ g_dsp.r.ac[1 - dreg].m;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// NOT $acD.m
// 0011 001d 1xxx xxxx
// Invert all bits in dest reg, aka xor with 0xffff
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void notc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u16 accm = g_dsp.r.ac[dreg].m ^ 0xffff;

  zeroWriteBackLogPreserveAcc(dreg);

  g_dsp.r.ac[dreg].m = accm;
  Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
}

// XORI $acD.m, #I
// 0000 001r 0010 0000
// iiii iiii iiii iiii
// Logic exclusive or (XOR) of accumulator mid part $acD.m with
// immediate value I.
//
// flags out: --xx xx00
void xori(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  u16 imm = dsp_fetch_code();
  g_dsp.r.ac[reg].m ^= imm;

  Update_SR_Register16((s16)g_dsp.r.ac[reg].m, false, false, isOverS32(dsp_get_long_acc(reg)));
}

// ANDI $acD.m, #I
// 0000 001r 0100 0000
// iiii iiii iiii iiii
// Logic AND of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void andi(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  u16 imm = dsp_fetch_code();
  g_dsp.r.ac[reg].m &= imm;

  Update_SR_Register16((s16)g_dsp.r.ac[reg].m, false, false, isOverS32(dsp_get_long_acc(reg)));
}

// ORI $acD.m, #I
// 0000 001r 0110 0000
// iiii iiii iiii iiii
// Logic OR of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void ori(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  u16 imm = dsp_fetch_code();
  g_dsp.r.ac[reg].m |= imm;

  Update_SR_Register16((s16)g_dsp.r.ac[reg].m, false, false, isOverS32(dsp_get_long_acc(reg)));
}

//----

// ADDR $acD.M, $axS.L
// 0100 0ssd xxxx xxxx
// Adds register $axS.L to accumulator $acD.M register.
//
// flags out: x-xx xxxx
void addr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  s64 acc = dsp_get_long_acc(dreg);
  s64 ax = 0;

  switch (sreg)
  {
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXL0].l;
    break;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXH0].h;
    break;
  default:
    ax = 0;
    break;
  }

  ax <<= 16;
  s64 res = acc + ax;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, ax, res));
}

// ADDAX $acD, $axS
// 0100 10sd xxxx xxxx
// Adds secondary accumulator $axS to accumulator register $acD.
//
// flags out: x-xx xxxx
void addax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 ax = dsp_get_long_acx(sreg);
  s64 res = acc + ax;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, ax, res));
}

// ADD $acD, $ac(1-D)
// 0100 110d xxxx xxxx
// Adds accumulator $ac(1-D) to accumulator register $acD.
//
// flags out: x-xx xxxx
void add(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc0 = dsp_get_long_acc(dreg);
  s64 acc1 = dsp_get_long_acc(1 - dreg);
  s64 res = acc0 + acc1;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc0, res), isOverflow(acc0, acc1, res));
}

// ADDP $acD
// 0100 111d xxxx xxxx
// Adds product register to accumulator register.
//
// flags out: x-xx xxxx
void addp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 prod = dsp_get_long_prod();
  s64 res = acc + prod;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, prod, res));
}

// ADDAXL $acD, $axS.l
// 0111 00sd xxxx xxxx
// Adds secondary accumulator $axS.l to accumulator register $acD.
// should be unsigned values!!
//
// flags out: x-xx xxxx
void addaxl(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 9) & 0x1;
  u8 dreg = (opc >> 8) & 0x1;

  u64 acc = dsp_get_long_acc(dreg);
  u16 acx = (u16)dsp_get_ax_l(sreg);

  u64 res = acc + acx;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, (s64)res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64((s64)res, isCarry(acc, res), isOverflow((s64)acc, (s64)acx, (s64)res));
}

// ADDI $amR, #I
// 0000 001r 0000 0000
// iiii iiii iiii iiii
// Adds immediate (16-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void addi(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(areg);
  s64 imm = (s16)dsp_fetch_code();
  imm <<= 16;
  s64 res = acc + imm;

  dsp_set_long_acc(areg, res);
  res = dsp_get_long_acc(areg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, imm, res));
}

// ADDIS $acD, #I
// 0000 010d iiii iiii
// Adds short immediate (8-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void addis(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 imm = (s8)(u8)opc;
  imm <<= 16;
  s64 res = acc + imm;

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, imm, res));
}

// INCM $acsD
// 0111 010d xxxx xxxx
// Increment 24-bit mid-accumulator $acsD.
//
// flags out: x-xx xxxx
void incm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 sub = 0x10000;
  s64 acc = dsp_get_long_acc(dreg);
  s64 res = acc + sub;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, sub, res));
}

// INC $acD
// 0111 011d xxxx xxxx
// Increment accumulator $acD.
//
// flags out: x-xx xxxx
void inc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 res = acc + 1;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, 1, res));
}

//----

// SUBR $acD.M, $axS.L
// 0101 0ssd xxxx xxxx
// Subtracts register $axS.L from accumulator $acD.M register.
//
// flags out: x-xx xxxx
void subr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  s64 acc = dsp_get_long_acc(dreg);
  s64 ax = 0;

  switch (sreg)
  {
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXL0].l;
    break;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXH0].h;
    break;
  default:
    ax = 0;
    break;
  }

  ax <<= 16;
  s64 res = acc - ax;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -ax, res));
}

// SUBAX $acD, $axS
// 0101 10sd xxxx xxxx
// Subtracts secondary accumulator $axS from accumulator register $acD.
//
// flags out: x-xx xxxx
void subax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 acx = dsp_get_long_acx(sreg);
  s64 res = acc - acx;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -acx, res));
}

// SUB $acD, $ac(1-D)
// 0101 110d xxxx xxxx
// Subtracts accumulator $ac(1-D) from accumulator register $acD.
//
// flags out: x-xx xxxx
void sub(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc1 = dsp_get_long_acc(dreg);
  s64 acc2 = dsp_get_long_acc(1 - dreg);
  s64 res = acc1 - acc2;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc1, res), isOverflow(acc1, -acc2, res));
}

// SUBP $acD
// 0101 111d xxxx xxxx
// Subtracts product register from accumulator register.
//
// flags out: x-xx xxxx
void subp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  s64 prod = dsp_get_long_prod();
  s64 res = acc - prod;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -prod, res));
}

// DECM $acsD
// 0111 100d xxxx xxxx
// Decrement 24-bit mid-accumulator $acsD.
//
// flags out: x-xx xxxx
void decm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;

  s64 sub = 0x10000;
  s64 acc = dsp_get_long_acc(dreg);
  s64 res = acc - sub;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -sub, res));
}

// DEC $acD
// 0111 101d xxxx xxxx
// Decrement accumulator $acD.
//
// flags out: x-xx xxxx
void dec(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;

  s64 acc = dsp_get_long_acc(dreg);
  s64 res = acc - 1;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, res);
  res = dsp_get_long_acc(dreg);
  Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -1, res));
}

//----

// NEG $acD
// 0111 110d xxxx xxxx
// Negate accumulator $acD.
//
// flags out: --xx xx00
void neg(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);
  acc = 0 - acc;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

// ABS  $acD
// 1010 d001 xxxx xxxx
// absolute value of $acD
//
// flags out: --xx xx00
void abs(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 11) & 0x1;

  s64 acc = dsp_get_long_acc(dreg);

  if (acc < 0)
    acc = 0 - acc;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}
//----

// MOVR $acD, $axS.R
// 0110 0srd xxxx xxxx
// Moves register $axS.R (sign extended) to middle accumulator $acD.hm.
// Sets $acD.l to 0.
//
// flags out: --xx xx00
void movr(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  s64 ax = 0;
  switch (sreg)
  {
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXL0].l;
    break;
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    ax = (s16)g_dsp.r.ax[sreg - DSP_REG_AXH0].h;
    break;
  default:
    ax = 0;
    break;
  }
  ax <<= 16;
  ax &= ~0xffff;

  zeroWriteBackLog();

  dsp_set_long_acc(areg, ax);
  Update_SR_Register64(ax);
}

// MOVAX $acD, $axS
// 0110 10sd xxxx xxxx
// Moves secondary accumulator $axS to accumulator $axD.
//
// flags out: --xx xx00
void movax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s64 acx = dsp_get_long_acx(sreg);

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acx);
  Update_SR_Register64(acx);
}

// MOV $acD, $ac(1-D)
// 0110 110d xxxx xxxx
// Moves accumulator $ax(1-D) to accumulator $axD.
//
// flags out: --x0 xx00
void mov(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u64 acc = dsp_get_long_acc(1 - dreg);

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(acc);
}

//----

// LSL16 $acR
// 1111 000r xxxx xxxx
// Logically shifts left accumulator $acR by 16.
//
// flags out: --xx xx00
void lsl16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;

  s64 acc = dsp_get_long_acc(areg);
  acc <<= 16;

  zeroWriteBackLog();

  dsp_set_long_acc(areg, acc);
  Update_SR_Register64(dsp_get_long_acc(areg));
}

// LSR16 $acR
// 1111 010r xxxx xxxx
// Logically shifts right accumulator $acR by 16.
//
// flags out: --xx xx00
void lsr16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;

  u64 acc = dsp_get_long_acc(areg);
  acc &=
      0x000000FFFFFFFFFFULL;  // Lop off the extraneous sign extension our 64-bit fake accum causes
  acc >>= 16;

  zeroWriteBackLog();

  dsp_set_long_acc(areg, (s64)acc);
  Update_SR_Register64(dsp_get_long_acc(areg));
}

// ASR16 $acR
// 1001 r001 xxxx xxxx
// Arithmetically shifts right accumulator $acR by 16.
//
// flags out: --xx xx00
void asr16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 11) & 0x1;

  s64 acc = dsp_get_long_acc(areg);
  acc >>= 16;

  zeroWriteBackLog();

  dsp_set_long_acc(areg, acc);
  Update_SR_Register64(dsp_get_long_acc(areg));
}

// LSL $acR, #I
// 0001 010r 00ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
//
// flags out: --xx xx00
void lsl(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift = opc & 0x3f;
  u64 acc = dsp_get_long_acc(rreg);

  acc <<= shift;

  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// LSR $acR, #I
// 0001 010r 01ii iiii
// Logically shifts right accumulator $acR by number specified by value
// calculated by negating sign extended bits 0-6.
//
// flags out: --xx xx00
void lsr(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift;
  u64 acc = dsp_get_long_acc(rreg);
  acc &=
      0x000000FFFFFFFFFFULL;  // Lop off the extraneous sign extension our 64-bit fake accum causes

  if ((opc & 0x3f) == 0)
    shift = 0;
  else
    shift = 0x40 - (opc & 0x3f);

  acc >>= shift;

  dsp_set_long_acc(rreg, (s64)acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// ASL $acR, #I
// 0001 010r 10ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
//
// flags out: --xx xx00
void asl(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift = opc & 0x3f;
  u64 acc = dsp_get_long_acc(rreg);

  acc <<= shift;

  dsp_set_long_acc(rreg, acc);
  Update_SR_Register64(dsp_get_long_acc(rreg));
}

// ASR $acR, #I
// 0001 010r 11ii iiii
// Arithmetically shifts right accumulator $acR by number specified by
// value calculated by negating sign extended bits 0-6.
//
// flags out: --xx xx00
void asr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;
  u16 shift;

  if ((opc & 0x3f) == 0)
    shift = 0;
  else
    shift = 0x40 - (opc & 0x3f);

  // arithmetic shift
  s64 acc = dsp_get_long_acc(dreg);
  acc >>= shift;

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

// LSRN  (fixed parameters)
// 0000 0010 1100 1010
// Logically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M
// (if value negative, becomes left shift).
//
// flags out: --xx xx00
void lsrn(const UDSPInstruction opc)
{
  s16 shift;
  u16 accm = (u16)dsp_get_acc_m(1);
  u64 acc = dsp_get_long_acc(0);
  acc &= 0x000000FFFFFFFFFFULL;

  if ((accm & 0x3f) == 0)
    shift = 0;
  else if (accm & 0x40)
    shift = -0x40 + (accm & 0x3f);
  else
    shift = accm & 0x3f;

  if (shift > 0)
  {
    acc >>= shift;
  }
  else if (shift < 0)
  {
    acc <<= -shift;
  }

  dsp_set_long_acc(0, (s64)acc);
  Update_SR_Register64(dsp_get_long_acc(0));
}

// ASRN  (fixed parameters)
// 0000 0010 1100 1011
// Arithmetically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M
// (if value negative, becomes left shift).
//
// flags out: --xx xx00
void asrn(const UDSPInstruction opc)
{
  s16 shift;
  u16 accm = (u16)dsp_get_acc_m(1);
  s64 acc = dsp_get_long_acc(0);

  if ((accm & 0x3f) == 0)
    shift = 0;
  else if (accm & 0x40)
    shift = -0x40 + (accm & 0x3f);
  else
    shift = accm & 0x3f;

  if (shift > 0)
  {
    acc >>= shift;
  }
  else if (shift < 0)
  {
    acc <<= -shift;
  }

  dsp_set_long_acc(0, acc);
  Update_SR_Register64(dsp_get_long_acc(0));
}

// LSRNRX $acD, $axS.h
// 0011 01sd 1xxx xxxx
// Logically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AX[S].H
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void lsrnrx(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s16 shift;
  u16 axh = g_dsp.r.ax[sreg].h;
  u64 acc = dsp_get_long_acc(dreg);
  acc &= 0x000000FFFFFFFFFFULL;

  if ((axh & 0x3f) == 0)
    shift = 0;
  else if (axh & 0x40)
    shift = -0x40 + (axh & 0x3f);
  else
    shift = axh & 0x3f;

  if (shift > 0)
  {
    acc <<= shift;
  }
  else if (shift < 0)
  {
    acc >>= -shift;
  }

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, (s64)acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

// ASRNRX $acD, $axS.h
// 0011 10sd 1xxx xxxx
// Arithmetically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AX[S].H
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void asrnrx(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  s16 shift;
  u16 axh = g_dsp.r.ax[sreg].h;
  s64 acc = dsp_get_long_acc(dreg);

  if ((axh & 0x3f) == 0)
    shift = 0;
  else if (axh & 0x40)
    shift = -0x40 + (axh & 0x3f);
  else
    shift = axh & 0x3f;

  if (shift > 0)
  {
    acc <<= shift;
  }
  else if (shift < 0)
  {
    acc >>= -shift;
  }

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

// LSRNR  $acD
// 0011 110d 1xxx xxxx
// Logically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AC[1-D].M
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void lsrnr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s16 shift;
  u16 accm = (u16)dsp_get_acc_m(1 - dreg);
  u64 acc = dsp_get_long_acc(dreg);
  acc &= 0x000000FFFFFFFFFFULL;

  if ((accm & 0x3f) == 0)
    shift = 0;
  else if (accm & 0x40)
    shift = -0x40 + (accm & 0x3f);
  else
    shift = accm & 0x3f;

  if (shift > 0)
    acc <<= shift;
  else if (shift < 0)
    acc >>= -shift;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, (s64)acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

// ASRNR  $acD
// 0011 111d 1xxx xxxx
// Arithmetically shift left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AC[1-D].M
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void asrnr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  s16 shift;
  u16 accm = (u16)dsp_get_acc_m(1 - dreg);
  s64 acc = dsp_get_long_acc(dreg);

  if ((accm & 0x3f) == 0)
    shift = 0;
  else if (accm & 0x40)
    shift = -0x40 + (accm & 0x3f);
  else
    shift = accm & 0x3f;

  if (shift > 0)
    acc <<= shift;
  else if (shift < 0)
    acc >>= -shift;

  zeroWriteBackLog();

  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(dsp_get_long_acc(dreg));
}

}  // namespace Interpreter
}  // namespace DSP
