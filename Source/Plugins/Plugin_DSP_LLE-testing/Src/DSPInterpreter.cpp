// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Globals.h"
#include "gdsp_memory.h"
#include "gdsp_interpreter.h"
#include "gdsp_condition_codes.h"
#include "gdsp_registers.h"
#include "gdsp_opcodes_helper.h"
#include "gdsp_ext_op.h"

namespace DSPInterpreter {

void unknown(const UDSPInstruction& opc)
{
	//_assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "assert while exception");
	ERROR_LOG(DSPLLE, "LLE: Unrecognized opcode 0x%04x, pc 0x%04x", opc.hex, g_dsp.err_pc);
}

// test register and updates SR accordingly
void tsta(int reg)
{
	s64 acc = dsp_get_long_acc(reg);

	Update_SR_Register64(acc);
}

// Generic call implementation
void call(const UDSPInstruction& opc)
{
	u16 dest = dsp_fetch_code();

	if (CheckCondition(opc.hex & 0xf))
	{
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
		g_dsp.pc = dest;
	}
}

// Generic callr implementation
void callr(const UDSPInstruction& opc)
{
	u16 addr;
	u8 reg;

	if (CheckCondition(opc.hex & 0xf))
	{
		reg  = (opc.hex >> 5) & 0x7;
		addr = dsp_op_read_reg(reg);
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
		g_dsp.pc = addr;
	}
}

// Generic if implementation
void ifcc(const UDSPInstruction& opc)
{
	if (!CheckCondition(opc.hex & 0xf))
	{
		// skip the next opcode - we have to lookup its size.
		g_dsp.pc += opSize[dsp_peek_code()];
	}
}

// Generic jmp implementation
void jcc(const UDSPInstruction& opc)
{
	u16 dest = dsp_fetch_code();

	if (CheckCondition(opc.hex & 0xf))
	{
		g_dsp.pc = dest;
	}
}

// Generic jmpr implementation
void jmprcc(const UDSPInstruction& opc)
{
	u8 reg;

	if (CheckCondition(opc.hex & 0xf))
	{
		reg  = (opc.hex >> 5) & 0x7;
		g_dsp.pc = dsp_op_read_reg(reg);
	}
}

// Generic ret implementation
void ret(const UDSPInstruction& opc)
{
	if (CheckCondition(opc.hex & 0xf))
	{
		g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);
	}
}

void rti(const UDSPInstruction& opc)
{
	g_dsp.r[DSP_REG_SR] = dsp_reg_load_stack(DSP_STACK_D);
	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);

	g_dsp.exception_in_progress_hack = false;
}

// HALT
// 0000 0000 0020 0001 
// Stops execution of DSP code. Sets bit DSP_CR_HALT in register DREG_CR.
void halt(const UDSPInstruction& opc)
{
	g_dsp.cr |= 0x4;
	g_dsp.pc = g_dsp.err_pc;
}

void loop(const UDSPInstruction& opc)
{
	u16 reg = opc.hex & 0x1f;
	u16 cnt = g_dsp.r[reg];
	u16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	//	g_dsp.pc = loop_pc;
	g_dsp.pc += opSize[dsp_peek_code()];
}

void loopi(const UDSPInstruction& opc)
{
	u16 cnt = opc.hex & 0xff;
	u16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	//	g_dsp.pc = loop_pc;
	g_dsp.pc += opSize[dsp_peek_code()];
}

void bloop(const UDSPInstruction& opc)
{
	u16 reg = opc.hex & 0x1f;
	u16 cnt = g_dsp.r[reg];
	u16 loop_pc = dsp_fetch_code();

	if (cnt)
	{
		dsp_reg_store_stack(0, g_dsp.pc);
		dsp_reg_store_stack(2, loop_pc);
		dsp_reg_store_stack(3, cnt);
	}
	else
	{
		g_dsp.pc = loop_pc;
		g_dsp.pc += opSize[dsp_peek_code()];
	}
}

