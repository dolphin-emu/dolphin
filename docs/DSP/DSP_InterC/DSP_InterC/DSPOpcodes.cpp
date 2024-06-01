#include <stdafx.h>

#include "DSPOpcodes.h"
#include "DSPExt.h"

#define SR_CMP_MASK     0x3f
#define DSP_REG_MASK    0x1f

/*

bool CheckCondition(uint8 _Condition)
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
		    // DebugLog("Unknown condition check: 0x%04x\n", _Condition & 0xf);
		    break;
	}

	return(taken);
}
*/

// =======================================================================

//-------------------------------------------------------------------------------
void (*dsp_op[])(uint16 opc) =
{
	dsp_op0, dsp_op1, dsp_op2, dsp_op3,
	dsp_op4, dsp_op5, dsp_op6, dsp_op7,
	dsp_op8, dsp_op9, dsp_opab, dsp_opab,
	dsp_opcd, dsp_opcd, dsp_ope, dsp_opf,
};

void DecodeOpcode(uint16 op)
{
	dsp_op[op >> 12](op);

}
void dsp_op_unknown(uint16 opc)
{
	ErrorLog("dsp_op_unknown somewhere");
	OutBuffer::AddCode("unknown somewhere");
}


void dsp_opc_call(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_call");

/*	uint16 dest = FetchOpcode();
	if (CheckCondition(opc & 0xf))
	{	
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
		g_dsp.pc = dest;
	} */
}


void dsp_opc_ifcc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_call");

/*	if (!CheckCondition(opc & 0xf))
	{
		FetchOpcode(); // skip the next opcode
	}*/
}


void dsp_opc_jcc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_call");

/*	uint16 dest = FetchOpcode();

	if (CheckCondition(opc & 0xf))
	{
		g_dsp.pc = dest;
	}*/
}


void dsp_opc_jmpa(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_call");

/*	uint8 reg;
	uint16 addr;

	if ((opc & 0xf) != 0xf)
	{
		ErrorLog("dsp_opc_jmpa");
	}

	reg  = (opc >> 5) & 0x7;
	addr = dsp_op_read_reg(reg);

	if (opc & 0x0010)
	{
		// CALLA
		dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
	}

	g_dsp.pc = addr;*/
}


// NEW (added condition check)
void dsp_opc_ret(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_ret");

/*	if (CheckCondition(opc & 0xf))
	{
		g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);
	}*/
}


void dsp_opc_rti(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_rti");

/*	if ((opc & 0xf) != 0xf)
	{
		ErrorLog("dsp_opc_rti");
	}

	g_dsp.r[R_SR] = dsp_reg_load_stack(DSP_STACK_D);
	g_dsp.pc = dsp_reg_load_stack(DSP_STACK_C);

	g_dsp.exception_in_progress_hack = false;*/
}


void dsp_opc_halt(uint16 opc)
{
	OutBuffer::AddCode("HALT");

/*	g_dsp.cr |= 0x4;
	g_dsp.pc = g_dsp.err_pc;*/
}


void dsp_opc_loop(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_loop");

/*	uint16 reg = opc & 0x1f;
	uint16 cnt = g_dsp.r[reg];
	uint16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	g_dsp.pc = loop_pc + 1; */
}


void dsp_opc_loopi(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_loopi");

/*	uint16 cnt = opc & 0xff;
	uint16 loop_pc = g_dsp.pc;

	while (cnt--)
	{
		gdsp_loop_step();
		g_dsp.pc = loop_pc;
	}

	g_dsp.pc = loop_pc + 1;*/
}


void dsp_opc_bloop(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_bloop");

/*	uint16 reg = opc & 0x1f;
	uint16 cnt = g_dsp.r[reg];
	uint16 loop_pc = FetchOpcode();

	if (cnt)
	{
		dsp_reg_store_stack(0, g_dsp.pc);
		dsp_reg_store_stack(2, loop_pc);
		dsp_reg_store_stack(3, cnt);
	}
	else
	{
		g_dsp.pc = loop_pc + 1;
	}*/
}


void dsp_opc_bloopi(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_bloopi");

/*	uint16 cnt = opc & 0xff;
	uint16 loop_pc = FetchOpcode();

	if (cnt)
	{
		dsp_reg_store_stack(0, g_dsp.pc);
		dsp_reg_store_stack(2, loop_pc);
		dsp_reg_store_stack(3, cnt);
	}
	else
	{
		g_dsp.pc = loop_pc + 1;
	}*/
}


