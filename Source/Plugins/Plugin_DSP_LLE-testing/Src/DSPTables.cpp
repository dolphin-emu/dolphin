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

#include "Globals.h"
#include "DSPTables.h"

#include "DSPInterpreter.h"
#include "DSPJit.h"

void nop(const UDSPInstruction&) {}

// TODO(XK): Fill up the tables with the corresponding instructions
DSPOPCTemplate opcodes[] =
{
	{"NOP",	    0x0000, 0xffff, nop, nop, 1, 0, {}, NULL, NULL,},
	{"HALT",    0x0021, 0xffff, DSPInterpreter::halt, nop, 1, 0, {}, NULL, NULL,},
	{"RET",	    0x02df, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL,},
	{"RETEQ",   0x02d5, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL,},
	{"RETNZ",   0x02dd, 0xffff, DSPInterpreter::ret, nop, 1, 0, {}, NULL, NULL,},
	{"RTI",	    0x02ff, 0xffff, DSPInterpreter::rti, nop, 1, 0, {}, NULL, NULL,},
	{"CALL",    0x02bf, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"CALLNE",  0x02b4, 0xffff, DSPInterpreter::call, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"IF_0",    0x0270, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_1",    0x0271, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_2",    0x0272, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_3",    0x0273, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_E",    0x0274, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_Q",    0x0275, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_R",    0x027c, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_Z",    0x027d, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},
	{"IF_P",    0x027f, 0xffff, DSPInterpreter::ifcc, nop, 1, 0, {}, NULL, NULL,},

	{"JX0",	    0x0290, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JX1",	    0x0291, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JX2",	    0x0292, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JX3",	    0x0293, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JNE",	    0x0294, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JEQ",	    0x0295, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JZR",		0x029c, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JNZ",		0x029d, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"JMP",		0x029f, 0xffff, DSPInterpreter::jcc, nop, 2, 1, {{P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"DAR",		0x0004, 0xfffc, DSPInterpreter::dar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"IAR",		0x0008, 0xfffc, DSPInterpreter::iar, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},

	{"CALLR",   0x171f, 0xff1f, nop, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL,},
	{"JMPR",    0x170f, 0xff1f, nop, nop, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, NULL, NULL,},

	{"SBCLR",   0x1200, 0xfff8, DSPInterpreter::sbclr, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL,},
	{"SBSET",   0x1300, 0xfff8, DSPInterpreter::sbset, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL,},

	{"LSL",		0x1400, 0xfec0, nop, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL,},
	{"LSR",		0x1440, 0xfec0, nop, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, NULL, NULL,},
	{"ASL",		0x1480, 0xfec0, nop, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x007f}}, NULL, NULL,},
	{"ASR",		0x14c0, 0xfec0, nop, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x007f}}, NULL, NULL,},


	{"LRI",		0x0080, 0xffe0, DSPInterpreter::lri, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"LR",      0x00c0, 0xffe0, DSPInterpreter::lr, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_MEM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"SR",      0x00e0, 0xffe0, DSPInterpreter::sr, nop, 2, 2, {{P_MEM, 2, 1, 0, 0xffff}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},

	{"MRR",		0x1c00, 0xfc00, DSPInterpreter::mrr, nop, 1, 2, {{P_REG, 1, 0, 5, 0x03e0}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},

	{"SI",      0x1600, 0xff00, DSPInterpreter::si, nop, 2, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"LRS",		0x2000, 0xf800, DSPInterpreter::lrs, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_MEM, 1, 0, 0, 0x00ff}}, NULL, NULL,},
	{"SRS",		0x2800, 0xf800, DSPInterpreter::srs, nop, 1, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_REG18, 1, 0, 8, 0x0700}}, NULL, NULL,},

	{"LRIS",    0x0800, 0xf800, DSPInterpreter::lris, nop, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL,},

	{"ADDIS",   0x0400, 0xfe00, DSPInterpreter::addis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL,},
	{"CMPIS",   0x0600, 0xfe00, DSPInterpreter::cmpis, nop, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL,},

	{"ANDI",    0x0240, 0xfeff, DSPInterpreter::andi, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"ANDF",    0x02c0, 0xfeff, DSPInterpreter::andf, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"XORI",    0x0220, 0xfeff, DSPInterpreter::xori, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"ANDCF",   0x02a0, 0xfeff, nop, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"ORI",		0x0260, 0xfeff, DSPInterpreter::ori, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"ORF",		0x02e0, 0xfeff, nop, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, NULL, NULL,},

	{"ADDI",    0x0200, 0xfeff, DSPInterpreter::addi, nop, 2, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},}, // missing S64
	{"CMPI",    0x0280, 0xfeff, nop, nop, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}},}, // missing S64

	{"ILRR",    0x0210, 0xfedc, DSPInterpreter::ilrr, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"ILRRD",	0x0214, 0xfedc, DSPInterpreter::ilrr, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"ILRRI",   0x0218, 0xfedc, DSPInterpreter::ilrr, nop, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRRI",    0x1900, 0xff80, DSPInterpreter::lrr, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL,},
	{"LRRD",    0x1880, 0xff80, DSPInterpreter::lrr, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL,},
	{"LRRN",    0x1980, 0xff80, DSPInterpreter::lrr, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL,},
	{"LRR",		0x1800, 0xff80, DSPInterpreter::lrr, nop, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, NULL, NULL,},
	{"SRRI",    0x1b00, 0xff80, DSPInterpreter::srr, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},
	{"SRRD",    0x1a80, 0xff80, DSPInterpreter::srr, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},
	{"SRRN",    0x1b80, 0xff80, DSPInterpreter::srr, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},
	{"SRR",		0x1a00, 0xff80, DSPInterpreter::srr, nop, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},

	{"LOOPI",   0x1000, 0xff00, DSPInterpreter::loopi, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x00ff}}, NULL, NULL,},
	{"BLOOPI",  0x1100, 0xff00, DSPInterpreter::bloopi, nop, 2, 2, {{P_IMM, 1, 0, 0, 0x00ff}, {P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},
	{"LOOP",    0x0040, 0xffe0, DSPInterpreter::loop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x001f}}, NULL, NULL,},
	{"BLOOP",   0x0060, 0xffe0, DSPInterpreter::bloop, nop, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_VAL, 2, 1, 0, 0xffff}}, NULL, NULL,},



	// opcodes that can be extended
	// extended opcodes, note size of opcode will be set to 0

	{"NX",      0x8000, 0xffff, DSPInterpreter::nx, nop, 1 | P_EXT, 0, {}, NULL, NULL,},

	{"S40",		0x8e00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	{"S16",		0x8f00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	{"M2",      0x8a00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	{"M0",      0x8b00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	{"CLR15",   0x8c00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	{"SET15",   0x8d00, 0xffff, nop, nop, 1 | P_EXT, 0, {}, NULL, NULL,},

	{"DECM",    0x7800, 0xfeff, DSPInterpreter::decm, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"INCM",    0x7400, 0xfeff, DSPInterpreter::incm, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"DEC",		0x7a00, 0xfeff, DSPInterpreter::dec, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"INC",		0x7600, 0xfeff, DSPInterpreter::inc, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"NEG",		0x7c00, 0xfeff, DSPInterpreter::neg, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"TST",		0xb100, 0xf7ff, nop, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}}, NULL, NULL,},
	{"TSTAXH",  0x8600, 0xfeff, DSPInterpreter::tstaxh, nop, 1 | P_EXT, 1, {{P_REG1A, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"CMP",		0x8200, 0xffff, DSPInterpreter::cmp, nop, 1 | P_EXT, 0, {}, NULL, NULL,},
	
	{"CMPAXH",  0xc100, 0xe7ff, nop, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, NULL, NULL,},

	{"CLR",		0x8100, 0xf7ff, DSPInterpreter::clr, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}}, NULL, NULL,},
	{"CLRP",    0x8400, 0xffff, DSPInterpreter::clrp, nop, 1 | P_EXT, 0, {}, NULL, NULL,},

	{"MOV",		0x6c00, 0xfeff, nop, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MOVAX",   0x6800, 0xfcff, DSPInterpreter::movax, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"MOVR",    0x6000, 0xf8ff, DSPInterpreter::movr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, NULL, NULL,},
	{"MOVP",    0x6e00, 0xfeff,  DSPInterpreter::movp, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MOVPZ",   0xfe00, 0xfeff, DSPInterpreter::movpz, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"ADDPAXZ", 0xf800, 0xfcff, DSPInterpreter::addpaxz, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG1A, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"ADDP",    0x4e00, 0xfeff, DSPInterpreter::addp, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"LSL16",   0xf000, 0xfeff, DSPInterpreter::lsl16, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"LSR16",   0xf400, 0xfeff, DSPInterpreter::lsr16, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"ASR16",   0x9100, 0xf7ff, DSPInterpreter::asr16, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 11, 0x0800}}, NULL, NULL,},

	{"XORR",    0x3000, 0xfcff, DSPInterpreter::xorr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"ANDR",    0x3400, 0xfcff, DSPInterpreter::andr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"ORR",		0x3800, 0xfcff, DSPInterpreter::orr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"ANDC",    0x3C00, 0xfeff, DSPInterpreter::andc, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"ORC",		0x3E00, 0xfeff, nop, nop, 1 | P_EXT, 1, {{P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"MULX",    0xa000, 0xe7ff, DSPInterpreter::mulx, nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}}, NULL, NULL,},
	{"MULXAC",  0xa400, 0xe6ff, DSPInterpreter::mulxac, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULXMV",  0xa600, 0xe6ff, DSPInterpreter::mulxmv, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULXMVZ", 0xa200, 0xe6ff, DSPInterpreter::mulxmvz, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"MUL",		0x9000, 0xf7ff, DSPInterpreter::mul, nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}}, NULL, NULL,},
	{"MULAC",   0x9400, 0xf6ff, DSPInterpreter::mulac, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULMV",   0x9600, 0xf6ff, DSPInterpreter::mulmv, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULMVZ",  0x9200, 0xf6ff, DSPInterpreter::mulmvz, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"MULC",    0xc000, 0xe7ff, DSPInterpreter::mulc, nop, 1 | P_EXT, 2, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}}, NULL, NULL,},
	{"MULCAC",  0xc400, 0xe6ff, DSPInterpreter::mulcac, nop, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULCMV",  0xc600, 0xe6ff, DSPInterpreter::mulcmv, nop, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MULCMVZ", 0xc200, 0xe6ff, DSPInterpreter::mulcmvz, nop, 1 | P_EXT, 3, {{P_REG1A, 1, 0, 11, 0x0800}, {P_ACCM, 1, 0, 12, 0x1000}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"ADDR",    0x4000, 0xf8ff, DSPInterpreter::addr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, NULL, NULL,},
	{"ADDAX",   0x4800, 0xfcff, DSPInterpreter::addax, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"ADD",		0x4c00, 0xfeff, DSPInterpreter::add, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"ADDAXL",  0x7000, 0xfcff, DSPInterpreter::addaxl, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, NULL, NULL,},

	{"SUBR",    0x5000, 0xf8ff, DSPInterpreter::subr, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, NULL, NULL,},
	{"SUBAX",   0x5800, 0xfcff, DSPInterpreter::subax, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, NULL, NULL,},
	{"SUB",		0x5c00, 0xfeff, DSPInterpreter::sub, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_ACCM_D, 1, 0, 8, 0x0100}}, NULL, NULL,},

	{"MADD",    0xf200, 0xfeff, DSPInterpreter::madd, nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MSUB",    0xf600, 0xfeff, nop , nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, NULL, NULL,},
	{"MADDX",   0xe000, 0xfcff, DSPInterpreter::maddx, nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, NULL, NULL,},
	{"MSUBX",   0xe400, 0xfcff, DSPInterpreter::msubx, nop, 1 | P_EXT, 2, {{P_REG18, 1, 0, 8, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, NULL, NULL,},
	{"MADDC",   0xe800, 0xfcff, DSPInterpreter::maddc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, NULL, NULL,},
	{"MSUBC",   0xec00, 0xfcff, DSPInterpreter::msubc, nop, 1 | P_EXT, 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, NULL, NULL,},

	// FIXME: nakee guessing (check masks and params!)
	{"SHIFTI?",	0x1400, 0xfec0, DSPInterpreter::shifti, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL,},

	{"JMPA?",	0x1700, 0xff1f, DSPInterpreter::jmpa, nop, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, NULL, NULL,},

	{"TSTA?",	0xa100, 0xe7ff, DSPInterpreter::tsta, nop, 1 | P_EXT, 3, {{P_REG18, 1, 0, 11, 0x1000}, {P_REG19, 1, 0, 10, 0x0800}, {P_ACCM, 1, 0, 8, 0x0100}}, NULL, NULL,},
	
	// assemble CW
	//{"CW",      0x0000, 0xffff, nop, nop, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, NULL, NULL,},
	// unknown opcode for disassemble
	//  	{"CW",      0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, NULL, NULL,},
};

DSPOPCTemplate opcodes_ext[] =
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
	{"LD",      0x00c0, 0x00cc, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDN",		0x00c4, 0x00cc, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDM",		0x00c8, 0x00cc, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"LDNM",    0x00cc, 0x00cc, nop, nop, 1, 3, {{P_REG18, 1, 0, 4, 0x0020}, {P_REG19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"MV",      0x0010, 0x00f0, nop, nop, 1, 2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"DR",      0x0004, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"IR",      0x0008, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"NR",      0x000c, 0x00fc, nop, nop, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, NULL, NULL,},
	{"XXX",		0x0000, 0x0000, nop, nop, 1, 1, {{P_VAL, 1, 0, 0, 0x00ff}}, NULL, NULL,},
};

const u32 opcodes_size = sizeof(opcodes) / sizeof(DSPOPCTemplate);
const u32 opcodes_ext_size = sizeof(opcodes_ext) / sizeof(DSPOPCTemplate);

dspInstFunc opTable[OPTABLE_SIZE];
dspInstFunc prologueTable[OPTABLE_SIZE];
dspInstFunc epilogueTable[OPTABLE_SIZE];

void InitInstructionTable()
{
	for(u32 i = 0; i < OPTABLE_SIZE; i++) {
		opTable[i] = DSPInterpreter::unknown;
		prologueTable[i] = NULL;
		epilogueTable[i] = NULL;
	}
	
	for(u32 i = 0; i < OPTABLE_SIZE; i++) {
		for(u32 j = 0; j < opcodes_size; j++) 
			if((opcodes[j].opcode_mask & i) == opcodes[j].opcode) {
				if (opTable[i] == DSPInterpreter::unknown) {
					opTable[i] = opcodes[j].interpFunc;
					prologueTable[i] = opcodes[j].prologue;
					epilogueTable[i] = opcodes[j].epilogue;
				} else {
					ERROR_LOG(DSPHLE, "opcode table place %d already in use for %s", i, opcodes[j].name); 
				}
			}
	}
}	

void ComputeInstruction(const UDSPInstruction& inst)
{
	if(prologueTable[inst.hex])
		prologueTable[inst.hex](inst);

	opTable[inst.hex](inst);
        
	if(epilogueTable[inst.hex])
		epilogueTable[inst.hex](inst);
}
