/*====================================================================

   filename:     disassemble.cpp
   project:      GameCube DSP Tool (gcdsp)
   created:      2005.03.04
   mail:         duddie@walla.com

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   ====================================================================*/

#include <cstdio>
#include <cstdlib>
#include <string>

#include "Common/Common.h"
#include "Common/FileUtil.h"

#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPTables.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#ifndef MAX_PATH
#include <Windows.h> // For MAX_PATH
#endif

extern void nop(const UDSPInstruction opc);

DSPDisassembler::DSPDisassembler(const AssemblerSettings &settings)
	: settings_(settings)
{
}

DSPDisassembler::~DSPDisassembler()
{
	// Some old code for logging unknown ops.
	std::string filename = File::GetUserPath(D_DUMPDSP_IDX) + "UnkOps.txt";
	File::IOFile uo(filename, "w");
	if (!uo)
		return;

	int count = 0;
	for (const auto& entry : unk_opcodes)
	{
		if (entry.second > 0)
		{
			count++;
			fprintf(uo.GetHandle(), "OP%04x\t%d", entry.first, entry.second);
			for (int j = 15; j >= 0; j--)  // print op bits
			{
				if ((j & 0x3) == 3)
					fprintf(uo.GetHandle(), "\tb");
				fprintf(uo.GetHandle(), "%d", (entry.first >> j) & 0x1);
			}
			fprintf(uo.GetHandle(), "\n");
		}
	}
	fprintf(uo.GetHandle(), "Unknown opcodes count: %d\n", count);
}

bool DSPDisassembler::Disassemble(int start_pc, const std::vector<u16> &code, int base_addr, std::string &text)
{
	const char *tmp1 = "tmp1.bin";

	// First we have to dump the code to a bin file.
	{
	File::IOFile f(tmp1, "wb");
	f.WriteArray(&code[0], code.size());
	}

	// Run the two passes.
	return DisFile(tmp1, base_addr, 1, text) && DisFile(tmp1, base_addr, 2, text);
}

char *DSPDisassembler::DisParams(const DSPOPCTemplate& opc, u16 op1, u16 op2, char *strbuf)
{
	char *buf = strbuf;
	for (int j = 0; j < opc.param_count; j++)
	{
		if (j > 0)
			buf += sprintf(buf, ", ");

		u32 val = (opc.params[j].loc >= 1) ? op2 : op1;
		val &= opc.params[j].mask;
		if (opc.params[j].lshift < 0)
			val = val << (-opc.params[j].lshift);
		else
			val = val >> opc.params[j].lshift;

		u32 type = opc.params[j].type;
		if ((type & 0xff) == 0x10)
			type &= 0xff00;

		if (type & P_REG)
		{
			// Check for _D parameter - if so flip.
			if ((type == P_ACC_D) || (type == P_ACCM_D))  // Used to be P_ACCM_D TODO verify
				val = (~val & 0x1) | ((type & P_REGS_MASK) >> 8);
			else
				val |= (type & P_REGS_MASK) >> 8;
			type &= ~P_REGS_MASK;
		}

		switch (type)
		{
		case P_REG:
			if (settings_.decode_registers)
				sprintf(buf, "$%s", pdregname(val));
			else
				sprintf(buf, "$%d", val);
			break;

		case P_PRG:
			if (settings_.decode_registers)
				sprintf(buf, "@$%s", pdregname(val));
			else
				sprintf(buf, "@$%d", val);
			break;

		case P_VAL:
		case P_ADDR_I:
		case P_ADDR_D:
			if (settings_.decode_names)
			{
				sprintf(buf, "%s", pdname(val));
			}
			else
				sprintf(buf, "0x%04x", val);
			break;

		case P_IMM:
			if (opc.params[j].size != 2)
			{
				if (opc.params[j].mask == 0x003f) // LSL, LSR, ASL, ASR
					sprintf(buf, "#%d", (val & 0x20) ? (val | 0xFFFFFFC0) : val);  // 6-bit sign extension
				else
					sprintf(buf, "#0x%02x", val);
			}
			else
			{
				sprintf(buf, "#0x%04x", val);
			}
			break;

		case P_MEM:
			if (opc.params[j].size != 2)
				val = (u16)(s16)(s8)val;

			if (settings_.decode_names)
				sprintf(buf, "@%s", pdname(val));
			else
				sprintf(buf, "@0x%04x", val);
			break;

		default:
			ERROR_LOG(DSPLLE, "Unknown parameter type: %x", opc.params[j].type);
			break;
		}

		buf += strlen(buf);
	}

	return strbuf;
}

