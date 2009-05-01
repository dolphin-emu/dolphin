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

#include "gdsp_opcodes_helper.h"
#include "gdsp_memory.h"

// Extended opcodes do not exist on their own. These opcodes can only be
// attached to opcodes that allow extending (8 lower bits of opcode not used by
// opcode). Extended opcodes do not modify program counter $pc register.

// Most of the suffixes increment or decrement one or more addressing registers
// (the first four, ARx). The increment/decrement is either 1, or the corresponding 
// "index" register (the second four, IXx). The addressing registers will wrap
// in odd ways, dictated by the corresponding wrapping register, WP0-3.

// The following should be applied as a decrement:
// ar[i] = (ar[i] & wp[i]) == 0 ? ar[i] | wp[i] : ar[i] - 1;
// I have not found the corresponding algorithms for increments yet.
// It's gotta be fairly simple though. See R3123, R3125 in Google Code.

namespace DSPInterpreter
{

namespace Ext 
{

// DR $arR
// xxxx xxxx 0000 01rr
// Decrement addressing register $arR.
void dr(const UDSPInstruction& opc) {
	dsp_decrement_addr_reg(opc.hex & 0x3);
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void ir(const UDSPInstruction& opc) {
	dsp_increment_addr_reg(opc.hex & 0x3);
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void nr(const UDSPInstruction& opc) {
	u8 reg = opc.hex & 0x3;	
 
	g_dsp.r[reg] += g_dsp.r[reg + DSP_REG_IX0];
}

// MV $axD, $acS.l
// xxxx xxxx 0001 ddss
// Move value of $acS.l to the $axD.l.
void mv(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 2) & 0x3);
	
	g_dsp.r[dreg + DSP_REG_AXL0] = g_dsp.r[sreg + DSP_REG_ACC0];
}

// S @$D, $acD.l
// xxxx xxxx 001s s0dd
// Store value of $(acS.l) in the memory pointed by register $D.
// Post increment register $D.
void s(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = ((opc.hex >> 3) & 0x3) + DSP_REG_ACC0;

	dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);

	dsp_increment_addr_reg(dreg);
}

// SN @$D, $acD.l
// xxxx xxxx 001s s1dd
// Store value of register $acS in the memory pointed by register $D.
// Add indexing register $ixD to register $D.
void sn(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = ((opc.hex >> 3) & 0x3) + DSP_REG_ACC0;

	dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);

	g_dsp.r[dreg] += g_dsp.r[dreg + DSP_REG_IX0];
}

// L axD.l, @$S
// xxxx xxxx 01dd d0ss
// Load $axD with value from memory pointed by register $S. 
// Post increment register $S.
void l(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + DSP_REG_AXL0;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg] = val;

	dsp_increment_addr_reg(sreg);
}

// LN axD.l, @$S
// xxxx xxxx 01dd d0ss
// Load $axD with value from memory pointed by register $S. 
// Add indexing register register $ixS to register $S.
void ln(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + DSP_REG_AXL0;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg] = val;

	g_dsp.r[sreg] += g_dsp.r[sreg + DSP_REG_IX0];
}

// Not in duddie's doc
// LD $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 00ss
void ld(const UDSPInstruction& opc)
{
	u8 dreg = (((opc.hex >> 5) & 0x1) << 1) + DSP_REG_AXL0;
	u8 rreg = (((opc.hex >> 4) & 0x1) << 1) + DSP_REG_AXL1;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[rreg] = dsp_dmem_read(g_dsp.r[DSP_REG_AR3]);

	dsp_increment_addr_reg(sreg);
	dsp_increment_addr_reg(DSP_REG_AR3);
}

// Not in duddie's doc
// LDN $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 01ss
void ldn(const UDSPInstruction& opc)
{
	u8 dreg = (((opc.hex >> 5) & 0x1) << 1) + DSP_REG_AXL0;
	u8 rreg = (((opc.hex >> 4) & 0x1) << 1) + DSP_REG_AXL1;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[rreg] = dsp_dmem_read(g_dsp.r[DSP_REG_AR3]);

	g_dsp.r[sreg] += g_dsp.r[sreg + DSP_REG_IX0];
	dsp_increment_addr_reg(DSP_REG_AR3);
}

// Not in duddie's doc
// LDM $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 10ss
void ldm(const UDSPInstruction& opc)
{
	u8 dreg = (((opc.hex >> 5) & 0x1) << 1) + DSP_REG_AXL0;
	u8 rreg = (((opc.hex >> 4) & 0x1) << 1) + DSP_REG_AXL1;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[rreg] = dsp_dmem_read(g_dsp.r[DSP_REG_AR3]);

	dsp_increment_addr_reg(sreg);
	g_dsp.r[DSP_REG_AR3] += g_dsp.r[DSP_REG_IX3];
}

