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
#ifndef _OPCODES_H
#define _OPCODES_H

enum parameterType
{
	P_NONE = 0x0000,
	P_VAL = 0x0001,
	P_IMM = 0x0002,
	P_MEM = 0x0003,
	P_STR = 0x0004,
	P_REG = 0x8000,
	P_REG08 = P_REG | 0x0800,
	P_REG10 = P_REG | 0x1000,
	P_REG18 = P_REG | 0x1800,
	P_REG19 = P_REG | 0x1900,
	P_REG1A = P_REG | 0x1a00,
	P_REG1C = P_REG | 0x1c00,
	P_ACCM = P_REG | 0x1e00,
	P_ACCM_D = P_REG | 0x1e80,
	P_ACC = P_REG | 0x2000,
	P_ACC_D = P_REG | 0x2080,
	P_AX = P_REG | 0x2200,
	P_AX_D = P_REG | 0x2280,
	P_REGS_MASK = 0x03f80,
	P_REF       = P_REG | 0x4000,
	P_PRG       = P_REF | P_REG,
};

#define P_EXT   0x80

union UDSPInstruction
{
	u16 hex;

	UDSPInstruction(u16 _hex)	{ hex = _hex; }
        UDSPInstruction()	        { hex = 0; }

    // TODO(XK): Figure out the instruction structure better (add structs here)
};

typedef void (*dspInstFunc)(UDSPInstruction&);

typedef struct DSPOParams
{
	parameterType type;
	u8 size;
	u8 loc;
	s8 lshift;
	u16 mask;
} opcpar_t;

typedef struct DSPOPCTemplate
{
    const char *name;
    u16 opcode;
    u16 opcode_mask;

    dspInstFunc interpFunc;
    dspInstFunc jitFunc;
    
    u8 size;
    u8 param_count;
    DSPOParams params[8];
} opc_t;

extern DSPOPCTemplate opcodes[];
extern const u32 opcodes_size;
extern opc_t opcodes_ext[];
extern const u32 opcodes_ext_size;

inline u16 swap16(u16 x)
{
	return((x >> 8) | (x << 8));
}


#endif

