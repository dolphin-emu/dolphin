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
//
//
// At the moment just ls and sl are using the prolog
// perhaps all actions on r03 must be in the prolog
//
#include <stdafx.h>

#include "OutBuffer.h"



//
void dsp_op_ext_r_epi(uint16 _Opcode)
{
	uint8 op  = (_Opcode >> 2) & 0x3;
	uint8 reg = _Opcode & 0x3;

	switch (op)
	{
	    case 0x00:
			OutBuffer::AddCode("Error: dsp_op_ext_r_epi");
		    break;

	    case 0x01:
			OutBuffer::AddCode("%s--", OutBuffer::GetRegName(reg));
		    // g_dsp.r[reg]--;
		    break;

	    case 0x02:
			OutBuffer::AddCode("%s++", OutBuffer::GetRegName(reg));
		    //g_dsp.r[reg]++;
		    break;

	    case 0x03:
			OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(reg), OutBuffer::GetRegName(reg+4));
		    // g_dsp.r[reg] += g_dsp.r[reg + 4];
		    break;
	}
}


void dsp_op_ext_mv(uint16 _Opcode)
{
	uint8 sreg = _Opcode & 0x3;
	uint8 dreg = ((_Opcode >> 2) & 0x3);

	OutBuffer::AddCode("%s = %s", OutBuffer::GetRegName(dreg + 0x18), OutBuffer::GetRegName(sreg + 0x1c));

	// g_dsp.r[dreg + 0x18] = g_dsp.r[sreg + 0x1c];
}


void dsp_op_ext_s(uint16 _Opcode)
{
	uint8 dreg = _Opcode & 0x3;
	uint8 sreg = ((_Opcode >> 3) & 0x3) + 0x1c;

	OutBuffer::AddCode("WriteDMEM(%s, %s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(sreg));
	// dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);

	if (_Opcode & 0x04)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(dreg+4));
		// g_dsp.r[dreg] += g_dsp.r[dreg + 4];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(dreg));
		//g_dsp.r[dreg]++;
	}
}


void dsp_op_ext_l(uint16 _Opcode)
{
	uint8 sreg = _Opcode & 0x3;
	uint8 dreg = ((_Opcode >> 3) & 0x7) + 0x18;

	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(sreg));
	// uint16 val = dsp_dmem_read(g_dsp.r[sreg]);
	// g_dsp.r[dreg] = val;

	if (_Opcode & 0x04)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(sreg), OutBuffer::GetRegName(sreg+4));
		// g_dsp.r[sreg] += g_dsp.r[sreg + 4];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(sreg));
		// g_dsp.r[sreg]++;
	}
}


void dsp_op_ext_ls_pro(uint16 _Opcode)
{
	uint8 areg = (_Opcode & 0x1) + 0x1e;

	OutBuffer::AddCode("WriteDMEM(%s, %s)", OutBuffer::GetRegName(0x03), OutBuffer::GetRegName(areg));
	// dsp_dmem_write(g_dsp.r[0x03], g_dsp.r[areg]);

	if (_Opcode & 0x8)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(0x03), OutBuffer::GetRegName(0x07));
		// g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(0x03));
		// g_dsp.r[0x03]++;
	}
}


void dsp_op_ext_ls_epi(uint16 _Opcode)
{
	uint8 dreg = ((_Opcode >> 4) & 0x3) + 0x18;

	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(0x00));
	// uint16 val = dsp_dmem_read(g_dsp.r[0x00]);
	// dsp_op_write_reg(dreg, val);

	if (_Opcode & 0x4)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(0x00), OutBuffer::GetRegName(0x04));
		// g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(0x00));
		// g_dsp.r[0x00]++;
	}
}


void dsp_op_ext_sl_pro(uint16 _Opcode)
{
	uint8 areg = (_Opcode & 0x1) + 0x1e;

	OutBuffer::AddCode("WriteDMEM(%s, %s)", OutBuffer::GetRegName(0x00), OutBuffer::GetRegName(areg));
	// dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	if (_Opcode & 0x4)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(0x00), OutBuffer::GetRegName(0x04));
		// g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(0x00));
		// g_dsp.r[0x00]++;
	}
}


void dsp_op_ext_sl_epi(uint16 _Opcode)
{
	uint8 dreg = ((_Opcode >> 4) & 0x3) + 0x18;

	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg), OutBuffer::GetRegName(0x03));
	// uint16 val = dsp_dmem_read(g_dsp.r[0x03]);
	// dsp_op_write_reg(dreg, val);

	if (_Opcode & 0x8)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(0x03), OutBuffer::GetRegName(0x07));
		// g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(0x03));
		// g_dsp.r[0x03]++;
	}
}


void dsp_op_ext_ld(uint16 _Opcode)
{
	uint8 dreg1 = (((_Opcode >> 5) & 0x1) << 1) + 0x18;
	uint8 dreg2 = (((_Opcode >> 4) & 0x1) << 1) + 0x19;
	uint8 sreg = _Opcode & 0x3;

	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg1), OutBuffer::GetRegName(sreg));
	OutBuffer::AddCode("%s = ReadDMEM(%s)", OutBuffer::GetRegName(dreg2), OutBuffer::GetRegName(0x03));

	// g_dsp.r[dreg1] = dsp_dmem_read(g_dsp.r[sreg]);
	// g_dsp.r[dreg2] = dsp_dmem_read(g_dsp.r[0x03]);

	if (_Opcode & 0x04)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(sreg), OutBuffer::GetRegName(sreg + 0x04));
		// g_dsp.r[sreg] += g_dsp.r[sreg + 0x04];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(sreg));
		// g_dsp.r[sreg]++;
	}

	if (_Opcode & 0x08)
	{
		OutBuffer::AddCode("%s += %s", OutBuffer::GetRegName(0x03), OutBuffer::GetRegName(sreg + 0x07));
		// g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		OutBuffer::AddCode("%s++", OutBuffer::GetRegName(0x03));
		// g_dsp.r[0x03]++;
	}
}


// ================================================================================
//
//
//
// ================================================================================

void dsp_op_ext_ops_pro(uint16 _Opcode)
{
	if ((_Opcode & 0xFF) == 0){return;}

	switch ((_Opcode >> 4) & 0xf)
	{
	    case 0x00:
		    dsp_op_ext_r_epi(_Opcode);
		    break;

	    case 0x01:
		    dsp_op_ext_mv(_Opcode);
		    break;

	    case 0x02:
	    case 0x03:
		    dsp_op_ext_s(_Opcode);
		    break;

	    case 0x04:
	    case 0x05:
	    case 0x06:
	    case 0x07:
		    dsp_op_ext_l(_Opcode);
		    break;

	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:

		    if (_Opcode & 0x2)
		    {
			    dsp_op_ext_sl_pro(_Opcode);
		    }
		    else
		    {
			    dsp_op_ext_ls_pro(_Opcode);
		    }

		    return;

	    case 0x0c:
	    case 0x0d:
	    case 0x0e:
	    case 0x0f:
		    dsp_op_ext_ld(_Opcode);
		    break;
	}
}


void dsp_op_ext_ops_epi(uint16 _Opcode)
{
	if ((_Opcode & 0xFF) == 0){return;}

	switch ((_Opcode >> 4) & 0xf)
	{
	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:

		    if (_Opcode & 0x2)
		    {
			    dsp_op_ext_sl_epi(_Opcode);
		    }
		    else
		    {
			    dsp_op_ext_ls_epi(_Opcode);
		    }

		    return;
	}
}


