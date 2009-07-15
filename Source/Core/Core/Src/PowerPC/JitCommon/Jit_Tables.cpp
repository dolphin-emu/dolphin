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
#include "../CoreGeneralize.h"

// Should be moved in to the Jit class
typedef void (cCore::*_Instruction) (UGeckoInstruction instCode);

_Instruction dynaOpTable[64];
_Instruction dynaOpTable4[1024];
_Instruction dynaOpTable19[1024];
_Instruction dynaOpTable31[1024];
_Instruction dynaOpTable59[32];
_Instruction dynaOpTable63[1024];
void cCore::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void cCore::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void cCore::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void cCore::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void cCore::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}



struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
	int runCount;
};

static GekkoOPTemplate primarytable[] = 
{
	{4,  &cCore::DynaRunTable4}, //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &cCore::DynaRunTable19}, //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &cCore::DynaRunTable31}, //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &cCore::DynaRunTable59}, //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &cCore::DynaRunTable63}, //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &cCore::bcx}, //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &cCore::bx}, //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &cCore::HLEFunction}, //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &cCore::Default}, //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &cCore::Default}, //"twi",         OPTYPE_SYSTEM, 0}},
	{17, &cCore::sc}, //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &cCore::mulli}, //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &cCore::subfic}, //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, &cCore::cmpXX}, //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &cCore::cmpXX}, //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &cCore::reg_imm}, //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &cCore::reg_imm}, //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &cCore::reg_imm}, //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &cCore::reg_imm}, //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &cCore::rlwimix}, //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &cCore::rlwinmx}, //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &cCore::rlwnmx}, //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &cCore::reg_imm}, //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &cCore::reg_imm}, //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &cCore::reg_imm}, //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &cCore::reg_imm}, //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &cCore::reg_imm}, //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &cCore::reg_imm}, //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &cCore::lXz}, //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &cCore::lXz}, //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &cCore::lXz}, //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &cCore::lXz}, //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &cCore::lXz}, //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &cCore::lXz}, //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	
	{42, &cCore::lha}, //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &cCore::Default}, //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &cCore::stX}, //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &cCore::stX}, //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &cCore::stX}, //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &cCore::stX}, //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &cCore::stX}, //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &cCore::stX}, //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &cCore::lmw}, //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &cCore::stmw}, //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &cCore::lfs}, //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &cCore::Default}, //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &cCore::lfd}, //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &cCore::Default}, //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &cCore::stfs}, //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &cCore::stfs}, //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &cCore::stfd}, //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &cCore::Default}, //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &cCore::psq_l}, //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &cCore::psq_l}, //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &cCore::psq_st}, //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &cCore::psq_st}, //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &cCore::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,    &cCore::Default}, //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &cCore::Default}, //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &cCore::ps_sign}, //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &cCore::ps_sign}, //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &cCore::ps_sign}, //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &cCore::Default}, //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &cCore::ps_mr}, //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &cCore::Default}, //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &cCore::ps_mergeXX}, //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &cCore::ps_mergeXX}, //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &cCore::ps_mergeXX}, //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &cCore::ps_mergeXX}, //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &cCore::Default}, //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10, &cCore::ps_sum}, //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &cCore::ps_sum}, //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &cCore::ps_muls}, //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &cCore::ps_muls}, //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &cCore::ps_maddXX}, //"ps_madds0", OPTYPE_PS, 0}},
	{15, &cCore::ps_maddXX}, //"ps_madds1", OPTYPE_PS, 0}},
	{18, &cCore::ps_arith}, //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &cCore::ps_arith}, //"ps_sub",    OPTYPE_PS, 0}},
	{21, &cCore::ps_arith}, //"ps_add",    OPTYPE_PS, 0}},
	{23, &cCore::ps_sel}, //"ps_sel",    OPTYPE_PS, 0}},
	{24, &cCore::Default}, //"ps_res",    OPTYPE_PS, 0}},
	{25, &cCore::ps_arith}, //"ps_mul",    OPTYPE_PS, 0}},
	{26, &cCore::ps_rsqrte}, //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &cCore::ps_maddXX}, //"ps_msub",   OPTYPE_PS, 0}},
	{29, &cCore::ps_maddXX}, //"ps_madd",   OPTYPE_PS, 0}},
	{30, &cCore::ps_maddXX}, //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &cCore::ps_maddXX}, //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,  &cCore::Default}, //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &cCore::Default}, //"psq_stx",  OPTYPE_PS, 0}},
	{38, &cCore::Default}, //"psq_lux",  OPTYPE_PS, 0}},
	{39, &cCore::Default}, //"psq_stux", OPTYPE_PS, 0}}, 
};

