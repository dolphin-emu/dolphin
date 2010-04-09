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
#include "DSPIntExtOps.h"
#include "DSPEmitter.h"

void nop(const UDSPInstruction opc)
{
	// The real nop is 0. Anything else is bad.
	if (opc)
		DSPInterpreter::unknown(opc);
}
 
const DSPOPCTemplate opcodes[] =
{
	{"NOP",		0x0000, 0xfffc, nop, &DSPEmitter::nop, 1, 0, {}, false, false},

	{"DAR",		0x0004, 0xfffc, DSPInterpreter::dar, NULL, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"IAR",		0x0008, 0xfffc, DSPInterpreter::iar, NULL, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"SUBARN",	0x000c, 0xfffc, DSPInterpreter::subarn, NULL, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"ADDARN",  0x0010, 0xfff0, DSPInterpreter::addarn, NULL, 1, 2, {{P_REG, 1, 0, 0, 0x0003}, {P_REG04, 1, 0, 2, 0x000c}}, false, false},

	{"HALT",	0x0021, 0xffff, DSPInterpreter::halt, NULL, 1, 0, {}, false, true},

	{"RETGE",	0x02d0, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETL",	0x02d1, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETG",	0x02d2, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETLE",	0x02d3, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETNZ",	0x02d4, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETZ",	0x02d5, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETNC",	0x02d6, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETC",	0x02d7, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETx8",	0x02d8, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETx9",	0x02d9, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETxA",	0x02da, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETxB",	0x02db, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETLNZ",	0x02dc, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETLZ",	0x02dd, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RETO",	0x02de, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	{"RET",		0x02df, 0xffff, DSPInterpreter::ret, NULL, 1, 0, {}, false, true},
	
	{"RTI",	    0x02ff, 0xffff, DSPInterpreter::rti, NULL, 1, 0, {}, false, true},

	{"CALLGE",	0x02b0, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLL",	0x02b1, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLG",	0x02b2, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLLE",	0x02b3, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLNZ",	0x02b4, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLZ",	0x02b5, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLNC",	0x02b6, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLC",	0x02b7, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLx8",	0x02b8, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLx9",	0x02b9, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLxA",	0x02ba, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLxB",	0x02bb, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLLNZ",	0x02bc, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLLZ",	0x02bd, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALLO",	0x02be, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"CALL",	0x02bf, 0xffff, DSPInterpreter::call, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},

	{"IFGE",	0x0270, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFL",		0x0271, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFG",		0x0272, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFLE",	0x0273, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFNZ",	0x0274, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFZ",		0x0275, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFNC",	0x0276, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFC",		0x0277, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFx8",	0x0278, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFx9",	0x0279, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFxA",	0x027a, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFxB",	0x027b, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFLNZ",	0x027c, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFLZ",	0x027d, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IFO",		0x027e, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},
	{"IF",		0x027f, 0xffff, DSPInterpreter::ifcc, NULL, 1, 0, {}, false, true},

	{"JGE",		0x0290, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JL",		0x0291, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JG",		0x0292, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JLE",		0x0293, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JNZ",		0x0294, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JZ",		0x0295, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JNC",		0x0296, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JC",		0x0297, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JMPx8",	0x0298, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JMPx9",	0x0299, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JMPxA",	0x029a, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JMPxB",	0x029b, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JLNZ",	0x029c, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JLZ",		0x029d, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JO",		0x029e, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"JMP",		0x029f, 0xffff, DSPInterpreter::jcc, NULL, 2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},

	{"JRGE",	0x1700, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRL",		0x1701, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRG",		0x1702, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRLE",	0x1703, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRNZ",	0x1704, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRZ",		0x1705, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRNC",	0x1706, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRC",		0x1707, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JMPRx8",	0x1708, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JMPRx9",	0x1709, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JMPRxA",	0x170a, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JMPRxB",	0x170b, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRLNZ",	0x170c, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRLZ",	0x170d, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JRO",		0x170e, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"JMPR",	0x170f, 0xff1f, DSPInterpreter::jmprcc, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},

	{"CALLRGE",	0x1710, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRL",	0x1711, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRG",	0x1712, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRLE",	0x1713, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRNZ",	0x1714, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRZ",	0x1715, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRNC",	0x1716, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRC",	0x1717, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRx8",	0x1718, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRx9",	0x1719, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRxA",	0x171a, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRxB",	0x171b, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRLNZ",0x171c, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRLZ",	0x171d, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLRO",	0x171e, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},
	{"CALLR",	0x171f, 0xff1f, DSPInterpreter::callr, NULL, 1, 1, {{P_REG, 1, 0, 5, 0x00e0}}, false, true},

	{"SBCLR",   0x1200, 0xff00, DSPInterpreter::sbclr, NULL, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, false, false},
	{"SBSET",   0x1300, 0xff00, DSPInterpreter::sbset, NULL, 1, 1, {{P_IMM, 1, 0, 0, 0x0007}}, false, false},

	{"LSL",		0x1400, 0xfec0, DSPInterpreter::lsl, NULL, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false, false},
	{"LSR",		0x1440, 0xfec0, DSPInterpreter::lsr, NULL, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false, false},
	{"ASL",		0x1480, 0xfec0, DSPInterpreter::asl, NULL, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false, false},
	{"ASR",		0x14c0, 0xfec0, DSPInterpreter::asr, NULL, 1, 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x003f}}, false, false},
	
	{"LSRN",	0x02ca, 0xffff, DSPInterpreter::lsrn, NULL, 1, 0, {}, false, false}, // discovered by ector!
	{"ASRN",	0x02cb, 0xffff, DSPInterpreter::asrn, NULL, 1, 0, {}, false, false}, // discovered by ector!

	{"LRI",		0x0080, 0xffe0, DSPInterpreter::lri, NULL, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},
	{"LR",      0x00c0, 0xffe0, DSPInterpreter::lr, NULL, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_MEM, 2, 1, 0, 0xffff}}, false, false},
	{"SR",      0x00e0, 0xffe0, DSPInterpreter::sr, NULL, 2, 2, {{P_MEM, 2, 1, 0, 0xffff}, {P_REG, 1, 0, 0, 0x001f}}, false, false},

	{"MRR",		0x1c00, 0xfc00, DSPInterpreter::mrr, NULL, 1, 2, {{P_REG, 1, 0, 5, 0x03e0}, {P_REG, 1, 0, 0, 0x001f}}, false, false},

	{"SI",      0x1600, 0xff00, DSPInterpreter::si, NULL, 2, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},

	{"ADDIS",	0x0400, 0xfe00, DSPInterpreter::addis, NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, false, false},
	{"CMPIS",	0x0600, 0xfe00, DSPInterpreter::cmpis, NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 1, 0, 0, 0x00ff}}, false, false},
	{"LRIS",	0x0800, 0xf800, DSPInterpreter::lris, NULL, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_IMM, 1, 0, 0, 0x00ff}}, false, false},

	{"ADDI",    0x0200, 0xfeff, DSPInterpreter::addi, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false}, // F|RES: missing S64
	{"XORI",    0x0220, 0xfeff, DSPInterpreter::xori, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},
	{"ANDI",	0x0240, 0xfeff, DSPInterpreter::andi, NULL, 2, 2,  {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},
	{"ORI",		0x0260, 0xfeff, DSPInterpreter::ori, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},
	{"CMPI",    0x0280, 0xfeff, DSPInterpreter::cmpi, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},

	{"ANDF",	0x02a0, 0xfeff, DSPInterpreter::andf, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},
	{"ANDCF",	0x02c0, 0xfeff, DSPInterpreter::andcf, NULL, 2, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_IMM, 2, 1, 0, 0xffff}}, false, false},

	{"ILRR",    0x0210, 0xfefc, DSPInterpreter::ilrr,  NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"ILRRD",	0x0214, 0xfefc, DSPInterpreter::ilrrd, NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false, false}, // Hermes doesn't list this
	{"ILRRI",   0x0218, 0xfefc, DSPInterpreter::ilrri, NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"ILRRN",   0x021c, 0xfefc, DSPInterpreter::ilrrn, NULL, 1, 2, {{P_ACCM, 1, 0, 8, 0x0100}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},

	// LOOPS
	{"LOOP",    0x0040, 0xffe0, DSPInterpreter::loop,   NULL, 1, 1, {{P_REG, 1, 0, 0, 0x001f}}, false, true},	
	{"BLOOP",   0x0060, 0xffe0, DSPInterpreter::bloop,  NULL, 2, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},
	{"LOOPI",   0x1000, 0xff00, DSPInterpreter::loopi,  NULL, 1, 1, {{P_IMM, 1, 0, 0, 0x00ff}}, false, true},
	{"BLOOPI",  0x1100, 0xff00, DSPInterpreter::bloopi, NULL, 2, 2, {{P_IMM, 1, 0, 0, 0x00ff}, {P_ADDR_I, 2, 1, 0, 0xffff}}, false, true},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRR",		0x1800, 0xff80, DSPInterpreter::lrr,  NULL, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false, false},
	{"LRRD",    0x1880, 0xff80, DSPInterpreter::lrrd, NULL, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false, false},
	{"LRRI",    0x1900, 0xff80, DSPInterpreter::lrri, NULL, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false, false},
	{"LRRN",    0x1980, 0xff80, DSPInterpreter::lrrn, NULL, 1, 2, {{P_REG, 1, 0, 0, 0x001f}, {P_PRG, 1, 0, 5, 0x0060}}, false, false},

	{"SRR",		0x1a00, 0xff80, DSPInterpreter::srr,  NULL, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false, false},
	{"SRRD",    0x1a80, 0xff80, DSPInterpreter::srrd, NULL, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false, false},
	{"SRRI",    0x1b00, 0xff80, DSPInterpreter::srri, NULL, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false, false},
	{"SRRN",    0x1b80, 0xff80, DSPInterpreter::srrn, NULL, 1, 2, {{P_PRG, 1, 0, 5, 0x0060}, {P_REG, 1, 0, 0, 0x001f}}, false, false},

	//2
	{"LRS",		0x2000, 0xf800, DSPInterpreter::lrs, NULL, 1, 2, {{P_REG18, 1, 0, 8, 0x0700}, {P_MEM, 1, 0, 0, 0x00ff}}, false, false},
	{"SRS",		0x2800, 0xf800, DSPInterpreter::srs, NULL, 1, 2, {{P_MEM, 1, 0, 0, 0x00ff}, {P_REG18, 1, 0, 8, 0x0700}}, false, false},

// opcodes that can be extended
// extended opcodes, note size of opcode will be set to 0

	//3 - main opcode defined by 9 bits, extension defined by last 7 bits!!
	{"XORR",    0x3000, 0xfc80, DSPInterpreter::xorr,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true, false},
	{"ANDR",    0x3400, 0xfc80, DSPInterpreter::andr,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true, false},
	{"ORR",		0x3800, 0xfc80, DSPInterpreter::orr,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true, false},
	{"ANDC",    0x3c00, 0xfe80, DSPInterpreter::andc,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true, false},
	{"ORC",     0x3e00, 0xfe80, DSPInterpreter::orc,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true, false},
	{"XORC",	0x3080, 0xfe80, DSPInterpreter::xorc,	NULL, 1 , 2, {{P_ACCM, 1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true, false}, 
	{"NOT",		0x3280, 0xfe80, DSPInterpreter::notc,	NULL, 1 , 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true, false},
	{"LSRNRX",	0x3480, 0xfc80, DSPInterpreter::lsrnrx,	NULL, 1 , 2, {{P_ACC,  1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true, false}, 
	{"ASRNRX",	0x3880, 0xfc80, DSPInterpreter::asrnrx,	NULL, 1 , 2, {{P_ACC,  1, 0, 8, 0x0100},{P_REG1A,  1, 0, 9, 0x0200}}, true, false}, 
	{"LSRNR",	0x3c80, 0xfe80, DSPInterpreter::lsrnr,	NULL, 1 , 2, {{P_ACC,  1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true, false}, 
	{"ASRNR",	0x3e80, 0xfe80, DSPInterpreter::asrnr,	NULL, 1 , 2, {{P_ACC,  1, 0, 8, 0x0100},{P_ACCM_D, 1, 0, 8, 0x0100}}, true, false}, 

	//4
	{"ADDR",    0x4000, 0xf800, DSPInterpreter::addr,	NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true, false},
	{"ADDAX",   0x4800, 0xfc00, DSPInterpreter::addax,	NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_AX, 1, 0, 9, 0x0200}}, true, false},
	{"ADD",		0x4c00, 0xfe00, DSPInterpreter::add,	NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true, false},
	{"ADDP",    0x4e00, 0xfe00, DSPInterpreter::addp,	NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},

	//5
	{"SUBR",    0x5000, 0xf800, DSPInterpreter::subr,  NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true, false},
	{"SUBAX",   0x5800, 0xfc00, DSPInterpreter::subax, NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_AX, 1, 0, 9, 0x0200}}, true, false},
	{"SUB",		0x5c00, 0xfe00, DSPInterpreter::sub,   NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true, false},
	{"SUBP",    0x5e00, 0xfe00, DSPInterpreter::subp,  NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},

	//6
	{"MOVR",    0x6000, 0xf800, DSPInterpreter::movr,  NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0600}}, true, false},
	{"MOVAX",   0x6800, 0xfc00, DSPInterpreter::movax, NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_AX, 1, 0, 9, 0x0200}}, true, false},
	{"MOV",		0x6c00, 0xfe00, DSPInterpreter::mov,   NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_ACC_D, 1, 0, 8, 0x0100}}, true, false},
	{"MOVP",    0x6e00, 0xfe00, DSPInterpreter::movp,  NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},

	//7
	{"ADDAXL",  0x7000, 0xfc00, DSPInterpreter::addaxl, NULL, 1 , 2, {{P_ACC, 1, 0, 8, 0x0100}, {P_REG18, 1, 0, 9, 0x0200}}, true, false},
	{"INCM",    0x7400, 0xfe00, DSPInterpreter::incm,   NULL, 1 , 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true, false},
	{"INC",		0x7600, 0xfe00, DSPInterpreter::inc,    NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"DECM",    0x7800, 0xfe00, DSPInterpreter::decm,   NULL, 1 , 1, {{P_ACCM, 1, 0, 8, 0x0100}}, true, false},
	{"DEC",		0x7a00, 0xfe00, DSPInterpreter::dec,    NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"NEG",		0x7c00, 0xfe00, DSPInterpreter::neg,    NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MOVNP",   0x7e00, 0xfe00, DSPInterpreter::movnp,  NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},

	//8
	{"NX",      0x8000, 0xf700, DSPInterpreter::nx,     NULL, 1 , 0, {}, true, false},
	{"CLR",		0x8100, 0xf700, DSPInterpreter::clr,  NULL, 1 ,  1, {{P_ACC, 1, 0, 11, 0x0800}}, true, false},
	{"CMP",		0x8200, 0xff00, DSPInterpreter::cmp,    NULL, 1 , 0, {}, true, false},
	{"MULAXH",  0x8300, 0xff00, DSPInterpreter::mulaxh, NULL, 1 , 0, {}, true, false},
	{"CLRP",	0x8400, 0xff00, DSPInterpreter::clrp, NULL, 1 ,  0, {}, true, false},
	{"TSTPROD", 0x8500, 0xff00, DSPInterpreter::tstprod, NULL, 1 ,  0, {}, true, false},
	{"TSTAXH",  0x8600, 0xfe00, DSPInterpreter::tstaxh, NULL, 1 , 1, {{P_REG1A, 1, 0, 8, 0x0100}}, true, false},
	{"M2",      0x8a00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},
	{"M0",      0x8b00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},
	{"CLR15",   0x8c00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},
	{"SET15",   0x8d00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},
	{"SET16",	0x8e00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},
	{"SET40",	0x8f00, 0xff00, DSPInterpreter::srbith, NULL, 1 , 0, {}, true, false},

	//9
	{"MUL",		0x9000, 0xf700, DSPInterpreter::mul,    NULL, 1 , 2, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}}, true, false},
	{"ASR16",   0x9100, 0xf700, DSPInterpreter::asr16, NULL, 1 , 1, {{P_ACC, 1, 0, 11, 0x0800}}, true, false},
	{"MULMVZ",  0x9200, 0xf600, DSPInterpreter::mulmvz, NULL, 1 , 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULAC",   0x9400, 0xf600, DSPInterpreter::mulac,  NULL, 1 , 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULMV",   0x9600, 0xf600, DSPInterpreter::mulmv,  NULL, 1 , 3, {{P_REG18, 1, 0, 11, 0x0800}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	
	//a-b
	{"MULX",    0xa000, 0xe700, DSPInterpreter::mulx,    NULL, 1 , 2, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}}, true, false},
	{"ABS",     0xa100, 0xf700, DSPInterpreter::abs,   NULL, 1 , 1, {{P_ACC, 1, 0, 11, 0x0800}}, true, false},
	{"MULXMVZ", 0xa200, 0xe600, DSPInterpreter::mulxmvz, NULL, 1 , 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULXAC",  0xa400, 0xe600, DSPInterpreter::mulxac,  NULL, 1 , 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULXMV",  0xa600, 0xe600, DSPInterpreter::mulxmv,  NULL, 1 , 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"TST",		0xb100, 0xf700, DSPInterpreter::tst,   NULL, 1 , 1, {{P_ACC, 1, 0, 11, 0x0800}}, true, false},

	//c-d
	{"MULC",    0xc000, 0xe700, DSPInterpreter::mulc,    NULL, 1 , 2, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, true, false},
	{"CMPAR" ,  0xc100, 0xe700, DSPInterpreter::cmpar, NULL, 1 , 2, {{P_ACC, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}}, true, false},
	{"MULCMVZ", 0xc200, 0xe600, DSPInterpreter::mulcmvz, NULL, 1 , 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULCAC",  0xc400, 0xe600, DSPInterpreter::mulcac,  NULL, 1 , 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MULCMV",  0xc600, 0xe600, DSPInterpreter::mulcmv,  NULL, 1 , 3, {{P_ACCM, 1, 0, 12, 0x1000}, {P_REG1A, 1, 0, 11, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false},

	//e
	{"MADDX",   0xe000, 0xfc00, DSPInterpreter::maddx, NULL, 1 , 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, true, false},
	{"MSUBX",   0xe400, 0xfc00, DSPInterpreter::msubx, NULL, 1 , 2, {{P_REGM18, 1, 0, 8, 0x0200}, {P_REGM19, 1, 0, 7, 0x0100}}, true, false},
	{"MADDC",   0xe800, 0xfc00, DSPInterpreter::maddc, NULL, 1 , 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, true, false},
	{"MSUBC",   0xec00, 0xfc00, DSPInterpreter::msubc, NULL, 1 , 2, {{P_ACCM, 1, 0, 9, 0x0200}, {P_REG19, 1, 0, 7, 0x0100}}, true, false},

	//f
	{"LSL16",   0xf000, 0xfe00, DSPInterpreter::lsl16, NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MADD",    0xf200, 0xfe00, DSPInterpreter::madd,  NULL, 1 , 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, true, false},
	{"LSR16",   0xf400, 0xfe00, DSPInterpreter::lsr16, NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
	{"MSUB",    0xf600, 0xfe00, DSPInterpreter::msub , NULL, 1 , 2, {{P_REG18, 1, 0, 8, 0x0100}, {P_REG1A, 1, 0, 8, 0x0100}}, true, false},
	{"ADDPAXZ", 0xf800, 0xfc00, DSPInterpreter::addpaxz, NULL, 1 , 2, {{P_ACC, 1, 0, 9, 0x0200}, {P_AX, 1, 0, 8, 0x0100}}, true, false},  //Think the args are wrong
	{"CLRL",	0xfc00, 0xfe00, DSPInterpreter::clrl, NULL, 1 ,  1, {{P_ACCL, 1, 0, 11, 0x0800}}, true, false}, // clear acl0
	{"MOVPZ",   0xfe00, 0xfe00, DSPInterpreter::movpz, NULL, 1 , 1, {{P_ACC, 1, 0, 8, 0x0100}}, true, false},
};

const DSPOPCTemplate cw = 
	{"CW",		0x0000, 0x0000, NULL, NULL, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, false, false};


const DSPOPCTemplate opcodes_ext[] =
{
	{"XXX",		0x0000, 0x00fc, DSPInterpreter::Ext::nop, &DSPEmitter::nop, 1, 1, {{P_VAL, 1, 0, 0, 0x00ff}}, false, false},
	
	{"DR",      0x0004, 0x00fc, DSPInterpreter::Ext::dr, &DSPEmitter::dr, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"IR",      0x0008, 0x00fc, DSPInterpreter::Ext::ir, &DSPEmitter::ir, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"NR",      0x000c, 0x00fc, DSPInterpreter::Ext::nr, NULL /*&DSPEmitter::nr*/, 1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"MV",      0x0010, 0x00f0, DSPInterpreter::Ext::mv, NULL /*&DSPEmitter::mv*/, 1, +2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}}, false, false},

	{"S",       0x0020, 0x00e4, DSPInterpreter::Ext::s, NULL /*&DSPEmitter::s*/, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false, false},
	{"SN",      0x0024, 0x00e4, DSPInterpreter::Ext::sn, NULL /*&DSPEmitter::sn*/, 1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false, false},

	{"L",       0x0040, 0x00c4, DSPInterpreter::Ext::l, NULL /*&DSPEmitter::l*/, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LN",      0x0044, 0x00c4, DSPInterpreter::Ext::ln, NULL /*&DSPEmitter::ln*/, 1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},

	{"LS",      0x0080, 0x00ce, DSPInterpreter::Ext::ls, NULL /*&DSPEmitter::ls*/, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SL",      0x0082, 0x00ce, DSPInterpreter::Ext::sl, NULL /*&DSPEmitter::sl*/, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSN",		0x0084, 0x00ce, DSPInterpreter::Ext::lsn, NULL /*&DSPEmitter::lsn*/, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLN",		0x0086, 0x00ce, DSPInterpreter::Ext::sln, NULL /*&DSPEmitter::sln*/, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSM",		0x0088, 0x00ce, DSPInterpreter::Ext::lsm, NULL /*&DSPEmitter::lsm*/, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLM",		0x008a, 0x00ce, DSPInterpreter::Ext::slm, NULL /*&DSPEmitter::slm*/, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSNM",    0x008c, 0x00ce, DSPInterpreter::Ext::lsnm, NULL /*&DSPEmitter::lsnm*/, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLNM",    0x008e, 0x00ce, DSPInterpreter::Ext::slnm, NULL /*&DSPEmitter::slnm*/, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},

	{"LD",      0x00c0, 0x00cc, DSPInterpreter::Ext::ld, NULL /*&DSPEmitter::ld*/, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDN",		0x00c4, 0x00cc, DSPInterpreter::Ext::ldn, NULL /*&DSPEmitter::ldn*/, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDM",		0x00c8, 0x00cc, DSPInterpreter::Ext::ldm, NULL /*&DSPEmitter::ldm*/, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDNM",    0x00cc, 0x00cc, DSPInterpreter::Ext::ldnm, NULL /*&DSPEmitter::ldnm*/, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
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

const DSPOPCTemplate *opTable[OPTABLE_SIZE];
const DSPOPCTemplate *extOpTable[EXT_OPTABLE_SIZE];
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
	return opTable[inst];
}


// This function could use the above GetOpTemplate, but then we'd lose the
// nice property that it catches colliding op masks.
void InitInstructionTable()
{
	// ext op table
	for (int i = 0; i < EXT_OPTABLE_SIZE; i++) 
		extOpTable[i] = &cw;

	for (int i = 0; i < EXT_OPTABLE_SIZE; i++)  
    {
		for (int j = 0; j < opcodes_ext_size; j++)
		{
			u16 mask = opcodes_ext[j].opcode_mask;
			if ((mask & i) == opcodes_ext[j].opcode) 
			{
				if (extOpTable[i] == &cw) 
					extOpTable[i] = &opcodes_ext[j];
				else
					ERROR_LOG(DSPLLE, "opcode ext table place %d already in use for %s", i, opcodes_ext[j].name); 	
			}
		}   
	}

	// op table
	for (int i = 0; i < OPTABLE_SIZE; i++)
		opTable[i] = &cw;
	
	for (int i = 0; i < OPTABLE_SIZE; i++)
	{
		for (int j = 0; j < opcodes_size; j++)
		{
			u16 mask = opcodes[j].opcode_mask;
			if ((mask & i) == opcodes[j].opcode)
			{
				if (opTable[i] == &cw)
					opTable[i] = &opcodes[j];
				else
					ERROR_LOG(DSPLLE, "opcode table place %d already in use for %s", i, opcodes[j].name); 
			}
		}
	}

	for (int i=0; i < WRITEBACKLOGSIZE; i++)
		writeBackLogIdx[i] = -1;
}	