// Not in duddie's doc
// LDNM $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 11ss
void ldnm(const UDSPInstruction& opc)
{
	u8 dreg = (((opc.hex >> 5) & 0x1) << 1) + DSP_REG_AXL0;
	u8 rreg = (((opc.hex >> 4) & 0x1) << 1) + DSP_REG_AXL1;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[rreg] = dsp_dmem_read(g_dsp.r[DSP_REG_AR3]);

	g_dsp.r[sreg] += g_dsp.r[sreg + DSP_REG_IX0];
	g_dsp.r[DSP_REG_AR3] += g_dsp.r[DSP_REG_IX3];
}

} // end namespace ext
} // end namespace DSPInterpeter

void dsp_op_ext_r_epi(const UDSPInstruction& opc)
{
	u8 op  = (opc.hex >> 2) & 0x3;
	u8 reg = opc.hex & 0x3;
	switch (op) {
	case 0x00: // 
		g_dsp.r[reg] = 0;
		break;
		
	case 0x01: // DR
		dsp_decrement_addr_reg(reg);
		break;
		
	case 0x02: // IR
		dsp_increment_addr_reg(reg);
		break;
		
	  case 0x03: // NR
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

	if (opc.hex & 0x04) // SN
	{
		g_dsp.r[dreg] += g_dsp.r[dreg + 4];
	}
	else
	{
		dsp_increment_addr_reg(dreg); // S
	}
}


void dsp_op_ext_l(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + 0x18;

	u16 val = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg] = val;

	if (opc.hex & 0x04) // LN/LSMN
	{
		g_dsp.r[sreg] += g_dsp.r[sreg + 4];
	}
	else
	{
		dsp_increment_addr_reg(sreg); // LS
	}
}


void dsp_op_ext_ls_pro(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + 0x1e;
	dsp_dmem_write(g_dsp.r[0x03], g_dsp.r[areg]);

	if (opc.hex & 0x8) // LSM/LSMN
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	} 
	else // LS
	{
		dsp_increment_addr_reg(0x03);
	}
}


void dsp_op_ext_ls_epi(const UDSPInstruction& opc)
{
	u8 dreg = ((opc.hex >> 4) & 0x3) + 0x18;
	u16 val = dsp_dmem_read(g_dsp.r[0x00]);
	dsp_op_write_reg(dreg, val);

	if (opc.hex & 0x4) // LSN/LSMN
	{
		g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else // LS
	{
		dsp_increment_addr_reg(0x00); 
	}
}


void dsp_op_ext_sl_pro(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + 0x1e;
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	if (opc.hex & 0x4) // SLN/SLNM
	{
		g_dsp.r[0x00] += g_dsp.r[0x04];
	}
	else // SL 
	{
		dsp_increment_addr_reg(0x00);
	}
}


void dsp_op_ext_sl_epi(const UDSPInstruction& opc)
{
	u8 dreg = ((opc.hex >> 4) & 0x3) + 0x18;
	u16 val = dsp_dmem_read(g_dsp.r[0x03]);
	dsp_op_write_reg(dreg, val);

	if (opc.hex & 0x8) // SLM/SLMN
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else // SL
	{
		dsp_increment_addr_reg(0x03);
	}
}


void dsp_op_ext_ld(const UDSPInstruction& opc)
{
	u8 dreg1 = (((opc.hex >> 5) & 0x1) << 1) + 0x18;
	u8 dreg2 = (((opc.hex >> 4) & 0x1) << 1) + 0x19;
	u8 sreg = opc.hex & 0x3;
	g_dsp.r[dreg1] = dsp_dmem_read(g_dsp.r[sreg]);
	g_dsp.r[dreg2] = dsp_dmem_read(g_dsp.r[0x03]);

   	if (opc.hex & 0x04) // N
	{
		g_dsp.r[sreg] += g_dsp.r[sreg + 0x04];
	}
	else
	{
		dsp_increment_addr_reg(sreg);
	}
	
	if (opc.hex & 0x08) // M
	{
		g_dsp.r[0x03] += g_dsp.r[0x07];
	}
	else
	{
		dsp_increment_addr_reg(0x03);
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
		  
		  if (opc.hex & 0x2) {
			  dsp_op_ext_sl_pro(opc.hex);
		  }
		  else {
			  dsp_op_ext_ls_pro(opc.hex);
		  }
		  
		  break;
			  
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
	if ((opc.hex & 0xFF) == 0)
	{
		return;
	}

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
		  break;

		  return;
	}
}