//-------------------------------------------------------------


void dsp_opc_mrr(uint16 opc)
{
	uint8 sreg = opc & 0x1f;
	uint8 dreg = (opc >> 5) & 0x1f;

	OutBuffer::AddCode("%s = %s", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(sreg));

//	uint16 val = dsp_op_read_reg(sreg);
//	dsp_op_write_reg(dreg, val);
}


void dsp_opc_lrr(uint16 opc)
{
	uint8 sreg = (opc >> 5) & 0x3;
	uint8 dreg = opc & 0x1f;

	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(sreg));

//	uint16 val = dsp_dmem_read(g_dsp.r[sreg]);
//	dsp_op_write_reg(dreg, val);

	// post processing of source reg
	switch ((opc >> 7) & 0x3)
	{
	    case 0x0: // LRR
		    break;

	    case 0x1: // LRRD
			OutBuffer::AddCode("%s--", OutBuffer::GetRegName(sreg));
		    // g_dsp.r[sreg]--;
		    break;

	    case 0x2: // LRRI
			OutBuffer::AddCode("%s++", OutBuffer::GetRegName(sreg));
		    //g_dsp.r[sreg]++;
		    break;

	    case 0x3:
			OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(sreg), OutBuffer::GetRegName(sreg+4));
		    // g_dsp.r[sreg] += g_dsp.r[sreg + 4];
		    break;
	}
}


void dsp_opc_srr(uint16 opc)
{
	uint8 dreg = (opc >> 5) & 0x3;
	uint8 sreg = opc & 0x1f;

	OutBuffer::AddCode("WriteDMEM(%s, %s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(sreg));

	//uint16 val = dsp_op_read_reg(sreg);
	// dsp_dmem_write(g_dsp.r[dreg], val);

	// post processing of source reg
	switch ((opc >> 7) & 0x3)
	{
	    case 0x0: // SRR
		    break;

	    case 0x1: // SRRD
			OutBuffer::AddCode("%s--", OutBuffer::GetRegName(dreg));
		    // g_dsp.r[dreg]--;
		    break;

	    case 0x2: // SRRI
			OutBuffer::AddCode("%s++", OutBuffer::GetRegName(dreg));
		    // g_dsp.r[dreg]++;
		    break;

	    case 0x3: // SRRX
			OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(dreg+4));
		    //g_dsp.r[dreg] += g_dsp.r[dreg + 4];
		    break;
	}
}


void dsp_opc_ilrr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_ilrr");

/*	uint16 reg  = opc & 0x3;
	uint16 dreg = 0x1e + ((opc >> 8) & 1);

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
		    ErrorLog("dsp_opc_ilrr");
	}*/
}


void dsp_opc_lri(uint16 opc)
{
	uint8 reg  = opc & DSP_REG_MASK;
	uint16 imm = FetchOpcode();

	OutBuffer::AddCode("%s = 0x%02x", OutBuffer::GetRegName(reg), imm);
	// dsp_op_write_reg(reg, imm);
}


void dsp_opc_lris(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_lris");

/*	uint8 reg  = ((opc >> 8) & 0x7) + 0x18;
	uint16 imm = (sint8)opc;
	dsp_op_write_reg(reg, imm);*/
}


void dsp_opc_lr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_lr");

/*	uint8 reg   = opc & DSP_REG_MASK;
	uint16 addr = FetchOpcode();
	uint16 val  = dsp_dmem_read(addr);
	dsp_op_write_reg(reg, val);*/
}


void dsp_opc_sr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_sr");

	/*uint8 reg   = opc & DSP_REG_MASK;
	uint16 addr = FetchOpcode();
	uint16 val  = dsp_op_read_reg(reg);
	dsp_dmem_write(addr, val);*/
}


void dsp_opc_si(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_si");

	//uint16 addr = (sint8)opc;
	//uint16 imm = readimemory();
	//dsp_dmem_write(addr, imm);
}


void dsp_opc_tstaxh(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_tstaxh");

	//uint8 reg  = (opc >> 8) & 0x1;
	//sint16 val = dsp_get_ax_h(reg);

	//Update_SR_Register(val);
}


