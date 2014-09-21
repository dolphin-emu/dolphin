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

	{1,  &JitArm64::HLEFunction},               //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitArm64::FallBackToInterpreter},     //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitArm64::Break},                     //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArm64::sc},                        //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArm64::FallBackToInterpreter},     //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArm64::FallBackToInterpreter},     //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |  FL_SET_CA}},
	{10, &JitArm64::FallBackToInterpreter},     //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArm64::FallBackToInterpreter},     //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArm64::FallBackToInterpreter},     //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArm64::FallBackToInterpreter},     //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArm64::arith_imm},                 //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArm64::arith_imm},                 //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArm64::FallBackToInterpreter},     //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArm64::FallBackToInterpreter},     //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArm64::FallBackToInterpreter},     //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArm64::arith_imm},                 //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArm64::arith_imm},                 //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArm64::arith_imm},                 //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArm64::arith_imm},                 //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArm64::arith_imm},                 //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArm64::arith_imm},                 //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArm64::FallBackToInterpreter},     //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArm64::FallBackToInterpreter},     //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArm64::FallBackToInterpreter},     //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArm64::FallBackToInterpreter},     //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArm64::FallBackToInterpreter},     //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArm64::FallBackToInterpreter},     //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArm64::FallBackToInterpreter},     //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArm64::FallBackToInterpreter},     //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArm64::FallBackToInterpreter},     //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArm64::FallBackToInterpreter},     //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArm64::FallBackToInterpreter},     //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArm64::FallBackToInterpreter},     //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArm64::FallBackToInterpreter},     //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArm64::FallBackToInterpreter},     //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArm64::FallBackToInterpreter},     //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArm64::FallBackToInterpreter},     //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArm64::FallBackToInterpreter},     //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArm64::FallBackToInterpreter},     //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArm64::FallBackToInterpreter},     //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArm64::FallBackToInterpreter},     //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArm64::FallBackToInterpreter},     //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArm64::FallBackToInterpreter},     //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArm64::FallBackToInterpreter},     //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArm64::FallBackToInterpreter},     //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArm64::FallBackToInterpreter},     //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArm64::FallBackToInterpreter},     //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArm64::FallBackToInterpreter},     //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArm64::FallBackToInterpreter},     //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitArm64::FallBackToInterpreter},     //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArm64::FallBackToInterpreter},   //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArm64::FallBackToInterpreter},   //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArm64::FallBackToInterpreter},   //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArm64::FallBackToInterpreter},   //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArm64::FallBackToInterpreter},   //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArm64::FallBackToInterpreter},   //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArm64::FallBackToInterpreter},   //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArm64::FallBackToInterpreter},   //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArm64::FallBackToInterpreter},   //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArm64::FallBackToInterpreter},   //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArm64::FallBackToInterpreter},   //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArm64::FallBackToInterpreter},   //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArm64::FallBackToInterpreter},   //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArm64::FallBackToInterpreter},     //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArm64::FallBackToInterpreter},     //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArm64::FallBackToInterpreter},     //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArm64::FallBackToInterpreter},     //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArm64::FallBackToInterpreter},     //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArm64::FallBackToInterpreter},     //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArm64::FallBackToInterpreter},     //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArm64::FallBackToInterpreter},     //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArm64::FallBackToInterpreter},     //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArm64::FallBackToInterpreter},     //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArm64::FallBackToInterpreter},     //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArm64::FallBackToInterpreter},     //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArm64::FallBackToInterpreter},     //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArm64::FallBackToInterpreter},     //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArm64::FallBackToInterpreter},     //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArm64::FallBackToInterpreter},     //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArm64::FallBackToInterpreter},     //"ps_nmadd",  OPTYPE_PS, 0}},
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
	{28,  &JitArm64::boolX},                    //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArm64::boolX},                    //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArm64::boolX},                    //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArm64::boolX},                    //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArm64::boolX},                    //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArm64::boolX},                    //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArm64::boolX},                    //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArm64::boolX},                    //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArm64::FallBackToInterpreter},    //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArm64::FallBackToInterpreter},    //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArm64::cntlzwx},                  //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArm64::extsXx},                   //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArm64::extsXx},                   //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArm64::FallBackToInterpreter},    //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArm64::FallBackToInterpreter},    //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArm64::FallBackToInterpreter},    //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArm64::FallBackToInterpreter},    //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArm64::FallBackToInterpreter},   //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArm64::FallBackToInterpreter},   //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArm64::DoNothing},               //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArm64::DoNothing},               //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArm64::FallBackToInterpreter},   //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArm64::DoNothing},               //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArm64::FallBackToInterpreter},   //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArm64::FallBackToInterpreter},    //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArm64::FallBackToInterpreter},    //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArm64::FallBackToInterpreter},    //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArm64::FallBackToInterpreter},    //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArm64::FallBackToInterpreter},    //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArm64::FallBackToInterpreter},    //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArm64::FallBackToInterpreter},    //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArm64::FallBackToInterpreter},    //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArm64::FallBackToInterpreter},    //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArm64::FallBackToInterpreter},    //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArm64::FallBackToInterpreter},    //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArm64::FallBackToInterpreter},    //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArm64::FallBackToInterpreter},    //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArm64::FallBackToInterpreter},    //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArm64::FallBackToInterpreter},    //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArm64::FallBackToInterpreter},    //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArm64::FallBackToInterpreter},    //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArm64::FallBackToInterpreter},    //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArm64::FallBackToInterpreter},    //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArm64::FallBackToInterpreter},    //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArm64::FallBackToInterpreter},    //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArm64::FallBackToInterpreter},    //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArm64::FallBackToInterpreter},    //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArm64::FallBackToInterpreter},    //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &JitArm64::FallBackToInterpreter},    //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArm64::FallBackToInterpreter},    //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArm64::FallBackToInterpreter},    //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArm64::FallBackToInterpreter},    //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArm64::FallBackToInterpreter},    //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArm64::FallBackToInterpreter},    //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArm64::FallBackToInterpreter},    //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArm64::FallBackToInterpreter},    //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArm64::FallBackToInterpreter},    //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArm64::FallBackToInterpreter},    //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArm64::FallBackToInterpreter},    //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArm64::FallBackToInterpreter},    //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArm64::mtmsr},                    //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArm64::FallBackToInterpreter},    //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArm64::FallBackToInterpreter},    //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArm64::FallBackToInterpreter},    //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArm64::FallBackToInterpreter},    //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArm64::FallBackToInterpreter},    //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArm64::FallBackToInterpreter},    //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArm64::FallBackToInterpreter},    //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArm64::FallBackToInterpreter},    //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArm64::Break},                    //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
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

