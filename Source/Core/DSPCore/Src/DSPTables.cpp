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
	{"NOP",		0x0000, 0xfffc, nop,                     &DSPEmitter::nop,    1, 0, {},                                                                                     false, false, false},

	{"DAR",		0x0004, 0xfffc, DSPInterpreter::dar,     &DSPEmitter::dar,    1, 1, {{P_REG, 1, 0, 0, 0x0003}},                                                             false, false, false},
	{"IAR",		0x0008, 0xfffc, DSPInterpreter::iar,     &DSPEmitter::iar,    1, 1, {{P_REG, 1, 0, 0, 0x0003}},                                                             false, false, false},
	{"SUBARN",	0x000c, 0xfffc, DSPInterpreter::subarn,  &DSPEmitter::subarn, 1, 1, {{P_REG, 1, 0, 0, 0x0003}},                                                             false, false, false},
	{"ADDARN",	0x0010, 0xfff0, DSPInterpreter::addarn,  &DSPEmitter::addarn, 1, 2, {{P_REG, 1, 0, 0, 0x0003},     {P_REG04, 1, 0, 2, 0x000c}},                             false, false, false},

	{"HALT",	0x0021, 0xffff, DSPInterpreter::halt,    NULL,                1, 0, {},                                                                                     false, true, true},

	{"RETGE",	0x02d0, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETL",	0x02d1, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETG",	0x02d2, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETLE",	0x02d3, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETNZ",	0x02d4, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETZ",	0x02d5, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETNC",	0x02d6, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETC",	0x02d7, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETx8",	0x02d8, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETx9",	0x02d9, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETxA",	0x02da, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETxB",	0x02db, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETLNZ",	0x02dc, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETLZ",	0x02dd, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RETO",	0x02de, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, false},
	{"RET",		0x02df, 0xffff, DSPInterpreter::ret,     NULL,                1, 0, {},                                                                                     false, true, true},

	{"RTI",		0x02ff, 0xffff, DSPInterpreter::rti,     NULL,                1, 0, {},                                                                                     false, true, true},

	{"CALLGE",	0x02b0, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLL",	0x02b1, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLG",	0x02b2, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLLE",	0x02b3, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLNZ",	0x02b4, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLZ",	0x02b5, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLNC",	0x02b6, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLC",	0x02b7, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLx8",	0x02b8, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLx9",	0x02b9, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLxA",	0x02ba, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLxB",	0x02bb, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLLNZ",	0x02bc, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLLZ",	0x02bd, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALLO",	0x02be, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"CALL",	0x02bf, 0xffff, DSPInterpreter::call,    NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, true},

	{"IFGE",	0x0270, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFL",		0x0271, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFG",		0x0272, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFLE",	0x0273, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFNZ",	0x0274, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFZ",		0x0275, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFNC",	0x0276, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFC",		0x0277, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFx8",	0x0278, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFx9",	0x0279, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFxA",	0x027a, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFxB",	0x027b, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFLNZ",	0x027c, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFLZ",	0x027d, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IFO",		0x027e, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, false},
	{"IF",		0x027f, 0xffff, DSPInterpreter::ifcc,    NULL,                1, 0, {},                                                                                     false, true, true},

	{"JGE",		0x0290, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JL",		0x0291, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JG",		0x0292, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JLE",		0x0293, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JNZ",		0x0294, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JZ",		0x0295, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JNC",		0x0296, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JC",		0x0297, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JMPx8",	0x0298, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JMPx9",	0x0299, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JMPxA",	0x029a, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JMPxB",	0x029b, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JLNZ",	0x029c, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JLZ",		0x029d, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JO",		0x029e, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, false},
	{"JMP",		0x029f, 0xffff, DSPInterpreter::jcc,     NULL,                2, 1, {{P_ADDR_I, 2, 1, 0, 0xffff}},                                                          false, true, true},

	{"JRGE",	0x1700, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRL",		0x1701, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRG",		0x1702, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRLE",	0x1703, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRNZ",	0x1704, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRZ",		0x1705, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRNC",	0x1706, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRC",		0x1707, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JMPRx8",	0x1708, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JMPRx9",	0x1709, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JMPRxA",	0x170a, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JMPRxB",	0x170b, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRLNZ",	0x170c, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRLZ",	0x170d, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JRO",		0x170e, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"JMPR",	0x170f, 0xff1f, DSPInterpreter::jmprcc,  NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, true},

	{"CALLRGE",	0x1710, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRL",	0x1711, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRG",	0x1712, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRLE",	0x1713, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRNZ",	0x1714, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRZ",	0x1715, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRNC",	0x1716, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRC",	0x1717, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRx8",	0x1718, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRx9",	0x1719, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRxA",	0x171a, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRxB",	0x171b, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRLNZ",	0x171c, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRLZ",	0x171d, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLRO",	0x171e, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, false},
	{"CALLR",	0x171f, 0xff1f, DSPInterpreter::callr,   NULL,                1, 1, {{P_REG, 1, 0, 5, 0x00e0}},                                                             false, true, true},

	{"SBCLR",	0x1200, 0xff00, DSPInterpreter::sbclr,   &DSPEmitter::sbclr,  1, 1, {{P_IMM, 1, 0, 0, 0x0007}},                                                             false, false, false},
	{"SBSET",	0x1300, 0xff00, DSPInterpreter::sbset,   &DSPEmitter::sbset,  1, 1, {{P_IMM, 1, 0, 0, 0x0007}},                                                             false, false, false},

	{"LSL",		0x1400, 0xfec0, DSPInterpreter::lsl,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_IMM, 1, 0, 0, 0x003f}},                               false, false, false},
	{"LSR",		0x1440, 0xfec0, DSPInterpreter::lsr,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_IMM, 1, 0, 0, 0x003f}},                               false, false, false},
	{"ASL",		0x1480, 0xfec0, DSPInterpreter::asl,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_IMM, 1, 0, 0, 0x003f}},                               false, false, false},
	{"ASR",		0x14c0, 0xfec0, DSPInterpreter::asr,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_IMM, 1, 0, 0, 0x003f}},                               false, false, false},

	{"LSRN",	0x02ca, 0xffff, DSPInterpreter::lsrn,    NULL,                1, 0, {},                                                                                     false, false, false}, // discovered by ector!
	{"ASRN",	0x02cb, 0xffff, DSPInterpreter::asrn,    NULL,                1, 0, {},                                                                                     false, false, false}, // discovered by ector!

	{"LRI",		0x0080, 0xffe0, DSPInterpreter::lri,     NULL,                2, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"LR",		0x00c0, 0xffe0, DSPInterpreter::lr,      NULL,                2, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_MEM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"SR",		0x00e0, 0xffe0, DSPInterpreter::sr,      NULL,                2, 2, {{P_MEM, 2, 1, 0, 0xffff},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},

	{"MRR",		0x1c00, 0xfc00, DSPInterpreter::mrr,     &DSPEmitter::mrr,    1, 2, {{P_REG, 1, 0, 5, 0x03e0},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},

	{"SI",		0x1600, 0xff00, DSPInterpreter::si,      NULL,                2, 2, {{P_MEM, 1, 0, 0, 0x00ff},     {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},

	{"ADDIS",	0x0400, 0xfe00, DSPInterpreter::addis,   NULL,                1, 2, {{P_ACCM,  1, 0, 8, 0x0100},   {P_IMM, 1, 0, 0, 0x00ff}},                               false, false, false},
	{"CMPIS",	0x0600, 0xfe00, DSPInterpreter::cmpis,   NULL,                1, 2, {{P_ACCM,  1, 0, 8, 0x0100},   {P_IMM, 1, 0, 0, 0x00ff}},                               false, false, false},
	{"LRIS",	0x0800, 0xf800, DSPInterpreter::lris,    &DSPEmitter::lris,   1, 2, {{P_REG18, 1, 0, 8, 0x0700},   {P_IMM, 1, 0, 0, 0x00ff}},                               false, false, false},

	{"ADDI",	0x0200, 0xfeff, DSPInterpreter::addi,    NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"XORI",	0x0220, 0xfeff, DSPInterpreter::xori,    NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"ANDI",	0x0240, 0xfeff, DSPInterpreter::andi,    NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"ORI",		0x0260, 0xfeff, DSPInterpreter::ori,     NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"CMPI",	0x0280, 0xfeff, DSPInterpreter::cmpi,    NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},

	{"ANDF",	0x02a0, 0xfeff, DSPInterpreter::andf,    NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},
	{"ANDCF",	0x02c0, 0xfeff, DSPInterpreter::andcf,   NULL,                2, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_IMM, 2, 1, 0, 0xffff}},                               false, false, false},

	{"ILRR",	0x0210, 0xfefc, DSPInterpreter::ilrr,    NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_PRG, 1, 0, 0, 0x0003}},                               false, false, false},
	{"ILRRD",	0x0214, 0xfefc, DSPInterpreter::ilrrd,   NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_PRG, 1, 0, 0, 0x0003}},                               false, false, false},
	{"ILRRI",	0x0218, 0xfefc, DSPInterpreter::ilrri,   NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_PRG, 1, 0, 0, 0x0003}},                               false, false, false},
	{"ILRRN",	0x021c, 0xfefc, DSPInterpreter::ilrrn,   NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_PRG, 1, 0, 0, 0x0003}},                               false, false, false},

	// LOOPS
	{"LOOP",	0x0040, 0xffe0, DSPInterpreter::loop,    NULL,                1, 1, {{P_REG, 1, 0, 0, 0x001f}},                                                             false, true, false},
	{"BLOOP",	0x0060, 0xffe0, DSPInterpreter::bloop,   NULL,                2, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_ADDR_I, 2, 1, 0, 0xffff}},                            false, true, false},
	{"LOOPI",	0x1000, 0xff00, DSPInterpreter::loopi,   NULL,                1, 1, {{P_IMM, 1, 0, 0, 0x00ff}},                                                             false, true, false},
	{"BLOOPI",	0x1100, 0xff00, DSPInterpreter::bloopi,  NULL,                2, 2, {{P_IMM, 1, 0, 0, 0x00ff},     {P_ADDR_I, 2, 1, 0, 0xffff}},                            false, true, false},

	// load and store value pointed by indexing reg and increment; LRR/SRR variants
	{"LRR",		0x1800, 0xff80, DSPInterpreter::lrr,     NULL,                1, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_PRG, 1, 0, 5, 0x0060}},                               false, false, false},
	{"LRRD",	0x1880, 0xff80, DSPInterpreter::lrrd,    NULL,                1, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_PRG, 1, 0, 5, 0x0060}},                               false, false, false},
	{"LRRI",	0x1900, 0xff80, DSPInterpreter::lrri,    NULL,                1, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_PRG, 1, 0, 5, 0x0060}},                               false, false, false},
	{"LRRN",	0x1980, 0xff80, DSPInterpreter::lrrn,    NULL,                1, 2, {{P_REG, 1, 0, 0, 0x001f},     {P_PRG, 1, 0, 5, 0x0060}},                               false, false, false},

	{"SRR",		0x1a00, 0xff80, DSPInterpreter::srr,     NULL,                1, 2, {{P_PRG, 1, 0, 5, 0x0060},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},
	{"SRRD",	0x1a80, 0xff80, DSPInterpreter::srrd,    NULL,                1, 2, {{P_PRG, 1, 0, 5, 0x0060},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},
	{"SRRI",	0x1b00, 0xff80, DSPInterpreter::srri,    NULL,                1, 2, {{P_PRG, 1, 0, 5, 0x0060},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},
	{"SRRN",	0x1b80, 0xff80, DSPInterpreter::srrn,    NULL,                1, 2, {{P_PRG, 1, 0, 5, 0x0060},     {P_REG, 1, 0, 0, 0x001f}},                               false, false, false},

	//2
	{"LRS",		0x2000, 0xf800, DSPInterpreter::lrs,     NULL,                1, 2, {{P_REG18, 1, 0, 8, 0x0700},   {P_MEM, 1, 0, 0, 0x00ff}},                               false, false, false},
	{"SRS",		0x2800, 0xf800, DSPInterpreter::srs,     NULL,                1, 2, {{P_MEM,   1, 0, 0, 0x00ff},   {P_REG18, 1, 0, 8, 0x0700}},                             false, false, false},

// opcodes that can be extended

	//3 - main opcode defined by 9 bits, extension defined by last 7 bits!!
	{"XORR",	0x3000, 0xfc80, DSPInterpreter::xorr,    NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_REG1A,  1, 0, 9, 0x0200}},                            true, false, false},
	{"ANDR",	0x3400, 0xfc80, DSPInterpreter::andr,    NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_REG1A,  1, 0, 9, 0x0200}},                            true, false, false},
	{"ORR", 	0x3800, 0xfc80, DSPInterpreter::orr,     NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_REG1A,  1, 0, 9, 0x0200}},                            true, false, false},
	{"ANDC",	0x3c00, 0xfe80, DSPInterpreter::andc,    NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_ACCM_D, 1, 0, 8, 0x0100}},                            true, false, false},
	{"ORC", 	0x3e00, 0xfe80, DSPInterpreter::orc,     NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_ACCM_D, 1, 0, 8, 0x0100}},                            true, false, false},
	{"XORC",	0x3080, 0xfe80, DSPInterpreter::xorc,    NULL,                1, 2, {{P_ACCM, 1, 0, 8, 0x0100},    {P_ACCM_D, 1, 0, 8, 0x0100}},                            true, false, false},
	{"NOT", 	0x3280, 0xfe80, DSPInterpreter::notc,    NULL,                1, 1, {{P_ACCM, 1, 0, 8, 0x0100}},                                                            true, false, false},
	{"LSRNRX",	0x3480, 0xfc80, DSPInterpreter::lsrnrx,  NULL,                1, 2, {{P_ACC,  1, 0, 8, 0x0100},    {P_REG1A,  1, 0, 9, 0x0200}},                            true, false, false},
	{"ASRNRX",	0x3880, 0xfc80, DSPInterpreter::asrnrx,  NULL,                1, 2, {{P_ACC,  1, 0, 8, 0x0100},    {P_REG1A,  1, 0, 9, 0x0200}},                            true, false, false},
	{"LSRNR",	0x3c80, 0xfe80, DSPInterpreter::lsrnr,   NULL,                1, 2, {{P_ACC,  1, 0, 8, 0x0100},    {P_ACCM_D, 1, 0, 8, 0x0100}},                            true, false, false},
	{"ASRNR",	0x3e80, 0xfe80, DSPInterpreter::asrnr,   NULL,                1, 2, {{P_ACC,  1, 0, 8, 0x0100},    {P_ACCM_D, 1, 0, 8, 0x0100}},                            true, false, false},

	//4
	{"ADDR",	0x4000, 0xf800, DSPInterpreter::addr,    NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_REG18, 1, 0, 9, 0x0600}},                             true, false, false},
	{"ADDAX",	0x4800, 0xfc00, DSPInterpreter::addax,   NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_AX, 1, 0, 9, 0x0200}},                                true, false, false},
	{"ADD", 	0x4c00, 0xfe00, DSPInterpreter::add,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_ACC_D, 1, 0, 8, 0x0100}},                             true, false, false},
	{"ADDP",	0x4e00, 0xfe00, DSPInterpreter::addp,    NULL,                1, 1, {{P_ACC, 1, 0, 8, 0x0100}},                                                             true, false, false},

	//5
	{"SUBR",	0x5000, 0xf800, DSPInterpreter::subr,    NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_REG18, 1, 0, 9, 0x0600}},                             true, false, false},
	{"SUBAX",	0x5800, 0xfc00, DSPInterpreter::subax,   NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_AX, 1, 0, 9, 0x0200}},                                true, false, false},
	{"SUB", 	0x5c00, 0xfe00, DSPInterpreter::sub,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_ACC_D, 1, 0, 8, 0x0100}},                             true, false, false},
	{"SUBP",	0x5e00, 0xfe00, DSPInterpreter::subp,    NULL,                1, 1, {{P_ACC, 1, 0, 8, 0x0100}},                                                             true, false, false},

	//6
	{"MOVR",	0x6000, 0xf800, DSPInterpreter::movr,    NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_REG18, 1, 0, 9, 0x0600}},                             true, false, false},
	{"MOVAX",	0x6800, 0xfc00, DSPInterpreter::movax,   NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_AX, 1, 0, 9, 0x0200}},                                true, false, false},
	{"MOV", 	0x6c00, 0xfe00, DSPInterpreter::mov,     NULL,                1, 2, {{P_ACC, 1, 0, 8, 0x0100},     {P_ACC_D, 1, 0, 8, 0x0100}},                             true, false, false},
	{"MOVP",	0x6e00, 0xfe00, DSPInterpreter::movp,    NULL,                1, 1, {{P_ACC, 1, 0, 8, 0x0100}},                                                             true, false, false},

	//7
	{"ADDAXL",	0x7000, 0xfc00, DSPInterpreter::addaxl,  NULL,                1, 2, {{P_ACC,  1, 0, 8, 0x0100},    {P_REG18, 1, 0, 9, 0x0200}},                             true, false, false},
	{"INCM",	0x7400, 0xfe00, DSPInterpreter::incm,    NULL,                1, 1, {{P_ACCM, 1, 0, 8, 0x0100}},                                                            true, false, false},
	{"INC",		0x7600, 0xfe00, DSPInterpreter::inc,     NULL,                1, 1, {{P_ACC,  1, 0, 8, 0x0100}},                                                            true, false, false},
	{"DECM",	0x7800, 0xfe00, DSPInterpreter::decm,    NULL,                1, 1, {{P_ACCM, 1, 0, 8, 0x0100}},                                                            true, false, false},
	{"DEC",		0x7a00, 0xfe00, DSPInterpreter::dec,     NULL,                1, 1, {{P_ACC,  1, 0, 8, 0x0100}},                                                            true, false, false},
	{"NEG",		0x7c00, 0xfe00, DSPInterpreter::neg,     NULL,                1, 1, {{P_ACC,  1, 0, 8, 0x0100}},                                                            true, false, false},
	{"MOVNP",	0x7e00, 0xfe00, DSPInterpreter::movnp,   NULL,                1, 1, {{P_ACC,  1, 0, 8, 0x0100}},                                                            true, false, false},

	//8
	{"NX",	 	0x8000, 0xf700, DSPInterpreter::nx,      &DSPEmitter::nx,     1, 0, {},                                                                                     true, false, false},
	{"CLR",		0x8100, 0xf700, DSPInterpreter::clr,     NULL,                1, 1, {{P_ACC,   1, 0, 11, 0x0800}},                                                          true, false, false},
	{"CMP",		0x8200, 0xff00, DSPInterpreter::cmp,     NULL,                1, 0, {},                                                                                     true, false, false},
	{"MULAXH",	0x8300, 0xff00, DSPInterpreter::mulaxh,  NULL,                1, 0, {},                                                                                     true, false, false},
	{"CLRP",	0x8400, 0xff00, DSPInterpreter::clrp,    NULL,                1, 0, {},                                                                                     true, false, false},
	{"TSTPROD",	0x8500, 0xff00, DSPInterpreter::tstprod, NULL,                1, 0, {},                                                                                     true, false, false},
	{"TSTAXH",	0x8600, 0xfe00, DSPInterpreter::tstaxh,  NULL,                1, 1, {{P_REG1A, 1, 0, 8, 0x0100}},                                                           true, false, false},
	{"M2",		0x8a00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},
	{"M0",		0x8b00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},
	{"CLR15",	0x8c00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},
	{"SET15",	0x8d00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},
	{"SET16",	0x8e00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},
	{"SET40",	0x8f00, 0xff00, DSPInterpreter::srbith,  &DSPEmitter::srbith, 1, 0, {},                                                                                     true, false, false},

	//9
	{"MUL",		0x9000, 0xf700, DSPInterpreter::mul,     NULL,                1, 2, {{P_REG18, 1, 0, 11, 0x0800},  {P_REG1A, 1, 0, 11, 0x0800}},                            true, false, false},
	{"ASR16",	0x9100, 0xf700, DSPInterpreter::asr16,   NULL,                1, 1, {{P_ACC,   1, 0, 11, 0x0800}},                                                          true, false, false},
	{"MULMVZ",	0x9200, 0xf600, DSPInterpreter::mulmvz,  NULL,                1, 3, {{P_REG18, 1, 0, 11, 0x0800},  {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULAC",	0x9400, 0xf600, DSPInterpreter::mulac,   NULL,                1, 3, {{P_REG18, 1, 0, 11, 0x0800},  {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULMV",	0x9600, 0xf600, DSPInterpreter::mulmv,   NULL,                1, 3, {{P_REG18, 1, 0, 11, 0x0800},  {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},

	//a-b
	{"MULX",	0xa000, 0xe700, DSPInterpreter::mulx,    NULL,                1, 2, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}},                           true, false, false},
	{"ABS",		0xa100, 0xf700, DSPInterpreter::abs,     NULL,                1, 1, {{P_ACC,    1, 0, 11, 0x0800}},                                                         true, false, false},
	{"MULXMVZ",	0xa200, 0xe600, DSPInterpreter::mulxmvz, NULL,                1, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULXAC",	0xa400, 0xe600, DSPInterpreter::mulxac,  NULL,                1, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULXMV",	0xa600, 0xe600, DSPInterpreter::mulxmv,  NULL,                1, 3, {{P_REGM18, 1, 0, 11, 0x1000}, {P_REGM19, 1, 0, 10, 0x0800}, {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"TST",		0xb100, 0xf700, DSPInterpreter::tst,     NULL,                1, 1, {{P_ACC,    1, 0, 11, 0x0800}},                                                         true, false, false},

	//c-d
	{"MULC",	0xc000, 0xe700, DSPInterpreter::mulc,    NULL,                1, 2, {{P_ACCM, 1, 0, 12, 0x1000},   {P_REG1A, 1, 0, 11, 0x0800}},                            true, false, false},
	{"CMPAR" ,	0xc100, 0xe700, DSPInterpreter::cmpar,   NULL,                1, 2, {{P_ACC,  1, 0, 12, 0x1000},   {P_REG1A, 1, 0, 11, 0x0800}},                            true, false, false},
	{"MULCMVZ",	0xc200, 0xe600, DSPInterpreter::mulcmvz, NULL,                1, 3, {{P_ACCM, 1, 0, 12, 0x1000},   {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULCAC",	0xc400, 0xe600, DSPInterpreter::mulcac,  NULL,                1, 3, {{P_ACCM, 1, 0, 12, 0x1000},   {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},
	{"MULCMV",	0xc600, 0xe600, DSPInterpreter::mulcmv,  NULL,                1, 3, {{P_ACCM, 1, 0, 12, 0x1000},   {P_REG1A, 1, 0, 11, 0x0800},  {P_ACC, 1, 0, 8, 0x0100}}, true, false, false},

	//e
	{"MADDX",	0xe000, 0xfc00, DSPInterpreter::maddx,   NULL,                1, 2, {{P_REGM18, 1, 0, 8, 0x0200},  {P_REGM19, 1, 0, 7, 0x0100}},                            true, false, false},
	{"MSUBX",	0xe400, 0xfc00, DSPInterpreter::msubx,   NULL,                1, 2, {{P_REGM18, 1, 0, 8, 0x0200},  {P_REGM19, 1, 0, 7, 0x0100}},                            true, false, false},
	{"MADDC",	0xe800, 0xfc00, DSPInterpreter::maddc,   NULL,                1, 2, {{P_ACCM,   1, 0, 9, 0x0200},  {P_REG19, 1, 0, 7, 0x0100}},                             true, false, false},
	{"MSUBC",	0xec00, 0xfc00, DSPInterpreter::msubc,   NULL,                1, 2, {{P_ACCM,   1, 0, 9, 0x0200},  {P_REG19, 1, 0, 7, 0x0100}},                             true, false, false},

	//f
	{"LSL16",	0xf000, 0xfe00, DSPInterpreter::lsl16,   NULL,                1, 1, {{P_ACC,   1, 0,  8, 0x0100}},                                                          true, false, false},
	{"MADD",	0xf200, 0xfe00, DSPInterpreter::madd,    NULL,                1, 2, {{P_REG18, 1, 0,  8, 0x0100},  {P_REG1A, 1, 0, 8, 0x0100}},                             true, false, false},
	{"LSR16",	0xf400, 0xfe00, DSPInterpreter::lsr16,   NULL,                1, 1, {{P_ACC,   1, 0,  8, 0x0100}},                                                          true, false, false},
	{"MSUB",	0xf600, 0xfe00, DSPInterpreter::msub,    NULL,                1, 2, {{P_REG18, 1, 0,  8, 0x0100},  {P_REG1A, 1, 0, 8, 0x0100}},                             true, false, false},
	{"ADDPAXZ",	0xf800, 0xfc00, DSPInterpreter::addpaxz, NULL,                1, 2, {{P_ACC,   1, 0,  9, 0x0200},  {P_AX, 1, 0, 8, 0x0100}},                                true, false, false},
	{"CLRL",	0xfc00, 0xfe00, DSPInterpreter::clrl,    NULL,                1, 1, {{P_ACCL,  1, 0, 11, 0x0800}},                                                          true, false, false},
	{"MOVPZ",	0xfe00, 0xfe00, DSPInterpreter::movpz,   NULL,                1, 1, {{P_ACC,   1, 0,  8, 0x0100}},                                                          true, false, false},
};

const DSPOPCTemplate cw = 
	{"CW",		0x0000, 0x0000, NULL, NULL, 1, 1, {{P_VAL, 2, 0, 0, 0xffff}}, false, false};

// extended opcodes

const DSPOPCTemplate opcodes_ext[] =
{
	{"XXX",		0x0000, 0x00fc, DSPInterpreter::Ext::nop,  &DSPEmitter::nop,  1, 1, {{P_VAL, 1, 0, 0, 0x00ff}}, false, false},

	{"DR",		0x0004, 0x00fc, DSPInterpreter::Ext::dr,   &DSPEmitter::dr,   1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"IR",		0x0008, 0x00fc, DSPInterpreter::Ext::ir,   &DSPEmitter::ir,   1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"NR",		0x000c, 0x00fc, DSPInterpreter::Ext::nr,   &DSPEmitter::nr,   1, 1, {{P_REG, 1, 0, 0, 0x0003}}, false, false},
	{"MV",		0x0010, 0x00f0, DSPInterpreter::Ext::mv,   &DSPEmitter::mv,   1, 2, {{P_REG18, 1, 0, 2, 0x000c}, {P_REG1C, 1, 0, 0, 0x0003}}, false, false},

	{"S",		0x0020, 0x00e4, DSPInterpreter::Ext::s,    &DSPEmitter::s,    1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false, false},
	{"SN",		0x0024, 0x00e4, DSPInterpreter::Ext::sn,   &DSPEmitter::sn,   1, 2, {{P_PRG, 1, 0, 0, 0x0003}, {P_REG1C, 1, 0, 3, 0x0018}}, false, false},

	{"L",		0x0040, 0x00c4, DSPInterpreter::Ext::l,    &DSPEmitter::l,    1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LN",		0x0044, 0x00c4, DSPInterpreter::Ext::ln,   &DSPEmitter::ln,   1, 2, {{P_REG18, 1, 0, 3, 0x0038}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},

	{"LS",		0x0080, 0x00ce, DSPInterpreter::Ext::ls,   &DSPEmitter::ls,   1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SL",		0x0082, 0x00ce, DSPInterpreter::Ext::sl,   &DSPEmitter::sl,   1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSN",		0x0084, 0x00ce, DSPInterpreter::Ext::lsn,  &DSPEmitter::lsn,  1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLN",		0x0086, 0x00ce, DSPInterpreter::Ext::sln,  &DSPEmitter::sln,  1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSM",		0x0088, 0x00ce, DSPInterpreter::Ext::lsm,  &DSPEmitter::lsm,  1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLM",		0x008a, 0x00ce, DSPInterpreter::Ext::slm,  &DSPEmitter::slm,  1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},
	{"LSNM",	0x008c, 0x00ce, DSPInterpreter::Ext::lsnm, &DSPEmitter::lsnm, 1, 2, {{P_REG18, 1, 0, 4, 0x0030}, {P_ACCM, 1, 0, 0, 0x0001}}, false, false},
	{"SLNM",	0x008e, 0x00ce, DSPInterpreter::Ext::slnm, &DSPEmitter::slnm, 1, 2, {{P_ACCM, 1, 0, 0, 0x0001}, {P_REG18, 1, 0, 4, 0x0030}}, false, false},

	{"LD",		0x00c0, 0x00cc, DSPInterpreter::Ext::ld,   &DSPEmitter::ld,   1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDN",		0x00c4, 0x00cc, DSPInterpreter::Ext::ldn,  &DSPEmitter::ldn,  1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDM",		0x00c8, 0x00cc, DSPInterpreter::Ext::ldm,  &DSPEmitter::ldm,  1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
	{"LDNM",	0x00cc, 0x00cc, DSPInterpreter::Ext::ldnm, &DSPEmitter::ldnm, 1, 3, {{P_REGM18, 1, 0, 4, 0x0020}, {P_REGM19, 1, 0, 3, 0x0010}, {P_PRG, 1, 0, 0, 0x0003}}, false, false},
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
