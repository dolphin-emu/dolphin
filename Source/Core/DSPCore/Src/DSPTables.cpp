// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"
#include "DSPTables.h"

#include "DSPInterpreter.h"
#include "DSPJit.h"
#include "DSPIntExtOps.h"

void nop(const UDSPInstruction& opc)
{
	// The real nop is 0. Anything else is bad.
	if (opc.hex )
		DSPInterpreter::unknown(opc);
}
 
// Unknown Ops
// All AX games: a100

// TODO: Fill up the tables with the corresponding instructions
const DSPOPCTemplate opcodes[] =
{
	{"NOP",		0x0000, 0xffff, nop, nop, 1, 0, {}, false},

	{"DAR",		0x0004, 0xfffc, DSPInterpreter::dar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false},
	{"IAR",		0x0008, 0xfffc, DSPInterpreter::iar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false},

	{"ADDARN",  0x0010, 0xfff0, DSPInterpreter::addarn, nop, 1, 2, {{P_REG, 1, 0, 0, 0x0003}, {P_REG04, 1, 0, 2, 0x000c}}, false},

	{"HALT",	0x0021, 0xffff, DSPInterpreter::halt, nop, 1, 0, {}, false},

	{"RETGE",	0x02d0, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETL",	0x02d1, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETG",	0x02d2, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETLE",	0x02d3, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETNZ",	0x02d4, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETZ",	0x02d5, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETNC",	0x02d6, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETC",	0x02d7, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETLNZ",	0x02dc, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RETLZ",   0x02dd, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RET",		0x02df, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, false},
	{"RTI",	    0x02ff, 0xffff, DSPInterpreter::rti, nop, 1, 0, {}, false},

	{"CALLGE",	0x02b0, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLL",	0x02b1, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLG",	0x02b2, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLLE",	0x02b3, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLNZ",  0x02b4, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLZ",	0x02b5, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLNC",	0x02b6, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLC",	0x02b7, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLLNZ",	0x02bc, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALLLZ",	0x02bd, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"CALL",    0x02bf, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},

	{"IFGE",	0x0270, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFL",     0x0271, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFG",     0x0272, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFLE",	0x0273, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFNZ",    0x0274, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFZ",     0x0275, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFNC",	0x0276, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFC",		0x0277, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFLNZ",	0x027c, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IFLZ",    0x027d, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false},
	{"IF",		0x027f, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, false}, // This is just nop

	{"JGE",	    0x0290, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JL",	    0x0291, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JG",	    0x0292, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JLE",	    0x0293, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JNZ",	    0x0294, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JZ",	    0x0295, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JNC",	    0x0296, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JC",	    0x0297, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JLNZ",	0x029c, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JLZ",		0x029d, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"JMP",		0x029f, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false},

	{"JRGE",	0x1700, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRL",		0x1701, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRG",		0x1702, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRLE",	0x1703, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRNZ",	0x1704, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRZ",		0x1705, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRNC",	0x1706, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRC",		0x1707, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRLNZ",	0x170c, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JRLZ",	0x170d, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"JMPR",	0x170f, 0xff1f, DSPInterpreter::jmprcc, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},

	{"CALLRGE",	0x1710, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRL",	0x1711, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRG",	0x1712, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRLE",	0x1713, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRNZ",	0x1714, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRZ",	0x1715, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRNC",	0x1716, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRC",	0x1717, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRLNZ",0x171c, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLRLZ",	0x171d, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},
	{"CALLR",	0x171f, 0xff1f, DSPInterpreter::callr, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false},

	{"SBCLR",   0x1200, 0xfff8, DSPInterpreter::sbclr, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, false},
	{"SBSET",   0x1300, 0xfff8, DSPInterpreter::sbset, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, false},

	{"LSL",		0x1400, 0xfec0, DSPInterpreter::lsl, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false},
	{"LSR",		0x1440, 0xfec0, DSPInterpreter::lsr, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false},
	{"ASL",		0x1480, 0xfec0, DSPInterpreter::asl, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false},
	{"ASR",		0x14c0, 0xfec0, DSPInterpreter::asr, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false},
	
	{"LSRN",	0x02ca, 0xffff, DSPInterpreter::lsrn, nop, 1, 0, {}, false}, // discovered by ector!
	{"ASRN",	0x02cb, 0xffff, DSPInterpreter::asrn, nop, 1, 0, {}, false}, // discovered by ector!

	{"LRI",		0x0080, 0xffe0, DSPInterpreter::lri, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"LR",      0x00c0, 0xffe0, DSPInterpreter::lr, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_MEM, 2, 1, 0, 0xffff}}, false},
	{"SR",      0x00e0, 0xffe0, DSPInterpreter::sr, nop, 2, 2, {{P_MEM, 2, 1, 0, 0xffff}, {P_REG, 1, 0, 0, 0x001f}}, false},

	{"MRR",		0x1c00, 0xfc00, DSPInterpreter::mrr, nop, 1, 2, {{P_REG, 1, 0, 5, 0x03e0}, {P_REG, 1, 0, 0, 0x001f}}, false},

	{"SI",      0x1600, 0xff00, DSPInterpreter::si, nop, 2, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_IMM, 2, 1, 0, 0xffff}}, false},

	{"ADDIS",	0x0400, 0xfe00, DSPInterpreter::addis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, false},
	{"CMPIS",	0x0600, 0xfe00, DSPInterpreter::cmpis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, false},
	{"LRIS",	0x0800, 0xf800, DSPInterpreter::lris, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_IMM, 1, 0, 0, 0x00ff}}, false},

	{"ADDI",    0x0200, 0xfeff, DSPInterpreter::addi, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false}, // F|RES: missing S64
	{"XORI",    0x0220, 0xfeff, DSPInterpreter::xori, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"ANDI",	0x0240, 0xfeff, DSPInterpreter::andi, nop, 2, 2,  {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"ORI",		0x0260, 0xfeff, DSPInterpreter::ori, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"CMPI",    0x0280, 0xfeff, DSPInterpreter::cmpi, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},

	{"ANDF",	0x02a0, 0xfeff, DSPInterpreter::andf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"ANDCF",	0x02c0, 0xfeff, DSPInterpreter::andcf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false},
	{"ORF",		0x02e0, 0xfeff, DSPInterpreter::orf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false}, // Hermes: ??? (has it commented out)

	{"ILRR",    0x0210, 0xfefc, DSPInterpreter::ilrr,  nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"ILRRD",	0x0214, 0xfefc, DSPInterpreter::ilrrd, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false}, // Hermes doesn't list this
	{"ILRRI",   0x0218, 0xfefc, DSPInterpreter::ilrri, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"ILRRN",   0x021c, 0xfefc, DSPInterpreter::ilrrn, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRR",		0x1800, 0xff80, DSPInterpreter::lrr,  nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false},
	{"LRRD",    0x1880, 0xff80, DSPInterpreter::lrrd, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false},
	{"LRRI",    0x1900, 0xff80, DSPInterpreter::lrri, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false},
	{"LRRN",    0x1980, 0xff80, DSPInterpreter::lrrn, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false},

	{"SRR",		0x1a00, 0xff80, DSPInterpreter::srr,  nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false},
	{"SRRD",    0x1a80, 0xff80, DSPInterpreter::srrd, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false},
	{"SRRI",    0x1b00, 0xff80, DSPInterpreter::srri, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false},
	{"SRRN",    0x1b80, 0xff80, DSPInterpreter::srrn, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false},

	// LOOPS
	{"LOOP",    0x0040, 0xffe0, DSPInterpreter::loop,   nop, 1, 1, {{P_REG, 1, 0, 0, 0x001f}}, false},	
	{"BLOOP",   0x0060, 0xffe0, DSPInterpreter::bloop,  nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_ADDR_I, 2, 1, 0, 0xffff}}, false},
	{"LOOPI",   0x1000, 0xff00, DSPInterpreter::loopi,  nop, 1, 1, {{P_IMM, 1, 0, 0, 0x00ff}}, false},
	{"BLOOPI",  0x1100, 0xff00, DSPInterpreter::bloopi, nop, 2, 2, {{P_IMM, 1, 0, 0, 0x00ff}, {P_ADDR_I, 2, 1, 0, 0xffff}}, false},

	//2
	{"LRS",		0x2000, 0xf800, DSPInterpreter::lrs, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_MEM, 1, 0, 0, 0x00ff}}, false},
	{"SRS",		0x2800, 0xf800, DSPInterpreter::srs, nop, 1, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_REG18, 1, 0, 8, 0x0700}}, false},

