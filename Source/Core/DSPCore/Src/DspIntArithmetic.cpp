// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "DSPInterpreter.h"
#include "DSPIntCCUtil.h"
#include "DSPIntUtil.h"

// Arithmetic and accumulator control.

namespace DSPInterpreter {

// CLR $acR
// 1000 r001 xxxx xxxx
// Clears accumulator $acR
void clr(const UDSPInstruction& opc)
{
	u8 reg = (opc.hex >> 11) & 0x1;

	dsp_set_long_acc(reg, 0);
	Update_SR_Register64((s64)0);   // really?
	zeroWriteBackLog();
}

// CLRL $acR.l
// 1111 110r xxxx xxxx
// Clears $acR.l - low 16 bits of accumulator $acR.
void clrl(const UDSPInstruction& opc)
{
	u16 reg = DSP_REG_ACL0 + ((opc.hex >> 8) & 0x1);
	g_dsp.r[reg] = 0;

	// Should this be 64bit?
	// nakee: it says the whole reg in duddie's doc sounds weird
	Update_SR_Register64((s64)reg);
	zeroWriteBackLog();
}


// ADDAXL $acD, $axS.l
// 0111 00sd xxxx xxxx
// Adds secondary accumulator $axS.l to accumulator register $acD.
void addaxl(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(dreg);
	s64 acx = dsp_get_ax_l(sreg);

	acc += acx;

	zeroWriteBackLog();

	dsp_set_long_acc(dreg, acc);
	Update_SR_Register64(acc);
}

// TSTAXH $axR.h
// 1000 011r xxxx xxxx
// Test high part of secondary accumulator $axR.h.
void tstaxh(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 8) & 0x1;
	s16 val = dsp_get_ax_h(reg);

	Update_SR_Register16(val);
	zeroWriteBackLog();
}

// SUB $acD, $ac(1-D)
// 0101 110d xxxx xxxx
// Subtracts accumulator $ac(1-D) from accumulator register $acD. 
void sub(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	s64 acc1 = dsp_get_long_acc(D);
	s64 acc2 = dsp_get_long_acc(1 - D);

	acc1 -= acc2;

	zeroWriteBackLog();

	dsp_set_long_acc(D, acc1);	
	Update_SR_Register64(acc1);
}

// MOVR $acD, $axS.R
// 0110 0srd xxxx xxxx
// Moves register $axS.R (sign extended) to middle accumulator $acD.hm.
// Sets $acD.l to 0.
// TODO: Check what happens to acD.h.
void movr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + DSP_REG_AXL0;
 		
	s64 acc = (s16)g_dsp.r[sreg];
	acc <<= 16;
	acc &= ~0xffff;

	zeroWriteBackLog();

	dsp_set_long_acc(areg, acc);
	Update_SR_Register64(acc);
}

// MOVAX $acD, $axS
// 0110 10sd xxxx xxxx
// Moves secondary accumulator $axS to accumulator $axD. 
void movax(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	u8 sreg = (opc.hex >> 9) & 0x1;

	s64 acx = dsp_get_long_acx(sreg);

	zeroWriteBackLog();

	dsp_set_long_acc(dreg, acx);
	Update_SR_Register64(acx);
}

// XORR $acD.m, $axS.h
// 0011 00sd 0xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with
// high part of secondary accumulator $axS.h.
// x = extension (7 bits!!)
void xorr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;
	u16 axh = g_dsp.r[DSP_REG_AXH0 + sreg];
	
	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0 + dreg] ^= axh;
	
	Update_SR_Register16(dsp_get_acc_m(dreg));
}

// ANDR $acD.m, $axS.h
// 0011 01sd 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
void andr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;
	u16 axh = g_dsp.r[DSP_REG_AXH0 + sreg];
	
	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0 + dreg] &= axh;
	
	Update_SR_Register16(dsp_get_acc_m(dreg));
}

