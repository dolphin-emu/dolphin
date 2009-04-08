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
#include "Globals.h"

#include "gdsp_opcodes_helper.h"


//
void dsp_op_ext_r_epi(const UDSPInstruction& opc)
{
	u8 op  = (opc.hex >> 2) & 0x3;
	u8 reg = opc.hex & 0x3;

	switch (op)
	{
	    case 0x00:
		    ERROR_LOG(DSPLLE, "dsp_op_ext_r_epi");
		    break;

	    case 0x01:
		    g_dsp.r[reg]--;
		    break;

	    case 0x02:
		    g_dsp.r[reg]++;
		    break;

	    case 0x03:
		    g_dsp.r[reg] += g_dsp.r[reg + 4];
		    break;
	}
}


void dsp_op_ext_mv(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 2) & 0x3);

	g_dsp.r[dreg + 0x18] = g_dsp.r[sreg + 0x1c];
}


void dsp_op_ext_s(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = ((opc.hex >> 3) & 0x3) + 0x1c;

	dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);

	if (opc.hex & 0x04)
	{
		g_dsp.r[dreg] += g_dsp.r[dreg + 4];
	}
	else
	{
		g_dsp.r[dreg]++;
	}
}


void dsp_op_ext_l(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + 0x18;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg] = val;

	if (opc.hex & 0x04)
	{
		g_dsp.r[sreg] += g_dsp.r[sreg + 4];
	}
	else
	{
		g_dsp.r[sreg]++;
	}
}


void dsp_op_ext_ls_pro(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + 0x1e;
	dsp_dmem_write(g_dsp.r[0x03], g_dsp.r[areg]);

	if (opc.hex & 0x8)
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		g_dsp.r[0x03]++;
	}
}


void dsp_op_ext_ls_epi(const UDSPInstruction& opc)
{
	u8 dreg = ((opc.hex >> 4) & 0x3) + 0x18;
	u16 val = dsp_dmem_read(g_dsp.r[0x00]);
	dsp_op_write_reg(dreg, val);

	if (opc.hex & 0x4)
	{
		g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else
	{
		g_dsp.r[0x00]++;
	}
}


void dsp_op_ext_sl_pro(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + 0x1e;
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	if (opc.hex & 0x4)
	{
		g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else
	{
		g_dsp.r[0x00]++;
	}
}


void dsp_op_ext_sl_epi(const UDSPInstruction& opc)
{
	u8 dreg = ((opc.hex >> 4) & 0x3) + 0x18;
	u16 val = dsp_dmem_read(g_dsp.r[0x03]);
	dsp_op_write_reg(dreg, val);

	if (opc.hex & 0x8)
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		g_dsp.r[0x03]++;
	}
}


void dsp_op_ext_ld(const UDSPInstruction& opc)
{
	u8 dreg1 = (((opc.hex >> 5) & 0x1) << 1) + 0x18;
	u8 dreg2 = (((opc.hex >> 4) & 0x1) << 1) + 0x19;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg1] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg2] = dsp_dmem_read(g_dsp.r[0x03]);

	if (opc.hex & 0x04)
	{
		g_dsp.r[sreg] += g_dsp.r[sreg + 0x04];
	}
	else
	{
		g_dsp.r[sreg]++;
	}

	if (opc.hex & 0x08)
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
{
		g_dsp.r[0x03]++;
	}
}


// ================================================================================
//
//
//
// ================================================================================

void dsp_op_ext_ops_pro(const UDSPInstruction& opc)
{
	if ((opc.hex & 0xFF) == 0){return;}

	switch ((opc.hex >> 4) & 0xf)
	{
	    case 0x00:
		    dsp_op_ext_r_epi(opc.hex);
		    break;

	    case 0x01:
		    dsp_op_ext_mv(opc.hex);
		    break;

	    case 0x02:
	    case 0x03:
		    dsp_op_ext_s(opc.hex);
		    break;

	    case 0x04:
	    case 0x05:
	    case 0x06:
	    case 0x07:
		    dsp_op_ext_l(opc.hex);
		    break;

	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:

		    if (opc.hex & 0x2)
		    {
			    dsp_op_ext_sl_pro(opc.hex);
		    }
		    else
		    {
			    dsp_op_ext_ls_pro(opc.hex);
		    }

		    return;

	    case 0x0c:
	    case 0x0d:
	    case 0x0e:
	    case 0x0f:
		    dsp_op_ext_ld(opc.hex);
		    break;
	}
}


void dsp_op_ext_ops_epi(const UDSPInstruction& opc)
{
	if ((opc.hex & 0xFF) == 0){return;}

	switch ((opc.hex >> 4) & 0xf)
	{
	    case 0x08:
	    case 0x09:
	    case 0x0a:
	    case 0x0b:

		    if (opc.hex & 0x2)
		    {
			    dsp_op_ext_sl_epi(opc.hex);
		    }
		    else
		    {
			    dsp_op_ext_ls_epi(opc.hex);
		    }

		    return;
	}
}


