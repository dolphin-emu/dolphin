// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArmIL/JitIL.h"
#include "Core/PowerPC/JitArmIL/JitIL_Tables.h"

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
	{4,  &JitArmIL::DynaRunTable4},             //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitArmIL::DynaRunTable19},            //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitArmIL::DynaRunTable31},            //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitArmIL::DynaRunTable59},            //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitArmIL::DynaRunTable63},            //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitArmIL::bcx},                       //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitArmIL::bx},                        //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &JitArmIL::HLEFunction},               //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitArmIL::FallBackToInterpreter},     //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitArmIL::Break},                     //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArmIL::sc},                        //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArmIL::FallBackToInterpreter},     //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArmIL::FallBackToInterpreter},     //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |  FL_SET_CA}},
	{10, &JitArmIL::cmpXX},                     //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArmIL::cmpXX},                     //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArmIL::FallBackToInterpreter},     //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArmIL::FallBackToInterpreter},     //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArmIL::reg_imm},                   //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArmIL::reg_imm},                   //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArmIL::FallBackToInterpreter},     //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArmIL::FallBackToInterpreter},     //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArmIL::FallBackToInterpreter},     //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArmIL::reg_imm},                   //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArmIL::reg_imm},                   //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArmIL::reg_imm},                   //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArmIL::reg_imm},                   //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArmIL::reg_imm},                   //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArmIL::reg_imm},                   //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArmIL::FallBackToInterpreter},     //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArmIL::FallBackToInterpreter},     //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArmIL::FallBackToInterpreter},     //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArmIL::FallBackToInterpreter},     //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArmIL::FallBackToInterpreter},     //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArmIL::FallBackToInterpreter},     //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArmIL::FallBackToInterpreter},     //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArmIL::FallBackToInterpreter},     //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArmIL::FallBackToInterpreter},     //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArmIL::FallBackToInterpreter},     //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArmIL::FallBackToInterpreter},     //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArmIL::FallBackToInterpreter},     //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArmIL::FallBackToInterpreter},     //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArmIL::FallBackToInterpreter},     //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArmIL::FallBackToInterpreter},     //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArmIL::FallBackToInterpreter},     //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArmIL::FallBackToInterpreter},     //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArmIL::FallBackToInterpreter},     //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArmIL::FallBackToInterpreter},     //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArmIL::FallBackToInterpreter},     //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArmIL::FallBackToInterpreter},     //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArmIL::FallBackToInterpreter},     //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArmIL::FallBackToInterpreter},     //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArmIL::FallBackToInterpreter},     //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArmIL::FallBackToInterpreter},     //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArmIL::FallBackToInterpreter},     //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArmIL::FallBackToInterpreter},     //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArmIL::FallBackToInterpreter},     //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitArmIL::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArmIL::FallBackToInterpreter},   //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArmIL::FallBackToInterpreter},   //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArmIL::FallBackToInterpreter},   //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArmIL::FallBackToInterpreter},   //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArmIL::FallBackToInterpreter},   //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArmIL::FallBackToInterpreter},   //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArmIL::FallBackToInterpreter},   //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArmIL::FallBackToInterpreter},   //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArmIL::FallBackToInterpreter},   //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArmIL::FallBackToInterpreter},   //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArmIL::FallBackToInterpreter},   //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArmIL::FallBackToInterpreter},   //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArmIL::FallBackToInterpreter},   //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArmIL::FallBackToInterpreter},     //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArmIL::FallBackToInterpreter},     //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArmIL::FallBackToInterpreter},     //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArmIL::FallBackToInterpreter},     //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArmIL::FallBackToInterpreter},     //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArmIL::FallBackToInterpreter},     //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArmIL::FallBackToInterpreter},     //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArmIL::FallBackToInterpreter},     //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArmIL::FallBackToInterpreter},     //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArmIL::FallBackToInterpreter},     //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArmIL::FallBackToInterpreter},     //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArmIL::FallBackToInterpreter},     //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArmIL::FallBackToInterpreter},     //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArmIL::FallBackToInterpreter},     //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArmIL::FallBackToInterpreter},     //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArmIL::FallBackToInterpreter},     //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArmIL::FallBackToInterpreter},     //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitArmIL::FallBackToInterpreter},     //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitArmIL::FallBackToInterpreter},     //"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitArmIL::FallBackToInterpreter},     //"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitArmIL::FallBackToInterpreter},     //"psq_stux", OPTYPE_PS, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, &JitArmIL::bcctrx},                   //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitArmIL::bclrx},                    //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitArmIL::crXX},                     //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitArmIL::crXX},                     //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitArmIL::crXX},                     //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitArmIL::crXX},                     //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitArmIL::crXX},                     //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitArmIL::crXX},                     //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitArmIL::crXX},                     //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitArmIL::crXX},                     //"crxor",  OPTYPE_CR, FL_EVIL}},

	{150, &JitArmIL::FallBackToInterpreter},    //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitArmIL::FallBackToInterpreter},    //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},

	{50,  &JitArmIL::rfi},                      //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitArmIL::Break},                    //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] =
{
	{28,  &JitArmIL::boolX},                    //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArmIL::boolX},                    //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArmIL::boolX},                    //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArmIL::boolX},                    //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArmIL::boolX},                    //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArmIL::boolX},                    //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArmIL::boolX},                    //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArmIL::boolX},                    //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArmIL::cmpXX},                    //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArmIL::cmpXX},                    //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArmIL::FallBackToInterpreter},    //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArmIL::FallBackToInterpreter},    //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArmIL::FallBackToInterpreter},    //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArmIL::FallBackToInterpreter},    //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArmIL::FallBackToInterpreter},    //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArmIL::FallBackToInterpreter},    //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArmIL::FallBackToInterpreter},    //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArmIL::FallBackToInterpreter},   //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArmIL::FallBackToInterpreter},   //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArmIL::FallBackToInterpreter},   //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArmIL::FallBackToInterpreter},   //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArmIL::FallBackToInterpreter},   //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArmIL::FallBackToInterpreter},   //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArmIL::FallBackToInterpreter},   //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArmIL::FallBackToInterpreter},    //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArmIL::FallBackToInterpreter},    //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArmIL::FallBackToInterpreter},    //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArmIL::FallBackToInterpreter},    //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArmIL::FallBackToInterpreter},    //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArmIL::FallBackToInterpreter},    //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArmIL::FallBackToInterpreter},    //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArmIL::FallBackToInterpreter},    //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArmIL::FallBackToInterpreter},    //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArmIL::FallBackToInterpreter},    //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArmIL::FallBackToInterpreter},    //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArmIL::FallBackToInterpreter},    //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArmIL::FallBackToInterpreter},    //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArmIL::FallBackToInterpreter},    //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArmIL::FallBackToInterpreter},    //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArmIL::FallBackToInterpreter},    //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArmIL::FallBackToInterpreter},    //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArmIL::FallBackToInterpreter},    //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArmIL::FallBackToInterpreter},    //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArmIL::FallBackToInterpreter},    //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArmIL::FallBackToInterpreter},    //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArmIL::FallBackToInterpreter},    //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArmIL::FallBackToInterpreter},    //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArmIL::FallBackToInterpreter},    //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &JitArmIL::FallBackToInterpreter},    //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArmIL::FallBackToInterpreter},    //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArmIL::FallBackToInterpreter},    //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArmIL::FallBackToInterpreter},    //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArmIL::FallBackToInterpreter},    //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArmIL::FallBackToInterpreter},    //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArmIL::FallBackToInterpreter},    //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArmIL::FallBackToInterpreter},    //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArmIL::FallBackToInterpreter},    //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArmIL::FallBackToInterpreter},    //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArmIL::FallBackToInterpreter},    //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArmIL::FallBackToInterpreter},    //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArmIL::mtmsr},                    //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArmIL::FallBackToInterpreter},    //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArmIL::FallBackToInterpreter},    //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArmIL::FallBackToInterpreter},    //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArmIL::FallBackToInterpreter},    //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArmIL::FallBackToInterpreter},    //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArmIL::FallBackToInterpreter},    //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArmIL::FallBackToInterpreter},    //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArmIL::FallBackToInterpreter},    //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArmIL::Break},                    //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &JitArmIL::FallBackToInterpreter},    //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitArmIL::icbi},                     //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitArmIL::FallBackToInterpreter},    //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitArmIL::FallBackToInterpreter},    //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitArmIL::FallBackToInterpreter},    //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitArmIL::FallBackToInterpreter},    //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitArmIL::FallBackToInterpreter},    //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitArmIL::FallBackToInterpreter},    //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] =
{
	{266,  &JitArmIL::FallBackToInterpreter},   //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArmIL::FallBackToInterpreter},   //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArmIL::FallBackToInterpreter},   //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &JitArmIL::FallBackToInterpreter},   //"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArmIL::FallBackToInterpreter},   //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &JitArmIL::FallBackToInterpreter},   //"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArmIL::FallBackToInterpreter},   //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitArmIL::FallBackToInterpreter},   //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitArmIL::FallBackToInterpreter},   //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArmIL::FallBackToInterpreter},   //"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArmIL::FallBackToInterpreter},   //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArmIL::FallBackToInterpreter},   //"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArmIL::FallBackToInterpreter},   //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArmIL::FallBackToInterpreter},   //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArmIL::FallBackToInterpreter},   //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArmIL::FallBackToInterpreter},   //"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArmIL::FallBackToInterpreter},   //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitArmIL::FallBackToInterpreter},   //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArmIL::FallBackToInterpreter},   //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArmIL::FallBackToInterpreter},   //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &JitArmIL::FallBackToInterpreter},   //"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArmIL::FallBackToInterpreter},   //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitArmIL::FallBackToInterpreter},   //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitArmIL::FallBackToInterpreter},   //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArmIL::FallBackToInterpreter},     //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &JitArmIL::FallBackToInterpreter},     //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArmIL::FallBackToInterpreter},     //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//  {22, &JitArmIL::FallBackToInterpreter},       //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{24, &JitArmIL::FallBackToInterpreter},     //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArmIL::FallBackToInterpreter},     //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArmIL::FallBackToInterpreter},     //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArmIL::FallBackToInterpreter},     //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArmIL::FallBackToInterpreter},     //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArmIL::FallBackToInterpreter},     //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArmIL::FallBackToInterpreter},    //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArmIL::FallBackToInterpreter},    //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArmIL::FallBackToInterpreter},    //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArmIL::FallBackToInterpreter},    //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArmIL::FallBackToInterpreter},    //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArmIL::FallBackToInterpreter},    //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArmIL::FallBackToInterpreter},    //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArmIL::FallBackToInterpreter},    //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitArmIL::FallBackToInterpreter},    //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitArmIL::FallBackToInterpreter},    //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitArmIL::FallBackToInterpreter},    //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitArmIL::FallBackToInterpreter},    //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitArmIL::FallBackToInterpreter},    //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitArmIL::FallBackToInterpreter},    //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitArmIL::FallBackToInterpreter},    //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitArmIL::FallBackToInterpreter},     //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitArmIL::FallBackToInterpreter},     //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArmIL::FallBackToInterpreter},     //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArmIL::FallBackToInterpreter},     //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArmIL::FallBackToInterpreter},     //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArmIL::FallBackToInterpreter},     //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArmIL::FallBackToInterpreter},     //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArmIL::FallBackToInterpreter},     //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArmIL::FallBackToInterpreter},     //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArmIL::FallBackToInterpreter},     //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArmIL::FallBackToInterpreter},     //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
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