// ORR $acD.m, $axS.h
// 0011 10sd 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
void orr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;
	u16 axh = g_dsp.r[DSP_REG_AXH0 + sreg];
	
	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0 + dreg] |= axh;

	Update_SR_Register16(dsp_get_acc_m(dreg));
}

// ANDC $acD.m, $ac(1-D).m
// 0011 110d 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m
// x = extension (7 bits!!)
void andc(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	u16 accm = dsp_get_acc_m(1-D);
	
	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0+D] &= accm;

	Update_SR_Register16(dsp_get_acc_m(D));
}

// ORC $acD.m, $ac(1-D).m
// 0011 111d 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m.
// x = extension (7 bits!!)
void orc(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	u16 accm = dsp_get_acc_m(1-D);
	
	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0+D] |= accm;

	Update_SR_Register16(dsp_get_acc_m(D));
}

// XORC $acD.m
// 0011 000d 1xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with $ac(1-D).m
// x = extension (7 bits!!)
void xorc(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	u16 res = dsp_get_acc_m(dreg) ^ dsp_get_acc_m(1 - dreg);

	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0 + dreg] = res;
	Update_SR_Register16(res);
}

// NOT $acD.m
// 0011 001d 1xxx xxxx
// Invert all bits in dest reg, aka xor with 0xffff
// x = extension (7 bits!!)
void not(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	u16 res = dsp_get_acc_m(dreg)^0xffff;

	zeroWriteBackLog();

	g_dsp.r[DSP_REG_ACM0 + dreg] = res;
	Update_SR_Register16(res);
}

void orf(const UDSPInstruction& opc)
{
	ERROR_LOG(DSPLLE, "orf not implemented");
}

// Hermes switched andf and andcf, so check to make sure they are still correct
// ANDCF $acD.m, #I
// 0000 001r 1100 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logic AND of
// accumulator mid part $acD.m with immediate value I is equal I.
void andcf(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 8) & 0x1;
	u16 imm = dsp_fetch_code();
	u16 val = dsp_get_acc_m(reg);

	Update_SR_LZ(((val & imm) == imm) ? 0 : 1);
}

// Hermes switched andf and andcf, so check to make sure they are still correct

// ANDF $acD.m, #I
// 0000 001r 1010 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logical AND
// operation of accumulator mid part $acD.m with immediate value I is equal
// immediate value 0.
void andf(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 8) & 0x1;
	u16 imm = dsp_fetch_code();
	u16 val = dsp_get_acc_m(reg);

	Update_SR_LZ(((val & imm) == 0) ? 0 : 1);
}

// CMPI $amD, #I
// 0000 001r 1000 0000
// iiii iiii iiii iiii
// Compares mid accumulator $acD.hm ($amD) with sign extended immediate value I. 
// Although flags are being set regarding whole accumulator register.
void cmpi(const UDSPInstruction& opc)
{
	int reg  = (opc.hex >> 8) & 0x1;

	// Immediate is considered to be at M level in the 40-bit accumulator.
	s64 imm = (s64)(s16)dsp_fetch_code() << 16;
	s64 val = dsp_get_long_acc(reg);
	Update_SR_Register64(val - imm);

}

// XORI $acD.m, #I
// 0000 001r 0010 0000
// iiii iiii iiii iiii
// Logic exclusive or (XOR) of accumulator mid part $acD.m with
// immediate value I.
void xori(const UDSPInstruction& opc)
{
	u8 reg  = DSP_REG_ACM0 + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();

	g_dsp.r[reg] ^= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}

// ANDI $acD.m, #I
// 0000 001r 0100 0000
// iiii iiii iiii iiii
// Logic AND of accumulator mid part $acD.m with immediate value I.
void andi(const UDSPInstruction& opc)
{
	u8 reg  = DSP_REG_ACM0 + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();

	g_dsp.r[reg] &= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}


