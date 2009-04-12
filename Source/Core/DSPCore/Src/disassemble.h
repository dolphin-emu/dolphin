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

struct gd_globals_t
{
	bool print_tabs;
	bool show_hex;
	bool show_pc;
	bool decode_names;
	bool decode_registers;

	u16* binbuf;
	u16 pc;
	char* buffer;
	u16 buffer_size;
	char ext_separator;
};

class DSPDisassembler
{
public:
	DSPDisassembler();
	~DSPDisassembler() {}

	char* gd_dis_opcode(gd_globals_t* gdg);
	bool gd_dis_file(gd_globals_t* gdg, const char* name, FILE* output);

	void gd_dis_close_unkop();
	void gd_dis_open_unkop();
	const char* gd_dis_get_reg_name(u16 reg);

private:
	char* gd_dis_params(gd_globals_t* gdg, const DSPOPCTemplate* opc, u16 op1, u16 op2, char* strbuf);
};

const char *gd_get_reg_name(u16 reg);
u16 gd_dis_get_opcode_size(gd_globals_t* gdg);

#endif  // _DSP_DISASSEMBLE_H

