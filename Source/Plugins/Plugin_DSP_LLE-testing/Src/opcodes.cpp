/*====================================================================

   filename:     opcodes.cpp
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

#include "Globals.h"
#include "opcodes.h"

opc_t opcodes[] =
{
	{"NOP",		0x0000, 0xffff, 1, 0, {},},
	{"HALT",    0x0021, 0xffff, 1, 0, {},},
	{"RET",		0x02df, 0xffff, 1, 0, {},},
	{"RETEQ",   0x02d5, 0xffff, 1, 0, {},},
	{"RETNZ",   0x02dd, 0xffff, 1, 0, {},},
	{"RTI",		0x02ff, 0xffff, 1, 0, {},},
	{"CALL",    0x02bf, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},

	{"CALLNE",  0x02b4, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},

	{"IF_0",    0x0270, 0xffff, 1, 0, {},},
	{"IF_1",    0x0271, 0xffff, 1, 0, {},},
	{"IF_2",    0x0272, 0xffff, 1, 0, {},},
	{"IF_3",    0x0273, 0xffff, 1, 0, {},},
	{"IF_E",    0x0274, 0xffff, 1, 0, {},},
	{"IF_Q",    0x0275, 0xffff, 1, 0, {},},
	{"IF_R",    0x027c, 0xffff, 1, 0, {},},
	{"IF_Z",    0x027d, 0xffff, 1, 0, {},},
	{"IF_P",    0x027f, 0xffff, 1, 0, {},},

	{"JX0",		0x0290, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JX1",		0x0291, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JX2",		0x0292, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JX3",		0x0293, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JNE",		0x0294, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JEQ",		0x0295, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JZR",		0x029c, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JNZ",		0x029d, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},
	{"JMP",		0x029f, 0xffff, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}},},

	{"DAR",		0x0004, 0xfffc, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},},
	{"IAR",		0x0008, 0xfffc, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},},

	{"CALLR",   0x171f, 0xff1f, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}},},
	{"JMPR",    0x170f, 0xff1f, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}},},

	{"SBCLR",   0x1200, 0xfff8, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}},},
	{"SBSET",   0x1300, 0xfff8, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}},},

	{"LSL",		0x1400, 0xfec0, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}},},
	{"LSR",		0x1440, 0xfec0, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}},},
	{"ASL",		0x1480, 0xfec0, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x007f}},},
	{"ASR",		0x14c0, 0xfec0, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x007f}},},


	{"LRI",		0x0080, 0xffe0, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_IMM, 2, 1, 0, 0xffff}},},
	{"LR",      0x00c0, 0xffe0, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_MEM, 2, 1, 0, 0xffff}},},
	{"SR",      0x00e0, 0xffe0, 2, 2, {{P_MEM, 2, 1, 0, 0xffff}, {P_REG, 1, 0, 0, 0x001f}},},

	{"MRR",		0x1c00, 0xfc00, 1, 2, {{P_REG, 1, 0, 5, 0x03e0}, {P_REG, 1, 0, 0, 0x001f}},},

	{"SI",      0x1600, 0xff00, 2, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_IMM, 2, 1, 0, 0xffff}},},

	{"LRS",		0x2000, 0xf800, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_MEM, 1, 0, 0, 0x00ff}},},
	{"SRS",		0x2800, 0xf800, 1, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_REG18, 1, 0, 8, 0x0700}},},

	{"LRIS",    0x0800, 0xf800, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_IMM, 1, 0, 0, 0x00ff}},},

	{"ADDIS",   0x0400, 0xfe00, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}},},
	{"CMPIS",   0x0600, 0xfe00, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}},},

	{"ANDI",    0x0240, 0xfeff, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},
	{"ANDF",    0x02c0, 0xfeff, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},

	{"XORI",    0x0220, 0xfeff, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},
	{"ANDCF",   0x02a0, 0xfeff, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},

	{"ORI",		0x0260, 0xfeff, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},
	{"ORF",		0x02e0, 0xfeff, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},},

	{"ADDI",    0x0200, 0xfeff, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},}, // missing S64
	{"CMPI",    0x0280, 0xfeff, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},}, // missing S64

	{"ILRR",    0x0210, 0xfedc, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"ILRRI",   0x0218, 0xfedc, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}},},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRRI",    0x1900, 0xff80, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}},},
	{"LRRD",    0x1880, 0xff80, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}},},
	{"LRRN",    0x1980, 0xff80, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}},},
	{"LRR",		0x1800, 0xff80, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}},},
	{"SRRI",    0x1b00, 0xff80, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}},},
	{"SRRD",    0x1a80, 0xff80, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}},},
	{"SRRN",    0x1b80, 0xff80, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}},},
	{"SRR",		0x1a00, 0xff80, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}},},

	{"LOOPI",   0x1000, 0xff00, 1, 1, {{P_IMM, 1, 0, 0, 0x00ff}},},
	{"BLOOPI",  0x1100, 0xff00, 2, 2, {{P_IMM, 1, 0, 0, 0x00ff}, {P_VAL, 2, 1, 0, 0xffff}},},
	{"LOOP",    0x0040, 0xffe0, 1, 1, {{P_REG, 1, 0, 0, 0x001f}},},
	{"BLOOP",   0x0060, 0xffe0, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_VAL, 2, 1, 0, 0xffff}},},



	// opcodes that can be extended
	// extended opcodes, note size of opcode will be set to 0

	{"NX",      0x8000, 0xffff, 1 | P_EXT, 0, {},},

	{"S40",		0x8e00, 0xffff, 1 | P_EXT, 0, {},},
	{"S16",		0x8f00, 0xffff, 1 | P_EXT, 0, {},},
	{"M2",      0x8a00, 0xffff, 1 | P_EXT, 0, {},},
	{"M0",      0x8b00, 0xffff, 1 | P_EXT, 0, {},},
	{"CLR15",   0x8c00, 0xffff, 1 | P_EXT, 0, {},},
	{"SET15",   0x8d00, 0xffff, 1 | P_EXT, 0, {},},

	{"DECM",    0x7800, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"INCM",    0x7400, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"DEC",		0x7a00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"INC",		0x7600, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},

	{"NEG",		0x7c00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},

	{"TST",		0xb100, 0xf7ff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}},},
	{"TSTAXH",  0x8600, 0xfeff, 1 | P_EXT, 1, {{P_REG1A, 1, 0, 8, 0x0100}},},
	{"CMP",		0x8200, 0xffff, 1 | P_EXT, 0, {},},
	{"CMPAXH",  0xc100, 0xe7ff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}},},

	{"CLR",		0x8100, 0xf7ff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}},},
	{"CLRP",    0x8400, 0xffff, 1 | P_EXT, 0, {},},

	{"MOV",		0x6c00, 0xfeff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}},},
	{"MOVAX",   0x6800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}},},
	{"MOVR",    0x6000, 0xf8ff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}},},
	{"MOVP",    0x6e00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"MOVPZ",   0xfe00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},

	{"ADDPAXZ", 0xf800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG1A, 1, 0, 8, 0x0100}},},
	{"ADDP",    0x4e00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},

	{"LSL16",   0xf000, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"LSR16",   0xf400, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"ASR16",   0x9100, 0xf7ff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}},},

	{"XORR",    0x3000, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}},},
	{"ANDR",    0x3400, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}},},
	{"ORR",		0x3800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}},},
	{"ANDC",    0x3C00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},
	{"ORC",		0x3E00, 0xfeff, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}},},

	{"MULX",    0xa000, 0xe7ff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}},},
	{"MULXAC",  0xa400, 0xe6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULXMV",  0xa600, 0xe6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULXMVZ", 0xa200, 0xe6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},

	{"MUL",		0x9000, 0xf7ff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}},},
	{"MULAC",   0x9400, 0xf6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULMV",   0x9600, 0xf6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULMVZ",  0x9200, 0xf6ff, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}},},

	{"MULC",    0xc000, 0xe7ff, 1 | P_EXT, 2, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}},},
	{"MULCAC",  0xc400, 0xe6ff, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULCMV",  0xc600, 0xe6ff, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}},},
	{"MULCMVZ", 0xc200, 0xe6ff, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}},},

	{"ADDR",    0x4000, 0xf8ff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}},},
	{"ADDAX",   0x4800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}},},
	{"ADD",		0x4c00, 0xfeff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}},},
	{"ADDAXL",  0x7000, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}},},

	{"SUBR",    0x5000, 0xf8ff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}},},
	{"SUBAX",   0x5800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}},},
	{"SUB",		0x5c00, 0xfeff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}},},

	{"MADD",    0xf200, 0xfeff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}},},
	{"MSUB",    0xf600, 0xfeff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}},},
	{"MADDX",   0xe000, 0xfcff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}},},
	{"MSUBX",   0xe400, 0xfcff, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}},},
	{"MADDC",   0xe800, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}},},
	{"MSUBC",   0xec00, 0xfcff, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}},},

	// assemble CW
	{"CW",      0x0000, 0xffff, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}},},
	// unknown opcode for disassemble
	{"CW",      0x0000, 0x0000, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}},},
};
opc_t opcodes_ext[] =
{
	{"L",       0x0040, 0x00c4, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"LN",      0x0044, 0x00c4, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"LS",      0x0080, 0x00ce, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}},},
	{"LSN",		0x0084, 0x00ce, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}},},
	{"LSM",		0x0088, 0x00ce, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}},},
	{"LSNM",    0x008c, 0x00ce, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}},},
	{"SL",      0x0082, 0x00ce, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}},},
	{"SLN",		0x0086, 0x00ce, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}},},
	{"SLM",		0x008a, 0x00ce, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}},},
	{"SLNM",    0x008e, 0x00ce, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}},},
	{"S",       0x0020, 0x00e4, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}},},
	{"SN",      0x0024, 0x00e4, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}},},
	{"LDX",		0x00c0, 0x00cf, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}},},
	{"LDXN",    0x00c4, 0x00cf, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}},},
	{"LDXM",    0x00c8, 0x00cf, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}},},
	{"LDXNM",   0x00cc, 0x00cf, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}},},
	{"LD",      0x00c0, 0x00cc, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"LDN",		0x00c4, 0x00cc, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"LDM",		0x00c8, 0x00cc, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"LDNM",    0x00cc, 0x00cc, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}},},
	{"MV",      0x0010, 0x00f0, 1, 2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}},},
	{"DR",      0x0004, 0x00fc, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},},
	{"IR",      0x0008, 0x00fc, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},},
	{"NR",      0x000c, 0x00fc, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},},
	{"XXX",		0x0000, 0x0000, 1, 1, {{P_VAL, 1, 0, 0, 0x00ff}},},
};

const u32 opcodes_size = sizeof(opcodes) / sizeof(opc_t);
const u32 opcodes_ext_size = sizeof(opcodes_ext) / sizeof(opc_t);

