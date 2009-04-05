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
	ERROR_LOG(DSPHLE, "LLE: Unrecognized opcode 0x%04x, pc 0x%04x", opc.hex, g_dsp.pc);
	/*PanicAlert("LLE: Unrecognized opcode 0x%04x", opc.hex);*/
	//g_dsp.pc = g_dsp.err_pc;
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
		dsp_fetch_code(); // skip the next opcode
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

// FIXME inside
void rti(const UDSPInstruction& opc)
{
	if ((opc.hex & 0xf) != 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp rti opcode");
	}

	g_dsp.r[R_SR] = dsp_reg_load_stack(DSP_STACK_D);
	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);

	g_dsp.exception_in_progress_hack = false;
}

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

	g_dsp.pc = loop_pc + 1;
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

	g_dsp.pc = loop_pc + 1;
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
		g_dsp.pc = loop_pc + 1;
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
		g_dsp.pc = loop_pc + 1;
	}
}

//-------------------------------------------------------------

void mrr(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x1f;
	u8 dreg = (opc.hex >> 5) & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_op_write_reg(dreg, val);
}

void lrr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 5) & 0x3;
	u8 dreg = opc.hex & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	dsp_op_write_reg(dreg, val);

	// post processing of source reg
	switch ((opc.hex >> 7) & 0x3)
	{
	case 0x0: // LRR
		break;

	case 0x1: // LRRD
		g_dsp.r[sreg]--;
		break;

	case 0x2: // LRRI
		g_dsp.r[sreg]++;
		break;

	case 0x3: //  LRRN
		g_dsp.r[sreg] += g_dsp.r[sreg + 4];
		break;
	}
}

void srr(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x3;
	u8 sreg = opc.hex & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);

	// post processing of dest reg
	switch ((opc.hex >> 7) & 0x3)
	{
	case 0x0: // SRR
		break;

	case 0x1: // SRRD
		g_dsp.r[dreg]--;
		break;

	case 0x2: // SRRI
		g_dsp.r[dreg]++;
		break;

	case 0x3: // SRRX
		g_dsp.r[dreg] += g_dsp.r[dreg + 4];
		break;
	}
}

// FIXME inside
void ilrr(const UDSPInstruction& opc)
{
	u16 reg  = opc.hex & 0x3;
	u16 dreg = 0x1e + ((opc.hex >> 8) & 1);

	// always to acc0 ?
	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);

	switch ((opc.hex >> 2) & 0x3)
	{
	case 0x0: // no change (ILRR)
		break;

	case 0x1: // post decrement (ILRRD?)
		g_dsp.r[reg]--;
		break;

	case 0x2: // post increment (ILRRI)
		g_dsp.r[reg]++;
		break;

	default:
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "Unknown ILRR: 0x%04x\n", (opc.hex >> 2) & 0x3);
	}
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

void clr(const UDSPInstruction& opc)
{
	u8 reg = (opc.hex >> 11) & 0x1;

	dsp_set_long_acc(reg, 0);

	Update_SR_Register64((s64)0);   // really?
}

void clrp(const UDSPInstruction& opc)
{
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

// TODO: Implement
void mulcmvz(const UDSPInstruction& opc)
{
	ERROR_LOG(DSPHLE, "dsp_opc.hex_mulcmvz ni");
}

// TODO: Implement
void mulcmv(const UDSPInstruction& opc)
{
	ERROR_LOG(DSPHLE, "dsp_opc.hex_mulcmv ni");
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

void tsta(const UDSPInstruction& opc)
{
	u8 reg  = (opc.hex >> 11) & 0x1;
	s64 acc = dsp_get_long_acc(reg);

	Update_SR_Register64(acc);
}

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

void addarn(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = (opc.hex >> 2) & 0x3;

	g_dsp.r[dreg] += (s16)g_dsp.r[0x04 + sreg];
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
	dsp_set_long_acc(rreg, TempProd);
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

	tsta(UDSPInstruction(dreg << 11));
}

void xorr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] ^= g_dsp.r[0x1a + sreg];

	tsta(UDSPInstruction(dreg << 11));
}

void andr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] &= g_dsp.r[0x1a + sreg];

	tsta(UDSPInstruction(dreg << 11));
}

void orr(const UDSPInstruction& opc)
{
	u8 sreg = (opc.hex >> 9) & 0x1;
	u8 dreg = (opc.hex >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] |= g_dsp.r[0x1a + sreg];

	tsta(UDSPInstruction(dreg << 11));
}

void andc(const UDSPInstruction& opc)
{
	u8 D = (opc.hex >> 8) & 0x1;

	u16 ac1 = dsp_get_acc_m(D);
	u16 ac2 = dsp_get_acc_m(1 - D);

	dsp_set_long_acc(D, ac1 & ac2);

	if ((ac1 & ac2) == 0)
	{
		g_dsp.r[R_SR] |= 0x20;  // 0x40?
	}
	else
	{
		g_dsp.r[R_SR] &= ~0x20; // 0x40?
	}
}


//-------------------------------------------------------------

void nx(const UDSPInstruction& opc)
{
	// This opcode is supposed to do nothing - it's used if you want to use
	// an opcode extension but not do anything. At least according to duddie.
}


