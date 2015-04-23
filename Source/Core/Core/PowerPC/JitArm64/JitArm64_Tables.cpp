// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_Tables.h"

// Should be moved in to the Jit class
typedef void (JitArm64::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitArm64::DynaRunTable4(UGeckoInstruction inst)  {(this->*dynaOpTable4 [inst.SUBOP10])(inst);}
void JitArm64::DynaRunTable19(UGeckoInstruction inst) {(this->*dynaOpTable19[inst.SUBOP10])(inst);}
void JitArm64::DynaRunTable31(UGeckoInstruction inst) {(this->*dynaOpTable31[inst.SUBOP10])(inst);}
void JitArm64::DynaRunTable59(UGeckoInstruction inst) {(this->*dynaOpTable59[inst.SUBOP5 ])(inst);}
void JitArm64::DynaRunTable63(UGeckoInstruction inst) {(this->*dynaOpTable63[inst.SUBOP10])(inst);}

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};

static GekkoOPTemplate primarytable[] =
{
	{4,  &JitArm64::DynaRunTable4},             //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitArm64::DynaRunTable19},            //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitArm64::DynaRunTable31},            //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitArm64::DynaRunTable59},            //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitArm64::DynaRunTable63},            //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitArm64::bcx},                       //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitArm64::bx},                        //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{3,  &JitArm64::twx},                       //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArm64::sc},                        //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArm64::mulli},                     //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArm64::FallBackToInterpreter},     //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |  FL_SET_CA}},
	{10, &JitArm64::cmpli},                     //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArm64::cmpi},                      //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArm64::addic},                     //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArm64::addic},                     //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArm64::arith_imm},                 //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArm64::arith_imm},                 //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArm64::FallBackToInterpreter},     //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArm64::rlwinmx},                   //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArm64::FallBackToInterpreter},     //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArm64::arith_imm},                 //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArm64::arith_imm},                 //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArm64::arith_imm},                 //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArm64::arith_imm},                 //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArm64::arith_imm},                 //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArm64::arith_imm},                 //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArm64::lXX},                       //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArm64::lXX},                       //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArm64::lXX},                       //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArm64::lXX},                       //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArm64::lXX},                       //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArm64::lXX},                       //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArm64::lXX},                       //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArm64::lXX},                       //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArm64::stX},                       //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArm64::stX},                       //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArm64::stX},                       //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArm64::stX},                       //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArm64::stX},                       //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArm64::stX},                       //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArm64::FallBackToInterpreter},     //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArm64::FallBackToInterpreter},     //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArm64::lfXX},                      //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArm64::lfXX},                      //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArm64::lfXX},                      //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArm64::lfXX},                      //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArm64::stfXX},                     //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArm64::stfXX},                     //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArm64::stfXX},                     //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArm64::stfXX},                     //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArm64::psq_l},                     //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArm64::psq_l},                     //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArm64::psq_st},                    //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArm64::psq_st},                    //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArm64::FallBackToInterpreter},   //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArm64::FallBackToInterpreter},   //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArm64::ps_neg},                  //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArm64::ps_nabs},                 //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArm64::ps_abs},                  //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArm64::FallBackToInterpreter},   //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArm64::ps_mr},                   //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArm64::FallBackToInterpreter},   //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArm64::ps_merge00},              //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArm64::ps_merge01},              //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArm64::ps_merge10},              //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArm64::ps_merge11},              //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArm64::FallBackToInterpreter},   //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArm64::ps_sum0},                   //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArm64::ps_sum1},                   //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArm64::ps_muls0},                  //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArm64::ps_muls1},                  //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArm64::ps_madds0},                 //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArm64::ps_madds1},                 //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArm64::ps_div},                    //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArm64::ps_sub},                    //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArm64::ps_add},                    //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArm64::ps_sel},                    //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArm64::ps_res},                    //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArm64::ps_mul},                    //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArm64::FallBackToInterpreter},     //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArm64::ps_msub},                   //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArm64::ps_madd},                   //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArm64::ps_nmsub},                  //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArm64::ps_nmadd},                  //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitArm64::FallBackToInterpreter},     //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitArm64::FallBackToInterpreter},     //"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitArm64::FallBackToInterpreter},     //"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitArm64::FallBackToInterpreter},     //"psq_stux", OPTYPE_PS, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, &JitArm64::bcctrx},                   //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitArm64::bclrx},                    //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitArm64::FallBackToInterpreter},    //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitArm64::FallBackToInterpreter},    //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitArm64::FallBackToInterpreter},    //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitArm64::FallBackToInterpreter},    //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitArm64::FallBackToInterpreter},    //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitArm64::FallBackToInterpreter},    //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitArm64::FallBackToInterpreter},    //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitArm64::FallBackToInterpreter},    //"crxor",  OPTYPE_CR, FL_EVIL}},

	{150, &JitArm64::DoNothing},                //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitArm64::mcrf},                     //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},

	{50,  &JitArm64::rfi},                      //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitArm64::Break},                    //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] =
{
	{266,  &JitArm64::addx},                    //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArm64::addx},                    //"addox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArm64::addcx},                   //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &JitArm64::addcx},                   //"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArm64::FallBackToInterpreter},   //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &JitArm64::FallBackToInterpreter},   //"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArm64::FallBackToInterpreter},   //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{746,  &JitArm64::FallBackToInterpreter},   //"addmeox"
	{202,  &JitArm64::addzex},                  //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{714,  &JitArm64::addzex},                  //"addzeox"
	{491,  &JitArm64::FallBackToInterpreter},   //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArm64::FallBackToInterpreter},   //"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArm64::FallBackToInterpreter},   //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArm64::FallBackToInterpreter},   //"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArm64::FallBackToInterpreter},   //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArm64::FallBackToInterpreter},   //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArm64::mullwx},                  //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArm64::mullwx},                  //"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArm64::negx},                    //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{616,  &JitArm64::negx},                    //"negox"
	{40,   &JitArm64::subfx},                   //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArm64::subfx},                   //"subfox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArm64::FallBackToInterpreter},   //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &JitArm64::FallBackToInterpreter},   //"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArm64::FallBackToInterpreter},   //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{648,  &JitArm64::FallBackToInterpreter},   //"subfeox"
	{232,  &JitArm64::FallBackToInterpreter},   //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{744,  &JitArm64::FallBackToInterpreter},   //"subfmeox"
	{200,  &JitArm64::FallBackToInterpreter},   //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{712,  &JitArm64::FallBackToInterpreter},   //"subfzeox"

	{28,  &JitArm64::boolX},                    //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArm64::boolX},                    //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArm64::boolX},                    //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArm64::boolX},                    //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArm64::boolX},                    //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArm64::boolX},                    //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArm64::boolX},                    //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArm64::boolX},                    //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArm64::cmp},                      //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArm64::cmpl},                     //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArm64::cntlzwx},                  //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArm64::extsXx},                   //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArm64::extsXx},                   //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArm64::FallBackToInterpreter},    //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArm64::FallBackToInterpreter},    //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArm64::srawix},                   //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArm64::FallBackToInterpreter},    //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArm64::FallBackToInterpreter},   //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArm64::FallBackToInterpreter},   //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArm64::DoNothing},               //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArm64::DoNothing},               //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArm64::FallBackToInterpreter},   //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArm64::DoNothing},               //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArm64::FallBackToInterpreter},   //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArm64::lXX},                      //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArm64::lXX},                      //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArm64::lXX},                      //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArm64::lXX},                      //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArm64::lXX},                      //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArm64::lXX},                      //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArm64::lXX},                      //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArm64::lXX},                      //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArm64::lXX},                      //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArm64::lXX},                      //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArm64::FallBackToInterpreter},    //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArm64::FallBackToInterpreter},    //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArm64::FallBackToInterpreter},    //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArm64::FallBackToInterpreter},    //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArm64::stX},                      //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArm64::stX},                      //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArm64::stX},                      //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArm64::stX},                      //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArm64::stX},                      //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArm64::stX},                      //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArm64::FallBackToInterpreter},    //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArm64::FallBackToInterpreter},    //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArm64::FallBackToInterpreter},    //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArm64::FallBackToInterpreter},    //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &JitArm64::lfXX},                     //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArm64::lfXX},                     //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArm64::lfXX},                     //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArm64::lfXX},                     //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArm64::stfXX},                    //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArm64::stfXX},                    //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArm64::stfXX},                    //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArm64::stfXX},                    //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArm64::FallBackToInterpreter},    //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArm64::FallBackToInterpreter},    //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArm64::mfmsr},                    //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArm64::FallBackToInterpreter},    //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArm64::mtmsr},                    //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArm64::mtsr},                     //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArm64::mtsrin},                   //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArm64::mfspr},                    //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArm64::mtspr},                    //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArm64::mftb},                     //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArm64::FallBackToInterpreter},    //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArm64::mfsr},                     //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArm64::mfsrin},                   //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArm64::twx},                      //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &JitArm64::DoNothing},                //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitArm64::icbi},                     //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitArm64::FallBackToInterpreter},    //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitArm64::FallBackToInterpreter},    //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitArm64::DoNothing},                //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitArm64::FallBackToInterpreter},    //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitArm64::FallBackToInterpreter},    //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitArm64::DoNothing},                //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArm64::FallBackToInterpreter},     //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &JitArm64::fsubsx},                    //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm64::faddsx},                    //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//  {22, &JitArm64::FallBackToInterpreter},       //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{24, &JitArm64::FallBackToInterpreter},     //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm64::fmulsx},                    //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm64::fmsubsx},                   //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm64::fmaddsx},                   //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm64::fnmsubsx},                  //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm64::fnmaddsx},                  //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArm64::fabsx},                    //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArm64::FallBackToInterpreter},    //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArm64::FallBackToInterpreter},    //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArm64::FallBackToInterpreter},    //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArm64::FallBackToInterpreter},    //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArm64::fmrx},                     //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArm64::fnabsx},                   //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArm64::fnegx},                    //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitArm64::FallBackToInterpreter},    //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitArm64::FallBackToInterpreter},    //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitArm64::FallBackToInterpreter},    //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitArm64::FallBackToInterpreter},    //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitArm64::FallBackToInterpreter},    //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitArm64::FallBackToInterpreter},    //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitArm64::FallBackToInterpreter},    //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitArm64::FallBackToInterpreter},     //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitArm64::fsubx},                     //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm64::faddx},                     //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArm64::FallBackToInterpreter},     //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArm64::fselx},                     //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm64::fmulx},                     //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArm64::FallBackToInterpreter},     //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm64::fmsubx},                    //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm64::fmaddx},                    //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm64::fnmsubx},                   //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm64::fnmaddx},                   //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};


namespace JitArm64Tables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitArm64 *jitarm = (JitArm64 *)jit;
	(jitarm->*dynaOpTable[op.inst.OPCD])(op.inst);
	GekkoOPInfo *info = op.opinfo;
	if (info)
	{
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
	for (auto& tpl : dynaOpTable)
	{
		tpl = &JitArm64::FallBackToInterpreter;
	}

	for (int i = 0; i < 32; i++)
	{
		dynaOpTable59[i] = &JitArm64::FallBackToInterpreter;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitArm64::FallBackToInterpreter;
		dynaOpTable19[i] = &JitArm64::FallBackToInterpreter;
		dynaOpTable31[i] = &JitArm64::FallBackToInterpreter;
		dynaOpTable63[i] = &JitArm64::FallBackToInterpreter;
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