void bloopi(const UDSPInstruction& opc)
{
	u16 cnt = opc.hex & 0xff;
	u16 loop_pc = dsp_fetch_code();

	if (cnt)
	{
		dsp_reg_store_stack(0, g_dsp.pc);
		dsp_reg_store_stack(2, loop_pc);
		dsp_reg_store_stack(3, cnt);
	}
	else
	{
		g_dsp.pc = loop_pc;
		g_dsp.pc += opSize[dsp_peek_code()];
	}
}

//-------------------------------------------------------------

// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
// FIXME: Perform additional operation depending on destination register.
void mrr(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x1f;
	u8 dreg = (opc.hex >> 5) & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_op_write_reg(dreg, val);
}

// LRR $D, @$S
// 0001 1000 0ssd dddd
// Move value from data memory pointed by addressing register $S toregister $D.
// FIXME: Perform additional operation depending on destination register.
void lrr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 5) & 0x3;
	u8 dreg = opc.hex & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	dsp_op_write_reg(dreg, val);
}

// LRRD $D, @$S
// 0001 1000 1ssd dddd
// Move value from data memory pointed by addressing register $S toregister $D.
// Decrement register $S. 
// FIXME: Perform additional operation depending on destination register.
void lrrd(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 5) & 0x3;
	u8 dreg = opc.hex & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	dsp_op_write_reg(dreg, val);
	g_dsp.r[sreg]--;
}

// LRRI $D, @$S
// 0001 1001 0ssd dddd
// Move value from data memory pointed by addressing register $S to register $D.
// Increment register $S. 
// FIXME: Perform additional operation depending on destination register.
void lrri(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 5) & 0x3;
	u8 dreg = opc.hex & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]); 
	dsp_op_write_reg(dreg, val);
	g_dsp.r[sreg]++;

}

// LRRN $D, @$S
// 0001 1001 1ssd dddd
// Move value from data memory pointed by addressing register $S to register $D.
// Add indexing register $(0x4+S) to register $S. 
// FIXME: Perform additional operation depending on destination register.
void lrrn(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 5) & 0x3;
	u8 dreg = opc.hex & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	dsp_op_write_reg(dreg, val);
	g_dsp.r[sreg] += g_dsp.r[sreg + 4];
}

// SRR @$D, $S
// 0001 1010 0dds ssss
// Store value from source register $S to a memory location pointed by 
// addressing register $D. 
// FIXME: Perform additional operation depending on source register.
void srr(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x3;
	u8 sreg = opc.hex & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);
}

// SRRD @$D, $S
// 0001 1010 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Decrement register $D. 
// FIXME: Perform additional operation depending on source register.
void srrd(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x3;
	u8 sreg = opc.hex & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);
	g_dsp.r[dreg]--;
}

// SRRI @$D, $S
// 0001 1011 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Increment register $D. 
// FIXME: Perform additional operation depending on source register.
void srri(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x3;
	u8 sreg = opc.hex & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);
	g_dsp.r[dreg]++;
}

// SRRN @$D, $S
// 0001 1011 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $D. Add indexing register $(0x4+D) to register $D.

// FIXME: Perform additional operation depending on source register.
void srrn(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x3;
	u8 sreg = opc.hex & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);
	g_dsp.r[dreg] += g_dsp.r[dreg + 4];
}

// ILRR $acD.m, @$arS
// 0000 001d 0001 00ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m.
void ilrr(const UDSPInstruction& opc)
{
	u16 reg  = opc.hex & 0x3;
	u16 dreg = DSP_REG_ACM0 + ((opc.hex >> 8) & 1);

	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);
}

// ILRRD $acD.m, @$arS
// 0000 001d 0001 01ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Decrement addressing register $arS.
void ilrrd(const UDSPInstruction& opc)
{
	u16 reg  = opc.hex & 0x3;
	u16 dreg = DSP_REG_ACM0 + ((opc.hex >> 8) & 1);

	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);

	g_dsp.r[reg]--;
}