static GekkoOPTemplate table19[] = 
{
	{528, &cCore::bcctrx}, //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &cCore::bclrx}, //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &cCore::Default}, //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &cCore::Default}, //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &cCore::Default}, //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &cCore::Default}, //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &cCore::Default}, //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &cCore::Default}, //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &cCore::Default}, //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &cCore::Default}, //"crxor",  OPTYPE_CR, FL_EVIL}},
												   
	{150, &cCore::DoNothing}, //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &cCore::Default}, //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},
												   
	{50,  &cCore::rfi}, //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &cCore::Default}, //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] = 
{
	{28,  &cCore::andx}, //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &cCore::Default}, //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &cCore::orx}, //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &cCore::Default}, //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &cCore::xorx}, //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &cCore::Default}, //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &cCore::Default}, //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &cCore::Default}, //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &cCore::cmpXX}, //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &cCore::cmpXX}, //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &cCore::cntlzwx}, //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &cCore::extshx}, //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &cCore::extsbx}, //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &cCore::srwx}, //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &cCore::srawx}, //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &cCore::srawix}, //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &cCore::slwx}, //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &cCore::Default}, //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &cCore::DoNothing}, //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &cCore::Default}, //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &cCore::Default}, //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &cCore::Default}, //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &cCore::Default}, //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &cCore::dcbz}, //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &cCore::lXzx}, //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &cCore::lXzx}, //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &cCore::lXzx}, //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &cCore::lXzx}, //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &cCore::lhax}, //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &cCore::Default}, //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &cCore::lXzx}, //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &cCore::lXzx}, //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},
	
	//load byte reverse
	{534, &cCore::Default}, //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &cCore::Default}, //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &cCore::Default}, //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &cCore::Default}, //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &cCore::Default}, //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &cCore::Default}, //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &cCore::stXx}, //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &cCore::stXx}, //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &cCore::stXx}, //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &cCore::stXx}, //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &cCore::stXx}, //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &cCore::stXx}, //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &cCore::Default}, //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &cCore::Default}, //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &cCore::Default}, //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &cCore::Default}, //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store	
	{535, &cCore::lfsx}, //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &cCore::Default}, //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &cCore::Default}, //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &cCore::Default}, //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &cCore::stfsx}, //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &cCore::Default}, //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &cCore::Default}, //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &cCore::Default}, //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &cCore::Default}, //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &cCore::mfcr}, //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &cCore::mfmsr}, //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &cCore::mtcrf}, //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &cCore::mtmsr}, //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &cCore::Default}, //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &cCore::Default}, //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &cCore::mfspr}, //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &cCore::mtspr}, //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &cCore::mftb}, //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &cCore::Default}, //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &cCore::Default}, //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &cCore::Default}, //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &cCore::Default}, //"tw",     OPTYPE_SYSTEM, 0, 1}},
	{598, &cCore::DoNothing}, //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &cCore::Default}, //"icbi",   OPTYPE_SYSTEM, 0, 3}},

	// Unused instructions on GC
	{310, &cCore::Default}, //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &cCore::Default}, //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &cCore::Default}, //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &cCore::Default}, //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &cCore::Default}, //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &cCore::Default}, //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266, &cCore::addx}, //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,  &cCore::Default}, //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138, &cCore::addex}, //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234, &cCore::Default}, //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202, &cCore::addzex}, //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491, &cCore::Default}, //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459, &cCore::divwux}, //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,  &cCore::Default}, //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,  &cCore::mulhwux}, //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235, &cCore::mullwx}, //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104, &cCore::negx}, //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,  &cCore::subfx}, //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,   &cCore::subfcx}, //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136, &cCore::subfex}, //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232, &cCore::Default}, //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200, &cCore::Default}, //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] = 
{
	{18, &cCore::Default},       //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, &cCore::fp_arith_s}, //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, &cCore::fp_arith_s}, //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, &cCore::Default}, //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, &cCore::Default}, //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, &cCore::fp_arith_s}, //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, &cCore::fmaddXX}, //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, &cCore::fmaddXX}, //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, &cCore::fmaddXX}, //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, &cCore::fmaddXX}, //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264, &cCore::Default}, //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &cCore::fcmpx}, //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &cCore::fcmpx}, //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &cCore::Default}, //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &cCore::Default}, //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &cCore::fmrx}, //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &cCore::Default}, //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &cCore::Default}, //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &cCore::Default}, //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &cCore::Default}, //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &cCore::Default}, //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &cCore::Default}, //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &cCore::Default}, //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &cCore::Default}, //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &cCore::Default}, //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] = 
{
	{18, &cCore::Default}, //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &cCore::Default}, //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &cCore::Default}, //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &cCore::Default}, //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &cCore::Default}, //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &cCore::fp_arith_s}, //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &cCore::fp_arith_s}, //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &cCore::fmaddXX}, //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &cCore::fmaddXX}, //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &cCore::fmaddXX}, //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &cCore::fmaddXX}, //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};

namespace JitTables
{
void CompileInstruction(UGeckoInstruction _inst)
{
	(jit->*dynaOpTable[_inst.OPCD])(_inst);
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info) {
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)){  ///"mcrfs"
			rsplocations.push_back(jit.js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit->js.compilerPC;
	} else {
		PanicAlert("Tried to compile illegal (or unknown) instruction %08x, at %08x", _inst.hex, jit->js.compilerPC);
	}
}
void InitTables()
{
	//clear
	for (int i = 0; i < 32; i++) 
	{
		dynaOpTable59[i] = &cCore::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &cCore::unknown_instruction;
		dynaOpTable19[i] = &cCore::unknown_instruction;
		dynaOpTable31[i] = &cCore::unknown_instruction;
		dynaOpTable63[i] = &cCore::unknown_instruction;	
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
