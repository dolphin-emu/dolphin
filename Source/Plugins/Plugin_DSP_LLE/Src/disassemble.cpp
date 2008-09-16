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

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "Globals.h"

#include "disassemble.h"
#include "opcodes.h"
// #include "gdsp_tool.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

uint32 unk_opcodes[0x10000];

uint16 swap16(uint16 x);


// predefined labels
typedef struct pdlabel_t
{
	uint16 addr;
	const char* name;
	const char* description;
} pdlabels_t;

const pdlabel_t pdlabels[] =
{
	{0xffa0, "COEF_A1_0", "COEF_A1_0",},
	{0xffa1, "COEF_A2_0", "COEF_A2_0",},
	{0xffa2, "COEF_A1_1", "COEF_A1_1",},
	{0xffa3, "COEF_A2_1", "COEF_A2_1",},
	{0xffa4, "COEF_A1_2", "COEF_A1_2",},
	{0xffa5, "COEF_A2_2", "COEF_A2_2",},
	{0xffa6, "COEF_A1_3", "COEF_A1_3",},
	{0xffa7, "COEF_A2_3", "COEF_A2_3",},
	{0xffa8, "COEF_A1_4", "COEF_A1_4",},
	{0xffa9, "COEF_A2_4", "COEF_A2_4",},
	{0xffaa, "COEF_A1_5", "COEF_A1_5",},
	{0xffab, "COEF_A2_5", "COEF_A2_5",},
	{0xffac, "COEF_A1_6", "COEF_A1_6",},
	{0xffad, "COEF_A2_6", "COEF_A2_6",},
	{0xffae, "COEF_A1_7", "COEF_A1_7",},
	{0xffaf, "COEF_A2_7", "COEF_A2_7",},
	{0xffc9, "DSCR", "DSP DMA Control Reg",},
	{0xffcb, "DSBL", "DSP DMA Block Length",},
	{0xffcd, "DSPA", "DSP DMA DMEM Address",},
	{0xffce, "DSMAH", "DSP DMA Mem Address H",},
	{0xffcf, "DSMAL", "DSP DMA Mem Address L",},
	{0xffd1, "SampleFormat", "SampleFormat",},
	{0xffd4, "ACSAH", "Accelerator start address H",},
	{0xffd5, "ACSAL", "Accelerator start address L",},
	{0xffd6, "ACEAH", "Accelerator end address H",},
	{0xffd7, "ACEAL", "Accelerator end address L",},
	{0xffd8, "ACCAH", "Accelerator current address H",},
	{0xffd9, "ACCAL", "Accelerator current address L",},
	{0xffda, "pred_scale", "pred_scale",},
	{0xffdb, "yn1", "yn1",},
	{0xffdc, "yn2", "yn2",},
	{0xffdd, "ARAM", "Direct Read from ARAM (uses ADPCM)",},
	{0xffde, "GAIN", "Gain",},
	{0xffef, "AMDM", "ARAM DMA Request Mask",},
	{0xfffb, "DIRQ", "DSP IRQ Request",},
	{0xfffc, "DMBH", "DSP Mailbox H",},
	{0xfffd, "DMBL", "DSP Mailbox L",},
	{0xfffe, "CMBH", "CPU Mailbox H",},
	{0xffff, "CMBL", "CPU Mailbox L",},
};

pdlabel_t regnames[] =
{
	{0x00, "R00",       "Register 00",},
	{0x01, "R01",       "Register 01",},
	{0x02, "R02",       "Register 02",},
	{0x03, "R03",       "Register 03",},
	{0x04, "R04",       "Register 04",},
	{0x05, "R05",       "Register 05",},
	{0x06, "R06",       "Register 06",},
	{0x07, "R07",       "Register 07",},
	{0x08, "R08",       "Register 08",},
	{0x09, "R09",       "Register 09",},
	{0x0a, "R10",       "Register 10",},
	{0x0b, "R11",       "Register 11",},
	{0x0c, "ST0",       "Call stack",},
	{0x0d, "ST1",       "Data stack",},
	{0x0e, "ST2",       "Loop address stack",},
	{0x0f, "ST3",       "Loop counter",},
	{0x00, "ACH0",      "Accumulator High 0",},
	{0x11, "ACH1",      "Accumulator High 1",},
	{0x12, "CR",        "Config Register",},
	{0x13, "SR",        "Special Register",},
	{0x14, "PROD.L",    "PROD L",},
	{0x15, "PROD.M1",   "PROD M1",},
	{0x16, "PROD.H",    "PROD H",},
	{0x17, "PROD.M2",   "PROD M2",},
	{0x18, "AX0.L", "Additional Accumulators Low 0",},
	{0x19, "AX1.L", "Additional Accumulators Low 1",},
	{0x1a, "AX0.H", "Additional Accumulators High 0",},
	{0x1b, "AX1.H", "Additional Accumulators High 1",},
	{0x1c, "AC0.L", "Register 28",},
	{0x1d, "AC1.L", "Register 29",},
	{0x1e, "AC0.M", "Register 00",},
	{0x1f, "AC1.M", "Register 00",},

// additional to resolve special names
	{0x20, "ACC0",      "Accumulators 0",},
	{0x21, "ACC1",      "Accumulators 1",},
	{0x22, "AX0",       "Additional Accumulators 0",},
	{0x23, "AX1",       "Additional Accumulators 1",},
};