// ILRRI $acD.m, @$S
// 0000 001d 0001 10ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Increment addressing register $arS.
void ilrri(const UDSPInstruction& opc)
{
	u16 reg  = opc.hex & 0x3;
	u16 dreg = DSP_REG_ACM0 + ((opc.hex >> 8) & 1);

	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);

	g_dsp.r[reg]++;
}

// ILRRN $acD.m, @$arS
// 0000 001d 0001 11ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Add corresponding indexing
// register $ixS to addressing register $arS.
void ilrrn(const UDSPInstruction& opc)
{
	u16 reg  = opc.hex & 0x3;
	u16 dreg = DSP_REG_ACM0 + ((opc.hex >> 8) & 1);

	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);

	g_dsp.r[reg] += g_dsp.r[DSP_REG_IX0 + reg];
}


void lri(const UDSPInstruction& opc)
{
	u8 reg  = opc.hex & DSP_REG_MASK;
	u16 imm = dsp_fetch_code();
	dsp_op_write_reg(reg, imm);
}

void lris(const UDSPInstruction& opc)
{
	u8 reg  = ((opc.hex >> 8) & 0x7) + 0x18;
	u16 imm = (s8)opc.hex;
	dsp_op_write_reg(reg, imm);
}

void lr(const UDSPInstruction& opc)
{
	u8 reg   = opc.hex & DSP_REG_MASK;
	u16 addr = dsp_fetch_code();
	u16 val  = dsp_dmem_read(addr);
	dsp_op_write_reg(reg, val);
}

void sr(const UDSPInstruction& opc)
{
	u8 reg   = opc.hex & DSP_REG_MASK;
	u16 addr = dsp_fetch_code();
	u16 val  = dsp_op_read_reg(reg);
	dsp_dmem_write(addr, val);
}

void si(const UDSPInstruction& opc)
{
	u16 addr = (s8)opc.hex;
	u16 imm = dsp_fetch_code();
	dsp_dmem_write(addr, imm);
}

void tstaxh(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 8) & 0x1;
	s16 val = dsp_get_ax_h(reg);

	Update_SR_Register16(val);
}

// CLR $acR
// 1000 r001 xxxx xxxx
// Clears accumulator $acR
void clr(const UDSPInstruction& opc)
{
	u8 reg = (opc.hex >> 11) & 0x1;

	dsp_set_long_acc(reg, 0);

	Update_SR_Register64((s64)0);   // really?
}

// CLRL $acR.l
// 1111 110r xxxx xxxx
// Clears $acR.l - low 16 bits of accumulator $acR.
void clrl(const UDSPInstruction& opc)
{
	u16 reg = DSP_REG_ACL0 + ((opc.hex >> 11) & 0x1);
	g_dsp.r[reg] &= 0xFF00;

	// Should this be 64bit?
	Update_SR_Register64((s64)reg);
}

// CLRP
// 1000 0100 xxxx xxxx
// Clears product register $prod.
void clrp(const UDSPInstruction& opc)
{
	// Magic numbers taken from doddie's doc
	g_dsp.r[0x14] = 0x0000;
	g_dsp.r[0x15] = 0xfff0;
	g_dsp.r[0x16] = 0x00ff;
	g_dsp.r[0x17] = 0x0010;
}

