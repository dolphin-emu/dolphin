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

#include "Jit_Tables.h"

// Should be moved in to the Jit class
typedef void (Jit64::*_Instruction) (UGeckoInstruction instCode);

_Instruction dynaOpTable[64];
_Instruction dynaOpTable4[1024];
_Instruction dynaOpTable19[1024];
_Instruction dynaOpTable31[1024];
_Instruction dynaOpTable59[32];
_Instruction dynaOpTable63[1024];
void Jit64::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void Jit64::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}



struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
	int runCount;
};

static GekkoOPTemplate primarytable[] = 
{
	{4,       &Jit64::DynaRunTable4},
	{19,     &Jit64::DynaRunTable19},
	{31,     &Jit64::DynaRunTable31},
	{59,     &Jit64::DynaRunTable59},
	{63,     &Jit64::DynaRunTable63},

	{16,            &Jit64::bcx},
	{18,             &Jit64::bx},

	{1,     &Jit64::HLEFunction},
	{2,   &Jit64::Default},
	{3,             &Jit64::Default},
	{17,             &Jit64::sc},

	{7,       &Jit64::mulli},
	{8,      &Jit64::subfic},
	{10,      &Jit64::cmpXX},
	{11,       &Jit64::cmpXX},
	{12,      &Jit64::reg_imm},
	{13,   &Jit64::reg_imm},
	{14,       &Jit64::reg_imm},
	{15,      &Jit64::reg_imm},

	{20,    &Jit64::rlwimix},
	{21,    &Jit64::rlwinmx},
	{23,     &Jit64::rlwnmx},

	{24,        &Jit64::reg_imm},
	{25,       &Jit64::reg_imm},
	{26,       &Jit64::reg_imm},
	{27,      &Jit64::reg_imm},
	{28,    &Jit64::reg_imm},
	{29,   &Jit64::reg_imm},
	
	{32,        &Jit64::lXz},
	{33,       &Jit64::Default},
	{34,        &Jit64::lXz},
	{35,       &Jit64::Default},
	{40,        &Jit64::lXz},
	{41,       &Jit64::Default},

	{42,        &Jit64::lha},
	{43,       &Jit64::Default},

	{44,        &Jit64::stX},
	{45,       &Jit64::stX},
	{36,        &Jit64::stX},
	{37,       &Jit64::stX},
	{38, 	     &Jit64::stX},
	{39,       &Jit64::stX},

	{46,        &Jit64::lmw},
	{47,       &Jit64::stmw},

	{48,        &Jit64::lfs},
	{49,       &Jit64::Default},
	{50,        &Jit64::lfd},
	{51,       &Jit64::Default},

	{52,       &Jit64::stfs},
	{53,      &Jit64::stfs},
	{54,       &Jit64::stfd},
	{55,      &Jit64::Default},

	{56,      &Jit64::psq_l},
	{57,     &Jit64::psq_l},
	{60,     &Jit64::psq_st},
	{61,    &Jit64::psq_st},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &Jit64::Default},
	{5,  &Jit64::Default},
	{6,  &Jit64::Default},
	{9,  &Jit64::Default},
	{22, &Jit64::Default},
	{30, &Jit64::Default},
	{62, &Jit64::Default},
	{58, &Jit64::Default},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,       &Jit64::Default},
	{32,      &Jit64::Default},
	{40,        &Jit64::ps_sign},
	{136,      &Jit64::ps_sign},
	{264,       &Jit64::ps_sign},
	{64,      &Jit64::Default},
	{72,         &Jit64::ps_mr},
	{96,      &Jit64::Default},
	{528,   &Jit64::ps_mergeXX},
	{560,   &Jit64::ps_mergeXX},
	{592,   &Jit64::ps_mergeXX},
	{624,   &Jit64::ps_mergeXX},

	{1014,      &Jit64::Default},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10,    &Jit64::ps_sum},
	{11,    &Jit64::ps_sum},
	{12,   &Jit64::ps_muls},
	{13,   &Jit64::ps_muls},
	{14,  &Jit64::ps_maddXX},
	{15,  &Jit64::ps_maddXX},
	{18,     &Jit64::ps_arith},
	{20,     &Jit64::ps_arith},
	{21,     &Jit64::ps_arith},
	{23,     &Jit64::ps_sel},
	{24,     &Jit64::Default},
	{25,     &Jit64::ps_arith},
	{26,  &Jit64::ps_rsqrte},
	{28,    &Jit64::ps_maddXX},
	{29,    &Jit64::ps_maddXX},
	{30,   &Jit64::ps_maddXX},
	{31,   &Jit64::ps_maddXX},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,     &Jit64::Default},
	{7,    &Jit64::Default},
	{38,   &Jit64::Default},
	{39,  &Jit64::Default}, 
};