void dsp_opc_clr(uint16 opc)
{
	uint8 reg = (opc >> 11) & 0x1;

	OutBuffer::AddCode("ACC%i = 0", reg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", reg);

	//dsp_set_long_acc(reg, 0);

	//Update_SR_Register((sint64)0);
}


void dsp_opc_clrp(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_clrp");

	//g_dsp.r[0x14] = 0x0000;
	//g_dsp.r[0x15] = 0xfff0;
	//g_dsp.r[0x16] = 0x00ff;
	//g_dsp.r[0x17] = 0x0010;
}


// NEW
void dsp_opc_mulc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulc");

	// math new prod
	//uint8 sreg = (opc >> 11) & 0x1;
	//uint8 treg = (opc >> 12) & 0x1;

	//sint64 prod = dsp_get_acc_m(sreg) * dsp_get_ax_h(treg) * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulcmvz(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulcmvz");

	ErrorLog("dsp_opc_mulcmvz ni");
}


// NEW
void dsp_opc_mulcmv(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulcmv");

	ErrorLog("dsp_opc_mulcmv ni");
}


void dsp_opc_cmpar(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_cmpar");

	//uint8 rreg = ((opc >> 12) & 0x1) + 0x1a;
	//uint8 areg = (opc >> 11) & 0x1;

	//// we compare
	//sint64 rr = (sint16)g_dsp.r[rreg];
	//rr <<= 16;

	//sint64 ar = dsp_get_long_acc(areg);

	//Update_SR_Register(ar - rr);
}


void dsp_opc_cmp(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_cmp");

	//sint64 acc0 = dsp_get_long_acc(0);
	//sint64 acc1 = dsp_get_long_acc(1);

	//Update_SR_Register(acc0 - acc1);
}


void dsp_opc_tsta(uint16 opc)
{
	uint8 reg  = (opc >> 11) & 0x1;
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", reg);

	//uint8 reg  = (opc >> 11) & 0x1;
	//sint64 acc = dsp_get_long_acc(reg);

	//Update_SR_Register(acc);
}


// NEW
void dsp_opc_addaxl(uint16 opc)
{
	uint8 sreg = (opc >> 9) & 0x1;
	uint8 dreg = (opc >> 8) & 0x1;

	OutBuffer::AddCode("ACC%i += AX%i_l", dreg, sreg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", dreg);

	//sint64 acc = dsp_get_long_acc(dreg);
	//sint64 acx = dsp_get_ax_l(sreg);

	//acc += acx;

	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


// NEW
void dsp_opc_addarn(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addarn");

	//uint8 dreg = opc & 0x3;
	//uint8 sreg = (opc >> 2) & 0x3;

	//g_dsp.r[dreg] += (sint16)g_dsp.r[0x04 + sreg];
}


// NEW
void dsp_opc_mulcac(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulcac");

	//sint64 TempProd = dsp_get_long_prod();

	//// update prod
	//uint8 sreg  = (opc >> 12) & 0x1;
	//sint64 Prod = (sint64)dsp_get_acc_m(sreg) * (sint64)dsp_get_acc_h(sreg) * GetMultiplyModifier();
	//dsp_set_long_prod(Prod);

	//// update acc
	//uint8 rreg = (opc >> 8) & 0x1;
	//dsp_set_long_acc(rreg, TempProd);
}


// NEW
void dsp_opc_movr(uint16 opc)
{
	uint8 areg = (opc >> 8) & 0x1;
	uint8 sreg = ((opc >> 9) & 0x3) + 0x18;

	OutBuffer::AddCode("ACC%i = (%s << 16) & ~0xFFFF", areg, OutBuffer::GetRegName(sreg));
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", areg);

	//sint64 acc = (sint16)g_dsp.r[sreg];
	//acc <<= 16;
	//acc &= ~0xffff;

	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_movax(uint16 opc)
{
	uint8 sreg = (opc >> 9) & 0x1;
	uint8 dreg = (opc >> 8) & 0x1;

	OutBuffer::AddCode("%s = %s", OutBuffer::GetRegName(0x1c + dreg), OutBuffer::GetRegName(0x18 + sreg));
	OutBuffer::AddCode("%s = %s", OutBuffer::GetRegName(0x1e + dreg), OutBuffer::GetRegName(0x1a + sreg));

	//g_dsp.r[0x1c + dreg] = g_dsp.r[0x18 + sreg];
	//g_dsp.r[0x1e + dreg] = g_dsp.r[0x1a + sreg];

	OutBuffer::AddCode("%s = (%s < 0) ? 0xFFFF : 0x0000",	OutBuffer::GetRegName(0x10 + dreg),
															OutBuffer::GetRegName(0x1a + sreg));

	//if ((sint16)g_dsp.r[0x1a + sreg] < 0)
	//{
	//	g_dsp.r[0x10 + dreg] = 0xffff;
	//}
	//else
	//{
	//	g_dsp.r[0x10 + dreg] = 0;
	//}

	dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_xorr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_xorr");

	//uint8 sreg = (opc >> 9) & 0x1;
	//uint8 dreg = (opc >> 8) & 0x1;

	//g_dsp.r[0x1e + dreg] ^= g_dsp.r[0x1a + sreg];

	//dsp_opc_tsta(dreg << 11);
}


void dsp_opc_andr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_andr");

	//uint8 sreg = (opc >> 9) & 0x1;
	//uint8 dreg = (opc >> 8) & 0x1;

	//g_dsp.r[0x1e + dreg] &= g_dsp.r[0x1a + sreg];

	//dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_orr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_orr");

	//uint8 sreg = (opc >> 9) & 0x1;
	//uint8 dreg = (opc >> 8) & 0x1;

	//g_dsp.r[0x1e + dreg] |= g_dsp.r[0x1a + sreg];

	//dsp_opc_tsta(dreg << 11);
}


// NEW
void dsp_opc_andc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_andc");

	//uint8 D = (opc >> 8) & 0x1;

	//uint16 ac1 = dsp_get_acc_m(D);
	//uint16 ac2 = dsp_get_acc_m(1 - D);

	//dsp_set_long_acc(D, ac1 & ac2);

	//if ((ac1 & ac2) == 0)
	//{
	//	g_dsp.r[R_SR] |= 0x20;
	//}
	//else
	//{
	//	g_dsp.r[R_SR] &= ~0x20;
	//}
}


//-------------------------------------------------------------

void dsp_opc_nx(uint16 opc)
{}


// NEW
void dsp_opc_andfc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_andfc");

	//if (opc & 0xf)
	//{
	//	ErrorLog("dsp_opc_andfc");
	//}

	//uint8 reg  = (opc >> 8) & 0x1;
	//uint16 imm = FetchOpcode();
	//uint16 val = dsp_get_acc_m(reg);

	//if ((val & imm) == imm)
	//{
	//	g_dsp.r[R_SR] |= 0x40;
	//}
	//else
	//{
	//	g_dsp.r[R_SR] &= ~0x40;
	//}
}


