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

#include "Common.h"
#include "DSPTables.h"

struct AssemblerSettings
{
	AssemblerSettings()
		: print_tabs(false),
		  show_hex(false),
		  show_pc(false),
		  decode_names(true),
		  decode_registers(true),
		  ext_separator('\''),
		  pc(0)
	{
	}

	bool print_tabs;
	bool show_hex;
	bool show_pc;
	bool decode_names;
	bool decode_registers;
	char ext_separator;

	u16 pc;
};

class DSPDisassembler
{
public:
	DSPDisassembler(const AssemblerSettings &settings);
	~DSPDisassembler() {}

	// Moves PC forward and writes the result to dest.
	void gd_dis_opcode(const u16 *binbuf, u16 *pc, std::string *dest);
	bool gd_dis_file(const char* name, FILE *output);

	void gd_dis_close_unkop();
	void gd_dis_open_unkop();
	const char* gd_dis_get_reg_name(u16 reg);

private:
	char* gd_dis_params(const DSPOPCTemplate* opc, u16 op1, u16 op2, char* strbuf);
	u32 unk_opcodes[0x10000];

	const AssemblerSettings settings_;
};

const char *gd_get_reg_name(u16 reg);
//u16 gd_dis_get_opcode_size(gd_globals_t* gdg);

#endif  // _DSP_DISASSEMBLE_H

