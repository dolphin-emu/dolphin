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

// Change Log

// fixed dsp_opc_mulx		(regarding DSP 0.0.4.pdf)
// fixed dsp_opc_mulxmv		(regarding DSP 0.0.4.pdf)
// fixed dsp_opc_mulxmvz	(regarding DSP 0.0.4.pdf)
// dsp_opc_shifti: removed strange " >> 9"
// added masking for SR_COMPARE_FLAGS
// added "UNKNOWN_CW"	but without a function (yet? :)
// added missing compare type to MISSING_COMPARES_JX ... but i dunno what it does

// added "MULXMV" to another function table

/*====================================================================
 
   filename:     gdsp_opcodes.cpp
   project:      GCemu
   created:      2004-6-18
   mail:		  duddie@walla.com

   Copyright (c) 2005 Duddie & Tratax

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   ====================================================================*/

#include <stdio.h>
#include "Globals.h"
#include "gdsp_opcodes.h"
#include "gdsp_memory.h"
#include "gdsp_interpreter.h"
#include "gdsp_registers.h"
#include "gdsp_opcodes_helper.h"
#include "gdsp_ext_op.h"

#define SR_CMP_MASK     0x3f
#define DSP_REG_MASK    0x1f

void Update_SR_Register(s64 _Value)
{
	g_dsp.r[R_SR] &= ~SR_CMP_MASK;

	if (_Value < 0)
	{
		g_dsp.r[R_SR] |= 0x8;
	}

	if (_Value == 0)
	{
		g_dsp.r[R_SR] |= 0x4;
	}

	// logic
	if ((_Value >> 62) == 0)
	{
		g_dsp.r[R_SR] |= 0x20;
	}
}


void Update_SR_Register(s16 _Value)
{
	g_dsp.r[R_SR] &= ~SR_CMP_MASK;

	if (_Value < 0)
	{
		g_dsp.r[R_SR] |= 0x8;
	}

	if (_Value == 0)
	{
		g_dsp.r[R_SR] |= 0x4;
	}

	// logic
	if ((_Value >> 14) == 0)
	{
		g_dsp.r[R_SR] |= 0x20;
	}
}


s8 GetMultiplyModifier()
{
	if (g_dsp.r[R_SR] & (1 << 13))
	{
		return(1);
	}

	return(2);
}


bool CheckCondition(u8 _Condition)
{
	bool taken = false;

	switch (_Condition & 0xf)
	{
	    case 0x0:

		    if ((!(g_dsp.r[R_SR] & 0x02)) && (!(g_dsp.r[R_SR] & 0x08)))
		    {
			    taken = true;
		    }

		    break;

	    case 0x3:

		    if ((g_dsp.r[R_SR] & 0x02) || (g_dsp.r[R_SR] & 0x04) || (g_dsp.r[R_SR] & 0x08))
		    {
			    taken = true;
		    }

		    break;

		    // old from duddie
	    case 0x1: // seems okay

		    if ((!(g_dsp.r[R_SR] & 0x02)) && (g_dsp.r[R_SR] & 0x08))
		    {
			    taken = true;
		    }

		    break;

	    case 0x2:

		    if (!(g_dsp.r[R_SR] & 0x08))
		    {
			    taken = true;
		    }

		    break;

	    case 0x4:

		    if (!(g_dsp.r[R_SR] & 0x04))
		    {
			    taken = true;
		    }

		    break;

	    case 0x5:

		    if (g_dsp.r[R_SR] & 0x04)
		    {
			    taken = true;
		    }

		    break;

	    case 0xc:

		    if (!(g_dsp.r[R_SR] & 0x40))
		    {
			    taken = true;
		    }

		    break;

	    case 0xd:

		    if (g_dsp.r[R_SR] & 0x40)
		    {
			    taken = true;
		    }

		    break;

	    case 0xf:
		    taken = true;
		    break;

	    default:
		    // DEBUG_LOG(DSPHLE, "Unknown condition check: 0x%04x\n", _Condition & 0xf);
		    break;
	}

	return(taken);
}


// =======================================================================

void dsp_op_unknown(u16 opc)
{
    _assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "assert while exception");
    ERROR_LOG(DSPHLE, "dsp_op_unknown somewhere");
    g_dsp.pc = g_dsp.err_pc;
}


void dsp_opc_call(u16 opc)
{
	u16 dest = dsp_fetch_code();

	if (CheckCondition(opc & 0xf))
	{
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
		g_dsp.pc = dest;
	}
}


void dsp_opc_ifcc(u16 opc)
{
	if (!CheckCondition(opc & 0xf))
	{
		dsp_fetch_code(); // skip the next opcode
	}
}