void dsp_opc_andf(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_andf");

	//uint8 reg;
	//uint16 imm;
	//uint16 val;

	//if (opc & 0xf)
	//{
	//	ErrorLog("dsp_opc_andf");
	//}

	//reg = 0x1e + ((opc >> 8) & 0x1);
	//imm = FetchOpcode();
	//val = g_dsp.r[reg];

	//if ((val & imm) == 0)
	//{
	//	g_dsp.r[R_SR] |= 0x40;
	//}
	//else
	//{
	//	g_dsp.r[R_SR] &= ~0x40;
	//}
}


void dsp_opc_subf(uint16 opc)
{
	if (opc & 0xf)
	{
		ErrorLog("dsp_opc_subf");
	}

	uint8 reg  = 0x1e + ((opc >> 8) & 0x1);
	sint64 imm = (sint16)FetchOpcode();

	OutBuffer::AddCode("missing: dsp_opc_subf");

	//sint64 val = (sint16)g_dsp.r[reg];
	//sint64 res = val - imm;

	//Update_SR_Register(res);
}


void dsp_opc_xori(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_xori");

	//if (opc & 0xf)
	//{
	//	ErrorLog("dsp_opc_xori");
	//}

	//uint8 reg  = 0x1e + ((opc >> 8) & 0x1);
	//uint16 imm = FetchOpcode();
	//g_dsp.r[reg] ^= imm;

	//Update_SR_Register((sint16)g_dsp.r[reg]);
}


void dsp_opc_andi(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_andi");

	//if (opc & 0xf)
	//{
	//	ErrorLog("dsp_opc_andi");
	//}

	//uint8 reg  = 0x1e + ((opc >> 8) & 0x1);
	//uint16 imm = FetchOpcode();
	//g_dsp.r[reg] &= imm;

	//Update_SR_Register((sint16)g_dsp.r[reg]);
}


// F|RES: i am not sure if this shouldnt be the whole ACC
//
void dsp_opc_ori(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_ori");

	//if (opc & 0xf)
	//{
	//	return(ErrorLog("dsp_opc_ori"));
	//}

	//uint8 reg  = 0x1e + ((opc >> 8) & 0x1);
	//uint16 imm = FetchOpcode();
	//g_dsp.r[reg] |= imm;

	//Update_SR_Register((sint16)g_dsp.r[reg]);
}