// opcodes that can be extended
// extended opcodes, note size of opcode will be set to 0

	//3 - main opcode defined by 9 bits, extension defined by last 7 bits!!
	{"XORR",    0x3000, 0xfc80, DSPInterpreter::xorr,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true},
	{"ANDR",    0x3400, 0xfc80, DSPInterpreter::andr,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true},
	{"ORR",		0x3800, 0xfc80, DSPInterpreter::orr,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true},
	{"ANDC",    0x3c00, 0xfe80, DSPInterpreter::andc,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true},
	{"ORC",     0x3e00, 0xfe80, DSPInterpreter::orc,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true},
	{"XORC",	0x3080, 0xfe80, DSPInterpreter::xorc,	nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true}, 
	{"NOT",		0x3280, 0xfe80, DSPInterpreter::notc,	nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true},
	{"LSRNRX",	0x3480, 0xfc80, DSPInterpreter::lsrnrx,	nop, 1 | P_EXT, 2, {{P_ACC,  1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true}, 
	{"ASRNRX",	0x3880, 0xfc80, DSPInterpreter::asrnrx,	nop, 1 | P_EXT, 2, {{P_ACC,  1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true}, 
	{"LSRNR",	0x3c80, 0xfe80, DSPInterpreter::lsrnr,	nop, 1 | P_EXT, 2, {{P_ACC,  1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true}, 
	{"ASRNR",	0x3e80, 0xfe80, DSPInterpreter::asrnr,	nop, 1 | P_EXT, 2, {{P_ACC,  1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true}, 

	//4
	{"ADDR",    0x4000, 0xf800, DSPInterpreter::addr,	nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true},
	{"ADDAX",   0x4800, 0xfc00, DSPInterpreter::addax,	nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, true},
	{"ADD",		0x4c00, 0xfe00, DSPInterpreter::add,	nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true},
	{"ADDP",    0x4e00, 0xfe00, DSPInterpreter::addp,	nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},

	//5
	{"SUBR",    0x5000, 0xf800, DSPInterpreter::subr,  nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true},
	{"SUBAX",   0x5800, 0xfc00, DSPInterpreter::subax, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, true},
	{"SUB",		0x5c00, 0xfe00, DSPInterpreter::sub,   nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true},
	{"SUBP",    0x5e00, 0xfe00, DSPInterpreter::subp,  nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},

	//6
	{"MOVR",    0x6000, 0xf800, DSPInterpreter::movr,  nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true},
	{"MOVAX",   0x6800, 0xfc00, DSPInterpreter::movax, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, true},
	{"MOV",		0x6c00, 0xfe00, DSPInterpreter::mov,   nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true},
	{"MOVP",    0x6e00, 0xfe00, DSPInterpreter::movp,  nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},

	//7
	{"ADDAXL",  0x7000, 0xfc00, DSPInterpreter::addaxl, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, true},
	{"INCM",    0x7400, 0xfe00, DSPInterpreter::incm,   nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true},
	{"INC",		0x7600, 0xfe00, DSPInterpreter::inc,    nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
	{"DECM",    0x7800, 0xfe00, DSPInterpreter::decm,   nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true},
	{"DEC",		0x7a00, 0xfe00, DSPInterpreter::dec,    nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
	{"NEG",		0x7c00, 0xfe00, DSPInterpreter::neg,    nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MOVNP",   0x7e00, 0xfe00, DSPInterpreter::movnp,  nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},

	//8
	{"NX",      0x8000, 0xf700, DSPInterpreter::nx,     nop, 1 | P_EXT, 0, {}, true},
	{"CLR",		0x8100, 0xf700, DSPInterpreter::clr,  nop, 1 | P_EXT,  1, {{P_ACC, 1, 0, 11, 0x0800}}, true}, // clear acc0
	{"CMP",		0x8200, 0xff00, DSPInterpreter::cmp,    nop, 1 | P_EXT, 0, {}, true},
	//0x8300 - unknown - not used atm - could be cmp(acc1-acc0)
	{"CLRP",	0x8400, 0xff00, DSPInterpreter::clrp, nop, 1 | P_EXT,  0, {}, true},
	//0x8500 - unknown mul opcode (modifies prod regs) - not used atm
	{"TSTAXH",  0x8600, 0xfe00, DSPInterpreter::tstaxh, nop, 1 | P_EXT, 1, {{P_REG1A, 1, 0, 8, 0x0100}}, true},
	{"M2",      0x8a00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},
	{"M0",      0x8b00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},
	{"CLR15",   0x8c00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},
	{"SET15",   0x8d00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},
	{"SET16",	0x8e00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},
	{"SET40",	0x8f00, 0xff00, DSPInterpreter::srbith, nop, 1 | P_EXT, 0, {}, true},

	//9
	{"MUL",		0x9000, 0xf700, DSPInterpreter::mul,    nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}}, true},
	{"ASR16",   0x9100, 0xf700, DSPInterpreter::asr16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, true},
	{"MULMVZ",  0x9200, 0xf600, DSPInterpreter::mulmvz, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULAC",   0x9400, 0xf600, DSPInterpreter::mulac,  nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULMV",   0x9600, 0xf600, DSPInterpreter::mulmv,  nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	
	//a-b (!!!retest TST/TSTAXL!!!)
	{"MULX",    0xa000, 0xe700, DSPInterpreter::mulx,    nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}}, true},
	{"TST",		0xa100, 0xe700, DSPInterpreter::tst,   nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, true},
	//{"TSTAXL",  0xa100, 0xff00, DSPInterpreter::tstaxl, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, true}, //Definitely not TSTAXL, it affects one of the accumulators
	//{"TST",		0xb100, 0xf700, DSPInterpreter::tst,   nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 11, 0x0800}}, true},
	{"MULXMVZ", 0xa200, 0xe600, DSPInterpreter::mulxmvz, nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULXAC",  0xa400, 0xe600, DSPInterpreter::mulxac,  nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULXMV",  0xa600, 0xe600, DSPInterpreter::mulxmv,  nop, 1 | P_EXT, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},

	//c-d
	{"MULC",    0xc000, 0xe700, DSPInterpreter::mulc,    nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, true},
	{"CMPAR" ,  0xc100, 0xe700, DSPInterpreter::cmpar, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, true}, //MIA in duddie dox
	{"MULCMVZ", 0xc200, 0xe600, DSPInterpreter::mulcmvz, nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULCAC",  0xc400, 0xe600, DSPInterpreter::mulcac,  nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MULCMV",  0xc600, 0xe600, DSPInterpreter::mulcmv,  nop, 1 | P_EXT, 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true},

	//e
	{"MADDX",   0xe000, 0xfc00, DSPInterpreter::maddx, nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, true},
	{"MSUBX",   0xe400, 0xfc00, DSPInterpreter::msubx, nop, 1 | P_EXT, 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, true},
	{"MADDC",   0xe800, 0xfc00, DSPInterpreter::maddc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, true},
	{"MSUBC",   0xec00, 0xfc00, DSPInterpreter::msubc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, true},

	//f
	{"LSL16",   0xf000, 0xfe00, DSPInterpreter::lsl16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MADD",    0xf200, 0xfe00, DSPInterpreter::madd,  nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, true},
	{"LSR16",   0xf400, 0xfe00, DSPInterpreter::lsr16, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
	{"MSUB",    0xf600, 0xfe00, DSPInterpreter::msub , nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, true},
	{"ADDPAXZ", 0xf800, 0xfc00, DSPInterpreter::addpaxz, nop, 1 | P_EXT, 2, {{P_ACC, 1, 0, 9, 0x0200}, {P_REG1A, 1, 0, 8, 0x0100}}, true},  //Think the args are wrong
	{"CLRL",	0xfc00, 0xfe00, DSPInterpreter::clrl, nop, 1 | P_EXT,  1, {{P_ACCL, 1, 0, 11, 0x0800}}, true}, // clear acl0
	{"MOVPZ",   0xfe00, 0xfe00, DSPInterpreter::movpz, nop, 1 | P_EXT, 1, {{P_ACC, 1, 0, 8, 0x0100}}, true},
};

const DSPOPCTemplate cw = 
	{"CW",		0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, false};


const DSPOPCTemplate opcodes_ext[] =
{
	{"XXX",		0x0000, 0x00fc, DSPInterpreter::Ext::nop, nop, 1, 1, {{P_VAL, 1, 0, 0, 0x00ff}}, false},
	
	{"DR",      0x0004, 0x00fc, DSPInterpreter::Ext::dr, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false},
	{"IR",      0x0008, 0x00fc, DSPInterpreter::Ext::ir, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false},
	{"NR",      0x000c, 0x00fc, DSPInterpreter::Ext::nr, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false},
	{"MV",      0x0010, 0x00f0, DSPInterpreter::Ext::mv, nop, 1, 2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}}, false},

	{"S",       0x0020, 0x00e4, DSPInterpreter::Ext::s, nop, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false},
	{"SN",      0x0024, 0x00e4, DSPInterpreter::Ext::sn, nop, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false},

	{"L",       0x0040, 0x00c4, DSPInterpreter::Ext::l, nop, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"LN",      0x0044, 0x00c4, DSPInterpreter::Ext::ln, nop, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false},

	{"LS",      0x0080, 0x00ce, DSPInterpreter::Ext::ls, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false},
	{"SL",      0x0082, 0x00ce, DSPInterpreter::Ext::sl, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false},
	{"LSN",		0x0084, 0x00ce, DSPInterpreter::Ext::lsn, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false},
	{"SLN",		0x0086, 0x00ce, DSPInterpreter::Ext::sln, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false},
	{"LSM",		0x0088, 0x00ce, DSPInterpreter::Ext::lsm, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false},
	{"SLM",		0x008a, 0x00ce, DSPInterpreter::Ext::slm, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false},
	{"LSNM",    0x008c, 0x00ce, DSPInterpreter::Ext::lsnm, nop, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false},
	{"SLNM",    0x008e, 0x00ce, DSPInterpreter::Ext::slnm, nop, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false},

	{"LD",      0x00c0, 0x00cc, DSPInterpreter::Ext::ld, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"LDN",		0x00c4, 0x00cc, DSPInterpreter::Ext::ldn, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"LDM",		0x00c8, 0x00cc, DSPInterpreter::Ext::ldm, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false},
	{"LDNM",    0x00cc, 0x00cc, DSPInterpreter::Ext::ldnm, nop, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false},
};

const int opcodes_size = sizeof(opcodes) / sizeof(DSPOPCTemplate);
const int opcodes_ext_size = sizeof(opcodes_ext) / sizeof(DSPOPCTemplate);

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

	{0xffb0, "0xffb0", 0,},
	{0xffb1, "0xffb1", 0,},
	{0xffb2, "0xffb2", 0,},
	{0xffb3, "0xffb3", 0,},
	{0xffb4, "0xffb4", 0,},
	{0xffb5, "0xffb5", 0,},
	{0xffb6, "0xffb6", 0,},
	{0xffb7, "0xffb7", 0,},
	{0xffb8, "0xffb8", 0,},
	{0xffb9, "0xffb9", 0,},
	{0xffba, "0xffba", 0,},
	{0xffbb, "0xffbb", 0,},
	{0xffbc, "0xffbc", 0,},
	{0xffbd, "0xffbd", 0,},
	{0xffbe, "0xffbe", 0,},
	{0xffbf, "0xffbf", 0,},

	{0xffc0, "0xffc0", 0,},
	{0xffc1, "0xffc1", 0,},
	{0xffc2, "0xffc2", 0,},
	{0xffc3, "0xffc3", 0,},
	{0xffc4, "0xffc4", 0,},
	{0xffc5, "0xffc5", 0,},
	{0xffc6, "0xffc6", 0,},
	{0xffc7, "0xffc7", 0,},
	{0xffc8, "0xffc8", 0,},
	{0xffc9, "DSCR", "DSP DMA Control Reg",},
	{0xffca, "0xffca", 0,},
	{0xffcb, "DSBL", "DSP DMA Block Length",},
	{0xffcc, "0xffcc", 0,},
	{0xffcd, "DSPA", "DSP DMA DMEM Address",},
	{0xffce, "DSMAH", "DSP DMA Mem Address H",},
	{0xffcf, "DSMAL", "DSP DMA Mem Address L",},

	{0xffd0, "0xffd0",0,},
	{0xffd1, "SampleFormat", "SampleFormat",},
	{0xffd2, "0xffd2",0,},
	{0xffd3, "UnkZelda", "Unk Zelda reads/writes from/to it",},
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
	{0xffdf, "0xffdf", 0,},

	{0xffe0, "0xffe0",0,},
	{0xffe1, "0xffe1",0,},
	{0xffe2, "0xffe2",0,},
	{0xffe3, "0xffe3",0,},
	{0xffe4, "0xffe4",0,},
	{0xffe5, "0xffe5",0,},
	{0xffe6, "0xffe6",0,},
	{0xffe7, "0xffe7",0,},
	{0xffe8, "0xffe8",0,},
	{0xffe9, "0xffe9",0,},
	{0xffea, "0xffea",0,},
	{0xffeb, "0xffeb",0,},
	{0xffec, "0xffec",0,},
	{0xffed, "0xffed",0,},
	{0xffee, "0xffee",0,},
	{0xffef, "AMDM", "ARAM DMA Request Mask",},

	{0xfff0, "0xfff0",0,},
	{0xfff1, "0xfff1",0,},
	{0xfff2, "0xfff2",0,},
	{0xfff3, "0xfff3",0,},
	{0xfff4, "0xfff4",0,},
	{0xfff5, "0xfff5",0,},
	{0xfff6, "0xfff6",0,},
	{0xfff7, "0xfff7",0,},
	{0xfff8, "0xfff8",0,},
	{0xfff9, "0xfff9",0,},
	{0xfffa, "0xfffa",0,},
	{0xfffb, "DIRQ", "DSP IRQ Request",},
	{0xfffc, "DMBH", "DSP Mailbox H",},
	{0xfffd, "DMBL", "DSP Mailbox L",},
	{0xfffe, "CMBH", "CPU Mailbox H",},
	{0xffff, "CMBL", "CPU Mailbox L",},
};

const u32 pdlabels_size = sizeof(pdlabels) / sizeof(pdlabel_t);

const pdlabel_t regnames[] =
{
	{0x00, "AR0",       "Addr Reg 00",},
	{0x01, "AR1",       "Addr Reg 01",},
	{0x02, "AR2",       "Addr Reg 02",},
	{0x03, "AR3",       "Addr Reg 03",},
	{0x04, "IX0",       "Index Reg 0",},
	{0x05, "IX1",       "Index Reg 1",},
	{0x06, "IX2",       "Index Reg 2",},
	{0x07, "IX3",       "Index Reg 3",},
	{0x08, "WR0",       "Wrapping Register 0",},
	{0x09, "WR1",       "Wrapping Register 1",},
	{0x0a, "WR2",       "Wrapping Register 2",},
	{0x0b, "WR3",       "Wrapping Register 3",},
	{0x0c, "ST0",       "Call stack",},
	{0x0d, "ST1",       "Data stack",},
	{0x0e, "ST2",       "Loop addr stack",},
	{0x0f, "ST3",       "Loop counter",},
	{0x10, "AC0.H",     "Accu High 0",},
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
	{0x1c, "AC0.L",		"Accu Low 0",},
	{0x1d, "AC1.L",		"Accu Low 1",},
	{0x1e, "AC0.M",		"Accu Mid 0",},
	{0x1f, "AC1.M",		"Accu Mid 1",},

	// To resolve combined register names.
	{0x20, "ACC0",		"Accu Full 0",},
	{0x21, "ACC1",		"Accu Full 1",},
	{0x22, "AX0",		"Extra Accu 0",},
	{0x23, "AX1",		"Extra Accu 1",},
};

u8 opSize[OPTABLE_SIZE];
dspInstFunc opTable[OPTABLE_SIZE];
dspInstFunc extOpTable[EXT_OPTABLE_SIZE];
bool opTableUseExt[OPTABLE_SIZE];
u16 writeBackLog[WRITEBACKLOGSIZE];
int writeBackLogIdx[WRITEBACKLOGSIZE];

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
	for (int i = 0; i < opcodes_size; i++)
	{
		u16 mask = opcodes[i].opcode_mask;
		if ((mask & inst.hex) == opcodes[i].opcode)
			return &opcodes[i];
	}
	return NULL;
}


// This function could use the above GetOpTemplate, but then we'd lose the
// nice property that it catches colliding op masks.
void InitInstructionTable()
{
	// ext op table
	for (int i = 0; i < EXT_OPTABLE_SIZE; i++) 
		extOpTable[i] = DSPInterpreter::unknown;

	for (int i = 0; i < EXT_OPTABLE_SIZE; i++)  
    {
		for (int j = 0; j < opcodes_ext_size; j++)
		{
			u16 mask = opcodes_ext[j].opcode_mask;
			if ((mask & i) == opcodes_ext[j].opcode) 
			{
				if (extOpTable[i] == DSPInterpreter::unknown) 
					extOpTable[i] = opcodes_ext[j].interpFunc;
				else
					ERROR_LOG(DSPLLE, "opcode ext table place %d already in use for %s", i, opcodes_ext[j].name); 	
			}
		}   
	}

	// op table
	for (int i = 0; i < OPTABLE_SIZE; i++)
	{
		opTable[i] = DSPInterpreter::unknown;
		opTableUseExt[i] = false;
		opSize[i] = 0;
	}
	
	for (int i = 0; i < OPTABLE_SIZE; i++)
	{
		for (int j = 0; j < opcodes_size; j++)
		{
			u16 mask = opcodes[j].opcode_mask;
			if ((mask & i) == opcodes[j].opcode)
			{
				if (opTable[i] == DSPInterpreter::unknown)
				{
					opTable[i] = opcodes[j].interpFunc;
					opSize[i] = opcodes[j].size & 3;
					opTableUseExt[i] = opcodes[j].extended;
				}
				else
				{
					ERROR_LOG(DSPLLE, "opcode table place %d already in use for %s", i, opcodes[j].name); 
				}
			}
		}
	}

	for (int i=0; i < WRITEBACKLOGSIZE; i++)
		writeBackLogIdx[i] = -1;
}	
