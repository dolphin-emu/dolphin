/*====================================================================

   filename:     disassemble.cpp
   project:      GameCube DSP Tool (gcdsp)
   created:      2005.03.04
   mail:		  duddie@walla.com

   Copyright (c) 2005 Duddie

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

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "disassemble.h"
#include "DSPTables.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

u32 unk_opcodes[0x10000];

extern void nop(const UDSPInstruction& opc);

char* gd_dis_params(gd_globals_t* gdg, const DSPOPCTemplate* opc, u16 op1, u16 op2, char* strbuf)
{
	char* buf = strbuf;
	u32 val;

	for (int j = 0; j < opc->param_count; j++)
	{
		if (j > 0)
		{
			sprintf(buf, ", ");
			buf += strlen(buf);
		}

		if (opc->params[j].loc >= 1)
			val = op2;
		else
			val = op1;

		val &= opc->params[j].mask;

		if (opc->params[j].lshift < 0)
			val = val << (-opc->params[j].lshift);
		else
			val = val >> opc->params[j].lshift;

		u32 type;
		type = opc->params[j].type;

		if ((type & 0xff) == 0x10)
			type &= 0xff00;

		if (type & P_REG)
		{
			if (type == P_ACC_D)  // Used to be P_ACCM_D TODO verify
				val = (~val & 0x1) | ((type & P_REGS_MASK) >> 8);
			else
				val |= (type & P_REGS_MASK) >> 8;

			type &= ~P_REGS_MASK;
		}

		switch (type)
		{
		case P_REG:
			if (gdg->decode_registers)
				sprintf(buf, "$%s", pdregname(val));
			else
				sprintf(buf, "$%d", val);
			break;

		case P_PRG:
			if (gdg->decode_registers)
				sprintf(buf, "@$%s", pdregname(val));
			else
				sprintf(buf, "@$%d", val);
			break;

		case P_VAL:
			if (gdg->decode_names)
				sprintf(buf, "%s", pdname(val));
			else
				sprintf(buf, "0x%04x", val);
			break;

		case P_IMM:
			if (opc->params[j].size != 2)
			{
				if (opc->params[j].mask == 0x007f) // LSL, LSR, ASL, ASR
					sprintf(buf, "#%d", val < 64 ? val : -(0x80 - (s32)val));
				else
					sprintf(buf, "#0x%02x", val);
			}
			else
				sprintf(buf, "#0x%04x", val);
			break;

		case P_MEM:
			if (opc->params[j].size != 2)
				val = (u16)(s8)val;

			if (gdg->decode_names)
				sprintf(buf, "@%s", pdname(val));
			else
				sprintf(buf, "@0x%04x", val);
			break;

		default:
			ERROR_LOG(DSPLLE, "Unknown parameter type: %x", opc->params[j].type);
//			exit(-1);
			break;
		}

		buf += strlen(buf);
	}

	return strbuf;
}

u16 gd_dis_get_opcode_size(gd_globals_t* gdg)
{
	const DSPOPCTemplate* opc = 0;
	const DSPOPCTemplate* opc_ext = 0;
	bool extended;

	// Undefined memory.
	if ((gdg->pc & 0x7fff) >= 0x1000)
		return 1;

	u32 op1 = gdg->binbuf[gdg->pc & 0x0fff];

	for (u32 j = 0; j < opcodes_size; j++)
	{
		u16 mask;

		if (opcodes[j].size & P_EXT)
			mask = opcodes[j].opcode_mask & 0xff00;
		else
			mask = opcodes[j].opcode_mask;

		if ((op1 & mask) == opcodes[j].opcode)
		{
			opc = &opcodes[j];
			break;
		}
	}

	if (!opc)
	{
		ERROR_LOG(DSPLLE, "get_opcode_size ARGH");
		exit(0);
	}

	if (opc->size & P_EXT && op1 & 0x00ff)
		extended = true;
	else
		extended = false;

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (u32 j = 0; j < opcodes_ext_size; j++)
		{
			if ((op1 & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
			{
				opc_ext = &opcodes_ext[j];
				break;
			}
		}
		if (!opc_ext)
		{
			ERROR_LOG(DSPLLE, "get_opcode_size ext ARGH");
		}
		return opc_ext->size;
	}

	return opc->size & ~P_EXT;
}


char* gd_dis_opcode(gd_globals_t* gdg)
{
	u32 op2;
	char *buf = gdg->buffer;

	u16 pc   = gdg->pc;

	// Start with a space.
	buf[0] = ' ';
	buf[1] = '\0';
	buf++;

	if ((pc & 0x7fff) >= 0x1000)
	{
		gdg->pc++;
		return gdg->buffer;
	}

	pc &= 0x0fff;
	u32 op1 = gdg->binbuf[pc];

	const DSPOPCTemplate *opc = NULL;
	const DSPOPCTemplate *opc_ext = NULL;
	// find opcode
	for (int j = 0; j < opcodes_size; j++)
	{
		u16 mask;

		if (opcodes[j].size & P_EXT)
			mask = opcodes[j].opcode_mask & 0xff00;
		else
			mask = opcodes[j].opcode_mask;

		if ((op1 & mask) == opcodes[j].opcode)
		{
			opc = &opcodes[j];
			break;
		}
	}

	
	const DSPOPCTemplate fake_op = {"CW",		0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, NULL, NULL,};
	if (!opc)
		opc = &fake_op;

	bool extended;
	if (opc->size & P_EXT && op1 & 0x00ff)
		extended = true;
	else
		extended = false;

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (int j = 0; j < opcodes_ext_size; j++)
		{
			if ((op1 & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
			{
				opc_ext = &opcodes_ext[j];
				break;
			}
		}
	}

	// printing

	if (gdg->show_pc)
		sprintf(buf, "%04x ", gdg->pc);

	buf += strlen(buf);

	if ((opc->size & ~P_EXT) == 2)
	{
		op2 = gdg->binbuf[pc + 1];

		if (gdg->show_hex)
			sprintf(buf, "%04x %04x ", op1, op2);
	}
	else
	{
		op2 = 0;

		if (gdg->show_hex)
			sprintf(buf, "%04x      ", op1);
	}

	buf += strlen(buf);

	char tmpbuf[20];

	if (extended)
		sprintf(tmpbuf, "%s%c%s", opc->name, gdg->ext_separator, opc_ext->name);
	else
		sprintf(tmpbuf, "%s", opc->name);

	if (gdg->print_tabs)
		sprintf(buf, "%s\t", tmpbuf);
	else
		sprintf(buf, "%-12s", tmpbuf);

	buf += strlen(buf);

	if (opc->param_count > 0)
		gd_dis_params(gdg, opc, op1, op2, buf);

	buf += strlen(buf);

	if (extended)
	{
		if (opc->param_count > 0)
			sprintf(buf, " ");

		buf += strlen(buf);

		sprintf(buf, ": ");
		buf += strlen(buf);

		if (opc_ext->param_count > 0)
			gd_dis_params(gdg, opc_ext, op1, op2, buf);

		buf += strlen(buf);
	}

	if (opc->opcode_mask == 0)
	{
		// unknown opcode
		unk_opcodes[op1]++;
		sprintf(buf, "\t\t; *** UNKNOWN OPCODE ***");
	}

	if (extended)
		gdg->pc += opc_ext->size;
	else
		gdg->pc += opc->size & ~P_EXT;

	return gdg->buffer;
}

bool gd_dis_file(gd_globals_t* gdg, const char* name, FILE* output)
{
	gd_dis_open_unkop();

	FILE* in;
	u32 size;

	in = fopen(name, "rb");
	if (in == NULL) {
		printf("gd_dis_file: No input\n");
		return false;
	}

	fseek(in, 0, SEEK_END);
	size = (int)ftell(in);
	fseek(in, 0, SEEK_SET);
	gdg->binbuf = (u16*)malloc(size);
	fread(gdg->binbuf, 1, size, in);

	gdg->buffer = (char*)malloc(256);
	gdg->buffer_size = 256;

	for (gdg->pc = 0; gdg->pc < (size / 2);)
		fprintf(output, "%s\n", gd_dis_opcode(gdg));

	fclose(in);

	free(gdg->binbuf);
	gdg->binbuf = NULL;

	free(gdg->buffer);
	gdg->buffer = NULL;
	gdg->buffer_size = 0;

	gd_dis_close_unkop();

	return true;
}

void gd_dis_close_unkop()
{
	FILE* uo;
	int i, j;
	u32 count = 0;

	char filename[MAX_PATH];
	sprintf(filename, "%sUnkOps.bin", FULL_DSP_DUMP_DIR);

	uo = fopen(filename, "wb");

	if (uo)
	{
		fwrite(unk_opcodes, 1, sizeof(unk_opcodes), uo);
		fclose(uo);
	}

	sprintf(filename, "%sUnkOps.txt", FULL_DSP_DUMP_DIR);
	uo = fopen(filename, "w");

	if (uo)
	{
		for (i = 0; i < 0x10000; i++)
		{
			if (unk_opcodes[i])
			{
				count++;
				fprintf(uo, "OP%04x\t%d", i, unk_opcodes[i]);

				for (j = 15; j >= 0; j--)
				{
					if ((j & 0x3) == 3)
						fprintf(uo, "\tb");

					fprintf(uo, "%d", (i >> j) & 0x1);
				}

				fprintf(uo, "\n");
			}
		}

		fprintf(uo, "Unknown opcodes count: %d\n", count);
		fclose(uo);
	}
}

void gd_dis_open_unkop()
{
	FILE* uo;
	char filename[MAX_PATH];
	sprintf(filename, "%sUnkOps.bin", FULL_DSP_DUMP_DIR);
	uo = fopen(filename, "rb");

	if (uo)
	{
		fread(unk_opcodes, 1, sizeof(unk_opcodes), uo);
		fclose(uo);
	}
	else
	{
		for (int i = 0; i < 0x10000; i++)
		{
			unk_opcodes[i] = 0;
		}
	}
}

const char *gd_dis_get_reg_name(u16 reg)
{
	return regnames[reg].name;
}
