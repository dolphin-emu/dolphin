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

#include "Jit.h"
#include "../JitInterface.h"
#include "JitArm_Tables.h"

// Should be moved in to the Jit class
typedef void (JitArm::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitArm::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void JitArm::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void JitArm::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};

static GekkoOPTemplate primarytable[] = 
{
	{4,  &JitArm::DynaRunTable4}, //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitArm::DynaRunTable19}, //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitArm::DynaRunTable31}, //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitArm::DynaRunTable59}, //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitArm::DynaRunTable63}, //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitArm::bcx}, //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitArm::bx}, //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &JitArm::HLEFunction}, //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitArm::Default}, //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitArm::Break}, //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArm::sc}, //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArm::mulli}, //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArm::Default}, //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, &JitArm::cmpli}, //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArm::cmpi}, //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArm::Default}, //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArm::Default}, //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArm::addi}, //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArm::addis}, //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArm::rlwimix}, //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArm::rlwinmx}, //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArm::Default}, //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArm::ori}, //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArm::oris}, //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArm::Default}, //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArm::Default}, //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArm::andi_rc}, //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArm::andis_rc}, //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArm::lwz}, //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArm::Default}, //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArm::lbz}, //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArm::Default}, //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArm::lhz}, //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArm::Default}, //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArm::lha}, //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArm::Default}, //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArm::sth}, //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArm::sthu}, //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArm::stw}, //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArm::stwu}, //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArm::stb}, //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArm::stbu}, //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArm::Default}, //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArm::Default}, //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArm::lfs}, //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArm::Default}, //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArm::lfd}, //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArm::Default}, //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArm::Default}, //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArm::Default}, //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArm::Default}, //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArm::Default}, //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArm::Default}, //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArm::Default}, //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArm::Default}, //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArm::Default}, //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitArm::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,    &JitArm::Default}, //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArm::Default}, //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArm::Default}, //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArm::Default}, //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArm::Default}, //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArm::Default}, //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArm::Default}, //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArm::Default}, //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArm::Default}, //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArm::Default}, //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArm::Default}, //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArm::Default}, //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArm::Default}, //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10, &JitArm::ps_sum0}, //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArm::Default}, //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArm::Default}, //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArm::Default}, //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArm::Default}, //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArm::Default}, //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArm::Default}, //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArm::ps_sub}, //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArm::ps_add}, //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArm::Default}, //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArm::Default}, //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArm::ps_mul}, //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArm::Default}, //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArm::Default}, //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArm::ps_madd}, //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArm::Default}, //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArm::Default}, //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,  &JitArm::Default}, //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitArm::Default}, //"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitArm::Default}, //"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitArm::Default}, //"psq_stux", OPTYPE_PS, 0}}, 
};

static GekkoOPTemplate table19[] = 
{
	{528, &JitArm::bcctrx}, //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitArm::bclrx}, //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitArm::Default}, //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitArm::Default}, //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitArm::Default}, //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitArm::Default}, //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitArm::Default}, //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitArm::Default}, //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitArm::Default}, //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitArm::Default}, //"crxor",  OPTYPE_CR, FL_EVIL}},
												   
	{150, &JitArm::DoNothing}, //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitArm::Default}, //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},
												   
	{50,  &JitArm::rfi}, //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitArm::Break}, //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] = 
{
	{28,  &JitArm::andx}, //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArm::Default}, //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArm::orx}, //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArm::Default}, //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArm::xorx}, //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArm::Default}, //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArm::Default}, //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArm::Default}, //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArm::cmp}, //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArm::cmpl}, //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArm::Default}, //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArm::extshx}, //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArm::extsbx}, //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArm::Default}, //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArm::Default}, //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArm::srawix}, //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArm::Default}, //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArm::dcbst}, //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArm::Default}, //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArm::Default}, //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArm::Default}, //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArm::Default}, //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArm::Default}, //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArm::Default}, //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArm::lwzx}, //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArm::Default}, //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArm::Default}, //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArm::Default}, //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArm::Default}, //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArm::Default}, //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArm::Default}, //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArm::Default}, //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArm::Default}, //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArm::Default}, //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArm::Default}, //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArm::Default}, //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArm::Default}, //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArm::Default}, //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArm::Default}, //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArm::Default}, //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArm::Default}, //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArm::Default}, //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArm::Default}, //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArm::Default}, //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArm::Default}, //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArm::Default}, //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArm::Default}, //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArm::Default}, //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store	
	{535, &JitArm::Default}, //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArm::Default}, //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArm::Default}, //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArm::Default}, //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArm::Default}, //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArm::Default}, //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArm::Default}, //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArm::Default}, //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArm::Default}, //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArm::Default}, //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArm::mfmsr}, //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArm::Default}, //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArm::mtmsr}, //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArm::Default}, //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArm::Default}, //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArm::mfspr}, //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArm::mtspr}, //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArm::mftb}, //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArm::Default}, //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArm::Default}, //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArm::Default}, //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArm::Break}, //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &JitArm::Default}, //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitArm::icbi}, //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitArm::Default}, //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitArm::Default}, //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitArm::Default}, //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitArm::Default}, //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitArm::Default}, //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitArm::Default}, //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266,  &JitArm::addx}, //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArm::addx}, //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArm::addcx}, //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArm::addex}, //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArm::Default}, //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitArm::Default}, //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitArm::Default}, //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArm::Default}, //"divwox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArm::Default}, //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArm::Default}, //"divwuox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArm::Default}, //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArm::mulhwux}, //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArm::mullwx}, //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArm::Default}, //"mullwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArm::negx}, //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitArm::subfx}, //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArm::Default}, //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArm::Default}, //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArm::Default}, //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitArm::Default}, //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitArm::Default}, //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] = 
{
	{18, &JitArm::Default},       //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, &JitArm::fsubsx}, //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, &JitArm::faddsx}, //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, &JitArm::Default}, //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, &JitArm::Default}, //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, &JitArm::fmulsx}, //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, &JitArm::Default}, //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, &JitArm::Default}, //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, &JitArm::Default}, //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, &JitArm::Default}, //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264, &JitArm::fabsx},   //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArm::Default},   //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArm::Default},   //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArm::Default}, //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArm::Default}, //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArm::fmrx},    //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArm::Default},   //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArm::Default},   //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitArm::Default}, //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitArm::Default}, //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitArm::Default}, //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitArm::Default}, //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitArm::Default}, //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitArm::Default}, //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitArm::Default}, //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] = 
{
	{18, &JitArm::Default}, //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitArm::fsubx}, //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm::faddx}, //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArm::Default}, //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArm::Default}, //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm::fmulx}, //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArm::Default}, //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm::Default}, //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm::Default}, //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm::Default}, //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm::Default}, //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};


namespace JitArmTables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitArm *jitarm = (JitArm *)jit;
	(jitarm->*dynaOpTable[op.inst.OPCD])(op.inst);
	GekkoOPInfo *info = op.opinfo;
	if (info) {
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)){  ///"mcrfs"
			rsplocations.push_back(jit.js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit->js.compilerPC;
	}
}

void InitTables()
{
	// once initialized, tables are read-only
	static bool initialized = false;
	if (initialized)
		return;

	//clear
	for (int i = 0; i < 32; i++) 
	{
		dynaOpTable59[i] = &JitArm::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitArm::unknown_instruction;
		dynaOpTable19[i] = &JitArm::unknown_instruction;
		dynaOpTable31[i] = &JitArm::unknown_instruction;
		dynaOpTable63[i] = &JitArm::unknown_instruction;	
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

	initialized = true;

}

}  // namespace
