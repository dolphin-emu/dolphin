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


// Multiplier and product register control

#include "DSPInterpreter.h"

#include "DSPIntCCUtil.h"
#include "DSPIntUtil.h"

namespace DSPInterpreter {

// Only MULX family instructions have unsigned support.
inline s64 dsp_get_multiply_prod(u16 a, u16 b, bool sign)
{
	s64 prod;

#if 0 //causing probs with all games atm
	if (sign && g_dsp.r[DSP_REG_SR] & SR_MUL_UNSIGNED)
		prod = (u64)a * (u64)b;  // won't overflow 32-bits
	else
#endif
		prod = (s32)(s16)a * (s32)(s16)b;  // won't overflow 32-bits

	// Conditionally multiply by 2.
	if ((g_dsp.r[DSP_REG_SR] & SR_MUL_MODIFY) == 0)
		prod <<= 1;

	return prod;
}
	
// Sets prod as a side effect.
s64 dsp_multiply(u16 a, u16 b, bool sign = false)
{
	s64 prod = dsp_get_multiply_prod(a, b, sign);
	
	// Store the product, and return it, in case the caller wants to read it.
	//	dsp_set_long_prod(prod);
	return prod;
}

s64 dsp_multiply_add(u16 a, u16 b, bool sign = false)
{
	s64 prod = dsp_get_multiply_prod(a, b, sign) + dsp_get_long_prod();

	// Store the product, and return it, in case the caller wants to read it.
	//	dsp_set_long_prod(prod);
	return prod;
}

s64 dsp_multiply_sub(u16 a, u16 b, bool sign = false)
{
	s64 prod = dsp_get_long_prod() -  dsp_get_multiply_prod(a, b, sign);

	// Store the product, and return it, in case the caller wants to read it.
	//	dsp_set_long_prod(prod);
	return prod;
}



// CLRP
// 1000 0100 xxxx xxxx
// Clears product register $prod.
void clrp(const UDSPInstruction& opc)
{
	// Magic numbers taken from duddie's doc
	// These are probably a bad idea to put here.
	zeroWriteBackLog();
	g_dsp.r[0x14] = 0x0000;
	g_dsp.r[0x15] = 0xfff0;
	g_dsp.r[0x16] = 0x00ff;
	g_dsp.r[0x17] = 0x0010;
}

// MOVP $acD
// 0110 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD register.
void movp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	zeroWriteBackLog();
	dsp_set_long_acc(dreg, prod);

	Update_SR_Register64(prod);
}

// MOVNP $acD
// 0111 111d xxxx xxxx 
// Moves negative of multiply product from $prod register to accumulator
// $acD register.
void movnp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	s64 acc = -prod;
	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);
	
	Update_SR_Register64(acc);
}