// ORI $acD.m, #I
// 0000 001r 0110 0000
// iiii iiii iiii iiii
// Logic OR of accumulator mid part $acD.m with immediate value I.
void ori(const UDSPInstruction& opc)
{
	u8 reg  = DSP_REG_ACM0 + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();

	g_dsp.r[reg] |= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}

//-------------------------------------------------------------


//  ADD $acD, $ac(1-D)
//  0100 110d xxxx xxxx
//  Adds accumulator $ac(1-D) to accumulator register $acD.
void add(const UDSPInstruction& opc)
{
	u8 areg  = (opc.hex >> 8) & 0x1;
	s64 acc0 = dsp_get_long_acc(0);
	s64 acc1 = dsp_get_long_acc(1);

	s64 res = acc0 + acc1;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, res);

	Update_SR_Register64(res);
}

// ADDP $acD
// 0100 111d xxxx xxxx
// Adds product register to accumulator register.
void addp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	s64 acc = dsp_get_long_acc(dreg);
	acc += dsp_get_long_prod();

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// SUBP $acD
// 0101 111d xxxx xxxx
// Subtracts product register from accumulator register.
void subp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	s64 acc = dsp_get_long_acc(dreg);
	acc -= dsp_get_long_prod();

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}


// CMPIS $acD, #I
// 0000 011d iiii iiii
// Compares accumulator with short immediate. Comaprison is executed
// by subtracting short immediate (8bit sign extended) from mid accumulator
// $acD.hm and computing flags based on whole accumulator $acD.
void cmpis(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	s64 val = (s8)opc.hex;
	val <<= 16; 

	s64 res = acc - val;

	Update_SR_Register64(res);
}


// DECM $acsD
// 0111 100d xxxx xxxx
// Decrement 24-bit mid-accumulator $acsD.
void decm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc -= sub;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// DEC $acD
// 0111 101d xxxx xxxx
// Decrement accumulator $acD.
void dec(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	s64 acc = dsp_get_long_acc(dreg) - 1;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// INCM $acsD
// 0111 010d xxxx xxxx
// Increment 24-bit mid-accumulator $acsD.
void incm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc += sub;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// INC $acD
// 0111 011d xxxx xxxx
// Increment accumulator $acD.
void inc(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(dreg) + 1;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// NEG $acD
// 0111 110d xxxx xxxx
// Negate accumulator $acD.
void neg(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	
	s64 acc = dsp_get_long_acc(areg);
	acc = 0 - acc;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);
	
	Update_SR_Register64(acc);
}
	
// MOV $acD, $ac(1-D)
// 0110 110d xxxx xxxx
// Moves accumulator $ax(1-D) to accumulator $axD.
void mov(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	u64 acc = dsp_get_long_acc(1 - D);

	zeroWriteBackLog();
	dsp_set_long_acc(D, acc);

	Update_SR_Register64(acc);
}

// ADDAX $acD, $axS
// 0100 10sd xxxx xxxx
// Adds secondary accumulator $axS to accumulator register $acD.
void addax(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = (opc.hex >> 9) & 0x1;

	s64 ax  = dsp_get_long_acx(sreg);
	s64 acc = dsp_get_long_acc(areg);
	acc += ax;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// ADDR $acD.M, $axS.L
// 0100 0ssd xxxx xxxx
// Adds register $axS.L to accumulator $acD.M register.
void addr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + DSP_REG_AXL0;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc += ax;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// SUBR $acD.M, $axS.L
// 0101 0ssd xxxx xxxx
// Subtracts register $axS.L from accumulator $acD.M register.
void subr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + DSP_REG_AXL0;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc -= ax;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// SUBAX $acD, $axS
// 0101 10sd xxxx xxxx
// Subtracts secondary accumulator $axS from accumulator register $acD.
void subax(const UDSPInstruction& opc)
{
	int regD = (opc.hex >> 8) & 0x1;
	int regS = (opc.hex >> 9) & 0x1;

	s64 acc = dsp_get_long_acc(regD) - dsp_get_long_acx(regS);

	zeroWriteBackLog();
	dsp_set_long_acc(regD, acc);
	Update_SR_Register64(acc);
}

// ADDIS $acD, #I
// 0000 010d iiii iiii
// Adds short immediate (8-bit sign extended) to mid accumulator $acD.hm.
void addis(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 Imm = (s8)(u8)opc.hex;
	Imm <<= 16;
	s64 acc = dsp_get_long_acc(areg);
	acc += Imm;

	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// ADDI $amR, #I 
// 0000 001r 0000 0000
// iiii iiii iiii iiii
// Adds immediate (16-bit sign extended) to mid accumulator $acD.hm.
void addi(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 sub = (s16)dsp_fetch_code();
	sub <<= 16;
	s64 acc = dsp_get_long_acc(areg);
	acc += sub;

	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// LSL16 $acR
// 1111 000r xxxx xxxx
// Logically shifts left accumulator $acR by 16.
void lsl16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc <<= 16;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);
	Update_SR_Register64(acc);
}

// LSR16 $acR
// 1111 010r xxxx xxxx
// Logically shifts right accumulator $acR by 16.
void lsr16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	u64 acc = dsp_get_long_acc(areg);

	acc >>= 16;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);
	Update_SR_Register64(acc);
}

