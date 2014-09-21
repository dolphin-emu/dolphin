// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_Tables.h"

// Should be moved in to the Jit class
typedef void (Jit64::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];
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
};

static GekkoOPTemplate primarytable[] =
{
	{4,  &Jit64::DynaRunTable4},         //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &Jit64::DynaRunTable19},        //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &Jit64::DynaRunTable31},        //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &Jit64::DynaRunTable59},        //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &Jit64::DynaRunTable63},        //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &Jit64::bcx},                   //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &Jit64::bx},                    //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &Jit64::HLEFunction},           //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &Jit64::FallBackToInterpreter}, //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &Jit64::twx},                   //"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{17, &Jit64::sc},                    //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &Jit64::mulli},                 //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &Jit64::subfic},                //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{10, &Jit64::cmpXX},                 //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &Jit64::cmpXX},                 //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &Jit64::reg_imm},               //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &Jit64::reg_imm},               //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA | FL_SET_CR0}},
	{14, &Jit64::reg_imm},               //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &Jit64::reg_imm},               //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &Jit64::rlwimix},               //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &Jit64::rlwinmx},               //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &Jit64::rlwnmx},                //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &Jit64::reg_imm},               //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &Jit64::reg_imm},               //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &Jit64::reg_imm},               //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &Jit64::reg_imm},               //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &Jit64::reg_imm},               //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &Jit64::reg_imm},               //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &Jit64::lXXx},                  //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &Jit64::lXXx},                  //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &Jit64::lXXx},                  //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &Jit64::lXXx},                  //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &Jit64::lXXx},                  //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &Jit64::lXXx},                  //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &Jit64::lXXx},                  //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &Jit64::lXXx},                  //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &Jit64::stX},                   //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &Jit64::stX},                   //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &Jit64::stX},                   //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &Jit64::stX},                   //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &Jit64::stX},                   //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &Jit64::stX},                   //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &Jit64::lmw},                   //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &Jit64::stmw},                  //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &Jit64::lfXXX},                 //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &Jit64::lfXXX},                 //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &Jit64::lfXXX},                 //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &Jit64::lfXXX},                 //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &Jit64::stfXXX},                //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &Jit64::stfXXX},                //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &Jit64::stfXXX},                //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &Jit64::stfXXX},                //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &Jit64::psq_lXX},                //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &Jit64::psq_lXX},                //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &Jit64::psq_stXX},               //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &Jit64::psq_stXX},               //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &Jit64::FallBackToInterpreter}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    &Jit64::ps_cmpXX},              //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &Jit64::ps_cmpXX},              //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &Jit64::ps_sign},               //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &Jit64::ps_sign},               //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &Jit64::ps_sign},               //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &Jit64::ps_cmpXX},              //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &Jit64::ps_mr},                 //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &Jit64::ps_cmpXX},              //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &Jit64::ps_mergeXX},            //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &Jit64::ps_mergeXX},            //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &Jit64::ps_mergeXX},            //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &Jit64::ps_mergeXX},            //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &Jit64::FallBackToInterpreter}, //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, &Jit64::ps_sum},    //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &Jit64::ps_sum},    //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &Jit64::ps_muls},   //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &Jit64::ps_muls},   //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &Jit64::ps_maddXX}, //"ps_madds0", OPTYPE_PS, 0}},
	{15, &Jit64::ps_maddXX}, //"ps_madds1", OPTYPE_PS, 0}},
	{18, &Jit64::ps_arith},  //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &Jit64::ps_arith},  //"ps_sub",    OPTYPE_PS, 0}},
	{21, &Jit64::ps_arith},  //"ps_add",    OPTYPE_PS, 0}},
	{23, &Jit64::ps_sel},    //"ps_sel",    OPTYPE_PS, 0}},
	{24, &Jit64::ps_res},    //"ps_res",    OPTYPE_PS, 0}},
	{25, &Jit64::ps_arith},  //"ps_mul",    OPTYPE_PS, 0}},
	{26, &Jit64::ps_rsqrte}, //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &Jit64::ps_maddXX}, //"ps_msub",   OPTYPE_PS, 0}},
	{29, &Jit64::ps_maddXX}, //"ps_madd",   OPTYPE_PS, 0}},
	{30, &Jit64::ps_maddXX}, //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &Jit64::ps_maddXX}, //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  &Jit64::psq_lXX},  //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &Jit64::psq_stXX}, //"psq_stx",  OPTYPE_PS, 0}},
	{38, &Jit64::psq_lXX},  //"psq_lux",  OPTYPE_PS, 0}},
	{39, &Jit64::psq_stXX}, //"psq_stux", OPTYPE_PS, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, &Jit64::bcctrx},                //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &Jit64::bclrx},                 //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &Jit64::crXXX},                 //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &Jit64::crXXX},                 //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &Jit64::crXXX},                 //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &Jit64::crXXX},                 //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &Jit64::crXXX},                 //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &Jit64::crXXX},                 //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &Jit64::crXXX},                 //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &Jit64::crXXX},                 //"crxor",  OPTYPE_CR, FL_EVIL}},

	{150, &Jit64::DoNothing},             //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &Jit64::mcrf},                  //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},

	{50,  &Jit64::rfi},                   //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &Jit64::FallBackToInterpreter}, //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] =
{
	{28,  &Jit64::boolX},                  //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &Jit64::boolX},                  //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &Jit64::boolX},                  //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &Jit64::boolX},                  //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &Jit64::boolX},                  //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &Jit64::boolX},                  //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &Jit64::boolX},                  //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &Jit64::boolX},                  //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &Jit64::cmpXX},                  //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &Jit64::cmpXX},                  //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &Jit64::cntlzwx},                //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &Jit64::extsXx},                 //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &Jit64::extsXx},                 //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &Jit64::srwx},                   //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &Jit64::srawx},                  //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_SET_CA | FL_RC_BIT}},
	{824, &Jit64::srawix},                 //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_SET_CA | FL_RC_BIT}},
	{24,  &Jit64::slwx},                   //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &Jit64::dcbst},                 //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &Jit64::FallBackToInterpreter}, //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &Jit64::DoNothing},             //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &Jit64::DoNothing},             //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &Jit64::FallBackToInterpreter}, //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &Jit64::DoNothing},             //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &Jit64::dcbz},                  //"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  &Jit64::lXXx},                   //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &Jit64::lXXx},                   //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &Jit64::lXXx},                   //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &Jit64::lXXx},                   //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &Jit64::lXXx},                   //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &Jit64::lXXx},                   //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &Jit64::lXXx},                   //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &Jit64::lXXx},                   //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &Jit64::lXXx},                   //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &Jit64::lXXx},                   //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &Jit64::FallBackToInterpreter},  //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &Jit64::FallBackToInterpreter},  //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &Jit64::FallBackToInterpreter},  //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &Jit64::FallBackToInterpreter},  //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &Jit64::stXx},                   //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &Jit64::stXx},                   //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &Jit64::stXx},                   //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &Jit64::stXx},                   //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &Jit64::stXx},                   //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &Jit64::stXx},                   //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &Jit64::stXx},                   //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &Jit64::stXx},                   //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &Jit64::FallBackToInterpreter},  //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &Jit64::FallBackToInterpreter},  //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store
	{535, &Jit64::lfXXX},                  //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &Jit64::lfXXX},                  //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &Jit64::lfXXX},                  //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &Jit64::lfXXX},                  //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &Jit64::stfXXX},                 //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &Jit64::stfXXX},                 //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &Jit64::stfXXX},                 //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &Jit64::stfXXX},                 //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &Jit64::stfiwx},                 //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &Jit64::mfcr},                   //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &Jit64::mfmsr},                  //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &Jit64::mtcrf},                  //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &Jit64::mtmsr},                  //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &Jit64::FallBackToInterpreter},  //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &Jit64::FallBackToInterpreter},  //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &Jit64::mfspr},                  //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &Jit64::mtspr},                  //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &Jit64::mftb},                   //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &Jit64::mcrxr},                  //"mcrxr",  OPTYPE_SYSTEM, FL_READ_CA | FL_SET_CA}},
	{595, &Jit64::FallBackToInterpreter},  //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &Jit64::FallBackToInterpreter},  //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &Jit64::twx},                    //"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},
	{598, &Jit64::DoNothing},              //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &Jit64::icbi},                   //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &Jit64::FallBackToInterpreter},  //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &Jit64::FallBackToInterpreter},  //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &Jit64::DoNothing},              //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &Jit64::FallBackToInterpreter},  //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &Jit64::FallBackToInterpreter},  //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &Jit64::DoNothing},              //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] =
{
	{266,  &Jit64::addx},                  //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &Jit64::addx},                  //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &Jit64::arithcx},               //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &Jit64::arithcx},               //"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &Jit64::arithXex},              //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &Jit64::arithXex},              //"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &Jit64::arithXex},              //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &Jit64::arithXex},              //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &Jit64::divwx},                 //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &Jit64::divwx},                 //"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &Jit64::divwux},                //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &Jit64::divwux},                //"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &Jit64::mulhwXx},               //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &Jit64::mulhwXx},               //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &Jit64::mullwx},                //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &Jit64::mullwx},                //"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &Jit64::negx},                  //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &Jit64::subfx},                 //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &Jit64::subfx},                 //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &Jit64::arithcx},               //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &Jit64::arithcx},               //"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &Jit64::arithXex},              //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &Jit64::arithXex},              //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &Jit64::arithXex},              //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] =
{
	{18, &Jit64::fp_arith},              //{"fdivsx",  OPTYPE_FPU, FL_RC_BIT_F, 16}},
	{20, &Jit64::fp_arith},              //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &Jit64::fp_arith},              //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}},
//	{22, &Jit64::FallBackToInterpreter},   //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, &Jit64::fresx},                 //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &Jit64::fp_arith},              //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &Jit64::fmaddXX},               //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &Jit64::fmaddXX},               //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &Jit64::fmaddXX},               //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &Jit64::fmaddXX},               //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}},
};

static GekkoOPTemplate table63[] =
{
	{264, &Jit64::fsign},                 //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &Jit64::fcmpx},                 //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &Jit64::fcmpx},                 //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &Jit64::fctiwx},                //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &Jit64::fctiwx},                //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &Jit64::fmrx},                  //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &Jit64::fsign},                 //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &Jit64::fsign},                 //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &Jit64::frspx},                 //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &Jit64::FallBackToInterpreter}, //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &Jit64::FallBackToInterpreter}, //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &Jit64::FallBackToInterpreter}, //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &Jit64::FallBackToInterpreter}, //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &Jit64::FallBackToInterpreter}, //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &Jit64::FallBackToInterpreter}, //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, &Jit64::fp_arith},              //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &Jit64::fp_arith},              //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &Jit64::fp_arith},              //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &Jit64::FallBackToInterpreter}, //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &Jit64::fselx},                 //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &Jit64::fp_arith},              //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &Jit64::frsqrtex},              //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &Jit64::fmaddXX},               //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &Jit64::fmaddXX},               //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &Jit64::fmaddXX},               //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &Jit64::fmaddXX},               //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};

namespace Jit64Tables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	Jit64 *jit64 = (Jit64 *)jit;
	(jit64->*dynaOpTable[op.inst.OPCD])(op.inst);
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
	for (auto& tpl : dynaOpTable59)
	{
		tpl = &Jit64::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &Jit64::unknown_instruction;
		dynaOpTable19[i] = &Jit64::unknown_instruction;
		dynaOpTable31[i] = &Jit64::unknown_instruction;
		dynaOpTable63[i] = &Jit64::unknown_instruction;
	}

	for (auto& tpl : primarytable)
	{
		dynaOpTable[tpl.opcode] = tpl.Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table4_2)
		{
			int op = fill+tpl.opcode;
			dynaOpTable4[op] = tpl.Inst;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (auto& tpl : table4_3)
		{
			int op = fill+tpl.opcode;
			dynaOpTable4[op] = tpl.Inst;
		}
	}

	for (auto& tpl : table4)
	{
		int op = tpl.opcode;
		dynaOpTable4[op] = tpl.Inst;
	}

	for (auto& tpl : table31)
	{
		int op = tpl.opcode;
		dynaOpTable31[op] = tpl.Inst;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (auto& tpl : table31_2)
		{
			int op = fill + tpl.opcode;
			dynaOpTable31[op] = tpl.Inst;
		}
	}

	for (auto& tpl : table19)
	{
		int op = tpl.opcode;
		dynaOpTable19[op] = tpl.Inst;
	}

	for (auto& tpl : table59)
	{
		int op = tpl.opcode;
		dynaOpTable59[op] = tpl.Inst;
	}

	for (auto& tpl : table63)
	{
		int op = tpl.opcode;
		dynaOpTable63[op] = tpl.Inst;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table63_2)
		{
			int op = fill + tpl.opcode;
			dynaOpTable63[op] = tpl.Inst;
		}
	}

	initialized = true;
}

}  // namespace