// ADDPAXZ $acD, $axS
// 1111 10sd xxxx xxxx
// Adds secondary accumulator $axS to product register and stores result
// in accumulator register. Low 16-bits of $acD ($acD.l) are set to 0.
void addpaxz(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	u8 sreg = (opc.hex >> 9) & 0x1;

	s64 prod = dsp_get_long_prod() & ~0xffff;  // hm, should we really mask here?
	s64 ax = dsp_get_long_acx(sreg);
	s64 acc = (prod + ax) & ~0xffff;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// MOVPZ $acD
// 1111 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD
// register and sets $acD.l to 0
void movpz(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	// overwrite acc and clear low part
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;

	zeroWriteBackLog();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}


// MULC $acS.m, $axT.h
// 110s t000 xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS (treat them both as signed).
void mulc(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 11) & 0x1;
	u8 treg = (opc.hex >> 12) & 0x1;
	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	s64 prod = dsp_multiply(accm, axh);
												
	zeroWriteBackLog();

	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULCMVZ $acS.m, $axT.h, $acR
// 110s	t01r xxxx xxxx
// (fixed possible bug in duddie's description, s->t)
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR, set low part of 
// accumulator $acR.l to zero.
void mulcmvz(const UDSPInstruction& opc)
{
	s64 TempProd = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	u8 treg  = (opc.hex >> 11) & 0x1;

	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	s64 prod = dsp_multiply(accm, axh);
											
	zeroWriteBackLog();
	dsp_set_long_prod(prod);

	// update acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acc = TempProd & ~0xffff; // clear lower 4 bytes
	dsp_set_long_acc(rreg, acc);

	Update_SR_Register64(acc);
}

// MULCMV $acS.m, $axT.h, $acR
// 110s t11r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR.
// possible mistake in duddie's doc axT.h rather than axS.h
void mulcmv(const UDSPInstruction& opc)
{
	s64 old_prod = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	u8 treg  = (opc.hex >> 11) & 0x1;
	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 prod = dsp_multiply(accm, axh);
	
	zeroWriteBackLog();

	dsp_set_long_prod(prod);
	// update acc
	dsp_set_long_acc(rreg, old_prod);

	Update_SR_Register64(old_prod);
}

// MULCAC $acS.m, $axT.h, $acR
// 110s	t10r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS  (treat them both as signed). Add product
// register before multiplication to accumulator $acR.
void mulcac(const UDSPInstruction& opc)
{
	s64 old_prod = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	u8 treg  = (opc.hex >> 11) & 0x1;
	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	s64 prod = dsp_multiply(accm, axh);
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acc = old_prod + dsp_get_long_acc(rreg);
	
	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	// update acc
	dsp_set_long_acc(rreg, acc);

	Update_SR_Register64(acc);
}


// MUL $axS.l, $axS.h
// 1001 s000 xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed).
void mul(const UDSPInstruction& opc)
{
	u8 sreg  = (opc.hex >> 11) & 0x1;
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply(axh, axl);
	
	zeroWriteBackLog();
	
	dsp_set_long_prod(prod);
	// FIXME: no update in duddie's docs
	Update_SR_Register64(prod);
}

// MULAC $axS.l, $axS.h, $acR
// 1001 s10r xxxx xxxx
// Add product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
void mulac(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	u8 sreg = (opc.hex >> 11) & 0x1;

	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply(axl, axh);
												
	zeroWriteBackLog();

	dsp_set_long_acc(rreg, acR);
	dsp_set_long_prod(prod);

	// FIXME: no update in duddie's docs
	Update_SR_Register64(prod);
}

// MULMV $axS.l, $axS.h, $acR
// 1001 s11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).
void mulmv(const UDSPInstruction& opc)
{
	u8 rreg  = (opc.hex >> 8) & 0x1;
	u8 sreg  = ((opc.hex >> 11) & 0x1);
	s64 acc = dsp_get_long_prod();
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply(axl, axh);
												
	zeroWriteBackLog();
	dsp_set_long_acc(rreg, acc);
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULMVZ $axS.l, $axS.h, $acR
// 1001 s01r xxxx xxxx
// Move product register to accumulator register $acR and clear low part
// of accumulator register $acR.l. Multiply low part $axS.l of secondary
// accumulator $axS by high part $axS.h of secondary accumulator $axS (treat
// them both as signed).
void mulmvz(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 11) & 0x1;
	u8 rreg = (opc.hex >> 8) & 0x1;

	// overwrite acc and clear low part
	s64 acc = dsp_get_long_prod() & ~0xffff;
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply(axl, axh);
	
	zeroWriteBackLog();

	dsp_set_long_acc(rreg, acc);
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULX $ax0.S, $ax1.T
// 101s t000 xxxx xxxx
// Multiply one part $ax0 by one part $ax1 (treat them both as signed).
// Part is selected by S and T bits. Zero selects low part, one selects high part.
void mulx(const UDSPInstruction& opc)
{
	u8 sreg = ((opc.hex >> 12) & 0x1);
	u8 treg = ((opc.hex >> 11) & 0x1);

	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
	s64 prod = dsp_multiply(val1, val2, true);
	
	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULXAC $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Add product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and
// T bits. Zero selects low part, one selects high part.
void mulxac(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();

	// math new prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
		
	s64 prod = dsp_multiply(val1, val2, true);
	
	zeroWriteBackLog();
	dsp_set_long_acc(rreg, acR);
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and
// T bits. Zero selects low part, one selects high part.
void mulxmv(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = ((opc.hex >> 8) & 0x1);
	s64 acR = dsp_get_long_prod();

	// math new prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	s16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
	s64 prod = dsp_multiply(val1, val2, true);

	zeroWriteBackLog();
	dsp_set_long_acc(rreg, acR);
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Move product register to accumulator register $acR and clear low part
// of accumulator register $acR.l. Multiply one part $ax0 by one part $ax1 (treat
// them both as signed). Part is selected by S and T bits. Zero selects low part,
// one selects high part.
void mulxmvz(const UDSPInstruction& opc)
{
	// overwrite acc and clear low part
	u8 rreg  = (opc.hex >> 8) & 0x1;
	s64 acc = dsp_get_long_prod() & ~0xffff;

	// math prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
	s64 prod = dsp_multiply(val1, val2, true);

	zeroWriteBackLog();

	dsp_set_long_acc(rreg, acc);
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MADDX ax0.S ax1.T
// 1110 00st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and add result to product register.
void maddx(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 treg = (opc.hex >> 8) & 0x1;

	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
	s64 prod = dsp_multiply_add(val1, val2);
	
	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MSUBX $(0x18+S*2), $(0x19+T*2)
// 1110 01st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and subtract result from product register.
void msubx(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 treg = (opc.hex >> 8) & 0x1;

	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
	s64 prod = dsp_multiply_sub(val1, val2);

	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MADDC $acS.m, $axT.h
// 1110 10st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and add result to product
// register.
void maddc(const UDSPInstruction& opc)
{
	u32 sreg = (opc.hex >> 9) & 0x1;
	u32 treg = (opc.hex >> 8) & 0x1;
	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	s64 prod = dsp_multiply_add(accm, axh);

	zeroWriteBackLog();

	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MSUBC $acS.m, $axT.h
// 1110 11st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and subtract result from
// product register.
void msubc(const UDSPInstruction& opc)
{
	u32 sreg = (opc.hex >> 9) & 0x1;
	u32 treg = (opc.hex >> 8) & 0x1;
	u16 accm = dsp_get_acc_m(sreg);
	u16 axh = dsp_get_ax_h(treg);
	s64 prod = dsp_multiply_sub(accm, axh);

	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MADD $axS.l, $axS.h
// 1111 001s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and add
// result to product register.
void madd(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 8) & 0x1;
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply_add(axl, axh);
												
	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

// MSUB $axS.l, $axS.h
// 1111 011s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and
// subtract result from product register.
void msub(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 8) & 0x1;
	u16 axl = dsp_get_ax_l(sreg);
	u16 axh = dsp_get_ax_h(sreg);
	s64 prod = dsp_multiply_sub(axl, axh);
												
	zeroWriteBackLog();
	dsp_set_long_prod(prod);
	Update_SR_Register64(prod);
}

}  // namespace