void mulc(const UDSPInstruction& opc)
{
	// math new prod
	u8 sreg = (opc.hex >> 11) & 0x1;
	u8 treg = (opc.hex >> 12) & 0x1;

	s64 prod = dsp_get_acc_m(sreg) * dsp_get_ax_h(treg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulcmvz(const UDSPInstruction& opc)
{
	s64 TempProd = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	s64 Prod = (s64)dsp_get_acc_m(sreg) * (s64)dsp_get_acc_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(Prod);

	// update acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acc = TempProd & ~0xffff; // clear lower 4 bytes
	dsp_set_long_acc(rreg, acc);
}

void mulcmv(const UDSPInstruction& opc)
{
	s64 TempProd = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	s64 Prod = (s64)dsp_get_acc_m(sreg) * (s64)dsp_get_acc_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(Prod);

	// update acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	dsp_set_long_acc(rreg, TempProd);
}

void cmpar(const UDSPInstruction& opc)
{
	u8 rreg = ((opc.hex >> 12) & 0x1) + 0x1a;
	u8 areg = (opc.hex >> 11) & 0x1;

	// we compare
	s64 rr = (s16)g_dsp.r[rreg];
	rr <<= 16;

	s64 ar = dsp_get_long_acc(areg);

	Update_SR_Register64(ar - rr);
}

void cmp(const UDSPInstruction& opc)
{
	s64 acc0 = dsp_get_long_acc(0);
	s64 acc1 = dsp_get_long_acc(1);

	Update_SR_Register64(acc0 - acc1);
}

void tst(const UDSPInstruction& opc)
{
	tsta((opc.hex >> 11) & 0x1);
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

	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

// ADDARN $arD, $ixS
// 0000 0000 0001 ssdd
// Adds indexing register $ixS to an addressing register $arD.
void addarn(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = (opc.hex >> 2) & 0x3;

	g_dsp.r[dreg] += (s16)g_dsp.r[DSP_REG_IX0 + sreg];
}

void mulcac(const UDSPInstruction& opc)
{
	s64 TempProd = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc.hex >> 12) & 0x1;
	s64 Prod = (s64)dsp_get_acc_m(sreg) * (s64)dsp_get_acc_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(Prod);

	// update acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	dsp_set_long_acc(rreg, TempProd + g_dsp.r[rreg]);
}

void movr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + 0x18;

	s64 acc = (s16)g_dsp.r[sreg];
	acc <<= 16;
	acc &= ~0xffff;

	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void movax(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1c + dreg] = g_dsp.r[0x18 + sreg];
	g_dsp.r[0x1e + dreg] = g_dsp.r[0x1a + sreg];

	if ((s16)g_dsp.r[0x1a + sreg] < 0)
	{
		g_dsp.r[0x10 + dreg] = 0xffff;
	}
	else
	{
		g_dsp.r[0x10 + dreg] = 0;
	}

	tsta(dreg);
}

void xorr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] ^= g_dsp.r[0x1a + sreg];

	tsta(dreg);
}

// ANDR $acD.m, $axS.h
// 0011 01sd xxxx xxxx
// Logic AND middle part of accumulator $acD.m with hight part of
// secondary accumulator $axS.h.
void andr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] &= g_dsp.r[0x1a + sreg];

	tsta(dreg);
}

void orr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] |= g_dsp.r[0x1a + sreg];

	tsta(dreg);
}

// ANDC $acD.m, $ac(1-D).m
// 0011 110d xxxx xxxx
// Logic AND middle part of accumulator $acD.m with middle part of
// accumulator $ax(1-D).m.s
void andc(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;

	u16 ac1 = dsp_get_acc_m(D);
	u16 ac2 = dsp_get_acc_m(1 - D);

	dsp_set_long_acc(D, ac1 & ac2);

	if ((ac1 & ac2) == 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x20;  // 0x40?
	}
	else
	{
		g_dsp.r[DSP_REG_SR] &= ~0x20; // 0x40?
	}
}


//-------------------------------------------------------------

void nx(const UDSPInstruction& opc)
{
	// This opcode is supposed to do nothing - it's used if you want to use
	// an opcode extension but not do anything. At least according to duddie.
}


// Hermes switched andf and andcf, so check to make sure they are still correct
// ANDCF $acD.m, #I
// 0000 001r 1100 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logic AND of
// accumulator mid part $acD.m with immediate value I is equal zero.
void andfc(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 8) & 0x1;
	u16 imm = dsp_fetch_code();
	u16 val = dsp_get_acc_m(reg);

	if ((val & imm) == imm)
	{
		g_dsp.r[DSP_REG_SR] |= 0x40;
	}
	else
	{
		g_dsp.r[DSP_REG_SR] &= ~0x40;
	}
}

// Hermes switched andf and andcf, so check to make sure they are still correct