// ASR16 $acR
// 1001 r001 xxxx xxxx
// Arithmetically shifts right accumulator $acR by 16.
void asr16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 11) & 0x1;

	s64 acc = dsp_get_long_acc(areg);

	acc >>= 16;

	zeroWriteBackLog();
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// LSL $acR, #I
// 0001 010r 00ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
void lsl(const UDSPInstruction& opc) 
{
	u16 shift = opc.ushift;
	u64 acc = dsp_get_long_acc(opc.areg);

	acc <<= shift;

	dsp_set_long_acc(opc.areg, acc);
	Update_SR_Register64(acc);
}

// LSR $acR, #I
// 0001 010r 01ii iiii
// Logically shifts left accumulator $acR by number specified by value
// calculated by negating sign extended bits 0-6.
void lsr(const UDSPInstruction& opc)
{
	u16 shift = (u16) -(((s8)(opc.ushift << 2)) >> 2);
	u64 acc = dsp_get_long_acc(opc.areg);
	// Lop off the extraneous sign extension our 64-bit fake accum causes
	acc &= 0x000000FFFFFFFFFFULL;
	acc >>= shift;

	dsp_set_long_acc(opc.areg, (s64)acc);
	Update_SR_Register64(acc);
}

// ASL $acR, #I
// 0001 010r 10ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
void asl(const UDSPInstruction& opc)
{
	u16 shift = opc.ushift;

	// arithmetic shift
	u64 acc = dsp_get_long_acc(opc.areg);
	acc <<= shift;

	dsp_set_long_acc(opc.areg, acc);

	Update_SR_Register64(acc);
}

// ASR $acR, #I
// 0001 010r 11ii iiii
// Arithmetically shifts right accumulator $acR by number specified by
// value calculated by negating sign extended bits 0-6.
void asr(const UDSPInstruction& opc)
{
	u16 shift = (u16) -(((s8)(opc.ushift << 2)) >> 2);

	// arithmetic shift
	s64 acc = dsp_get_long_acc(opc.areg);
	acc >>= shift;

	dsp_set_long_acc(opc.areg, acc);

	Update_SR_Register64(acc);
}


// (NEW)
// LSRN  (fixed parameters)
// 0000 0010 1100 1010
// Logically shifts right accumulator $ACC0 by signed 16-bit value $AC1.M
// (if value negative, becomes left shift).
void lsrn(const UDSPInstruction& opc)
{
	s16 shift = dsp_get_acc_m(1);
	u64 acc = dsp_get_long_acc(0);
	// Lop off the extraneous sign extension our 64-bit fake accum causes
	acc &= 0x000000FFFFFFFFFFULL;
	if (shift > 0) {
		acc >>= shift;
	} else if (shift < 0) {
		acc <<= -shift;
	}
	dsp_set_long_acc(0, (s64)acc);
	Update_SR_Register64(acc);
}

