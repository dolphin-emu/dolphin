// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "JitIL_Tables.h"

// Should be moved in to the Jit class
typedef void (JitIL::*_Instruction) (UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitIL::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void JitIL::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void JitIL::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void JitIL::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void JitIL::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}



struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	//GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};

static GekkoOPTemplate primarytable[] = 
{
	{4,  &JitIL::DynaRunTable4}, //"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, &JitIL::DynaRunTable19}, //"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, &JitIL::DynaRunTable31}, //"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, &JitIL::DynaRunTable59}, //"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, &JitIL::DynaRunTable63}, //"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, &JitIL::bcx}, //"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, &JitIL::bx}, //"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  &JitIL::HLEFunction}, //"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  &JitIL::Default}, //"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  &JitIL::Default}, //"twi",         OPTYPE_SYSTEM, 0}},
	{17, &JitIL::sc}, //"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  &JitIL::mulli}, //"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  &JitIL::subfic}, //"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, &JitIL::cmpXX}, //"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, &JitIL::cmpXX}, //"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, &JitIL::reg_imm}, //"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, &JitIL::reg_imm}, //"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, &JitIL::reg_imm}, //"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, &JitIL::reg_imm}, //"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, &JitIL::rlwimix}, //"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, &JitIL::rlwinmx}, //"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, &JitIL::rlwnmx}, //"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, &JitIL::reg_imm}, //"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, &JitIL::reg_imm}, //"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, &JitIL::reg_imm}, //"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, &JitIL::reg_imm}, //"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, &JitIL::reg_imm}, //"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, &JitIL::reg_imm}, //"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, &JitIL::lXz}, //"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, &JitIL::lXz}, //"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, &JitIL::lXz}, //"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, &JitIL::lbzu}, //"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, &JitIL::lXz}, //"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, &JitIL::lXz}, //"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, &JitIL::lha}, //"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, &JitIL::Default}, //"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, &JitIL::stX}, //"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, &JitIL::stX}, //"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, &JitIL::stX}, //"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, &JitIL::stX}, //"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, &JitIL::stX}, //"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, &JitIL::stX}, //"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, &JitIL::lmw}, //"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, &JitIL::stmw}, //"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, &JitIL::lfs}, //"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, &JitIL::Default}, //"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, &JitIL::lfd}, //"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, &JitIL::Default}, //"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, &JitIL::stfs}, //"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, &JitIL::stfs}, //"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, &JitIL::stfd}, //"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, &JitIL::Default}, //"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, &JitIL::psq_l}, //"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, &JitIL::psq_l}, //"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, &JitIL::psq_st}, //"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, &JitIL::psq_st}, //"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, &JitIL::Default}, //"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,    &JitIL::Default}, //"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   &JitIL::Default}, //"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   &JitIL::ps_sign}, //"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  &JitIL::ps_sign}, //"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  &JitIL::ps_sign}, //"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   &JitIL::Default}, //"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   &JitIL::ps_mr}, //"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   &JitIL::Default}, //"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  &JitIL::ps_mergeXX}, //"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  &JitIL::ps_mergeXX}, //"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  &JitIL::ps_mergeXX}, //"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  &JitIL::ps_mergeXX}, //"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, &JitIL::Default}, //"dcbz_l",     OPTYPE_SYSTEM, 0}},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10, &JitIL::ps_sum}, //"ps_sum0",   OPTYPE_PS, 0}},
	{11, &JitIL::ps_sum}, //"ps_sum1",   OPTYPE_PS, 0}},
	{12, &JitIL::ps_muls}, //"ps_muls0",  OPTYPE_PS, 0}},
	{13, &JitIL::ps_muls}, //"ps_muls1",  OPTYPE_PS, 0}},
	{14, &JitIL::ps_maddXX}, //"ps_madds0", OPTYPE_PS, 0}},
	{15, &JitIL::ps_maddXX}, //"ps_madds1", OPTYPE_PS, 0}},
	{18, &JitIL::ps_arith}, //"ps_div",    OPTYPE_PS, 0, 16}},
	{20, &JitIL::ps_arith}, //"ps_sub",    OPTYPE_PS, 0}},
	{21, &JitIL::ps_arith}, //"ps_add",    OPTYPE_PS, 0}},
	{23, &JitIL::ps_sel}, //"ps_sel",    OPTYPE_PS, 0}},
	{24, &JitIL::Default}, //"ps_res",    OPTYPE_PS, 0}},
	{25, &JitIL::ps_arith}, //"ps_mul",    OPTYPE_PS, 0}},
	{26, &JitIL::ps_rsqrte}, //"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, &JitIL::ps_maddXX}, //"ps_msub",   OPTYPE_PS, 0}},
	{29, &JitIL::ps_maddXX}, //"ps_madd",   OPTYPE_PS, 0}},
	{30, &JitIL::ps_maddXX}, //"ps_nmsub",  OPTYPE_PS, 0}},
	{31, &JitIL::ps_maddXX}, //"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,  &JitIL::Default}, //"psq_lx",   OPTYPE_PS, 0}},
	{7,  &JitIL::Default}, //"psq_stx",  OPTYPE_PS, 0}},
	{38, &JitIL::Default}, //"psq_lux",  OPTYPE_PS, 0}},
	{39, &JitIL::Default}, //"psq_stux", OPTYPE_PS, 0}}, 
};

