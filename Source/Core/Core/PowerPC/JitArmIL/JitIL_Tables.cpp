// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "JitIL.h"
#include "../JitInterface.h"
#include "JitIL_Tables.h"

// Should be moved in to the Jit class
typedef void (JitArmIL::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitArmIL::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void JitArmIL::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void JitArmIL::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void JitArmIL::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void JitArmIL::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};

static GekkoOPTemplate primarytable[] =
{
	{4,  &JitArmIL::DynaRunTable4},	//"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitArmIL::DynaRunTable19},	//"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitArmIL::DynaRunTable31},	//"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitArmIL::DynaRunTable59},	//"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitArmIL::DynaRunTable63},	//"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitArmIL::bcx},		//"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitArmIL::bx},		//"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &JitArmIL::HLEFunction},		//"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitArmIL::Default},		//"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitArmIL::Break},		//"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArmIL::sc},		//"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArmIL::Default},		//"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArmIL::Default},		//"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, &JitArmIL::cmpXX},		//"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArmIL::cmpXX},		//"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArmIL::Default},		//"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArmIL::Default},		//"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArmIL::reg_imm},		//"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArmIL::reg_imm},		//"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArmIL::Default},		//"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArmIL::Default},		//"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArmIL::Default},		//"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArmIL::reg_imm},		//"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArmIL::reg_imm},		//"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArmIL::reg_imm},		//"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArmIL::reg_imm},		//"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArmIL::reg_imm},		//"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArmIL::reg_imm},		//"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArmIL::Default},		//"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArmIL::Default},		//"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArmIL::Default},		//"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArmIL::Default},		//"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArmIL::Default},		//"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArmIL::Default},		//"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArmIL::Default},		//"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArmIL::Default},		//"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArmIL::Default},		//"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArmIL::Default},		//"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArmIL::Default},		//"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArmIL::Default},		//"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArmIL::Default},		//"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArmIL::Default},		//"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArmIL::Default},		//"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArmIL::Default},		//"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArmIL::Default},		//"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArmIL::Default},		//"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArmIL::Default},		//"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArmIL::Default},		//"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArmIL::Default},		//"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArmIL::Default},		//"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArmIL::Default},		//"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArmIL::Default},		//"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArmIL::Default},		//"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArmIL::Default},		//"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArmIL::Default},		//"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArmIL::Default},		//"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitArmIL::Default},		//"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArmIL::Default},		//"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArmIL::Default},		//"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArmIL::Default},		//"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArmIL::Default},		//"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArmIL::Default},		//"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArmIL::Default},		//"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArmIL::Default},		//"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArmIL::Default},		//"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArmIL::Default},		//"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArmIL::Default},		//"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArmIL::Default},		//"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArmIL::Default},		//"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArmIL::Default},		//"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArmIL::Default},		//"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArmIL::Default},		//"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArmIL::Default},		//"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArmIL::Default},		//"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArmIL::Default},		//"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArmIL::Default},		//"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArmIL::Default},		//"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArmIL::Default},		//"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArmIL::Default},		//"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArmIL::Default},		//"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArmIL::Default},		//"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArmIL::Default},		//"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArmIL::Default},		//"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArmIL::Default},		//"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArmIL::Default},		//"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArmIL::Default},		//"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArmIL::Default},		//"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitArmIL::Default},		//"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitArmIL::Default},		//"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitArmIL::Default},		//"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitArmIL::Default},		//"psq_stux", OPTYPE_PS, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, &JitArmIL::bcctrx},		//"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitArmIL::bclrx},		//"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitArmIL::crXX},		//"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitArmIL::crXX},		//"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitArmIL::crXX},		//"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitArmIL::crXX},		//"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitArmIL::crXX},		//"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitArmIL::crXX},		//"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitArmIL::crXX},		//"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitArmIL::crXX},		//"crxor",  OPTYPE_CR, FL_EVIL}},

	{150, &JitArmIL::Default},		//"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitArmIL::Default},		//"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},

	{50,  &JitArmIL::rfi},		//"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitArmIL::Break},		//"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] =
{
	{28,  &JitArmIL::boolX},		//"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArmIL::boolX},		//"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArmIL::boolX},		//"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArmIL::boolX},		//"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArmIL::boolX},		//"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArmIL::boolX},		//"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArmIL::boolX},		//"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArmIL::boolX},		//"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArmIL::cmpXX},		//"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArmIL::cmpXX},		//"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArmIL::Default},		//"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArmIL::Default},		//"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArmIL::Default},		//"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArmIL::Default},		//"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArmIL::Default},		//"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArmIL::Default},		//"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArmIL::Default},		//"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArmIL::Default},		//"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArmIL::Default},		//"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArmIL::Default},		//"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArmIL::Default},		//"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArmIL::Default},		//"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArmIL::Default},		//"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArmIL::Default},		//"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArmIL::Default},		//"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArmIL::Default},		//"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArmIL::Default},		//"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArmIL::Default},		//"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArmIL::Default},		//"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArmIL::Default},		//"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArmIL::Default},		//"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArmIL::Default},		//"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArmIL::Default},		//"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArmIL::Default},		//"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArmIL::Default},		//"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArmIL::Default},		//"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArmIL::Default},		//"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArmIL::Default},		//"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArmIL::Default},		//"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArmIL::Default},		//"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArmIL::Default},		//"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArmIL::Default},		//"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArmIL::Default},		//"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArmIL::Default},		//"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArmIL::Default},		//"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArmIL::Default},		//"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArmIL::Default},		//"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArmIL::Default},		//"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &JitArmIL::Default},		//"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArmIL::Default},		//"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArmIL::Default},		//"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArmIL::Default},		//"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArmIL::Default},		//"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArmIL::Default},		//"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArmIL::Default},		//"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArmIL::Default},		//"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArmIL::Default},		//"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArmIL::Default},		//"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArmIL::Default},		//"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArmIL::Default},		//"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArmIL::mtmsr},		//"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArmIL::Default},		//"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArmIL::Default},		//"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArmIL::Default},		//"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArmIL::Default},		//"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArmIL::Default},		//"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArmIL::Default},		//"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArmIL::Default},		//"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArmIL::Default},		//"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArmIL::Break},		//"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &JitArmIL::Default},		//"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitArmIL::icbi},		//"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitArmIL::Default},		//"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitArmIL::Default},		//"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitArmIL::Default},		//"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitArmIL::Default},		//"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitArmIL::Default},		//"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitArmIL::Default},		//"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] =
{
	{266,  &JitArmIL::Default},		//"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArmIL::Default},		//"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArmIL::Default},		//"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArmIL::Default},		//"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArmIL::Default},		//"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitArmIL::Default},		//"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitArmIL::Default},		//"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArmIL::Default},		//"divwox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArmIL::Default},		//"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArmIL::Default},		//"divwuox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArmIL::Default},		//"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArmIL::Default},		//"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArmIL::Default},		//"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArmIL::Default},		//"mullwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArmIL::Default},		//"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitArmIL::Default},		//"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArmIL::Default},		//"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArmIL::Default},		//"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArmIL::Default},		//"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitArmIL::Default},		//"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitArmIL::Default},		//"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArmIL::Default},		//{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &JitArmIL::Default},		//"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArmIL::Default},		//"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//	{22, &JitArmIL::Default},		//"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{24, &JitArmIL::Default},		//"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArmIL::Default},		//"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArmIL::Default},		//"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArmIL::Default},		//"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArmIL::Default},		//"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArmIL::Default},		//"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArmIL::Default},		//"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArmIL::Default},		//"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArmIL::Default},		//"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArmIL::Default},		//"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArmIL::Default},		//"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArmIL::Default},		//"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArmIL::Default},		//"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArmIL::Default},		//"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitArmIL::Default},		//"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitArmIL::Default},		//"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitArmIL::Default},		//"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitArmIL::Default},		//"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitArmIL::Default},		//"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitArmIL::Default},		//"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitArmIL::Default},		//"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitArmIL::Default},		//"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitArmIL::Default},		//"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArmIL::Default},		//"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArmIL::Default},		//"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArmIL::Default},		//"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArmIL::Default},		//"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArmIL::Default},		//"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArmIL::Default},		//"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArmIL::Default},		//"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArmIL::Default},		//"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArmIL::Default},		//"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};


namespace JitArmILTables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitArmIL *jitarm = (JitArmIL *)jit;
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
		dynaOpTable59[i] = &JitArmIL::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitArmIL::unknown_instruction;
		dynaOpTable19[i] = &JitArmIL::unknown_instruction;
		dynaOpTable31[i] = &JitArmIL::unknown_instruction;
		dynaOpTable63[i] = &JitArmIL::unknown_instruction;
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