static GekkoOPTemplate table31_2[] =
{
	{266,  &JitArm64::FallBackToInterpreter},   //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArm64::FallBackToInterpreter},   //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArm64::FallBackToInterpreter},   //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &JitArm64::FallBackToInterpreter},   //"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArm64::FallBackToInterpreter},   //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &JitArm64::FallBackToInterpreter},   //"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArm64::FallBackToInterpreter},   //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitArm64::FallBackToInterpreter},   //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitArm64::FallBackToInterpreter},   //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArm64::FallBackToInterpreter},   //"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArm64::FallBackToInterpreter},   //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArm64::FallBackToInterpreter},   //"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArm64::FallBackToInterpreter},   //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArm64::FallBackToInterpreter},   //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArm64::FallBackToInterpreter},   //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArm64::FallBackToInterpreter},   //"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArm64::negx},                    //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitArm64::FallBackToInterpreter},   //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArm64::FallBackToInterpreter},   //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArm64::FallBackToInterpreter},   //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &JitArm64::FallBackToInterpreter},   //"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArm64::FallBackToInterpreter},   //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitArm64::FallBackToInterpreter},   //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitArm64::FallBackToInterpreter},   //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArm64::FallBackToInterpreter},     //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &JitArm64::FallBackToInterpreter},     //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm64::FallBackToInterpreter},     //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//  {22, &JitArm64::FallBackToInterpreter},       //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{24, &JitArm64::FallBackToInterpreter},     //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm64::FallBackToInterpreter},     //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm64::FallBackToInterpreter},     //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm64::FallBackToInterpreter},     //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm64::FallBackToInterpreter},     //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm64::FallBackToInterpreter},     //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArm64::FallBackToInterpreter},    //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArm64::FallBackToInterpreter},    //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArm64::FallBackToInterpreter},    //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArm64::FallBackToInterpreter},    //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArm64::FallBackToInterpreter},    //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArm64::FallBackToInterpreter},    //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArm64::FallBackToInterpreter},    //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArm64::FallBackToInterpreter},    //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
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
	{20, &JitArm64::FallBackToInterpreter},     //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm64::FallBackToInterpreter},     //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArm64::FallBackToInterpreter},     //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArm64::FallBackToInterpreter},     //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm64::FallBackToInterpreter},     //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArm64::FallBackToInterpreter},     //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm64::FallBackToInterpreter},     //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm64::FallBackToInterpreter},     //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm64::FallBackToInterpreter},     //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm64::FallBackToInterpreter},     //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
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
	for (int i = 0; i < 32; i++)
	{
		dynaOpTable59[i] = &JitArm64::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitArm64::unknown_instruction;
		dynaOpTable19[i] = &JitArm64::unknown_instruction;
		dynaOpTable31[i] = &JitArm64::unknown_instruction;
		dynaOpTable63[i] = &JitArm64::unknown_instruction;
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