// (NEW)
// ASRN  (fixed parameters)
// 0000 0010 1100 1011
// Arithmetically shifts right accumulator $ACC0 by signed 16-bit value $AC1.M
// (if value negative, becomes left shift).
void asrn(const UDSPInstruction& opc)
{
	s16 shift = dsp_get_acc_m(1);
	s64 acc = dsp_get_long_acc(0);
	if (shift > 0) {
		acc >>= shift;
	} else if (shift < 0) {
		acc <<= -shift;
	}
	dsp_set_long_acc(0, acc);
	Update_SR_Register64(acc);
}

// LSRNRX
// 0011 01sd 1xxx xxxx
// 0011 10sd 1xxx xxxx
// Logically shifts right accumulator $ACC[D] by signed 16-bit value $AX[S].H
// Not described by Duddie's doc.
// x = extension (7 bits!!)
void lsrnrx(const UDSPInstruction& opc)
{
  u8 dreg = (opc.hex >> 8) & 0x1; //accD
  u8 sreg = (opc.hex >> 9) & 0x1; //axhS 
  u64 acc = dsp_get_long_acc(dreg);
  s16 shift = g_dsp.r[DSP_REG_AXH0 + sreg];
  acc & 0x000000FFFFFFFFFFULL;
  if (shift > 0) {
    acc <<= shift;
  } else if (shift < 0) {
    acc >>= -shift;
  }

  zeroWriteBackLog();
  
  dsp_set_long_acc(dreg, acc);
  Update_SR_Register64(acc);
}

// LSRNR  $acR
// 0011 11?d 1xxx xxxx
// Logically shifts right accumulator $ACC[D] by signed 16-bit value $AC[1-D].M
// Not described by Duddie's doc - at least not as a separate instruction.
// x = extension (7 bits!!)
void lsrnr(const UDSPInstruction& opc)
{
  u8 D = (opc.hex >> 8) & 0x1;
  s16 shift = dsp_get_acc_m(1-D);
  u64 acc = dsp_get_long_acc(D);
  acc &= 0x000000FFFFFFFFFFULL;
  if (shift > 0) {
    acc <<= shift;
  } else if (shift < 0) {
    acc >>= -shift;
  }

  zeroWriteBackLog();
 
  dsp_set_long_acc(D, acc);
  Update_SR_Register64(acc);
}

// CMPAR $acS axR.h
// 1100 0001 xxxx xxxx
// Compares accumulator $acS with accumulator axR.h.
// Not described by Duddie's doc - at least not as a separate instruction.
void cmpar(const UDSPInstruction& opc)
{
	u8 rreg = ((opc.hex >> 12) & 0x1) + DSP_REG_AXH0;
	u8 sreg = (opc.hex >> 11) & 0x1;

	// we compare
	s64 rr = (s16)g_dsp.r[rreg];
	rr <<= 16;

	s64 sr = dsp_get_long_acc(sreg);

	Update_SR_Register64(sr - rr);
	zeroWriteBackLog();
}

	
// CMP
// 1000 0010 xxxx xxxx
// Compares accumulator $ac0 with accumulator $ac1.
void cmp(const UDSPInstruction& opc)
{
	s64 acc0 = dsp_get_long_acc(0);
	s64 acc1 = dsp_get_long_acc(1);

	Update_SR_Register64(acc0 - acc1);
	zeroWriteBackLog();
}

// TST
// 1011 r001 xxxx xxxx
// Test accumulator %acR.
void tst(const UDSPInstruction& opc)
{
	s8 reg = (opc.hex >> 11) & 0x1;
	s64 acc = dsp_get_long_acc(reg);

	Update_SR_Register64(acc);
	zeroWriteBackLog();
}

}  // namespace
