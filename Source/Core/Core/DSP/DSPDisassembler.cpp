// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPTables.h"

DSPDisassembler::DSPDisassembler(const AssemblerSettings &settings)
	: settings_(settings)
{
}

DSPDisassembler::~DSPDisassembler()
{
	// Some old code for logging unknown ops.
	std::string filename = File::GetUserPath(D_DUMPDSP_IDX) + "UnkOps.txt";
	std::ofstream uo(filename);
	if (!uo)
		return;

	int count = 0;
	for (const auto& entry : unk_opcodes)
	{
		if (entry.second > 0)
		{
			count++;
			uo << StringFromFormat("OP%04x\t%d", entry.first, entry.second);
			for (int j = 15; j >= 0; j--)  // print op bits
			{
				if ((j & 0x3) == 3)
					uo << "\tb";

				uo << StringFromFormat("%d", (entry.first >> j) & 0x1);
			}

			uo << "\n";
		}
	}

	uo << StringFromFormat("Unknown opcodes count: %d\n", count);
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
	return DisassembleFile(tmp1, base_addr, 1, text) && DisassembleFile(tmp1, base_addr, 2, text);
}

std::string DSPDisassembler::DisassembleParameters(const DSPOPCTemplate& opc, u16 op1, u16 op2)
{
	std::string buf;

	for (int j = 0; j < opc.param_count; j++)
	{
		if (j > 0)
			buf += ", ";

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
				buf += StringFromFormat("$%s", pdregname(val));
			else
				buf += StringFromFormat("$%d", val);
			break;

		case P_PRG:
			if (settings_.decode_registers)
				buf += StringFromFormat("@$%s", pdregname(val));
			else
				buf += StringFromFormat("@$%d", val);
			break;

		case P_VAL:
		case P_ADDR_I:
		case P_ADDR_D:
			if (settings_.decode_names)
			{
				buf += pdname(val);
			}
			else
			{
				buf += StringFromFormat("0x%04x", val);
			}
			break;

		case P_IMM:
			if (opc.params[j].size != 2)
			{
				if (opc.params[j].mask == 0x003f) // LSL, LSR, ASL, ASR
					buf += StringFromFormat("#%d", (val & 0x20) ? (val | 0xFFFFFFC0) : val);  // 6-bit sign extension
				else
					buf += StringFromFormat("#0x%02x", val);
			}
			else
			{
				buf += StringFromFormat("#0x%04x", val);
			}
			break;

		case P_MEM:
			if (opc.params[j].size != 2)
				val = (u16)(s16)(s8)val;

			if (settings_.decode_names)
				buf += StringFromFormat("@%s", pdname(val));
			else
				buf += StringFromFormat("@0x%04x", val);
			break;

		default:
			ERROR_LOG(DSPLLE, "Unknown parameter type: %x", opc.params[j].type);
			break;
		}
	}

	return buf;
}

static std::string MakeLowerCase(std::string in)
{
	std::transform(in.begin(), in.end(), in.begin(), ::tolower);

	return in;
}

bool DSPDisassembler::DisassembleOpcode(const u16 *binbuf, int base_addr, int pass, u16 *pc, std::string &dest)
{
	std::string buf(" ");

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

	if (((opc->opcode >> 12) == 0x3) && (op1 & 0x007f))
	{
		extended = true;
		only7bitext = true;
	}
	else if (((opc->opcode >> 12) > 0x3) && (op1 & 0x00ff))
	{
		extended = true;
	}

	if (extended)
	{
		// opcode has an extension
		// find opcode
		for (int j = 0; j < opcodes_ext_size; j++)
		{
			if (only7bitext)
			{
				if (((op1 & 0x7f) & opcodes_ext[j].opcode_mask) == opcodes_ext[j].opcode)
				{
					opc_ext = &opcodes_ext[j];
					break;
				}
			}
			else
			{
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
		buf += StringFromFormat("%04x ", *pc);

	u32 op2;

	// Size 2 - the op has a large immediate.
	if (opc->size == 2)
	{
		op2 = binbuf[(*pc + 1) & 0x0fff];
		if (settings_.show_hex)
			buf += StringFromFormat("%04x %04x ", op1, op2);
	}
	else
	{
		op2 = 0;
		if (settings_.show_hex)
			buf += StringFromFormat("%04x      ", op1);
	}

	std::string opname = opc->name;
	if (settings_.lower_case_ops)
		opname = MakeLowerCase(opname);

	std::string ext_buf;
	if (extended)
		ext_buf = StringFromFormat("%s%c%s", opname.c_str(), settings_.ext_separator, opc_ext->name);
	else
		ext_buf = opname;
	if (settings_.lower_case_ops)
		ext_buf = MakeLowerCase(ext_buf);

	if (settings_.print_tabs)
		buf += StringFromFormat("%s\t", ext_buf.c_str());
	else
		buf += StringFromFormat("%-12s", ext_buf.c_str());

	if (opc->param_count > 0)
		buf += DisassembleParameters(*opc, op1, op2);

	// Handle opcode extension.
	if (extended)
	{
		if (opc->param_count > 0)
			buf += " ";

		buf += ": ";

		if (opc_ext->param_count > 0)
			buf += DisassembleParameters(*opc_ext, op1, op2);
	}

	if (opc->opcode_mask == 0)
	{
		// unknown opcode
		unk_opcodes[op1]++;
		buf += "\t\t; *** UNKNOWN OPCODE ***";
	}

	if (extended)
		*pc += opc_ext->size;
	else
		*pc += opc->size;

	if (pass == 2)
		dest.append(buf);

	return true;
}

bool DSPDisassembler::DisassembleFile(const std::string& name, int base_addr, int pass, std::string &output)
{
	File::IOFile in(name, "rb");
	if (!in)
	{
		printf("gd_dis_file: No input\n");
		return false;
	}

	const int size = ((int)in.GetSize() & ~1) / 2;
	std::vector<u16> binbuf(size);
	in.ReadArray(binbuf.data(), size);
	in.Close();

	// Actually do the disassembly.
	for (u16 pc = 0; pc < size;)
	{
		DisassembleOpcode(binbuf.data(), base_addr, pass, &pc, output);
		if (pass == 2)
			output.append("\n");
	}

	return true;
}
