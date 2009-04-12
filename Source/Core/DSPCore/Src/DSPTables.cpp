// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Additional copyrights go to Duddie (c) 2005 (duddie@walla.com)

/* NOTES BY HERMES:

LZ flag: original opcodes andf and andcf are swaped. Also "jzr" and "jnz" are swaped but now named 'jlz' and 'jlnz' 
As you can see it obtain the same result but now LZ=1 correctly

Added conditional instructions:

conditional names:

NZ -> NOT ZERO
Z  -> ZERO 

NS -> NOT SIGN
S  ->  SIGN

LZ  -> LOGIC ZERO (only used with andcf-andf instructions?)
LNZ -> LOGIC NOT ZERO

G -> GREATER
LE-> LESS EQUAL

GE-> GREATER EQUAL
L -> LESS

Examples:

jnz, ifs, retlnz

*/


#include "Common.h"
#include "DSPTables.h"

#include "DSPInterpreter.h"
#include "DSPJit.h"
#include "gdsp_ext_op.h"

void nop(const UDSPInstruction& opc)
{
	// The real nop is 0. Anything else is bad.
	if (opc.hex)
		DSPInterpreter::unknown(opc);
}
 
// Unknown Ops
// All AX games: a100
// Zelda Four Swords: 02ca


// TODO: Fill up the tables with the corresponding instructions
const DSPOPCTemplate opcodes[] =
{
	{"NOP",		0x0000, 0xffff, nop, nop, 1, 0, {}, NULL, NULL},

	{"DAR",		0x0004, 0xfffc, DSPInterpreter::dar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL},
	{"IAR",		0x0008, 0xfffc, DSPInterpreter::iar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL},

	{"HALT",	0x0021, 0xffff, DSPInterpreter::halt, nop, 1, 0, {}, NULL, NULL},

	{"RETNS",	0x02d0, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETS",	0x02d1, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETG",	0x02d2, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETLE",	0x02d3, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETNZ",	0x02d4, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETZ",	0x02d5, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETL",	0x02d6, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETGE",	0x02d7, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETLNZ",	0x02dc, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RETLZ",   0x02dd, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RET",		0x02df, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL},
	{"RTI",	    0x02ff, 0xffff, DSPInterpreter::rti, nop, 1, 0, {}, NULL, NULL},

	{"CALLNS",	0x02b0, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLS",	0x02b1, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLG",	0x02b2, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLLE",	0x02b3, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLNE",  0x02b4, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLZ",	0x02b5, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLL",	0x02b6, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLGE",	0x02b7, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLLNZ",	0x02bc, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALLLZ",	0x02bd, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"CALL",    0x02bf, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},

	{"IFNS",	0x0270, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFS",     0x0271, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFG",     0x0272, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFLE",	0x0273, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFNZ",    0x0274, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFZ",     0x0275, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFL",	    0x0276, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFGE",	0x0277, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFLNZ",	0x027c, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IFLZ",    0x027d, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL},
	{"IF",		0x027f, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL}, // Hermes doesn't list this

	{"JNS",	    0x0290, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JS",	    0x0291, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JG",	    0x0292, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JLE",	    0x0293, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JNZ",	    0x0294, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JZ",	    0x0295, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JL",	    0x0296, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JGE",	    0x0297, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JLNZ",	0x029c, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JLZ",		0x029d, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"JMP",		0x029f, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},


	{"JRNS",	0x1700, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRS",		0x1701, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRG",		0x1702, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRLE",	0x1703, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRNZ",	0x1704, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRZ",		0x1705, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRL",		0x1706, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRGE",	0x1707, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRLNZ",	0x170c, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JRLZ",	0x170d, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"JMPR",	0x170f, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},

	{"CALLRNS",	0x1710, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRS",	0x1711, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRG",	0x1712, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRLE",	0x1713, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRNZ",	0x1714, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRZ",	0x1715, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRL",	0x1716, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRGE",	0x1717, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRLNZ",0x171c, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLRLZ",	0x171d, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},
	{"CALLR",	0x171f, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL},

	{"SBCLR",   0x1200, 0xfff8, DSPInterpreter::sbclr, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL},
	{"SBSET",   0x1300, 0xfff8, DSPInterpreter::sbset, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL},

	// actually, given the masks these should probably be 0x3f. need investigation.
	{"LSL",		0x1400, 0xfec0, DSPInterpreter::lsl, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL},
	{"LSR",		0x1440, 0xfec0, DSPInterpreter::lsr, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL},
	{"ASL",		0x1480, 0xfec0, DSPInterpreter::asl, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL},
	{"ASR",		0x14c0, 0xfec0, DSPInterpreter::asr, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL},

	{"LRI",		0x0080, 0xffe0, DSPInterpreter::lri, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"LR",      0x00c0, 0xffe0, DSPInterpreter::lr, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_MEM, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"SR",      0x00e0, 0xffe0, DSPInterpreter::sr, nop, 2, 2, {{P_MEM, 2, 1, 0, 0xffff}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},

	{"MRR",		0x1c00, 0xfc00, DSPInterpreter::mrr, nop, 1, 2, {{P_REG, 1, 0, 5, 0x03e0}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},

	{"SI",      0x1600, 0xff00, DSPInterpreter::si, nop, 2, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},

	{"LRS",		0x2000, 0xf800, DSPInterpreter::lrs, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_MEM, 1, 0, 0, 0x00ff}}, NULL, NULL},
	{"SRS",		0x2800, 0xf800, DSPInterpreter::srs, nop, 1, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_REG18, 1, 0, 8, 0x0700}}, NULL, NULL},

	{"LRIS",	0x0800, 0xf800, DSPInterpreter::lris, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL},

	{"ADDIS",	0x0400, 0xfe00, DSPInterpreter::addis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL},
	{"CMPIS",	0x0600, 0xfe00, DSPInterpreter::cmpis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL},
	{"ANDI",	0x0240, 0xfeff, DSPInterpreter::andi, nop, 2, 2,  {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"ANDCF",	0x02c0, 0xfeff, DSPInterpreter::andfc, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"XORI",    0x0220, 0xfeff, DSPInterpreter::xori, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"ANDF",	0x02a0, 0xfeff, DSPInterpreter::andf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},

	{"ORI",		0x0260, 0xfeff, DSPInterpreter::ori, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"ORF",		0x02e0, 0xfeff, DSPInterpreter::orf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL}, // Hermes: ??? (has it commented out)

	{"ADDI",    0x0200, 0xfeff, DSPInterpreter::addi, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL}, // F|RES: missing S64
	{"CMPI",    0x0280, 0xfeff, DSPInterpreter::cmpi, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL},

	{"ILRR",    0x0210, 0xfedc, DSPInterpreter::ilrr,  nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL},
	{"ILRRD",	0x0214, 0xfedc, DSPInterpreter::ilrrd, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL}, // Hermes doesn't list this
	{"ILRRI",   0x0218, 0xfedc, DSPInterpreter::ilrri, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL},
	{"ILRRN",   0x0222, 0xfedc, DSPInterpreter::ilrrn, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRR",		0x1800, 0xff80, DSPInterpreter::lrr,  nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL},
	{"LRRD",    0x1880, 0xff80, DSPInterpreter::lrrd, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL},
	{"LRRI",    0x1900, 0xff80, DSPInterpreter::lrri, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL},
	{"LRRN",    0x1980, 0xff80, DSPInterpreter::lrrn, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL},

	{"SRR",		0x1a00, 0xff80, DSPInterpreter::srr,  nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},
	{"SRRD",    0x1a80, 0xff80, DSPInterpreter::srrd, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},
	{"SRRI",    0x1b00, 0xff80, DSPInterpreter::srri, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},
	{"SRRN",    0x1b80, 0xff80, DSPInterpreter::srrn, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},

	// LOOPS
	{"LOOP",    0x0040, 0xffe0, DSPInterpreter::loop,   nop, 1, 1, {{P_REG, 1, 0, 0, 0x001f}}, NULL, NULL},	
	{"BLOOP",   0x0060, 0xffe0, DSPInterpreter::bloop,  nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},
	{"LOOPI",   0x1000, 0xff00, DSPInterpreter::loopi,  nop, 1, 1, {{P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL},
	{"BLOOPI",  0x1100, 0xff00, DSPInterpreter::bloopi, nop, 2, 2, {{P_IMM, 1, 0, 0, 0x00ff}, {P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL},

	{"ADDARN",  0x0010, 0xfff0, DSPInterpreter::addarn, nop, 2, 2, {{P_REG, 1, 0, 0, 0x00c0}, {P_REG, 2, 1, 0, 0x0003}}, NULL, NULL},
	

	// opcodes that can be extended
	// extended opcodes, note size of opcode will be set to 0

	{"NX",      0x8000, 0xf700, DSPInterpreter::nx,     nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"M2",      0x8a00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"M0",      0x8b00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	// These guys probably change the precision or range of some operations.
	// The question is which. 16-bit mode vs 40-bit mode sounds plausible for SET40/SET16.
	// Maybe Set15 makes the dsp drop the top bit from all calculations or something? Or clamp?
	// SET15/CLR15 is commonly used around MULXAC in Zeldas.
	// SET16 is done around complicated loops with many madds etc.
	{"CLR15",   0x8c00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SET15",   0x8d00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SET40",	0x8e00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SET16",	0x8f00, 0xffff, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},


	{"INCM",    0x7400, 0xfeff, DSPInterpreter::incm,   nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"INC",		0x7600, 0xfeff, DSPInterpreter::inc,    nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"DECM",    0x7800, 0xfeff, DSPInterpreter::decm,   nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"DEC",		0x7a00, 0xfeff, DSPInterpreter::dec,    nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"NEG",		0x7c00, 0xfeff, DSPInterpreter::neg,    nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MOVNP",   0x7e00, 0xfeff, DSPInterpreter::movnp,  nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},


	{"TST",		0xb100, 0xf7ff, DSPInterpreter::tst,   nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	// GUESSING NOT SURE AT ALL!!!!
	{"TSTAXL",  0xa100, 0xffff, DSPInterpreter::tstaxl, nop, 1 | P_EXT, 1, {{P_REG1A, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"TSTAXH",  0x8600, 0xfeff, DSPInterpreter::tstaxh, nop, 1 | P_EXT, 1, {{P_REG1A, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"CMP",		0x8200, 0xffff, DSPInterpreter::cmp,    nop, 1 | P_EXT, 0, {}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	
	// This op does NOT exist, at least not under this name, in duddie's docs!
	{"CMPAR" ,  0xc100, 0xe7ff, DSPInterpreter::cmpar, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"CLRL",	0xfc00, 0xffff, DSPInterpreter::clrl, nop, 1 | P_EXT,  1, {{P_ACCL, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi}, // clear acl0
	{"CLR",		0x8100, 0xf7ff, DSPInterpreter::clr,  nop, 1 | P_EXT,  1, {{P_ACC, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi}, // clear acc0
	{"CLRP",	0x8400, 0xffff, DSPInterpreter::clrp, nop, 1 | P_EXT,  0, {}, },

      
	{"MOV",		0x6c00, 0xfeff, DSPInterpreter::mov,   nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MOVAX",   0x6800, 0xfcff, DSPInterpreter::movax, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MOVR",    0x6000, 0xf8ff, DSPInterpreter::movr,  nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MOVP",    0x6e00, 0xfeff, DSPInterpreter::movp,  nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MOVPZ",   0xfe00, 0xfeff, DSPInterpreter::movpz, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"ADDPAXZ", 0xf800, 0xfcff, DSPInterpreter::addpaxz, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 9, 0x0200}, {P_REG1A, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},  //Think the args are wrong
	{"ADDP",    0x4e00, 0xfeff, DSPInterpreter::addp,    nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"LSL16",   0xf000, 0xfeff, DSPInterpreter::lsl16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"LSR16",   0xf400, 0xfeff, DSPInterpreter::lsr16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ASR16",   0x9100, 0xf7ff, DSPInterpreter::asr16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"XORR",    0x3000, 0xfcff, DSPInterpreter::xorr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ANDR",    0x3400, 0xfcff, DSPInterpreter::andr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ORR",		0x3800, 0xfcff, DSPInterpreter::orr,  nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ANDC",    0x3C00, 0xfeff, DSPInterpreter::andc, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi}, // Hermes doesn't list this
	{"ORC",		0x3E00, 0xfeff, DSPInterpreter::orc,  nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi}, // Hermes doesn't list this

	{"MULX",    0xa000, 0xe7ff, DSPInterpreter::mulx,    nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULXMVZ", 0xa200, 0xe6ff, DSPInterpreter::mulxmvz, nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULXAC",  0xa400, 0xe6ff, DSPInterpreter::mulxac,  nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULXMV",  0xa600, 0xe6ff, DSPInterpreter::mulxmv,  nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"MUL",		0x9000, 0xf7ff, DSPInterpreter::mul,    nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULMVZ",  0x9200, 0xf6ff, DSPInterpreter::mulmvz, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULAC",   0x9400, 0xf6ff, DSPInterpreter::mulac,  nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULMV",   0x9600, 0xf6ff, DSPInterpreter::mulmv,  nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"MULC",    0xc000, 0xe7ff, DSPInterpreter::mulc,    nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULCMVZ", 0xc200, 0xe6ff, DSPInterpreter::mulcmvz, nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULCAC",  0xc400, 0xe6ff, DSPInterpreter::mulcac,  nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MULCMV",  0xc600, 0xe6ff, DSPInterpreter::mulcmv,  nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"ADDR",    0x4000, 0xf8ff, DSPInterpreter::addr,   nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ADDAX",   0x4800, 0xfcff, DSPInterpreter::addax,  nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ADD",		0x4c00, 0xfeff, DSPInterpreter::add,    nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"ADDAXL",  0x7000, 0xfcff, DSPInterpreter::addaxl, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"SUBR",    0x5000, 0xf8ff, DSPInterpreter::subr,  nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SUBAX",   0x5800, 0xfcff, DSPInterpreter::subax, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SUB",		0x5c00, 0xfeff, DSPInterpreter::sub,   nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACCM, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"SUBP",    0x5e00, 0xfeff, DSPInterpreter::subp,  nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},

	{"MADD",    0xf200, 0xfeff, DSPInterpreter::madd,  nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MSUB",    0xf600, 0xfeff, DSPInterpreter::msub , nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MADDX",   0xe000, 0xfcff, DSPInterpreter::maddx, nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MSUBX",   0xe400, 0xfcff, DSPInterpreter::msubx, nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MADDC",   0xe800, 0xfcff, DSPInterpreter::maddc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
	{"MSUBC",   0xec00, 0xfcff, DSPInterpreter::msubc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, dsp_op_ext_ops_pro, dsp_op_ext_ops_epi},
};

const DSPOPCTemplate cw = 
	{"CW",		0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, NULL, NULL,};


const DSPOPCTemplate opcodes_ext[] =
{
	{"L",       0x0040, 0x00c4, nop, nop, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LN",      0x0044, 0x00c4, nop, nop, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LS",      0x0080, 0x00ce, nop, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, NULL, NULL,},
	{"LSN",		0x0084, 0x00ce, nop, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, NULL, NULL,},
	{"LSM",		0x0088, 0x00ce, nop, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, NULL, NULL,},
	{"LSNM",    0x008c, 0x00ce, nop, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, NULL, NULL,},
	{"SL",      0x0082, 0x00ce, nop, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, NULL, NULL,},
	{"SLN",		0x0086, 0x00ce, nop, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, NULL, NULL,},
	{"SLM",		0x008a, 0x00ce, nop, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, NULL, NULL,},
	{"SLNM",    0x008e, 0x00ce, nop, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, NULL, NULL,},
	{"S",       0x0020, 0x00e4, nop, nop, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, NULL, NULL,},
	{"SN",      0x0024, 0x00e4, nop, nop, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, NULL, NULL,},
	{"LDX",		0x00c0, 0x00cf, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}}, NULL, NULL,},
	{"LDXN",    0x00c4, 0x00cf, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}}, NULL, NULL,},
	{"LDXM",    0x00c8, 0x00cf, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}}, NULL, NULL,},
	{"LDXNM",   0x00cc, 0x00cf, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0010}, {P_REG1A, 1, 0, 4, 0x0010}, {P_PRG, 1, 0, 5, 0x0020}}, NULL, NULL,},
	{"LD",      0x00c0, 0x00cc, nop, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDN",		0x00c4, 0x00cc, nop, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDM",		0x00c8, 0x00cc, nop, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDNM",    0x00cc, 0x00cc, nop, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"MV",      0x0010, 0x00f0, nop, nop, 1, 2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"DR",      0x0004, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"IR",      0x0008, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"NR",      0x000c, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"XXX",		0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 1, 0, 0, 0x00ff}}, NULL, NULL,},
};

const u32 opcodes_size = sizeof(opcodes) / sizeof(DSPOPCTemplate);
const u32 opcodes_ext_size = sizeof(opcodes_ext) / sizeof(DSPOPCTemplate);

const pdlabel_t pdlabels[] =
{
	{0xffa0, "COEF_A1_0", "COEF_A1_0",},
	{0xffa1, "COEF_A2_0", "COEF_A2_0",},
	{0xffa2, "COEF_A1_1", "COEF_A1_1",},
	{0xffa3, "COEF_A2_1", "COEF_A2_1",},
	{0xffa4, "COEF_A1_2", "COEF_A1_2",},
	{0xffa5, "COEF_A2_2", "COEF_A2_2",},
	{0xffa6, "COEF_A1_3", "COEF_A1_3",},
	{0xffa7, "COEF_A2_3", "COEF_A2_3",},
	{0xffa8, "COEF_A1_4", "COEF_A1_4",},
	{0xffa9, "COEF_A2_4", "COEF_A2_4",},
	{0xffaa, "COEF_A1_5", "COEF_A1_5",},
	{0xffab, "COEF_A2_5", "COEF_A2_5",},
	{0xffac, "COEF_A1_6", "COEF_A1_6",},
	{0xffad, "COEF_A2_6", "COEF_A2_6",},
	{0xffae, "COEF_A1_7", "COEF_A1_7",},
	{0xffaf, "COEF_A2_7", "COEF_A2_7",},
	{0xffc9, "DSCR", "DSP DMA Control Reg",},
	{0xffcb, "DSBL", "DSP DMA Block Length",},
	{0xffcd, "DSPA", "DSP DMA DMEM Address",},
	{0xffce, "DSMAH", "DSP DMA Mem Address H",},
	{0xffcf, "DSMAL", "DSP DMA Mem Address L",},
	{0xffd1, "SampleFormat", "SampleFormat",},

	{0xffd3, "Unk Zelda", "Unk Zelda writes to it",},

	{0xffd4, "ACSAH", "Accelerator start address H",},
	{0xffd5, "ACSAL", "Accelerator start address L",},
	{0xffd6, "ACEAH", "Accelerator end address H",},
	{0xffd7, "ACEAL", "Accelerator end address L",},
	{0xffd8, "ACCAH", "Accelerator current address H",},
	{0xffd9, "ACCAL", "Accelerator current address L",},
	{0xffda, "pred_scale", "pred_scale",},
	{0xffdb, "yn1", "yn1",},
	{0xffdc, "yn2", "yn2",},
	{0xffdd, "ARAM", "Direct Read from ARAM (uses ADPCM)",},
	{0xffde, "GAIN", "Gain",},
	{0xffef, "AMDM", "ARAM DMA Request Mask",},
	{0xfffb, "DIRQ", "DSP IRQ Request",},
	{0xfffc, "DMBH", "DSP Mailbox H",},
	{0xfffd, "DMBL", "DSP Mailbox L",},
	{0xfffe, "CMBH", "CPU Mailbox H",},
	{0xffff, "CMBL", "CPU Mailbox L",},
};

const u32 pdlabels_size = sizeof(pdlabels) / sizeof(pdlabel_t);

const pdlabel_t regnames[] =
{
	{0x00, "AR0",       "Register 00",},
	{0x01, "AR1",       "Register 01",},
	{0x02, "AR2",       "Register 02",},
	{0x03, "AR3",       "Register 03",},
	{0x04, "IX0",       "Register 04",},
	{0x05, "IX1",       "Register 05",},
	{0x06, "IX2",       "Register 06",},
	{0x07, "IX3",       "Register 07",},
	{0x08, "R08",       "Register 08",},
	{0x09, "R09",       "Register 09",},
	{0x0a, "R10",       "Register 10",},
	{0x0b, "R11",       "Register 11",},
	{0x0c, "ST0",       "Call stack",},
	{0x0d, "ST1",       "Data stack",},
	{0x0e, "ST2",       "Loop addr stack",},
	{0x0f, "ST3",       "Loop counter",},
	{0x00, "AC0.H",     "Accu High 0",},
	{0x11, "AC1.H",     "Accu High 1",},
	{0x12, "CR",        "Config Register",},
	{0x13, "SR",        "Special Register",},
	{0x14, "PROD.L",    "Prod L",},
	{0x15, "PROD.M1",   "Prod M1",},
	{0x16, "PROD.H",    "Prod H",},
	{0x17, "PROD.M2",   "Prod M2",},
	{0x18, "AX0.L",		"Extra Accu L 0",},
	{0x19, "AX1.L",		"Extra Accu L 1",},
	{0x1a, "AX0.H",		"Extra Accu H 0",},
	{0x1b, "AX1.H",		"Extra Accu H 1",},
	{0x1c, "AC0.L",		"Register 28",},
	{0x1d, "AC1.L",		"Register 29",},
	{0x1e, "AC0.M",		"Register 00",},
	{0x1f, "AC1.M",		"Register 00",},

	// To resolve special names.
	{0x20, "ACC0",		"Accu Full 0",},
	{0x21, "ACC1",		"Accu Full 1",},
	{0x22, "AX0",		"Extra Accu 0",},
	{0x23, "AX1",		"Extra Accu 1",},
};

u8 opSize[OPTABLE_SIZE];
dspInstFunc opTable[OPTABLE_SIZE];
dspInstFunc prologueTable[OPTABLE_SIZE];
dspInstFunc epilogueTable[OPTABLE_SIZE];


const char* pdname(u16 val)
{
	static char tmpstr[12]; // nasty

	for (int i = 0; i < (int)(sizeof(pdlabels) / sizeof(pdlabel_t)); i++)
	{
		if (pdlabels[i].addr == val)
			return pdlabels[i].name;
	}

	sprintf(tmpstr, "0x%04x", val);
	return tmpstr;
}

const char *pdregname(int val)
{
	return regnames[val].name;
}

const char *pdregnamelong(int val)
{
	return regnames[val].description;
}

const DSPOPCTemplate *GetOpTemplate(const UDSPInstruction &inst)
{
	for (u32 i = 0; i < opcodes_size; i++)
	{
		u16 mask = opcodes[i].opcode_mask;
		if (opcodes[i].size & P_EXT) {
			// Ignore extension bits.
			mask &= 0xFF00;
		}
		if ((mask & inst.hex) == opcodes[i].opcode)
			return &opcodes[i];
	}
	return NULL;
}


// This function could use the above GetOpTemplate, but then we'd lose the
// nice property that it catches colliding op masks.
void InitInstructionTable()
{
	for (int i = 0; i < OPTABLE_SIZE; i++)
	{
		opTable[i] = DSPInterpreter::unknown;
		prologueTable[i] = NULL;
		epilogueTable[i] = NULL;
		opSize[i] = 0;
	}
	
	for (int i = 0; i < OPTABLE_SIZE; i++)
	{
		for (u32 j = 0; j < opcodes_size; j++)
		{
			u16 mask = opcodes[j].opcode_mask;
			if (opcodes[j].size & P_EXT) {
				// Ignore extension bits.
				mask &= 0xFF00;
			}
			if ((mask & i) == opcodes[j].opcode)
			{
				if (opTable[i] == DSPInterpreter::unknown)
				{
					opTable[i] = opcodes[j].interpFunc;
					opSize[i] = opcodes[j].size & 3;
					prologueTable[i] = opcodes[j].prologue;
					epilogueTable[i] = opcodes[j].epilogue;
				}
				else
				{
					ERROR_LOG(DSPLLE, "opcode table place %d already in use for %s", i, opcodes[j].name); 
				}
			}
		}
	}
}	