// FIXME inside
// Hermes switched andf and andcf, so check to make sure they are still correct
void andfc(const UDSPInstruction& opc)
{
	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp_opc.hex_andfc");
	}

	u8 reg  = (opc.hex >> 8) & 0x1;
	u16 imm = dsp_fetch_code();
	u16 val = dsp_get_acc_m(reg);

	if ((val & imm) == imm)
	{
		g_dsp.r[R_SR] |= 0x40;
	}
	else
	{
		g_dsp.r[R_SR] &= ~0x40;
	}
}

// FIXME inside
// Hermes switched andf and andcf, so check to make sure they are still correct
void andf(const UDSPInstruction& opc)
{
	u8 reg;
	u16 imm;
	u16 val;

	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp andf opcode");
	}

	reg = 0x1e + ((opc.hex >> 8) & 0x1);
	imm = dsp_fetch_code();
	val = g_dsp.r[reg];

	if ((val & imm) == 0)
	{
		g_dsp.r[R_SR] |= 0x40;
	}
	else
	{
		g_dsp.r[R_SR] &= ~0x40;
	}
}

// FIXME inside
void subf(const UDSPInstruction& opc)
{
	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp subf opcode");
	}

	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
	s64 imm = (s16)dsp_fetch_code();

	s64 val = (s16)g_dsp.r[reg];
	s64 res = val - imm;

	Update_SR_Register64(res);
}

// FIXME inside
void xori(const UDSPInstruction& opc)
{
	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp xori opcode");
	}

	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] ^= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}

//FIXME inside
void andi(const UDSPInstruction& opc)
{
	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp andi opcode");
	}

	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] &= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}


// F|RES: i am not sure if this shouldnt be the whole ACC
//
//FIXME inside
void ori(const UDSPInstruction& opc)
{
	if (opc.hex & 0xf)
	{
		// FIXME: Implement
		ERROR_LOG(DSPHLE, "dsp ori opcode");
		return;
	}

	u8 reg  = 0x1e + ((opc.hex >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] |= imm;

	Update_SR_Register16((s16)g_dsp.r[reg]);
}

//-------------------------------------------------------------

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

void addp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;
	s64 acc = dsp_get_long_acc(dreg);
	acc = acc + dsp_get_long_prod();
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


void movnp(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	s64 acc = -prod;
	dsp_set_long_acc(dreg, acc);
}

// TODO: Implement
void mov(const UDSPInstruction& opc)
{
	// UNIMPLEMENTED
	ERROR_LOG(DSPHLE, "dsp_opc.hex_mov\n");
}

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

void lsr16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc >>= 16;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void asr16(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex >> 11) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc >>= 16;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register64(acc);
}

void shifti(const UDSPInstruction& opc)
{
	// direction: left
	bool ShiftLeft = true;
	u16 shift = opc.ushift;

	if ((opc.negating) && (opc.shift < 0))
	{
		ShiftLeft = false;
		shift = -opc.shift;
	}

	s64 acc;
	u64 uacc;

	if (opc.arithmetic)
	{
		// arithmetic shift
		uacc = dsp_get_long_acc(opc.areg);

		if (!ShiftLeft)
		{
			uacc >>= shift;
		}
		else
		{
			uacc <<= shift;
		}

		acc = uacc;
	}
	else
	{
		acc = dsp_get_long_acc(opc.areg);

		if (!ShiftLeft)
		{
			acc >>= shift;
		}
		else
		{
			acc <<= shift;
		}
	}

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
	g_dsp.r[R_SR] &= ~(1 << bit);
}


void sbset(const UDSPInstruction& opc)
{
	u8 bit = (opc.hex & 0xff) + 6;
	g_dsp.r[R_SR] |= (1 << bit);
}


// FIXME inside
// No idea what most of this is supposed to do.
void srbith(const UDSPInstruction& opc)
{
	switch ((opc.hex >> 8) & 0xf)
	{
	case 0xa: // M2
		ERROR_LOG(DSPHLE, "dsp_opc.hex_m2\n");
		break;
		// FIXME: Both of these appear in the beginning of the Wind Waker
	case 0xb: // M0
		ERROR_LOG(DSPHLE, "dsp_opc.hex_m0\n");
		break;
	case 0xc: // CLR15
		ERROR_LOG(DSPHLE, "dsp_opc.hex_clr15\n");
		break;
	case 0xd: // SET15
		ERROR_LOG(DSPHLE, "dsp_opc.hex_set15\n");
		break;

	case 0xe: // SET40  (really, clear SR's 0x4000?) something about "set 40-bit operation"?
		g_dsp.r[R_SR] &= ~(1 << 14);
		break;

	case 0xf: // SET16  (really, set SR's 0x4000?) something about "set 16-bit operation"?
		// that doesnt happen on a real console  << what does this comment mean?
		g_dsp.r[R_SR] |= (1 << 14);
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

void srs(const UDSPInstruction& opc)
{
	u8 reg   = ((opc.hex >> 8) & 0x7) + 0x18;
	u16 addr = (s8)opc.hex;
	dsp_dmem_write(addr, g_dsp.r[reg]);	
}
    
void lrs(const UDSPInstruction& opc)
{
	u8 reg   = ((opc.hex >> 8) & 0x7) + 0x18;
	u16 addr = (s8) opc.hex;
	g_dsp.r[reg] = dsp_dmem_read(addr);
}

}  // namespace