//-------------------------------------------------------------

void dsp_opc_add(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_add");

	//uint8 areg  = (opc >> 8) & 0x1;
	//sint64 acc0 = dsp_get_long_acc(0);
	//sint64 acc1 = dsp_get_long_acc(1);

	//sint64 res = acc0 + acc1;

	//dsp_set_long_acc(areg, res);

	//Update_SR_Register(res);
}


//-------------------------------------------------------------

void dsp_opc_addp(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addp");

	//uint8 dreg = (opc >> 8) & 0x1;
	//sint64 acc = dsp_get_long_acc(dreg);
	//acc = acc + dsp_get_long_prod();
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_cmpis(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_cmpis");

	//uint8 areg = (opc >> 8) & 0x1;

	//sint64 acc = dsp_get_long_acc(areg);
	//sint64 val = (sint8)opc;
	//val <<= 16;

	//sint64 res = acc - val;

	//Update_SR_Register(res);
}


// NEW
// verified at the console
void dsp_opc_addpaxz(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addpaxz");

	//uint8 dreg = (opc >> 8) & 0x1;
	//uint8 sreg = (opc >> 9) & 0x1;

	//sint64 prod = dsp_get_long_prod() & ~0x0ffff;
	//sint64 ax_h = dsp_get_long_acx(sreg);
	//sint64 acc = (prod + ax_h) & ~0x0ffff;

	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


// NEW
void dsp_opc_movpz(uint16 opc)
{
	uint8 dreg = (opc >> 8) & 0x01;

	OutBuffer::AddCode("ACC%i = PROD & ~0xffff", dreg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", dreg);

	//// overwrite acc and clear low part
	//sint64 prod = dsp_get_long_prod();
	//sint64 acc = prod & ~0xffff;
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_decm(uint16 opc)
{
	uint8 dreg = (opc >> 8) & 0x01;

	OutBuffer::AddCode("ACC%i -= 0x10000", dreg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", dreg);

	//sint64 sub = 0x10000;
	//sint64 acc = dsp_get_long_acc(dreg);
	//acc -= sub;
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_dec(uint16 opc)
{
	uint8 dreg = (opc >> 8) & 0x01;

	OutBuffer::AddCode("ACC%i -= 1", dreg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", dreg);

	//sint64 acc = dsp_get_long_acc(dreg) - 1;
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_incm(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_incm");

	//uint8 dreg = (opc >> 8) & 0x1;

	//sint64 sub = 0x10000;
	//sint64 acc = dsp_get_long_acc(dreg);
	//acc += sub;
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_inc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_inc");

	//uint8 dreg = (opc >> 8) & 0x1;

	//sint64 acc = dsp_get_long_acc(dreg);
	//acc++;
	//dsp_set_long_acc(dreg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_neg(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_neg");

	//uint8 areg = (opc >> 8) & 0x1;

	//sint64 acc = dsp_get_long_acc(areg);
	//acc = 0 - acc;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_movnp(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_movnp");

	ErrorLog("dsp_opc_movnp\n");
}


// NEW
void dsp_opc_addax(uint16 opc)
{
	// OutBuffer::AddCode("missing: dsp_opc_addax");

	uint8 areg = (opc >> 8) & 0x1;
	uint8 sreg = (opc >> 9) & 0x1;

	OutBuffer::AddCode("ACC%i += AX%i", areg, sreg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", areg);

	//sint64 ax  = dsp_get_long_acx(sreg);
	//sint64 acc = dsp_get_long_acc(areg);
	//acc += ax;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_addr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addr");

	//uint8 areg = (opc >> 8) & 0x1;
	//uint8 sreg = ((opc >> 9) & 0x3) + 0x18;

	//sint64 ax = (sint16)g_dsp.r[sreg];
	//ax <<= 16;

	//sint64 acc = dsp_get_long_acc(areg);
	//acc += ax;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_subr(uint16 opc)
{
	uint8 areg = (opc >> 8) & 0x1;
	uint8 sreg = ((opc >> 9) & 0x3) + 0x18;

	OutBuffer::AddCode("ACC%i -= (sint64)((sint16)%s << 16)", areg, OutBuffer::GetRegName(sreg));
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", areg);

	//sint64 ax = (sint16)g_dsp.r[sreg];
	//ax <<= 16;

	//sint64 acc = dsp_get_long_acc(areg);
	//acc -= ax;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}

// NEW
void dsp_opc_subax(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_subax");

	//int regD = (opc >> 8) & 0x1;
	//int regT = (opc >> 9) & 0x1;

	//sint64 Acc = dsp_get_long_acc(regD) - dsp_get_long_acx(regT);

	//dsp_set_long_acc(regD, Acc);
	//Update_SR_Register(Acc);
}

void dsp_opc_addis(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addis");

	//uint8 areg = (opc >> 8) & 0x1;

	//sint64 Imm = (sint8)opc;
	//Imm <<= 16;
	//sint64 acc = dsp_get_long_acc(areg);
	//acc += Imm;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_addi(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_addi");

	//uint8 areg = (opc >> 8) & 0x1;

	//sint64 sub = (sint16)FetchOpcode();
	//sub <<= 16;
	//sint64 acc = dsp_get_long_acc(areg);
	//acc += sub;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_lsl16(uint16 opc)
{
	uint8 areg = (opc >> 8) & 0x1;

	OutBuffer::AddCode("ACC%i <<= 16", areg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", areg);

	//sint64 acc = dsp_get_long_acc(areg);
	//acc <<= 16;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


// NEW
void dsp_opc_madd(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_madd");

	//uint8 sreg = (opc >> 8) & 0x1;

	//sint64 prod = dsp_get_long_prod();
	//prod += (sint64)dsp_get_ax_l(sreg) * (sint64)dsp_get_ax_h(sreg) * GetMultiplyModifier();
	//dsp_set_long_prod(prod);
}


void dsp_opc_lsr16(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_lsr16");

	//uint8 areg = (opc >> 8) & 0x1;

	//sint64 acc = dsp_get_long_acc(areg);
	//acc >>= 16;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


void dsp_opc_asr16(uint16 opc)
{
	uint8 areg = (opc >> 11) & 0x1;

	OutBuffer::AddCode("ACC%i >>= 16", areg);
	OutBuffer::AddCode("Update_SR_Register(ACC%i)", areg);

	//sint64 acc = dsp_get_long_acc(areg);
	//acc >>= 16;
	//dsp_set_long_acc(areg, acc);

	//Update_SR_Register(acc);
}


union UOpcodeShifti
{
	uint16 Hex;
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
	UOpcodeShifti(uint16 _Hex)
		: Hex(_Hex) {}
};

void dsp_opc_shifti(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_shifti");

	//UOpcodeShifti Opcode(opc);

	//// direction: left
	//bool ShiftLeft = true;
	//uint16 shift = Opcode.ushift;

	//if ((Opcode.negating) && (Opcode.shift < 0))
	//{
	//	ShiftLeft = false;
	//	shift = -Opcode.shift;
	//}

	//sint64 acc;
	//uint64 uacc;

	//if (Opcode.arithmetic)
	//{
	//	// arithmetic shift
	//	uacc = dsp_get_long_acc(Opcode.areg);

	//	if (!ShiftLeft)
	//	{
	//		uacc >>= shift;
	//	}
	//	else
	//	{
	//		uacc <<= shift;
	//	}

	//	acc = uacc;
	//}
	//else
	//{
	//	acc = dsp_get_long_acc(Opcode.areg);

	//	if (!ShiftLeft)
	//	{
	//		acc >>= shift;
	//	}
	//	else
	//	{
	//		acc <<= shift;
	//	}
	//}

	//dsp_set_long_acc(Opcode.areg, acc);

	//Update_SR_Register(acc);
}


//-------------------------------------------------------------
// hcs give me this code!!
void dsp_opc_dar(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_dar");

	//uint8 reg = opc & 0x3;

	//int temp = g_dsp.r[reg] + g_dsp.r[8];

	//if (temp <= 0x7ff){g_dsp.r[reg] = temp;}
	//else {g_dsp.r[reg]--;}
}


// hcs give me this code!!
void dsp_opc_iar(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_iar");

	//uint8 reg = opc & 0x3;

	//int temp = g_dsp.r[reg] + g_dsp.r[8];

	//if (temp <= 0x7ff){g_dsp.r[reg] = temp;}
	//else {g_dsp.r[reg]++;}
}


//-------------------------------------------------------------

void dsp_opc_sbclr(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_sbclr");

	//uint8 bit = (opc & 0xff) + 6;
	//g_dsp.r[R_SR] &= ~(1 << bit);
}


void dsp_opc_sbset(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_sbset");

	//uint8 bit = (opc & 0xff) + 6;
	//g_dsp.r[R_SR] |= (1 << bit);
}


void dsp_opc_srbith(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_srbith");

//	switch ((opc >> 8) & 0xf)
//	{
//	    case 0xe: // SET40
//		    g_dsp.r[R_SR] &= ~(1 << 14);
//		    break;
//
///*	case 0xf:	// SET16   // that doesnt happen on a real console
//   	g_dsp.r[R_SR] |= (1 << 14);
//   	break;*/
//
//	    default:
//		    break;
//	}
}


//-------------------------------------------------------------

void dsp_opc_movp(uint16 opc)
{
	uint8 dreg = (opc >> 8) & 0x1;

	OutBuffer::AddCode("ACC%i = PROD", dreg);

	//sint64 prod = dsp_get_long_prod();
	//sint64 acc = prod;
	//dsp_set_long_acc(dreg, acc);
}


void dsp_opc_mul(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mul");

	//uint8 sreg  = (opc >> 11) & 0x1;
	//sint64 prod = (sint64)dsp_get_ax_h(sreg) * (sint64)dsp_get_ax_l(sreg) * GetMultiplyModifier();

	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}

// NEW
void dsp_opc_mulac(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulac");

	//// add old prod to acc
	//uint8 rreg = (opc >> 8) & 0x1;
	//sint64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	//dsp_set_long_acc(rreg, acR);

	//// math new prod
	//uint8 sreg = (opc >> 11) & 0x1;
	//sint64 prod = dsp_get_ax_l(sreg) * dsp_get_ax_h(sreg) * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


void dsp_opc_mulmv(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulmv");

	//uint8 rreg  = (opc >> 8) & 0x1;
	//sint64 prod = dsp_get_long_prod();
	//sint64 acc = prod;
	//dsp_set_long_acc(rreg, acc);

	//uint8 areg  = ((opc >> 11) & 0x1) + 0x18;
	//uint8 breg  = ((opc >> 11) & 0x1) + 0x1a;
	//sint64 val1 = (sint16)g_dsp.r[areg];
	//sint64 val2 = (sint16)g_dsp.r[breg];

	//prod = val1 * val2 * GetMultiplyModifier();

	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulmvz(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulmvz");

	//uint8 sreg = (opc >> 11) & 0x1;
	//uint8 rreg = (opc >> 8) & 0x1;

	//// overwrite acc and clear low part
	//sint64 prod = dsp_get_long_prod();
	//sint64 acc = prod & ~0xffff;
	//dsp_set_long_acc(rreg, acc);

	//// math prod
	//prod = (sint64)g_dsp.r[0x18 + sreg] * (sint64)g_dsp.r[0x1a + sreg] * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulx(uint16 opc)
{
	// OutBuffer::AddCode("missing: dsp_opc_mulx");

	uint8 sreg = ((opc >> 12) & 0x1);
	uint8 treg = ((opc >> 11) & 0x1);

	OutBuffer::AddCode("PROD = %s * %s * MultiplyModifier", (sreg == 0) ? "AX0_l" : "AX0_h",
															(treg == 0) ? "AX1_l" : "AX1_h");
	OutBuffer::AddCode("Update_SR_Register(PROD)");

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//sint64 prod = val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxac(uint16 opc)
{
	//// add old prod to acc
	uint8 rreg = (opc >> 8) & 0x1;
	//sint64 acR = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	//dsp_set_long_acc(rreg, acR);
	
	OutBuffer::AddCode("ACC%i += PROD", rreg);

	//// math new prod
	uint8 sreg = (opc >> 12) & 0x1;
	uint8 treg = (opc >> 11) & 0x1;

	OutBuffer::AddCode("PROD = %s * %s * MultiplyModifier", (sreg == 0) ? "AX0_l" : "AX0_h",
															(treg == 0) ? "AX1_l" : "AX1_h");
	OutBuffer::AddCode("Update_SR_Register(PROD)");

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//sint64 prod = val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxmv(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulxmv");

	//// add old prod to acc
	//uint8 rreg = ((opc >> 8) & 0x1);
	//sint64 acR = dsp_get_long_prod();
	//dsp_set_long_acc(rreg, acR);

	//// math new prod
	//uint8 sreg = (opc >> 12) & 0x1;
	//uint8 treg = (opc >> 11) & 0x1;

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//sint64 prod = val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_mulxmvz(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_mulxmvz");

	//// overwrite acc and clear low part
	//uint8 rreg  = (opc >> 8) & 0x1;
	//sint64 prod = dsp_get_long_prod();
	//sint64 acc = prod & ~0xffff;
	//dsp_set_long_acc(rreg, acc);

	//// math prod
	//uint8 sreg = (opc >> 12) & 0x1;
	//uint8 treg = (opc >> 11) & 0x1;

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//prod = val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);

	//Update_SR_Register(prod);
}


// NEW
void dsp_opc_sub(uint16 opc)
{
	uint8 D = (opc >> 8) & 0x1;

	OutBuffer::AddCode("ACC%i -= ACC%i", D, 1-D);

	//sint64 Acc1 = dsp_get_long_acc(D);
	//sint64 Acc2 = dsp_get_long_acc(1 - D);

	//Acc1 -= Acc2;

	//dsp_set_long_acc(D, Acc1);
}


//-------------------------------------------------------------
//
// --- Table E
//
//-------------------------------------------------------------

// NEW
void dsp_opc_maddx(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_maddx");

	//uint8 sreg = (opc >> 9) & 0x1;
	//uint8 treg = (opc >> 8) & 0x1;

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//sint64 prod = dsp_get_long_prod();
	//prod += val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_msubx(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_msubx");

	//uint8 sreg = (opc >> 9) & 0x1;
	//uint8 treg = (opc >> 8) & 0x1;

	//sint64 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
	//sint64 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);

	//sint64 prod = dsp_get_long_prod();
	//prod -= val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_maddc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_maddc");

	//uint sreg = (opc >> 9) & 0x1;
	//uint treg = (opc >> 8) & 0x1;

	//sint64 val1 = dsp_get_acc_m(sreg);
	//sint64 val2 = dsp_get_ax_h(treg);

	//sint64 prod = dsp_get_long_prod();
	//prod += val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);
}


// NEW
void dsp_opc_msubc(uint16 opc)
{
	OutBuffer::AddCode("missing: dsp_opc_msubc");

	//uint sreg = (opc >> 9) & 0x1;
	//uint treg = (opc >> 8) & 0x1;

	//sint64 val1 = dsp_get_acc_m(sreg);
	//sint64 val2 = dsp_get_ax_h(treg);

	//sint64 prod = dsp_get_long_prod();
	//prod -= val1 * val2 * GetMultiplyModifier();
	//dsp_set_long_prod(prod);
}


//-------------------------------------------------------------
void dsp_op0(uint16 opc)
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
					    ErrorLog("dsp_op0");
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
				ErrorLog("dsp_op0");
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
				ErrorLog("dsp_op0");
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
				ErrorLog("dsp_op0");
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
		    ErrorLog("dsp_op0");
		    break;
	}
}


void dsp_op1(uint16 opc)
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
		    ErrorLog("dsp_op1");
		    break;
	}
}


void dsp_op2(uint16 opc)
{
	// lrs, srs
	uint8 reg   = ((opc >> 8) & 0x7) + 0x18;
	uint16 addr = (sint8) opc;

	if (opc & 0x0800)
	{
		OutBuffer::AddCode("WriteDMEM(%s, %s)", OutBuffer::GetMemName(addr), OutBuffer::GetRegName(reg));

		// srs
		// dsp_dmem_write(addr, g_dsp.r[reg]);
	}
	else
	{
		OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(reg), OutBuffer::GetMemName(addr));

		// lrs
		// g_dsp.r[reg] = dsp_dmem_read(addr);
	}
}


void dsp_op3(uint16 opc)
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
		    ErrorLog("dsp_op3");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op4(uint16 opc)
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
		    ErrorLog("dsp_op4");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op5(uint16 opc)
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
			ErrorLog("dsp_op5: %x", (opc >> 8) & 0xf);
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op6(uint16 opc)
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
		    ErrorLog("dsp_op6");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op7(uint16 opc)
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
		    ErrorLog("dsp_op7");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op8(uint16 opc)
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
		    ErrorLog("dsp_op8");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_op9(uint16 opc)
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
		    ErrorLog("dsp_op9");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opab(uint16 opc)
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
		    ErrorLog("dsp_opab");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opcd(uint16 opc)
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
		    ErrorLog("dsp_opcd");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_ope(uint16 opc)
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
		    ErrorLog("dsp_ope");
	}

	dsp_op_ext_ops_epi(opc);
}


void dsp_opf(uint16 opc)
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
		    ErrorLog("dsp_opf");
		    break;
	}

	dsp_op_ext_ops_epi(opc);
}