void dsp_opc_jcc(u16 opc)
{
	u16 dest = dsp_fetch_code();

	if (CheckCondition(opc & 0xf))
	{
		g_dsp.pc = dest;
	}
}


void dsp_opc_jmpa(u16 opc)
{
	u8 reg;
	u16 addr;

	if ((opc & 0xf) != 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_jmpa");
	}

	reg  = (opc >> 5) & 0x7;
	addr = dsp_op_read_reg(reg);

	if (opc & 0x0010)
	{
		// CALLA
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
	}

	g_dsp.pc = addr;
}


// NEW (added condition check)
void dsp_opc_ret(u16 opc)
{
	if (CheckCondition(opc & 0xf))
	{
		g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);
	}
}


void dsp_opc_rti(u16 opc)
{
	if ((opc & 0xf) != 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_rti");
	}

	g_dsp.r[R_SR] = dsp_reg_load_stack(DSP_STACK_D);
	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);

	g_dsp.exception_in_progress_hack = false;
}


void dsp_opc_halt(u16 opc)
{
	g_dsp.cr |= 0x4;
	g_dsp.pc = g_dsp.err_pc;
}


void dsp_opc_loop(u16 opc)
{
	u16 reg = opc & 0x1f;
	u16 cnt = g_dsp.r[reg];
	u16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	g_dsp.pc = loop_pc + 1;
}


void dsp_opc_loopi(u16 opc)
{
	u16 cnt = opc & 0xff;
	u16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	g_dsp.pc = loop_pc + 1;
}