const char* pdname(uint16 val)
{
	static char tmpstr[12]; // nasty

	for (int i = 0; i < (int)(sizeof(pdlabels) / sizeof(pdlabel_t)); i++)
	{
		if (pdlabels[i].addr == val)
		{
			return(pdlabels[i].name);
		}
	}

	sprintf(tmpstr, "0x%04x", val);
	return(tmpstr);
}


char* gd_dis_params(gd_globals_t* gdg, opc_t* opc, uint16 op1, uint16 op2, char* strbuf)
{
	char* buf = strbuf;
	uint32 val;
	int j;

	for (j = 0; j < opc->param_count; j++)
	{
		if (j > 0)
		{
			sprintf(buf, ", ");
			buf += strlen(buf);
		}

		if (opc->params[j].loc >= 1)
		{
			val = op2;
		}
		else
		{
			val = op1;
		}

		val &= opc->params[j].mask;

		if (opc->params[j].lshift < 0)
		{
			val = val << (-opc->params[j].lshift);
		}
		else
		{
			val = val >> opc->params[j].lshift;
		}

		uint32 type;
		type = opc->params[j].type;

		if (type & P_REG)
		{
			if (type == P_ACCM_D)
			{
				val = (~val & 0x1) | ((type & P_REGS_MASK) >> 8);
			}
			else
			{
				val |= (type & P_REGS_MASK) >> 8;
			}

			type &= ~P_REGS_MASK;
		}

		switch (type)
		{
		    case P_REG:

			    if (gdg->decode_registers){sprintf(buf, "$%s", regnames[val].name);}
			    else {sprintf(buf, "$%d", val);}

			    break;

		    case P_PRG:

			    if (gdg->decode_registers){sprintf(buf, "@$%s", regnames[val].name);}
			    else {sprintf(buf, "@$%d", val);}

			    break;

		    case P_VAL:
			    sprintf(buf, "0x%04x", val);
			    break;

		    case P_IMM:

			    if (opc->params[j].size != 2)
			    {
				    sprintf(buf, "#0x%02x", val);
			    }
			    else
			    {
				    sprintf(buf, "#0x%04x", val);
			    }

			    break;

		    case P_MEM:

			    if (opc->params[j].size != 2)
			    {
				    val = (uint16)(sint8)val;
			    }

			    if (gdg->decode_names)
			    {
				    sprintf(buf, "@%s", pdname(val));
			    }
			    else
			    {
				    sprintf(buf, "@0x%04x", val);
			    }

			    break;

		    default:
			    ErrorLog("Unknown parameter type: %x\n", opc->params[j].type);
			    exit(-1);
			    break;
		}

		buf += strlen(buf);
	}

	return(strbuf);
}


gd_globals_t* gd_init()
{
	gd_globals_t* gdg = (gd_globals_t*)malloc(sizeof(gd_globals_t));
	memset(gdg, 0, sizeof(gd_globals_t));
	return(gdg);
}


