/*====================================================================

   filename:     gdsp_memory.h
   project:      GCemu
   created:      2004-6-18
   mail:         duddie@walla.com

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   ====================================================================*/

#pragma once

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

u16  dsp_imem_read(u16 addr);
void dsp_dmem_write(u16 addr, u16 val);
u16  dsp_dmem_read(u16 addr);

inline u16 dsp_fetch_code()
{
	u16 opc = dsp_imem_read(g_dsp.pc);

	g_dsp.pc++;
	return opc;
}

inline u16 dsp_peek_code()
{
	return dsp_imem_read(g_dsp.pc);
}

inline void dsp_skip_inst()
{
	g_dsp.pc += opTable[dsp_peek_code()]->size;
}