void dsp_opc_bloop(u16 opc)
{
	u16 reg = opc & 0x1f;
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


void dsp_opc_bloopi(u16 opc)
{
	u16 cnt = opc & 0xff;
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


void dsp_opc_mrr(u16 opc)
{
	u8 sreg = opc & 0x1f;
	u8 dreg = (opc >> 5) & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_op_write_reg(dreg, val);
}


void dsp_opc_lrr(u16 opc)
{
	u8 sreg = (opc >> 5) & 0x3;
	u8 dreg = opc & 0x1f;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	dsp_op_write_reg(dreg, val);

	// post processing of source reg
	switch ((opc >> 7) & 0x3)
	{
	    case 0x0: // LRR
		    break;

	    case 0x1: // LRRD
		    g_dsp.r[sreg]--;
		    break;

	    case 0x2: // LRRI
		    g_dsp.r[sreg]++;
		    break;

	    case 0x3:
		    g_dsp.r[sreg] += g_dsp.r[sreg + 4];
		    break;
	}
}


void dsp_opc_srr(u16 opc)
{
	u8 dreg = (opc >> 5) & 0x3;
	u8 sreg = opc & 0x1f;

	u16 val = dsp_op_read_reg(sreg);
	dsp_dmem_write(g_dsp.r[dreg], val);

	// post processing of dest reg
	switch ((opc >> 7) & 0x3)
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


void dsp_opc_ilrr(u16 opc)
{
	u16 reg  = opc & 0x3;
	u16 dreg = 0x1e + ((opc >> 8) & 1);

	// always to acc0 ?
	g_dsp.r[dreg] = dsp_imem_read(g_dsp.r[reg]);

	switch ((opc >> 2) & 0x3)
	{
	    case 0x0: // no change
		    break;

	    case 0x1: // post decrement
		    g_dsp.r[reg]--;
		    break;

	    case 0x2: // post increment
		    g_dsp.r[reg]++;
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_opc_ilrr");
	}
}


void dsp_opc_lri(u16 opc)
{
	u8 reg  = opc & DSP_REG_MASK;
	u16 imm = dsp_fetch_code();
	dsp_op_write_reg(reg, imm);
}


void dsp_opc_lris(u16 opc)
{
	u8 reg  = ((opc >> 8) & 0x7) + 0x18;
	u16 imm = (s8)opc;
	dsp_op_write_reg(reg, imm);
}


void dsp_opc_lr(u16 opc)
{
	u8 reg   = opc & DSP_REG_MASK;
	u16 addr = dsp_fetch_code();
	u16 val  = dsp_dmem_read(addr);
	dsp_op_write_reg(reg, val);
}


void dsp_opc_sr(u16 opc)
{
	u8 reg   = opc & DSP_REG_MASK;
	u16 addr = dsp_fetch_code();
	u16 val  = dsp_op_read_reg(reg);
	dsp_dmem_write(addr, val);
}


void dsp_opc_si(u16 opc)
{
	u16 addr = (s8)opc;
	u16 imm = dsp_fetch_code();
	dsp_dmem_write(addr, imm);
}


void dsp_opc_tstaxh(u16 opc)
{
	u8 reg  = (opc >> 8) & 0x1;
	s16 val = dsp_get_ax_h(reg);

	Update_SR_Register(val);
}


void dsp_opc_clr(u16 opc)
{
	u8 reg = (opc >> 11) & 0x1;

	dsp_set_long_acc(reg, 0);

	Update_SR_Register((s64)0);
}


void dsp_opc_clrp(u16 opc)
{
	g_dsp.r[0x14] = 0x0000;
	g_dsp.r[0x15] = 0xfff0;
	g_dsp.r[0x16] = 0x00ff;
	g_dsp.r[0x17] = 0x0010;
}


// NEW
void dsp_opc_mulc(u16 opc)
{
	// math new prod
	u8 sreg = (opc >> 11) & 0x1;
	u8 treg = (opc >> 12) & 0x1;

	s64 prod = dsp_get_acc_m(sreg) * dsp_get_ax_h(treg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulcmvz(u16 opc)
{
	ERROR_LOG(DSPHLE, "dsp_opc_mulcmvz ni");
}


// NEW
void dsp_opc_mulcmv(u16 opc)
{
	ERROR_LOG(DSPHLE, "dsp_opc_mulcmv ni");
}


void dsp_opc_cmpar(u16 opc)
{
	u8 rreg = ((opc >> 12) & 0x1) + 0x1a;
	u8 areg = (opc >> 11) & 0x1;

	// we compare
	s64 rr = (s16)g_dsp.r[rreg];
	rr <<= 16;

	s64 ar = dsp_get_long_acc(areg);

	Update_SR_Register(ar - rr);
}


void dsp_opc_cmp(u16 opc)
{
	s64 acc0 = dsp_get_long_acc(0);
	s64 acc1 = dsp_get_long_acc(1);

	Update_SR_Register(acc0 - acc1);
}


void dsp_opc_tsta(u16 opc)
{
	u8 reg  = (opc >> 11) & 0x1;
	s64 acc = dsp_get_long_acc(reg);

	Update_SR_Register(acc);
}


// NEW
void dsp_opc_addaxl(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 dreg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(dreg);
	s64 acx = dsp_get_ax_l(sreg);

	acc += acx;

	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


// NEW
void dsp_opc_addarn(u16 opc)
{
	u8 dreg = opc & 0x3;
	u8 sreg = (opc >> 2) & 0x3;

	g_dsp.r[dreg] += (s16)g_dsp.r[0x04 + sreg];
}


// NEW
void dsp_opc_mulcac(u16 opc)
{
	s64 TempProd = dsp_get_long_prod();

	// update prod
	u8 sreg  = (opc >> 12) & 0x1;
	s64 Prod = (s64)dsp_get_acc_m(sreg) * (s64)dsp_get_acc_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(Prod);

	// update acc
	u8 rreg = (opc >> 8) & 0x1;
	dsp_set_long_acc(rreg, TempProd);
}


// NEW
void dsp_opc_movr(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;
	u8 sreg = ((opc >> 9) & 0x3) + 0x18;

	s64 acc = (s16)g_dsp.r[sreg];
	acc <<= 16;
	acc &= ~0xffff;

	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_movax(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 dreg = (opc >> 8) & 0x1;

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

	dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_xorr(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 dreg = (opc >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] ^= g_dsp.r[0x1a + sreg];

	dsp_opc_tsta(dreg << 11);
}


void dsp_opc_andr(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 dreg = (opc >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] &= g_dsp.r[0x1a + sreg];

	dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_orr(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 dreg = (opc >> 8) & 0x1;

	g_dsp.r[0x1e + dreg] |= g_dsp.r[0x1a + sreg];

	dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_andc(u16 opc)
{
	u8 D = (opc >> 8) & 0x1;

	u16 ac1 = dsp_get_acc_m(D);
	u16 ac2 = dsp_get_acc_m(1 - D);

	dsp_set_long_acc(D, ac1 & ac2);

	if ((ac1 & ac2) == 0)
	{
		g_dsp.r[R_SR] |= 0x20;
	}
	else
	{
		g_dsp.r[R_SR] &= ~0x20;
	}
}


//-------------------------------------------------------------

void dsp_opc_nx(u16 opc)
{}


// NEW
void dsp_opc_andfc(u16 opc)
{
	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_andfc");
	}

	u8 reg  = (opc >> 8) & 0x1;
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


void dsp_opc_andf(u16 opc)
{
	u8 reg;
	u16 imm;
	u16 val;

	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_andf");
	}

	reg = 0x1e + ((opc >> 8) & 0x1);
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


void dsp_opc_subf(u16 opc)
{
	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_subf");
	}

	u8 reg  = 0x1e + ((opc >> 8) & 0x1);
	s64 imm = (s16)dsp_fetch_code();

	s64 val = (s16)g_dsp.r[reg];
	s64 res = val - imm;

	Update_SR_Register(res);
}


void dsp_opc_xori(u16 opc)
{
	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_xori");
	}

	u8 reg  = 0x1e + ((opc >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] ^= imm;

	Update_SR_Register((s16)g_dsp.r[reg]);
}


void dsp_opc_andi(u16 opc)
{
	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_andi");
	}

	u8 reg  = 0x1e + ((opc >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] &= imm;

	Update_SR_Register((s16)g_dsp.r[reg]);
}


// F|RES: i am not sure if this shouldnt be the whole ACC
//
void dsp_opc_ori(u16 opc)
{
	if (opc & 0xf)
	{
		ERROR_LOG(DSPHLE, "dsp_opc_ori");
		return;
	}

	u8 reg  = 0x1e + ((opc >> 8) & 0x1);
	u16 imm = dsp_fetch_code();
	g_dsp.r[reg] |= imm;

	Update_SR_Register((s16)g_dsp.r[reg]);
}


//-------------------------------------------------------------

void dsp_opc_add(u16 opc)
{
	u8 areg  = (opc >> 8) & 0x1;
	s64 acc0 = dsp_get_long_acc(0);
	s64 acc1 = dsp_get_long_acc(1);

	s64 res = acc0 + acc1;

	dsp_set_long_acc(areg, res);

	Update_SR_Register(res);
}


//-------------------------------------------------------------

void dsp_opc_addp(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x1;
	s64 acc = dsp_get_long_acc(dreg);
	acc = acc + dsp_get_long_prod();
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_cmpis(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	s64 val = (s8)opc;
	val <<= 16;

	s64 res = acc - val;

	Update_SR_Register(res);
}


// NEW
// verified at the console
void dsp_opc_addpaxz(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x1;
	u8 sreg = (opc >> 9) & 0x1;

	s64 prod = dsp_get_long_prod() & ~0x0ffff;
	s64 ax_h = dsp_get_long_acx(sreg);
	s64 acc = (prod + ax_h) & ~0x0ffff;

	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


// NEW
void dsp_opc_movpz(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x01;

	// overwrite acc and clear low part
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_decm(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x01;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc -= sub;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_dec(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x01;

	s64 acc = dsp_get_long_acc(dreg) - 1;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_incm(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x1;

	s64 sub = 0x10000;
	s64 acc = dsp_get_long_acc(dreg);
	acc += sub;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_inc(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(dreg);
	acc++;
	dsp_set_long_acc(dreg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_neg(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc = 0 - acc;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_movnp(u16 opc)
{
	ERROR_LOG(DSPHLE, "dsp_opc_movnp\n");
}


// NEW
void dsp_opc_addax(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;
	u8 sreg = (opc >> 9) & 0x1;

	s64 ax  = dsp_get_long_acx(sreg);
	s64 acc = dsp_get_long_acc(areg);
	acc += ax;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_addr(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;
	u8 sreg = ((opc >> 9) & 0x3) + 0x18;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc += ax;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_subr(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;
	u8 sreg = ((opc >> 9) & 0x3) + 0x18;

	s64 ax = (s16)g_dsp.r[sreg];
	ax <<= 16;

	s64 acc = dsp_get_long_acc(areg);
	acc -= ax;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}

// NEW
void dsp_opc_subax(u16 opc)
{
	int regD = (opc >> 8) & 0x1;
	int regT = (opc >> 9) & 0x1;

	s64 Acc = dsp_get_long_acc(regD) - dsp_get_long_acx(regT);

	dsp_set_long_acc(regD, Acc);
	Update_SR_Register(Acc);
}

void dsp_opc_addis(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 Imm = (s8)opc;
	Imm <<= 16;
	s64 acc = dsp_get_long_acc(areg);
	acc += Imm;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_addi(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 sub = (s16)dsp_fetch_code();
	sub <<= 16;
	s64 acc = dsp_get_long_acc(areg);
	acc += sub;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_lsl16(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc <<= 16;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


// NEW
void dsp_opc_madd(u16 opc)
{
	u8 sreg = (opc >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	prod += (s64)dsp_get_ax_l(sreg) * (s64)dsp_get_ax_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}


void dsp_opc_lsr16(u16 opc)
{
	u8 areg = (opc >> 8) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc >>= 16;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


void dsp_opc_asr16(u16 opc)
{
	u8 areg = (opc >> 11) & 0x1;

	s64 acc = dsp_get_long_acc(areg);
	acc >>= 16;
	dsp_set_long_acc(areg, acc);

	Update_SR_Register(acc);
}


union UOpcodeShifti
{
	u16 Hex;
	struct
	{
		signed shift        : 6;
		unsigned negating   : 1;
		unsigned arithmetic : 1;
		unsigned areg       : 1;
		unsigned op         : 7;
	};
	struct
	{
		unsigned ushift     : 6;
	};
	UOpcodeShifti(u16 _Hex)
		: Hex(_Hex) {}
};

void dsp_opc_shifti(u16 opc)
{
	UOpcodeShifti Opcode(opc);

	// direction: left
	bool ShiftLeft = true;
	u16 shift = Opcode.ushift;

	if ((Opcode.negating) && (Opcode.shift < 0))
	{
		ShiftLeft = false;
		shift = -Opcode.shift;
	}

	s64 acc;
	u64 uacc;

	if (Opcode.arithmetic)
	{
		// arithmetic shift
		uacc = dsp_get_long_acc(Opcode.areg);

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
		acc = dsp_get_long_acc(Opcode.areg);

		if (!ShiftLeft)
		{
			acc >>= shift;
		}
		else
		{
			acc <<= shift;
		}
	}

	dsp_set_long_acc(Opcode.areg, acc);

	Update_SR_Register(acc);
}


//-------------------------------------------------------------
// hcs give me this code!!
void dsp_opc_dar(u16 opc)
{
	u8 reg = opc & 0x3;

	int temp = g_dsp.r[reg] + g_dsp.r[8];

	if (temp <= 0x7ff){g_dsp.r[reg] = temp;}
	else {g_dsp.r[reg]--;}
}


// hcs give me this code!!
void dsp_opc_iar(u16 opc)
{
	u8 reg = opc & 0x3;

	int temp = g_dsp.r[reg] + g_dsp.r[8];

	if (temp <= 0x7ff){g_dsp.r[reg] = temp;}
	else {g_dsp.r[reg]++;}
}


//-------------------------------------------------------------

void dsp_opc_sbclr(u16 opc)
{
	u8 bit = (opc & 0xff) + 6;
	g_dsp.r[R_SR] &= ~(1 << bit);
}


void dsp_opc_sbset(u16 opc)
{
	u8 bit = (opc & 0xff) + 6;
	g_dsp.r[R_SR] |= (1 << bit);
}


void dsp_opc_srbith(u16 opc)
{
	switch ((opc >> 8) & 0xf)
	{
	    case 0xe: // SET40
		    g_dsp.r[R_SR] &= ~(1 << 14);
		    break;

		// FIXME: Both of these appear in the beginning of the Wind Waker
		//case 0xb:
		//case 0xc:

/*	case 0xf:	// SET16   // that doesnt happen on a real console
   	g_dsp.r[R_SR] |= (1 << 14);
   	break;*/

	    default:
		    break;
	}
}


//-------------------------------------------------------------

void dsp_opc_movp(u16 opc)
{
	u8 dreg = (opc >> 8) & 0x1;

	s64 prod = dsp_get_long_prod();
	s64 acc = prod;
	dsp_set_long_acc(dreg, acc);
}


void dsp_opc_mul(u16 opc)
{
	u8 sreg  = (opc >> 11) & 0x1;
	s64 prod = (s64)dsp_get_ax_h(sreg) * (s64)dsp_get_ax_l(sreg) * GetMultiplyModifier();

	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}

// NEW
void dsp_opc_mulac(u16 opc)
{
	// add old prod to acc
	u8 rreg = (opc >> 8) & 0x1;
	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc >> 11) & 0x1;
	s64 prod = dsp_get_ax_l(sreg) * dsp_get_ax_h(sreg) * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


void dsp_opc_mulmv(u16 opc)
{
	u8 rreg  = (opc >> 8) & 0x1;
	s64 prod = dsp_get_long_prod();
	s64 acc = prod;
	dsp_set_long_acc(rreg, acc);

	u8 areg  = ((opc >> 11) & 0x1) + 0x18;
	u8 breg  = ((opc >> 11) & 0x1) + 0x1a;
	s64 val1 = (s16)g_dsp.r[areg];
	s64 val2 = (s16)g_dsp.r[breg];

	prod = val1 * val2 * GetMultiplyModifier();

	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulmvz(u16 opc)
{
	u8 sreg = (opc >> 11) & 0x1;
	u8 rreg = (opc >> 8) & 0x1;

	// overwrite acc and clear low part
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(rreg, acc);

	// math prod
	prod = (s64)g_dsp.r[0x18 + sreg] * (s64)g_dsp.r[0x1a + sreg] * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulx(u16 opc)
{
	u8 sreg = ((opc >> 12) & 0x1);
	u8 treg = ((opc >> 11) & 0x1);

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxac(u16 opc)
{
	// add old prod to acc
	u8 rreg = (opc >> 8) & 0x1;
	s64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc >> 12) & 0x1;
	u8 treg = (opc >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxmv(u16 opc)
{
	// add old prod to acc
	u8 rreg = ((opc >> 8) & 0x1);
	s64 acR = dsp_get_long_prod();
	dsp_set_long_acc(rreg, acR);

	// math new prod
	u8 sreg = (opc >> 12) & 0x1;
	u8 treg = (opc >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxmvz(u16 opc)
{
	// overwrite acc and clear low part
	u8 rreg  = (opc >> 8) & 0x1;
	s64 prod = dsp_get_long_prod();
	s64 acc = prod & ~0xffff;
	dsp_set_long_acc(rreg, acc);

	// math prod
	u8 sreg = (opc >> 12) & 0x1;
	u8 treg = (opc >> 11) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	prod = val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);

	Update_SR_Register(prod);
}


// NEW
void dsp_opc_sub(u16 opc)
{
	u8 D = (opc >> 8) & 0x1;
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

// NEW
void dsp_opc_maddx(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 treg = (opc >> 8) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = dsp_get_long_prod();
	prod += val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_msubx(u16 opc)
{
	u8 sreg = (opc >> 9) & 0x1;
	u8 treg = (opc >> 8) & 0x1;

	s64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	s64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	s64 prod = dsp_get_long_prod();
	prod -= val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_maddc(u16 opc)
{
	u32 sreg = (opc >> 9) & 0x1;
	u32 treg = (opc >> 8) & 0x1;

	s64 val1 = dsp_get_acc_m(sreg);
	s64 val2 = dsp_get_ax_h(treg);

	s64 prod = dsp_get_long_prod();
	prod += val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_msubc(u16 opc)
{
	u32 sreg = (opc >> 9) & 0x1;
	u32 treg = (opc >> 8) & 0x1;

	s64 val1 = dsp_get_acc_m(sreg);
	s64 val2 = dsp_get_ax_h(treg);

	s64 prod = dsp_get_long_prod();
	prod -= val1 * val2 * GetMultiplyModifier();
	dsp_set_long_prod(prod);
}


//-------------------------------------------------------------
void dsp_op0(u16 opc)
{
	if (opc == 0)
	{
		return;
	}

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:

		    switch ((opc >> 4) & 0xf)
		    {
			case 0x0:

				switch (opc & 0xf)
				{
				    case 0x4:
				    case 0x5:
				    case 0x6:
				    case 0x7:
					    dsp_opc_dar(opc);
					    break;

				    case 0x8:
				    case 0x9:
				    case 0xa:
				    case 0xb:
					    dsp_opc_iar(opc);
					    break;

				    default:
					    ERROR_LOG(DSPHLE, "dsp_op0");
					    break;
				}

				break;

			case 0x1:
				dsp_opc_addarn(opc);
				break;

			case 0x2: // HALT
				dsp_opc_halt(opc);
				break;

			case 0x4: // LOOP
			case 0x5: // LOOP
				dsp_opc_loop(opc);
				break;

			case 0x6: // BLOOP
			case 0x7: // BLOOP
				dsp_opc_bloop(opc);
				break;

			case 0x8: // LRI
			case 0x9: // LRI
				dsp_opc_lri(opc);
				break;

			case 0xC: // LR
			case 0xD: // LR
				dsp_opc_lr(opc);
				break;

			case 0xE: // SR
			case 0xF: // SR
				dsp_opc_sr(opc);
				break;

			default:
				ERROR_LOG(DSPHLE, "dsp_op0");
				break;
		    }

		    break;

	    case 0x2:

		    switch ((opc >> 4) & 0xf)
		    {
			case 0x0: // ADDI
				dsp_opc_addi(opc);
				break;

			case 0x1: // IL
				dsp_opc_ilrr(opc);
				break;

			case 0x2: // XORI
				dsp_opc_xori(opc);
				break;

			case 0x4: // ANDI
				dsp_opc_andi(opc);
				break;

			case 0x6: // ORI
				dsp_opc_ori(opc);
				break;

			case 0x7: //
				dsp_opc_ifcc(opc);
				break;

			case 0x8: // SUBF
				dsp_opc_subf(opc);
				break;

			case 0x9: // Jxx
				dsp_opc_jcc(opc);
				break;

			case 0xa: // ANDF
				dsp_opc_andf(opc);
				break;

			case 0xb: // CALL
				dsp_opc_call(opc);
				break;

			case 0xc:
				dsp_opc_andfc(opc);
				break;

			case 0xd: // RET
				dsp_opc_ret(opc);
				break;

			case 0xf: // RTI
				dsp_opc_rti(opc);
				break;

			default:
				ERROR_LOG(DSPHLE, "dsp_op0");
				break;
		    }

		    break;

	    case 0x3:

		    switch ((opc >> 4) & 0xf)
		    {
			case 0x0: // ADDAI
				dsp_opc_addi(opc);
				break;

			case 0x1: // ILR
				dsp_opc_ilrr(opc);
				break;

			case 0x2: // XORI
				dsp_opc_xori(opc);
				break;

			case 0x4: // ANDI
				dsp_opc_andi(opc);
				break;

			case 0x6: // ORI
				dsp_opc_ori(opc);
				break;

			case 0x8: // SUBF
				dsp_opc_subf(opc);
				break;

			case 0xa: // ANDF
				dsp_opc_andf(opc);
				break;

			case 0xc: // ANDFC
				dsp_opc_andfc(opc);
				break;

			default:
				ERROR_LOG(DSPHLE, "dsp_op0");
				break;
		    }

		    break;

	    case 0x4:
	    case 0x5:
		    dsp_opc_addis(opc);
		    break;

	    case 0x6: // SUBISF
	    case 0x7:
		    dsp_opc_cmpis(opc);
		    break;

	    case 0x8: // LRIS
	    case 0x9:
	    case 0xa:
	    case 0xb:
	    case 0xc:
	    case 0xd:
	    case 0xe:
	    case 0xf:
		    dsp_opc_lris(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op0");
		    break;
	}
}


void dsp_op1(u16 opc)
{
	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
		    dsp_opc_loopi(opc);
		    break;

	    case 0x1: // BLOOPI
		    dsp_opc_bloopi(opc);
		    break;

	    case 0x2: // SBCLR
		    dsp_opc_sbclr(opc);
		    break;

	    case 0x3: // SBSET
		    dsp_opc_sbset(opc);
		    break;

	    case 0x4: // shifti
	    case 0x5:
		    dsp_opc_shifti(opc);
		    break;

	    case 0x6: // SI
		    dsp_opc_si(opc);
		    break;

	    case 0x7: // JMPA/CALLA
		    dsp_opc_jmpa(opc);
		    break;

	    case 0x8: // LRRx
	    case 0x9: // LRRx
		    dsp_opc_lrr(opc);
		    break;

	    case 0xa: // SRRx
	    case 0xb: // SRRx
		    dsp_opc_srr(opc);
		    break;

	    case 0xc: // MRR
	    case 0xd: // MRR
	    case 0xe: // MRR
	    case 0xf: // MRR
		    dsp_opc_mrr(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op1");
		    break;
	}
}


void dsp_op2(u16 opc)
{
	// lrs, srs
	u8 reg   = ((opc >> 8) & 0x7) + 0x18;
	u16 addr = (s8) opc;

	if (opc & 0x0800)
	{
		// srs
		dsp_dmem_write(addr, g_dsp.r[reg]);
	}
	else
	{
		// lrs
		g_dsp.r[reg] = dsp_dmem_read(addr);
	}
}


void dsp_op3(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x1:
	    case 0x2:
	    case 0x3:
		    dsp_opc_xorr(opc);
		    break;

	    case 0x4:
	    case 0x5:
	    case 0x6:
	    case 0x7:
		    dsp_opc_andr(opc);
		    break;

	    case 0x8:
	    case 0x9:
	    case 0xa:
	    case 0xb:
		    dsp_opc_orr(opc);
		    break;

	    case 0xc:
	    case 0xd:
		    dsp_opc_andc(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op3");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op4(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x1:
	    case 0x2:
	    case 0x3:
	    case 0x4:
	    case 0x5:
	    case 0x6:
	    case 0x7:
		    dsp_opc_addr(opc);
		    break;

	    case 0x8:
	    case 0x9:
	    case 0xa:
	    case 0xb:
		    dsp_opc_addax(opc);
		    break;

	    case 0xc:
	    case 0xd:
		    dsp_opc_add(opc);
		    break;

	    case 0xe:
	    case 0xf:
		    dsp_opc_addp(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op4");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op5(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x1:
	    case 0x2:
	    case 0x3:
	    case 0x4:
	    case 0x5:
	    case 0x6:
	    case 0x7:
		    dsp_opc_subr(opc);
		    break;

		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
			dsp_opc_subax(opc);
			break;

	    case 0xc:
	    case 0xd:
		    dsp_opc_sub(opc);
		    break;

	    default:
			ERROR_LOG(DSPHLE, "dsp_op5: %x", (opc >> 8) & 0xf);
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op6(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x00: // MOVR
	    case 0x01:
	    case 0x02:
	    case 0x03:
	    case 0x04:
	    case 0x05:
	    case 0x06:
	    case 0x07:
		    dsp_opc_movr(opc);
		    break;

	    case 0x8: // MVAXA
	    case 0x9:
	    case 0xa:
	    case 0xb:
		    dsp_opc_movax(opc);
		    break;

	    case 0xe:
	    case 0xf:
		    dsp_opc_movp(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op6");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op7(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x1:
	    case 0x2:
	    case 0x3:
		    dsp_opc_addaxl(opc);
		    break;

	    case 0x4:
	    case 0x5:
		    dsp_opc_incm(opc);
		    break;

	    case 0x6:
	    case 0x7:
		    dsp_opc_inc(opc);
		    break;

	    case 0x8:
	    case 0x9:
		    dsp_opc_decm(opc);
		    break;

	    case 0xa:
	    case 0xb:
		    dsp_opc_dec(opc);
		    break;

	    case 0xc:
	    case 0xd:
		    dsp_opc_neg(opc);
		    break;

	    case 0xe:
	    case 0xf:
		    dsp_opc_movnp(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op7");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op8(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x8:
		    dsp_opc_nx(opc);
		    break;

	    case 0x1: // CLR 0
	    case 0x9: // CLR	1
		    dsp_opc_clr(opc);
		    break;

	    case 0x2: // CMP
		    dsp_opc_cmp(opc);
		    break;

	    case 0x4: // CLRP
		    dsp_opc_clrp(opc);
		    break;

	    case 0x6:
	    case 0x7:
		    dsp_opc_tstaxh(opc);
		    break;

	    case 0xc:
	    case 0xb:
	    case 0xe: // SET40
	    case 0xd:
	    case 0xa:
	    case 0xf:
		    dsp_opc_srbith(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op8");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op9(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x02:
	    case 0x03:
	    case 0x0a:
	    case 0x0b:
		    dsp_opc_mulmvz(opc);
		    break;

	    case 0x04:
	    case 0x05:
	    case 0x0c:
	    case 0x0d:
		    dsp_opc_mulac(opc);
		    break;

	    case 0x6:
	    case 0x7:
	    case 0xe:
	    case 0xf:
		    dsp_opc_mulmv(opc);
		    break;

	    case 0x0:
	    case 0x8:
		    dsp_opc_mul(opc);
		    break;

	    case 0x1:
	    case 0x9:
		    dsp_opc_asr16(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_op9");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opab(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0x7)
	{
	    case 0x0:
		    dsp_opc_mulx(opc);
		    break;

	    case 0x1:
		    dsp_opc_tsta(opc);
		    break;

	    case 0x2:
	    case 0x3:
		    dsp_opc_mulxmvz(opc);
		    break;

	    case 0x4:
	    case 0x5:
		    dsp_opc_mulxac(opc);
		    break;

	    case 0x6:
	    case 0x7:
		    dsp_opc_mulxmv(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_opab");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opcd(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0x7)
	{
	    case 0x0: // MULC
		    dsp_opc_mulc(opc);
		    break;

	    case 0x1: // CMPAR
		    dsp_opc_cmpar(opc);
		    break;

	    case 0x2: // MULCMVZ
	    case 0x3:
		    dsp_opc_mulcmvz(opc);
		    break;

	    case 0x4: // MULCAC
	    case 0x5:
		    dsp_opc_mulcac(opc);
		    break;

	    case 0x6: // MULCMV
	    case 0x7:
		    dsp_opc_mulcmv(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_opcd");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_ope(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 10) & 0x3)
	{
	    case 0x00: // MADDX
		    dsp_opc_maddx(opc);
		    break;

	    case 0x01: // MSUBX
		    dsp_opc_msubx(opc);
		    break;

	    case 0x02: // MADDC
		    dsp_opc_maddc(opc);
		    break;

	    case 0x03: // MSUBC
		    dsp_opc_msubc(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_ope");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opf(u16 opc)
{
	dsp_op_ext_ops_pro(opc);

	switch ((opc >> 8) & 0xf)
	{
	    case 0x0:
	    case 0x1:
		    dsp_opc_lsl16(opc);
		    break;

	    case 0x02:
	    case 0x03:
		    dsp_opc_madd(opc);
		    break;

	    case 0x4:
	    case 0x5:
		    dsp_opc_lsr16(opc);
		    break;

	    case 0x8:
	    case 0x9:
	    case 0xa:
	    case 0xb:
		    dsp_opc_addpaxz(opc);
		    break;

	    case 0xe:
	    case 0xf:
		    dsp_opc_movpz(opc);
		    break;

	    default:
		    ERROR_LOG(DSPHLE, "dsp_opf");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