static GekkoOPTemplate table19[] = 
{
	{528, &JitIL::bcctrx}, //"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  &JitIL::bclrx}, //"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, &JitIL::crXX}, //"crand",  OPTYPE_CR, FL_EVIL}},
	{129, &JitIL::crXX}, //"crandc", OPTYPE_CR, FL_EVIL}},
	{289, &JitIL::crXX}, //"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, &JitIL::crXX}, //"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  &JitIL::crXX}, //"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, &JitIL::crXX}, //"cror",   OPTYPE_CR, FL_EVIL}},
	{417, &JitIL::crXX}, //"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, &JitIL::crXX}, //"crxor",  OPTYPE_CR, FL_EVIL}},
												   
	{150, &JitIL::DoNothing}, //"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   &JitIL::mcrf}, //"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},
												   
	{50,  &JitIL::rfi}, //"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  &JitIL::Default}, //"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] = 
{
	{28,  &JitIL::boolX}, //"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  &JitIL::boolX}, //"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, &JitIL::boolX}, //"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, &JitIL::boolX}, //"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, &JitIL::boolX}, //"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, &JitIL::boolX}, //"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, &JitIL::boolX}, //"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, &JitIL::boolX}, //"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   &JitIL::cmpXX}, //"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  &JitIL::cmpXX}, //"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  &JitIL::cntlzwx}, //"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, &JitIL::extshx}, //"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, &JitIL::extsbx}, //"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, &JitIL::srwx}, //"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, &JitIL::srawx}, //"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, &JitIL::srawix}, //"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  &JitIL::slwx}, //"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   &JitIL::dcbst}, //"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   &JitIL::Default}, //"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  &JitIL::DoNothing}, //"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  &JitIL::DoNothing}, //"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  &JitIL::Default}, //"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  &JitIL::DoNothing}, //"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, &JitIL::dcbz}, //"dcbz",   OPTYPE_DCACHE, 0, 4}},
	//load word
	{23,  &JitIL::lXzx}, //"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  &JitIL::lXzx}, //"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, &JitIL::lXzx}, //"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, &JitIL::lXzx}, //"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, &JitIL::lhax}, //"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, &JitIL::Default}, //"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  &JitIL::lXzx}, //"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, &JitIL::lXzx}, //"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, &JitIL::Default}, //"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, &JitIL::Default}, //"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, &JitIL::Default}, //"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  &JitIL::Default}, //"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (interpret these)
	{533, &JitIL::Default}, //"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, &JitIL::Default}, //"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, &JitIL::stXx}, //"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, &JitIL::stXx}, //"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, &JitIL::stXx}, //"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, &JitIL::stXx}, //"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, &JitIL::stXx}, //"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, &JitIL::stXx}, //"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, &JitIL::Default}, //"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, &JitIL::Default}, //"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, &JitIL::Default}, //"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, &JitIL::Default}, //"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store	
	{535, &JitIL::lfsx}, //"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, &JitIL::Default}, //"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, &JitIL::Default}, //"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, &JitIL::Default}, //"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, &JitIL::stfsx}, //"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, &JitIL::Default}, //"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, &JitIL::Default}, //"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, &JitIL::Default}, //"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, &JitIL::Default}, //"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  &JitIL::mfcr}, //"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  &JitIL::mfmsr}, //"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, &JitIL::mtcrf}, //"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, &JitIL::mtmsr}, //"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, &JitIL::Default}, //"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, &JitIL::Default}, //"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, &JitIL::mfspr}, //"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, &JitIL::mtspr}, //"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, &JitIL::mftb}, //"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, &JitIL::Default}, //"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, &JitIL::Default}, //"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, &JitIL::Default}, //"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   &JitIL::Default}, //"tw",     OPTYPE_SYSTEM, 0, 1}},
	{598, &JitIL::DoNothing}, //"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, &JitIL::icbi}, //"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, &JitIL::Default}, //"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, &JitIL::Default}, //"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, &JitIL::Default}, //"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, &JitIL::Default}, //"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, &JitIL::Default}, //"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, &JitIL::Default}, //"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266,  &JitIL::addx}, //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{778,  &JitIL::addx}, //"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,   &JitIL::Default}, //"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{522,  &JitIL::Default}, //"addcox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138,  &JitIL::addex}, //"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{650,  &JitIL::addex}, //"addeox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234,  &JitIL::Default}, //"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202,  &JitIL::addzex}, //"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491,  &JitIL::Default}, //"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{1003, &JitIL::Default}, //"divwox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459,  &JitIL::divwux}, //"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{971,  &JitIL::divwux}, //"divwuox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,   &JitIL::Default}, //"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,   &JitIL::mulhwux}, //"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235,  &JitIL::mullwx}, //"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{747,  &JitIL::mullwx}, //"mullwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104,  &JitIL::negx}, //"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,   &JitIL::subfx}, //"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{552,  &JitIL::subfx}, //"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,    &JitIL::subfcx}, //"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{520,  &JitIL::subfcx}, //"subfcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136,  &JitIL::subfex}, //"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232,  &JitIL::Default}, //"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200,  &JitIL::Default}, //"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] = 
{
	{18, &JitIL::Default},       //{"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, &JitIL::fp_arith_s}, //"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, &JitIL::fp_arith_s}, //"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, &JitIL::Default}, //"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, &JitIL::Default}, //"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, &JitIL::fp_arith_s}, //"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, &JitIL::fmaddXX}, //"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, &JitIL::fmaddXX}, //"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, &JitIL::fmaddXX}, //"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, &JitIL::fmaddXX}, //"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264, &JitIL::fsign},   //"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  &JitIL::fcmpx},   //"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   &JitIL::fcmpx},   //"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  &JitIL::Default}, //"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  &JitIL::Default}, //"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  &JitIL::fmrx},    //"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, &JitIL::fsign},   //"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  &JitIL::fsign},   //"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  &JitIL::Default}, //"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  &JitIL::Default}, //"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, &JitIL::Default}, //"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  &JitIL::Default}, //"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  &JitIL::Default}, //"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, &JitIL::Default}, //"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, &JitIL::Default}, //"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] = 
{
	{18, &JitIL::Default}, //"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, &JitIL::Default}, //"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, &JitIL::Default}, //"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, &JitIL::Default}, //"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, &JitIL::Default}, //"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, &JitIL::fp_arith_s}, //"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, &JitIL::fp_arith_s}, //"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, &JitIL::fmaddXX}, //"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, &JitIL::fmaddXX}, //"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, &JitIL::fmaddXX}, //"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, &JitIL::fmaddXX}, //"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};

namespace JitILTables
{

void CompileInstruction(PPCAnalyst::CodeOp & op)
{
	JitIL *jitil = (JitIL *)jit;
	(jitil->*dynaOpTable[op.inst.OPCD])(op.inst);
	GekkoOPInfo *info = op.opinfo;
	if (info) {
#ifdef OPLOG
		if (!strcmp(info->opname, OP_TO_LOG)){  ///"mcrfs"
			rsplocations.push_back(jit.js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit->js.compilerPC;
	} else {
		PanicAlert("Tried to compile illegal (or unknown) instruction %08x, at %08x", op.inst.hex, jit->js.compilerPC);
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
		dynaOpTable59[i] = &JitIL::unknown_instruction;
	}

	for (int i = 0; i < 1024; i++)
	{
		dynaOpTable4 [i] = &JitIL::unknown_instruction;
		dynaOpTable19[i] = &JitIL::unknown_instruction;
		dynaOpTable31[i] = &JitIL::unknown_instruction;
		dynaOpTable63[i] = &JitIL::unknown_instruction;	
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

}