uint16 gd_dis_get_opcode_size(gd_globals_t* gdg)
{
	opc_t* opc = 0;
	opc_t* opc_ext = 0;
	bool extended;

	if ((gdg->pc & 0x7fff) >= 0x1000)
	{
		return(1);
	}

	uint32 op1 = swap16(gdg->binbuf[gdg->pc & 0x0fff]);

	for (uint32 j = 0; j < opcodes_size; j++)
	{
		uint16 mask;

		if (opcodes[j].size & P_EXT)
		{
			mask = opcodes[j].opcode_mask & 0xff00;
		}
		else
		{
			mask = opcodes[j].opcode_mask;
		}

		if ((op1 & mask) == opcodes[j].opcode)
		{
			opc = &opcodes[j];
			break;
		}
	}

	if (!opc)
	{
		ErrorLog("get_opcode_size ARGH");
		exit(0);
	}

	if (opc->size & P_EXT && op1 & 0x00ff)
	{
		extended = true;
	}
	else
	{
		extended = false;
	}

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (uint32 j = 0; j < opcodes_ext_size; j++)
		{
			if ((op1 & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
			{
				opc_ext = &opcodes_ext[j];
				break;
			}
		}

		if (!opc_ext)
		{
			ErrorLog("get_opcode_size ext ARGH");
		}

		return(opc_ext->size);
	}

	return(opc->size & ~P_EXT);
}


char* gd_dis_opcode(gd_globals_t* gdg)
{
	uint32 j;
	uint32 op1, op2;
	opc_t* opc = NULL;
	opc_t* opc_ext = NULL;
	uint16 pc;
	char* buf = gdg->buffer;
	bool extended;

	pc   = gdg->pc;
	*buf = '\0';

	if ((pc & 0x7fff) >= 0x1000)
	{
		gdg->pc++;
		return(gdg->buffer);
	}

	pc &= 0x0fff;
	op1 = swap16(gdg->binbuf[pc]);

	// find opcode
	for (j = 0; j < opcodes_size; j++)
	{
		uint16 mask;

		if (opcodes[j].size & P_EXT)
		{
			mask = opcodes[j].opcode_mask & 0xff00;
		}
		else
		{
			mask = opcodes[j].opcode_mask;
		}

		if ((op1 & mask) == opcodes[j].opcode)
		{
			opc = &opcodes[j];
			break;
		}
	}

	if (opc->size & P_EXT && op1 & 0x00ff)
	{
		extended = true;
	}
	else
	{
		extended = false;
	}

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (j = 0; j < opcodes_ext_size; j++)
		{
			if ((op1 & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
			{
				opc_ext = &opcodes_ext[j];
				break;
			}
		}
	}

	// printing

	if (gdg->show_pc){sprintf(buf, "%04x ", gdg->pc);}

	buf += strlen(buf);

	if ((opc->size & ~P_EXT) == 2)
	{
		op2 = swap16(gdg->binbuf[pc + 1]);

		if (gdg->show_hex){sprintf(buf, "%04x %04x ", op1, op2);}
	}
	else
	{
		op2 = 0;

		if (gdg->show_hex){sprintf(buf, "%04x      ", op1);}
	}

	buf += strlen(buf);

	char tmpbuf[20];

	if (extended)
	{
		sprintf(tmpbuf, "%s%c%s", opc->name, gdg->ext_separator, opc_ext->name);
	}
	else
	{
		sprintf(tmpbuf, "%s", opc->name);
	}

	if (gdg->print_tabs)
	{
		sprintf(buf, "%s\t", tmpbuf);
	}
	else
	{
		sprintf(buf, "%-12s", tmpbuf);
	}

	buf += strlen(buf);

	if (opc->param_count > 0)
	{
		gd_dis_params(gdg, opc, op1, op2, buf);
	}

	buf += strlen(buf);

	if (extended)
	{
		if (opc->param_count > 0)
		{
			sprintf(buf, " ");
		}

		buf += strlen(buf);

		sprintf(buf, ": ");
		buf += strlen(buf);

		if (opc_ext->param_count > 0)
		{
			gd_dis_params(gdg, opc_ext, op1, op2, buf);
		}

		buf += strlen(buf);
	}

	if (opc->opcode_mask == 0)
	{
		// unknown opcode
		unk_opcodes[op1]++;
		sprintf(buf, "\t\t; *** UNKNOWN OPCODE ***");
	}

	if (extended)
	{
		gdg->pc += opc_ext->size;
	}
	else
	{
		gdg->pc += opc->size & ~P_EXT;
	}

	return(gdg->buffer);
}


bool gd_dis_file(gd_globals_t* gdg, char* name, FILE* output)
{
	FILE* in;
	uint32 size;

	in = fopen(name, "rb");

	if (in == NULL)
	{
		return(false);
	}

	fseek(in, 0, SEEK_END);
	size = ftell(in);
	fseek(in, 0, SEEK_SET);
	gdg->binbuf = (uint16*)malloc(size);
	fread(gdg->binbuf, 1, size, in);

	gdg->buffer = (char*)malloc(256);
	gdg->buffer_size = 256;

	for (gdg->pc = 0; gdg->pc < (size / 2);)
	{
		fprintf(output, "%s\n", gd_dis_opcode(gdg));
	}

	fclose(in);

	free(gdg->binbuf);
	gdg->binbuf = NULL;

	free(gdg->buffer);
	gdg->buffer = NULL;
	gdg->buffer_size = 0;

	return(true);
}


void gd_dis_close_unkop()
{
	FILE* uo;
	int i, j;
	uint32 count = 0;

	uo = fopen("uo.bin", "wb");

	if (uo)
	{
		fwrite(unk_opcodes, 1, sizeof(unk_opcodes), uo);
		fclose(uo);
	}

	uo = fopen("unkopc.txt", "w");

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
					{
						fprintf(uo, "\tb");
					}

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

	uo = fopen("uo.bin", "rb");

	if (uo)
	{
		fread(unk_opcodes, 1, sizeof(unk_opcodes), uo);
		fclose(uo);
	}
	else
	{
		int i;

		for (i = 0; i < 0x10000; i++)
		{
			unk_opcodes[i] = 0;
		}
	}
}


const char* gd_dis_get_reg_name(uint16 reg)
{
	return(regnames[reg].name);
}


