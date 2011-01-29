/*====================================================================

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

#ifndef _DSP_DISASSEMBLE_H
#define _DSP_DISASSEMBLE_H

#include <map>
#include <vector>

#include "Common.h"
#include "DSPTables.h"
#include "LabelMap.h"

struct AssemblerSettings
{
	AssemblerSettings()
		: print_tabs(false),
		  show_hex(false),
		  show_pc(false),
		  force(false),
		  decode_names(true),
		  decode_registers(true),
		  ext_separator('\''),
		  lower_case_ops(true),
		  pc(0)
	{
	}

	bool print_tabs;
	bool show_hex;
	bool show_pc;
	bool force;
	bool decode_names;
	bool decode_registers;
	char ext_separator;
	bool lower_case_ops;

	u16 pc;
};

class DSPDisassembler
{
public:
	DSPDisassembler(const AssemblerSettings &settings);
	~DSPDisassembler();

	bool Disassemble(int start_pc, const std::vector<u16> &code, int base_addr, std::string &text);

	// Warning - this one is trickier to use right.
	// Use pass == 2 if you're just using it by itself.
	bool DisOpcode(const u16 *binbuf, int base_addr, int pass, u16 *pc, std::string &dest);

private:
	// Moves PC forward and writes the result to dest.
	bool DisFile(const char* name, int base_addr, int pass, std::string &output);

	char* DisParams(const DSPOPCTemplate& opc, u16 op1, u16 op2, char* strbuf);
	std::map<u16, int> unk_opcodes;

	const AssemblerSettings settings_;

	LabelMap labels;
};

#endif  // _DSP_DISASSEMBLE_H