static GekkoOPTemplate table19[] = 
{
	{528,  &Jit64::bcctrx},
	{16,    &Jit64::bclrx},
	{257,   &Jit64::Default},
	{129,  &Jit64::Default},
	{289,   &Jit64::Default},
	{225,  &Jit64::Default},
	{33,    &Jit64::Default},
	{449,    &Jit64::Default},
	{417,   &Jit64::Default},
	{193,   &Jit64::Default},
												   
	{150,   &Jit64::DoNothing},
	{0,      &Jit64::Default},
												   
	{50,      &Jit64::rfi},
	{18,     &Jit64::Default}
};


static GekkoOPTemplate table31[] = 
{
	{28,      &Jit64::andx},
	{60,     &Jit64::Default},
	{444,      &Jit64::orx},
	{124,     &Jit64::Default},
	{316,     &Jit64::xorx},
	{412,     &Jit64::Default},
	{476,    &Jit64::Default},
	{284,     &Jit64::Default},
	{0,        &Jit64::cmpXX},
	{32,      &Jit64::cmpXX},
	{26,   &Jit64::cntlzwx},
	{922,   &Jit64::extshx},
	{954,   &Jit64::extsbx},
	{536,     &Jit64::srwx},
	{792,    &Jit64::srawx},
	{824,   &Jit64::srawix},
	{24,      &Jit64::slwx},

	{54,     &Jit64::Default},
	{86,      &Jit64::DoNothing},
	{246,   &Jit64::Default},
	{278,     &Jit64::Default},
	{470,     &Jit64::Default},
	{758,     &Jit64::Default},
	{1014,    &Jit64::dcbz},
#if JITTEST
	//load word
	{23,    &Jit64::lXzx},
	{55,   &Jit64::lXzx},

	//load halfword
	{279,   &Jit64::lXzx},
	{311,  &Jit64::lXzx},

	//load halfword signextend
	{343,   &Jit64::lhax},
	{375,  &Jit64::Default},

	//load byte
	{87,    &Jit64::lXzx},
	{119,  &Jit64::lXzx},
#else
	//load word
	{23,    &Jit64::lwzx},
	{55,   &Jit64::lwzux},

	//load halfword
	{279,   &Jit64::Default},
	{311,  &Jit64::Default},

	//load halfword signextend
	{343,   &Jit64::lhax},
	{375,  &Jit64::Default},

	//load byte
	{87,    &Jit64::lbzx},
	{119,  &Jit64::Default},
#endif
	//load byte reverse
	{534,  &Jit64::Default},
	{790,  &Jit64::Default},

	// Conditional load/store (Wii SMP)
	{150,   &Jit64::Default},
	{20,     &Jit64::Default},

	//load string (interpret these)
	{533,   &Jit64::Default},
	{597,   &Jit64::Default},

	//store word
	{151,    &Jit64::stXx},
	{183,   &Jit64::stXx},

	//store halfword
	{407,    &Jit64::stXx},
	{439,   &Jit64::stXx},

	//store byte
	{215,    &Jit64::stXx},
	{247,   &Jit64::stXx},

	//store bytereverse
	{662,  &Jit64::Default},
	{918,  &Jit64::Default},

	{661,   &Jit64::Default},
	{725,   &Jit64::Default},

	// fp load/store	
	{535,   &Jit64::lfsx},
	{567,  &Jit64::Default},
	{599,   &Jit64::Default},
	{631,  &Jit64::Default},

	{663,   &Jit64::stfsx},
	{695,  &Jit64::Default},
	{727,   &Jit64::Default},
	{759,  &Jit64::Default},
	{983,  &Jit64::Default},

	{19,     &Jit64::mfcr},
	{83,    &Jit64::mfmsr},
	{144,   &Jit64::mtcrf},
	{146,   &Jit64::mtmsr},
	{210,    &Jit64::Default},
	{242,  &Jit64::Default},
	{339,   &Jit64::mfspr},
	{467,   &Jit64::mtspr},
	{371,    &Jit64::mftb},
	{512,   &Jit64::Default},
	{595,    &Jit64::Default},
	{659,  &Jit64::Default},

	{4,         &Jit64::Default},
	{598,     &Jit64::DoNothing},
	{982,     &Jit64::Default},

	// Unused instructions on GC
	{310,    &Jit64::Default},
	{438,    &Jit64::Default},
	{854,    &Jit64::Default},
	{306,    &Jit64::Default},
	{370,    &Jit64::Default},
	{566,  &Jit64::Default},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266,     &Jit64::addx},
	{10,     &Jit64::Default},
	{138,    &Jit64::addex},
	{234,   &Jit64::Default},