// ANDF $acD.m, #I
// 0000 001r 1010 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logical AND
// operation of accumulator mid part $acD.m with immediate value I is equal
// immediate value I.
void andf(const UDSPInstruction& opc)
{
	u8 reg;
	u16 imm;
	u16 val;

	reg = 0x1e + ((opc.hex >> 8) & 0x1);
	imm = dsp_fetch_code();
	val = g_dsp.r[reg];

	if ((val & imm) == 0)
	{
		g_dsp.r[DSP_REG_SR] |= 0x40;
	}
	else
	{
		g_dsp.r[DSP_REG_SR] &= ~0x40;
	}
}

void cmpi(const UDSPInstruction& opc)
{
	int reg  = (opc.hex >> 8) & 0x1;

	// Immediate is considered to be at M level in the 40-bit accumulator.
	s64 imm = (s64)(s16)dsp_fetch_code() << 16;
	s64 val = dsp_get_long_acc(reg);
	s64 res = val - imm;

	Update_SR_Register64(res);
}

void xori(const UDSPInstruction& opc)
{
	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
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
	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] &= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}


// F|RES: i am not sure if this shouldnt be the whole ACC
void ori(const UDSPInstruction& opc)
{
	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
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

	dsp_set_long_acc(areg, res);

	Update_SR_Register64(res);
}

//-------------------------------------------------------------

// ADDP $acD
// 0100 111d xxxx xxxx
// Adds product register to accumulator register.
void addp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	s64 acc = dsp_get_long_acc(dreg);
	acc += dsp_get_long_prod();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void cmpis(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	s64 val = (s8)opc.hex;
	val <<= 16; 

	s64 res = acc - val;

	Update_SR_Register64(res);
}

