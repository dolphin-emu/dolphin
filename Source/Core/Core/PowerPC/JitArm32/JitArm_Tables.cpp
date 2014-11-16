// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_Tables.h"

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
	{4,  &JitArm::DynaRunTable4},         //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitArm::DynaRunTable19},        //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitArm::DynaRunTable31},        //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitArm::DynaRunTable59},        //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitArm::DynaRunTable63},        //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitArm::bcx},                   //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitArm::bx},                    //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &JitArm::HLEFunction},           //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitArm::FallBackToInterpreter}, //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitArm::twx},                   //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &JitArm::sc},                    //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitArm::arith},                 //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitArm::subfic},                //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{10, &JitArm::FallBackToInterpreter}, //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitArm::cmpi},                  //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitArm::arith},                 //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitArm::arith},                 //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitArm::arith},                 //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitArm::arith},                 //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitArm::rlwimix},               //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitArm::rlwinmx},               //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitArm::rlwnmx},                //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitArm::arith},                 //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitArm::arith},                 //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitArm::arith},                 //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitArm::arith},                 //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitArm::arith},                 //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitArm::arith},                 //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitArm::lXX},                   //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitArm::lXX},                   //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitArm::lXX},                   //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitArm::lXX},                   //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitArm::lXX},                   //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitArm::lXX},                   //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitArm::lXX},                   //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitArm::lXX},                   //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitArm::stX},                   //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitArm::stX},                   //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitArm::stX},                   //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitArm::stX},                   //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitArm::stX},                   //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitArm::stX},                   //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitArm::lmw},                   //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitArm::stmw},                  //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitArm::lfXX},                  //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitArm::lfXX},                  //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitArm::lfXX},                  //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitArm::lfXX},                  //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitArm::stfXX},                 //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitArm::stfXX},                 //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitArm::stfXX},                 //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitArm::stfXX},                 //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitArm::psq_l},                 //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitArm::psq_l},                 //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitArm::psq_st},                //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitArm::psq_st},                //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitArm::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &JitArm::FallBackToInterpreter}, //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitArm::FallBackToInterpreter}, //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitArm::ps_neg},                //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitArm::ps_nabs},               //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitArm::ps_abs},                //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitArm::FallBackToInterpreter}, //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitArm::ps_mr},                 //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitArm::FallBackToInterpreter}, //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitArm::ps_merge00},            //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitArm::ps_merge01},            //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitArm::ps_merge10},            //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitArm::ps_merge11},            //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitArm::FallBackToInterpreter}, //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &JitArm::ps_sum0},   //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitArm::ps_sum1},   //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitArm::ps_muls0},  //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitArm::ps_muls1},  //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitArm::ps_madds0}, //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitArm::ps_madds1}, //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitArm::ps_div},    //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitArm::ps_sub},    //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitArm::ps_add},    //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitArm::ps_sel},    //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitArm::ps_res},    //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitArm::ps_mul},    //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitArm::ps_rsqrte}, //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitArm::ps_msub},   //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitArm::ps_madd},   //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitArm::ps_nmsub},  //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitArm::ps_nmadd},  //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &JitArm::psq_lx},  //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitArm::psq_stx}, //"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitArm::psq_lx},  //"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitArm::psq_stx}, //"psq_stux", OPTYPE_PS, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, &JitArm::bcctrx},    //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitArm::bclrx},     //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitArm::FallBackToInterpreter},     //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitArm::FallBackToInterpreter},     //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitArm::FallBackToInterpreter},     //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitArm::FallBackToInterpreter},     //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitArm::FallBackToInterpreter},     //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitArm::FallBackToInterpreter},     //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitArm::FallBackToInterpreter},     //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitArm::FallBackToInterpreter},     //"crxor",  OPTYPE_CR, FL_EVIL}},

	{150, &JitArm::DoNothing}, //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitArm::mcrf},      //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},

	{50,  &JitArm::rfi},       //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitArm::Break},     //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] =
{
	{28,  &JitArm::arith},                  //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitArm::arith},                  //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitArm::arith},                  //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitArm::arith},                  //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitArm::arith},                  //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitArm::arith},                  //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitArm::arith},                  //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitArm::arith},                  //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitArm::cmp},                    //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitArm::FallBackToInterpreter},  //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitArm::cntlzwx},                //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitArm::extshx},                 //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitArm::extsbx},                 //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitArm::arith},                  //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitArm::arith},                  //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitArm::srawix},                 //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitArm::arith},                  //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitArm::dcbst},                 //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitArm::FallBackToInterpreter}, //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitArm::DoNothing},             //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitArm::DoNothing},             //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitArm::FallBackToInterpreter}, //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitArm::DoNothing},             //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitArm::FallBackToInterpreter}, //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &JitArm::lXX},                    //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitArm::FallBackToInterpreter},  //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitArm::lXX},                    //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitArm::lXX},                    //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitArm::lXX},                    //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitArm::lXX},                    //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitArm::lXX},                    //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitArm::lXX},                    //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitArm::lXX},                    //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitArm::lXX},                    //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitArm::FallBackToInterpreter},  //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitArm::FallBackToInterpreter},  //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitArm::FallBackToInterpreter},  //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitArm::FallBackToInterpreter},  //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitArm::stX},                    //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitArm::stX},                    //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitArm::stX},                    //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitArm::stX},                    //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitArm::stX},                    //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitArm::stX},                    //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitArm::FallBackToInterpreter},  //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitArm::FallBackToInterpreter},  //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitArm::FallBackToInterpreter},  //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitArm::FallBackToInterpreter},  //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &JitArm::lfXX},                   //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitArm::lfXX},                   //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitArm::lfXX},                   //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitArm::lfXX},                   //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitArm::stfXX},                  //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitArm::stfXX},                  //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitArm::stfXX},                  //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitArm::stfXX},                  //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitArm::FallBackToInterpreter},  //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitArm::FallBackToInterpreter},  //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitArm::mfmsr},                  //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitArm::FallBackToInterpreter},  //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitArm::mtmsr},                  //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitArm::mtsr},                   //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitArm::FallBackToInterpreter},  //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitArm::mfspr},                  //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitArm::mtspr},                  //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitArm::mftb},                   //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitArm::FallBackToInterpreter},  //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitArm::mfsr},                   //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitArm::FallBackToInterpreter},  //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitArm::twx},                    //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &JitArm::DoNothing},              //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitArm::icbi},                   //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitArm::FallBackToInterpreter},  //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitArm::FallBackToInterpreter},  //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitArm::DoNothing},              //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitArm::FallBackToInterpreter},  //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitArm::FallBackToInterpreter},  //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitArm::DoNothing},              //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] =
{
	{266,  &JitArm::arith},                 //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitArm::arith},                 //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitArm::arith},                 //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &JitArm::arith},                 //"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitArm::addex},                 //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &JitArm::addex},                 //"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitArm::FallBackToInterpreter}, //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitArm::FallBackToInterpreter}, //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitArm::FallBackToInterpreter}, //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitArm::FallBackToInterpreter}, //"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitArm::FallBackToInterpreter}, //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitArm::FallBackToInterpreter}, //"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitArm::FallBackToInterpreter}, //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitArm::mulhwux},               //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitArm::arith},                 //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitArm::arith},                 //"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitArm::negx},                  //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitArm::arith},                 //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitArm::arith},                 //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitArm::FallBackToInterpreter}, //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &JitArm::FallBackToInterpreter}, //"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitArm::FallBackToInterpreter}, //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitArm::FallBackToInterpreter}, //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitArm::FallBackToInterpreter}, //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] =
{
	{18, &JitArm::FallBackToInterpreter},  //{"fdivsx",  OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &JitArm::fsubsx},                 //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm::faddsx},                 //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//	{22, &JitArm::FallBackToInterpreter},    //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, &JitArm::fresx},                  //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm::fmulsx},                 //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm::FallBackToInterpreter},  //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm::fmaddsx},                //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm::FallBackToInterpreter},  //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm::fnmaddsx},               //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &JitArm::fabsx},                 //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitArm::FallBackToInterpreter}, //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitArm::FallBackToInterpreter}, //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitArm::fctiwx},                //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitArm::fctiwzx},               //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitArm::fmrx},                  //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitArm::fnabsx},                //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitArm::fnegx},                 //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitArm::FallBackToInterpreter}, //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitArm::FallBackToInterpreter}, //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitArm::FallBackToInterpreter}, //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitArm::FallBackToInterpreter}, //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitArm::FallBackToInterpreter}, //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitArm::FallBackToInterpreter}, //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitArm::FallBackToInterpreter}, //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, &JitArm::FallBackToInterpreter}, //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitArm::fsubx},                 //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitArm::faddx},                 //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitArm::FallBackToInterpreter}, //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitArm::fselx},                 //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitArm::fmulx},                 //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitArm::frsqrtex},              //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitArm::FallBackToInterpreter}, //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitArm::fmaddx},                //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitArm::FallBackToInterpreter}, //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitArm::fnmaddx},               //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};


namespace JitArmTables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitArm *jitarm = (JitArm *)jit;
	(jitarm->*dynaOpTable[op.inst.OPCD])(op.inst);
	GekkoOPInfo *info = op.opinfo;

	if (info)
	{
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)) // "mcrfs"
		{
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