#if JITTEST
	{202,   &Jit64::addzex},
#else
	{202,   &Jit64::Default},
#endif
	{491,    &Jit64::Default},
	{459,   &Jit64::divwux},
	{75,    &Jit64::Default},
	{11,   &Jit64::mulhwux},
	{235,   &Jit64::mullwx},
	{104,     &Jit64::negx},
	{40,     &Jit64::subfx},
	{8,     &Jit64::subfcx},
	{136,   &Jit64::subfex},
	{232,  &Jit64::Default},
	{200,  &Jit64::Default},
};

static GekkoOPTemplate table59[] = 
{
	{18,    &Jit64::Default}, 
	{20,    &Jit64::fp_arith_s}, 
	{21,    &Jit64::fp_arith_s}, 
//	{22,   &Jit64::Default}, // Not implemented on gekko
	{24,     &Jit64::Default}, 
	{25,    &Jit64::fp_arith_s}, 
	{28,   &Jit64::fmaddXX}, 
	{29,   &Jit64::fmaddXX}, 
	{30,  &Jit64::fmaddXX}, 
	{31,  &Jit64::fmaddXX}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264,    &Jit64::Default},
	{32,     &Jit64::fcmpx},
	{0,      &Jit64::fcmpx},
	{14,    &Jit64::Default},
	{15,   &Jit64::Default},
	{72,      &Jit64::fmrx},
	{136,   &Jit64::Default},
	{40,     &Jit64::Default},
	{12,     &Jit64::Default},

	{64,     &Jit64::Default},
	{583,    &Jit64::Default},
	{70,   &Jit64::Default},
	{38,   &Jit64::Default},
	{134,  &Jit64::Default},
	{711,   &Jit64::Default},
};

static GekkoOPTemplate table63_2[] = 
{
	{18,    &Jit64::Default},
	{20,    &Jit64::Default},
	{21,    &Jit64::Default},
	{22,   &Jit64::Default},
	{23,    &Jit64::Default},
	{25,    &Jit64::fp_arith_s},
	{26, &Jit64::fp_arith_s},
	{28,   &Jit64::fmaddXX},
	{29,   &Jit64::fmaddXX},
	{30,  &Jit64::fmaddXX},
	{31,  &Jit64::fmaddXX},
};
namespace JitTables
{
void CompileInstruction(UGeckoInstruction _inst)
{
	(jit.*dynaOpTable[_inst.OPCD])(_inst);
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info) {
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)){  ///"mcrfs"
			rsplocations.push_back(jit.js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit.js.compilerPC;
	} else {
		PanicAlert("Tried to compile illegal (or unknown) instruction %08x, at %08x", _inst.hex, jit.js.compilerPC);
	}
}
void InitTables()
{
	//clear
	for (int i = 0; i < 32; i++) 
	{
		dynaOpTable59[i] = &Jit64::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &Jit64::unknown_instruction;
		dynaOpTable19[i] = &Jit64::unknown_instruction;
		dynaOpTable31[i] = &Jit64::unknown_instruction;
		dynaOpTable63[i] = &Jit64::unknown_instruction;	
	}

	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
	{
		dynaOpTable[primarytable[i].opcode] = primarytable[i].Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_2[j].opcode;
			dynaOpTable4[op] = table4_2[j].Inst;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (int j = 0; j < (int)(sizeof(table4_3) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_3[j].opcode;
			dynaOpTable4[op] = table4_3[j].Inst;
		}
	}

	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table4[i].opcode;
		dynaOpTable4[op] = table4[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table31[i].opcode;
		dynaOpTable31[op] = table31[i].Inst;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (int j = 0; j < (int)(sizeof(table31_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table31_2[j].opcode;
			dynaOpTable31[op] = table31_2[j].Inst;
		}
	}

	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table19[i].opcode;
		dynaOpTable19[op] = table19[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table59[i].opcode;
		dynaOpTable59[op] = table59[i].Inst;
	}

	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table63[i].opcode;
		dynaOpTable63[op] = table63[i].Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table63_2[j].opcode;
			dynaOpTable63[op] = table63_2[j].Inst;
		}
	}
}
}