// ADDPAXZ $acD, $axS
// 1111 10sd xxxx xxxx
// Adds secondary accumulator $axS to product register and stores result
// in accumulator register. Low 16-bits of $acD ($acD.l) are set to 0.
void addpaxz(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	u8 sreg = (opc.hex >> 9) & 0x1;

	s64 prod = dsp_get_long_prod() & ~0x0ffff;
	s64 ax_h = dsp_get_long_acx(sreg);
	s64 acc = (prod + ax_h) & ~0x0ffff;

	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void movpz(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	// overwrite acc and clear low part
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void decm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc -= sub;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void dec(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x01;

	s64 acc = dsp_get_long_acc(dreg) - 1;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void incm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc += sub;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void inc(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(dreg);
	acc++;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register64(acc);
}

void neg(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	
	s64 acc = dsp_get_long_acc(areg);
	acc = 0 - acc;
	dsp_set_long_acc(areg, acc);
	
	Update_SR_Register64(acc);
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
	dsp_set_long_acc(dreg, acc);
}

// MOV $acD, $ac(1-D)
// 0110 110d xxxx xxxx
// Moves accumulator $ax(1-D) to accumulator $axD.
void mov(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	u16 acc = dsp_get_acc_m(1 - D);

	dsp_set_long_acc(D, acc);
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
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

// ADDR $acD, $(0x18+S)
// 0100 0ssd xxxx xxxx
// Adds register $(0x18+S) to accumulator $acD register.
void addr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + 0x18;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc += ax;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void subr(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;
	u8 sreg = ((opc.hex >> 9) & 0x3) + 0x18;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc -= ax;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void subax(const UDSPInstruction& opc)
{
	int regD = (opc.hex >> 8) & 0x1;
	int regT = (opc.hex >> 9) & 0x1;

	s64 Acc = dsp_get_long_acc(regD) - dsp_get_long_acx(regT);

	dsp_set_long_acc(regD, Acc);
	Update_SR_Register64(Acc);
}

// ADDIS $acD, #I
// 0000 010d iiii iiii
// Adds short immediate (8-bit sign extended) to mid accumulator $acD.hm.
void addis(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 Imm = (s8)opc.hex;
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
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void madd(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	prod += (s64)dsp_get_ax_l(sreg) * (s64)dsp_get_ax_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

void msub(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	prod -= (s64)dsp_get_ax_l(sreg) * (s64)dsp_get_ax_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

// LSR16 $acR
// 1111 010r xxxx xxxx
// Logically shifts right accumulator $acR by 16.
void lsr16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);

	acc >>= 16;
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
	u16 shift = -opc.ushift;
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
	u16 shift = -opc.ushift;

	// arithmetic shift
	s64 acc = dsp_get_long_acc(opc.areg);
	acc >>= shift;

	dsp_set_long_acc(opc.areg, acc);

	Update_SR_Register64(acc);
}

//-------------------------------------------------------------

// hcs give me this code!!
// More docs needed - the operation is really odd!
// Decrement Address Register
void dar(const UDSPInstruction& opc)
{
	int reg = opc.hex & 0x3;

	int temp = g_dsp.r[reg] + g_dsp.r[8];

	if (temp <= 0x7ff)  // ???
		g_dsp.r[reg] = temp;
	else
		g_dsp.r[reg]--;
}


// hcs give me this code!!
// More docs needed - the operation is really odd!
// Increment Address Register
void iar(const UDSPInstruction& opc)
{
	int reg = opc.hex & 0x3;

	int temp = g_dsp.r[reg] + g_dsp.r[8];

	if (temp <= 0x7ff)  // ???
		g_dsp.r[reg] = temp;
	else
		g_dsp.r[reg]++;
}

//-------------------------------------------------------------

void sbclr(const UDSPInstruction& opc)
{
	u8 bit = (opc.hex & 0xff) + 6;
	g_dsp.r[DSP_REG_SR] &= ~(1 << bit);
}


void sbset(const UDSPInstruction& opc)
{
	u8 bit = (opc.hex & 0xff) + 6;
	g_dsp.r[DSP_REG_SR] |= (1 << bit);
}


// FIXME inside
// No idea what most of this is supposed to do.
void srbith(const UDSPInstruction& opc)
{
	switch ((opc.hex >> 8) & 0xf)
	{
	// M0 seems to be the default. M2 is used in functions in Zelda
	// and then reset with M0 at the end. Like the other bits here, it's
	// done around loops with lots of multiplications.

	case 0xa: // M2
		ERROR_LOG(DSPLLE, "M2");
		break;
		// FIXME: Both of these appear in the beginning of the Wind Waker
	case 0xb: // M0
		ERROR_LOG(DSPLLE, "M0");
		break;

	// 15-bit precision? clamping? no idea :(
	// CLR15 seems to be the default.
	case 0xc: // CLR15
		ERROR_LOG(DSPLLE, "CLR15");
		break;
	case 0xd: // SET15
		ERROR_LOG(DSPLLE, "SET15");
		break;

	// 40-bit precision? clamping? no idea :(
	// 40 seems to be the default.
	case 0xe: // SET40  (really, clear SR's 0x4000?) something about "set 40-bit operation"?
		g_dsp.r[DSP_REG_SR] &= ~(1 << 14);
		ERROR_LOG(DSPLLE, "SET40");
		break;

	case 0xf: // SET16  (really, set SR's 0x4000?) something about "set 16-bit operation"?
		// that doesnt happen on a real console  << what does this comment mean?
		g_dsp.r[DSP_REG_SR] |= (1 << 14);
		ERROR_LOG(DSPLLE, "SET16");
		break;

	default:
		break;
	}
}

//-------------------------------------------------------------

void movp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	s64 acc = prod;
	dsp_set_long_acc(dreg, acc);
}

void mul(const UDSPInstruction& opc)
{
	u8 sreg  = (opc.hex >> 11) & 0x1;
	s64 prod = (s64)dsp_get_ax_h(sreg) * (s64)dsp_get_ax_l(sreg) * GetMultiplyModifier();

	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulac(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc.hex >> 11) & 0x1;
	s64 prod = dsp_get_ax_l(sreg) * dsp_get_ax_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulmv(const UDSPInstruction& opc)
{
	u8 rreg  = (opc.hex >> 8) & 0x1;
	s64 prod = dsp_get_long_prod();
	s64 acc = prod;
	dsp_set_long_acc(rreg, acc);

	u8 areg  = ((opc.hex >> 11) & 0x1) + 0x18;
	u8 breg  = ((opc.hex >> 11) & 0x1) + 0x1a;
	s64 val1 = (s16)g_dsp.r[areg];
	s64 val2 = (s16)g_dsp.r[breg];

	prod = val1 * val2 * GetMultiplyModifier();

	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulmvz(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 11) & 0x1;
	u8 rreg = (opc.hex >> 8) & 0x1;

	// overwrite acc and clear low part
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(rreg, acc);

	// math prod
	prod = (s64)g_dsp.r[0x18 + sreg] * (s64)g_dsp.r[0x1a + sreg] * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulx(const UDSPInstruction& opc)
{
	u8 sreg = ((opc.hex >> 12) & 0x1);
	u8 treg = ((opc.hex >> 11) & 0x1);

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulxac(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = (opc.hex >> 8) & 0x1;
	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulxmv(const UDSPInstruction& opc)
{
	// add old prod to acc
	u8 rreg = ((opc.hex >> 8) & 0x1);
	s64 acR = dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void mulxmvz(const UDSPInstruction& opc)
{
	// overwrite acc and clear low part
	u8 rreg  = (opc.hex >> 8) & 0x1;
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(rreg, acc);

	// math prod
	u8 sreg = (opc.hex >> 12) & 0x1;
	u8 treg = (opc.hex >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register64(prod);
}

void sub(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;
	s64 Acc1 = dsp_get_long_acc(D);
	s64 Acc2 = dsp_get_long_acc(1 - D);

	Acc1 -= Acc2;

	dsp_set_long_acc(D, Acc1);
}


//-------------------------------------------------------------
//
// --- Table E
//
//-------------------------------------------------------------

void maddx(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 treg = (opc.hex >> 8) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = dsp_get_long_prod();
	prod += val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

void msubx(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 treg = (opc.hex >> 8) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = dsp_get_long_prod();
	prod -= val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

void maddc(const UDSPInstruction& opc)
{
	u32 sreg = (opc.hex >> 9) & 0x1;
	u32 treg = (opc.hex >> 8) & 0x1;

	s64 val1 = dsp_get_acc_m(sreg);
	s64 val2 = dsp_get_ax_h(treg);

	s64 prod = dsp_get_long_prod();
	prod += val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

void msubc(const UDSPInstruction& opc)
{
	u32 sreg = (opc.hex >> 9) & 0x1;
	u32 treg = (opc.hex >> 8) & 0x1;

	s64 val1 = dsp_get_acc_m(sreg);
	s64 val2 = dsp_get_ax_h(treg);

	s64 prod = dsp_get_long_prod();
	prod -= val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}

// SRS @M, $(0x18+S)
// 0010 1sss mmmm mmmm
// Store value from register $(0x18+S) to a memory pointed by address M. 
// (8-bit sign extended). 
// FIXME: Perform additional operation depending on destination register.
// Note: pc+=2 in doddie's doc seems wrong
void srs(const UDSPInstruction& opc)
{
	u8 reg   = ((opc.hex >> 8) & 0x7) + 0x18;
	u16 addr = (s8)opc.hex;
	dsp_dmem_write(addr, g_dsp.r[reg]);
}
  
// LRS $(0x18+D), @M
// 0010 0ddd mmmm mmmm
// Move value from data memory pointed by address M (8-bit sign
// extended) to register $(0x18+D). 
// FIXME: Perform additional operation depending on destination register.
// Note: pc+=2 in doddie's doc seems wrong
void lrs(const UDSPInstruction& opc)
{
	u8 reg   = ((opc.hex >> 8) & 0x7) + 0x18;
	u16 addr = (s8) opc.hex;
	g_dsp.r[reg] = dsp_dmem_read(addr);
}

}  // namespace