static void MakeLowerCase(char *ptr)
{
	int i = 0;
	while (ptr[i])
	{
		ptr[i] = tolower(ptr[i]);
		i++;
	}
}

bool DSPDisassembler::DisOpcode(const u16 *binbuf, int base_addr, int pass, u16 *pc, std::string &dest)
{
	char buffer[256];
	char *buf = buffer;

	// Start with 8 spaces, if there's no label.
	buf[0] = ' ';
	buf[1] = '\0';
	buf++;

	if ((*pc & 0x7fff) >= 0x1000)
	{
		++pc;
		dest.append("; outside memory");
		return false;
	}

	const u32 op1 = binbuf[*pc & 0x0fff];

	const DSPOPCTemplate *opc = nullptr;
	const DSPOPCTemplate *opc_ext = nullptr;

	// find opcode
	for (int j = 0; j < opcodes_size; j++)
	{
		u16 mask = opcodes[j].opcode_mask;

		if ((op1 & mask) == opcodes[j].opcode)
		{
			opc = &opcodes[j];
			break;
		}
	}
	const DSPOPCTemplate fake_op = {"CW", 0x0000, 0x0000, nop, nullptr, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, false, false, false, false, false};
	if (!opc)
		opc = &fake_op;

	bool extended = false;
	bool only7bitext = false;

	if (((opc->opcode >> 12) == 0x3) && (op1 & 0x007f)) {
		extended = true;
		only7bitext = true;
	}
	else if (((opc->opcode >> 12) > 0x3) && (op1 & 0x00ff))
		extended = true;
	else
		extended = false;

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (int j = 0; j < opcodes_ext_size; j++)
		{
			if (only7bitext) {
				if (((op1 & 0x7f) & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
				{
					opc_ext = &opcodes_ext[j];
					break;
				}
			}
			else {
				if ((op1 & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
				{
					opc_ext = &opcodes_ext[j];
					break;
				}
			}
		}
	}

	// printing

	if (settings_.show_pc)
		buf += sprintf(buf, "%04x ", *pc);

	u32 op2;

	// Size 2 - the op has a large immediate.
	if (opc->size == 2)
	{
		op2 = binbuf[(*pc + 1) & 0x0fff];
		if (settings_.show_hex)
			buf += sprintf(buf, "%04x %04x ", op1, op2);
	}
	else
	{
		op2 = 0;
		if (settings_.show_hex)
			buf += sprintf(buf, "%04x      ", op1);
	}

	char opname[20];
	strcpy(opname, opc->name);
	if (settings_.lower_case_ops)
		MakeLowerCase(opname);
	char ext_buf[20];
	if (extended)
		sprintf(ext_buf, "%s%c%s", opname, settings_.ext_separator, opc_ext->name);
	else
		sprintf(ext_buf, "%s", opname);
	if (settings_.lower_case_ops)
		MakeLowerCase(ext_buf);

	if (settings_.print_tabs)
		buf += sprintf(buf, "%s\t", ext_buf);
	else
		buf += sprintf(buf, "%-12s", ext_buf);

	if (opc->param_count > 0)
		DisParams(*opc, op1, op2, buf);

	buf += strlen(buf);

	// Handle opcode extension.
	if (extended)
	{
		if (opc->param_count > 0)
			buf += sprintf(buf, " ");
		buf += sprintf(buf, ": ");
		if (opc_ext->param_count > 0)
			DisParams(*opc_ext, op1, op2, buf);

		buf += strlen(buf);
	}

	if (opc->opcode_mask == 0)
	{
		// unknown opcode
		unk_opcodes[op1]++;
		sprintf(buf, "\t\t; *** UNKNOWN OPCODE ***");
	}

	if (extended)
		*pc += opc_ext->size;
	else
		*pc += opc->size;

	if (pass == 2)
		dest.append(buffer);
	return true;
}

bool DSPDisassembler::DisFile(const std::string& name, int base_addr, int pass, std::string &output)
{
	File::IOFile in(name, "rb");
	if (!in)
	{
		printf("gd_dis_file: No input\n");
		return false;
	}

	const int size = ((int)in.GetSize() & ~1) / 2;
	u16 *const binbuf = new u16[size];
	in.ReadArray(binbuf, size);
	in.Close();

	// Actually do the disassembly.
	for (u16 pc = 0; pc < size;)
	{
		DisOpcode(binbuf, base_addr, pass, &pc, output);
		if (pass == 2)
			output.append("\n");
	}
	delete[] binbuf;
	return true;
}
