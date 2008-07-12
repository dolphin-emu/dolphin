/*====================================================================

   filename:     opcodes.h
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

#pragma once

typedef struct gd_globals_t
{
	bool print_tabs;
	bool show_hex;
	bool show_pc;
	bool decode_names;
	bool decode_registers;

	uint16* binbuf;
	uint16 pc;
	char* buffer;
	uint16 buffer_size;
	char ext_separator;
} gd_globals_t;


char* gd_dis_opcode(gd_globals_t* gdg);
bool gd_dis_file(gd_globals_t* gdg, char* name, FILE* output);
const char* gd_dis_get_reg_name(uint16 reg);
