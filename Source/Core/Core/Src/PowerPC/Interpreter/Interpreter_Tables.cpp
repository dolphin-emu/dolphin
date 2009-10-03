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

#include "Interpreter_Tables.h"

typedef void (*_Instruction) (UGeckoInstruction instCode);

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	GekkoOPInfo opinfo;
	int runCount;
};

static GekkoOPTemplate primarytable[] = 
{
	{4,  Interpreter::RunTable4,       {"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, Interpreter::RunTable19,     {"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, Interpreter::RunTable31,     {"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, Interpreter::RunTable59,     {"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, Interpreter::RunTable63,     {"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, Interpreter::bcx,                    {"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, Interpreter::bx,                      {"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  Interpreter::HLEFunction,    {"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  Interpreter::CompiledBlock,      {"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  Interpreter::twi,                {"twi",         OPTYPE_SYSTEM, 0}},
	{17, Interpreter::sc,                      {"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  Interpreter::mulli,             {"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  Interpreter::subfic,           {"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, Interpreter::cmpli,             {"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, Interpreter::cmpi,              {"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, Interpreter::addic,           {"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, Interpreter::addic_rc,        {"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, Interpreter::addi,            {"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, Interpreter::addis,           {"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, Interpreter::rlwimix,         {"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, Interpreter::rlwinmx,         {"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, Interpreter::rlwnmx,           {"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, Interpreter::ori,             {"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, Interpreter::oris,            {"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, Interpreter::xori,            {"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, Interpreter::xoris,           {"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, Interpreter::andi_rc,         {"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, Interpreter::andis_rc,        {"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, Interpreter::lwz,             {"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, Interpreter::lwzu,            {"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, Interpreter::lbz,             {"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, Interpreter::lbzu,            {"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, Interpreter::lhz,             {"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, Interpreter::lhzu,            {"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{42, Interpreter::lha,             {"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, Interpreter::lhau,        {"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, Interpreter::sth,             {"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, Interpreter::sthu,            {"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, Interpreter::stw,             {"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, Interpreter::stwu,            {"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, Interpreter::stb,	           {"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, Interpreter::stbu,            {"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, Interpreter::lmw,             {"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, Interpreter::stmw,           {"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, Interpreter::lfs,             {"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, Interpreter::lfsu,        {"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, Interpreter::lfd,             {"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, Interpreter::lfdu,        {"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, Interpreter::stfs,           {"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, Interpreter::stfsu,          {"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, Interpreter::stfd,           {"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, Interpreter::stfdu,       {"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, Interpreter::psq_l,         {"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, Interpreter::psq_lu,        {"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, Interpreter::psq_st,       {"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, Interpreter::psq_stu,      {"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,    Interpreter::ps_cmpu0,        {"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   Interpreter::ps_cmpo0,        {"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   Interpreter::ps_neg,          {"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  Interpreter::ps_nabs,         {"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  Interpreter::ps_abs,          {"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   Interpreter::ps_cmpu1,        {"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   Interpreter::ps_mr,             {"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   Interpreter::ps_cmpo1,        {"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  Interpreter::ps_merge00,   {"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  Interpreter::ps_merge01,   {"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  Interpreter::ps_merge10,   {"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  Interpreter::ps_merge11,   {"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, Interpreter::dcbz_l,          {"dcbz_l",     OPTYPE_SYSTEM, 0}},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10, Interpreter::ps_sum0,       {"ps_sum0",   OPTYPE_PS, 0}},
	{11, Interpreter::ps_sum1,       {"ps_sum1",   OPTYPE_PS, 0}},
	{12, Interpreter::ps_muls0,     {"ps_muls0",  OPTYPE_PS, 0}},
	{13, Interpreter::ps_muls1,     {"ps_muls1",  OPTYPE_PS, 0}},
	{14, Interpreter::ps_madds0,  {"ps_madds0", OPTYPE_PS, 0}},
	{15, Interpreter::ps_madds1,  {"ps_madds1", OPTYPE_PS, 0}},
	{18, Interpreter::ps_div,      {"ps_div",    OPTYPE_PS, 0, 16}},
	{20, Interpreter::ps_sub,      {"ps_sub",    OPTYPE_PS, 0}},
	{21, Interpreter::ps_add,      {"ps_add",    OPTYPE_PS, 0}},
	{23, Interpreter::ps_sel,        {"ps_sel",    OPTYPE_PS, 0}},
	{24, Interpreter::ps_res,       {"ps_res",    OPTYPE_PS, 0}},
	{25, Interpreter::ps_mul,      {"ps_mul",    OPTYPE_PS, 0}},
	{26, Interpreter::ps_rsqrte,  {"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, Interpreter::ps_msub,    {"ps_msub",   OPTYPE_PS, 0}},
	{29, Interpreter::ps_madd,    {"ps_madd",   OPTYPE_PS, 0}},
	{30, Interpreter::ps_nmsub,   {"ps_nmsub",  OPTYPE_PS, 0}},
	{31, Interpreter::ps_nmadd,   {"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,  Interpreter::psq_lx,    {"psq_lx",   OPTYPE_PS, 0}},
	{7,  Interpreter::psq_stx,   {"psq_stx",  OPTYPE_PS, 0}},
	{38, Interpreter::psq_lux,   {"psq_lux",  OPTYPE_PS, 0}},
	{39, Interpreter::psq_stux,  {"psq_stux", OPTYPE_PS, 0}}, 
};

static GekkoOPTemplate table19[] = 
{
	{528, Interpreter::bcctrx,     {"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  Interpreter::bclrx,       {"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, Interpreter::crand,     {"crand",  OPTYPE_CR, FL_EVIL}},
	{129, Interpreter::crandc,    {"crandc", OPTYPE_CR, FL_EVIL}},
	{289, Interpreter::creqv,     {"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, Interpreter::crnand,    {"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  Interpreter::crnor,     {"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, Interpreter::cror,      {"cror",   OPTYPE_CR, FL_EVIL}},
	{417, Interpreter::crorc,     {"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, Interpreter::crxor,     {"crxor",  OPTYPE_CR, FL_EVIL}},
												   
	{150, Interpreter::isync,   {"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   Interpreter::mcrf,      {"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},
												   
	{50,  Interpreter::rfi,           {"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  Interpreter::rfid,      {"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] = 
{
	{28,  Interpreter::andx,         {"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  Interpreter::andcx,     {"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, Interpreter::orx,           {"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, Interpreter::norx,      {"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, Interpreter::xorx,         {"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, Interpreter::orcx,      {"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, Interpreter::nandx,     {"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, Interpreter::eqvx,      {"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   Interpreter::cmp,         {"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  Interpreter::cmpl,        {"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  Interpreter::cntlzwx,   {"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, Interpreter::extshx,     {"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, Interpreter::extsbx,     {"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, Interpreter::srwx,         {"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, Interpreter::srawx,       {"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, Interpreter::srawix,     {"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  Interpreter::slwx,         {"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   Interpreter::dcbst,    {"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   Interpreter::dcbf,     {"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  Interpreter::dcbtst,   {"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  Interpreter::dcbt,     {"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  Interpreter::dcbi,     {"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  Interpreter::dcba,     {"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, Interpreter::dcbz,        {"dcbz",   OPTYPE_DCACHE, 0, 4}},
	
	//load word
	{23,  Interpreter::lwzx,         {"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  Interpreter::lwzux,        {"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, Interpreter::lhzx,         {"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, Interpreter::lhzux,        {"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, Interpreter::lhax,         {"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, Interpreter::lhaux,     {"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  Interpreter::lbzx,         {"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, Interpreter::lbzux,        {"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},
	
	//load byte reverse
	{534, Interpreter::lwbrx,   {"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, Interpreter::lhbrx,   {"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, Interpreter::stwcxd,      {"stwcxd", OPTYPE_STORE, FL_EVIL | FL_SET_CR0}},
	{20,  Interpreter::lwarx,       {"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0}},

	//load string (Inst these)
	{533, Interpreter::lswx,     {"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, Interpreter::lswi,     {"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, Interpreter::stwx,         {"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, Interpreter::stwux,        {"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, Interpreter::sthx,         {"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, Interpreter::sthux,        {"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, Interpreter::stbx,         {"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, Interpreter::stbux,        {"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, Interpreter::stwbrx,    {"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, Interpreter::sthbrx,    {"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, Interpreter::stswx,     {"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, Interpreter::stswi,     {"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store	
	{535, Interpreter::lfsx,        {"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, Interpreter::lfsux,    {"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, Interpreter::lfdx,     {"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, Interpreter::lfdux,    {"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, Interpreter::stfsx,       {"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, Interpreter::stfsux,    {"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, Interpreter::stfdx,     {"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, Interpreter::stfdux,    {"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, Interpreter::stfiwx,    {"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  Interpreter::mfcr,          {"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  Interpreter::mfmsr,        {"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, Interpreter::mtcrf,        {"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, Interpreter::mtmsr,        {"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, Interpreter::mtsr,       {"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, Interpreter::mtsrin,     {"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, Interpreter::mfspr,        {"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, Interpreter::mtspr,        {"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, Interpreter::mftb,          {"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, Interpreter::mcrxr,      {"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, Interpreter::mfsr,       {"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, Interpreter::mfsrin,     {"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   Interpreter::tw,           {"tw",     OPTYPE_SYSTEM, 0, 1}},
	{598, Interpreter::sync,       {"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, Interpreter::icbi,         {"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 3}},

	// Unused instructions on GC
	{310, Interpreter::eciwx,       {"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, Interpreter::ecowx,       {"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, Interpreter::eieio,       {"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, Interpreter::tlbie,       {"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, Interpreter::tlbia,       {"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, Interpreter::tlbsync,     {"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266, Interpreter::addx,             {"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,  Interpreter::addcx,         {"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138, Interpreter::addex,           {"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234, Interpreter::addmex,        {"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202, Interpreter::addzex,         {"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491, Interpreter::divwx,         {"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459, Interpreter::divwux,         {"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,  Interpreter::mulhwx,        {"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,  Interpreter::mulhwux,       {"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235, Interpreter::mullwx,         {"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104, Interpreter::negx,             {"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,  Interpreter::subfx,           {"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,   Interpreter::subfcx,         {"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136, Interpreter::subfex,         {"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232, Interpreter::subfmex,       {"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200, Interpreter::subfzex,       {"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] = 
{
	{18, Interpreter::fdivsx,    /*TODO*/       {"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, Interpreter::fsubsx,       {"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, Interpreter::faddsx,       {"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, Interpreter::fsqrtsx,         {"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, Interpreter::fresx,           {"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, Interpreter::fmulsx,       {"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, Interpreter::fmsubsx,         {"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, Interpreter::fmaddsx,         {"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, Interpreter::fnmsubsx,        {"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, Interpreter::fnmaddsx,        {"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264, Interpreter::fabsx,       {"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  Interpreter::fcmpo,         {"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   Interpreter::fcmpu,         {"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  Interpreter::fctiwx,      {"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  Interpreter::fctiwzx,     {"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  Interpreter::fmrx,           {"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, Interpreter::fnabsx,      {"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  Interpreter::fnegx,       {"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  Interpreter::frspx,       {"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  Interpreter::mcrfs,       {"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, Interpreter::mffsx,       {"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  Interpreter::mtfsb0x,     {"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  Interpreter::mtfsb1x,     {"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, Interpreter::mtfsfix,     {"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, Interpreter::mtfsfx,      {"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] = 
{
	{18, Interpreter::fdivx,        {"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, Interpreter::fsubx,        {"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, Interpreter::faddx,        {"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, Interpreter::fsqrtx,       {"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, Interpreter::fselx,        {"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, Interpreter::fmulx,     {"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, Interpreter::frsqrtex,  {"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, Interpreter::fmsubx,       {"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, Interpreter::fmaddx,       {"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, Interpreter::fnmsubx,      {"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, Interpreter::fnmaddx,      {"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};
namespace InterpreterTables
{
void InitTables()
{
	//clear
	for (int i = 0; i < 32; i++) 
	{
		Interpreter::m_opTable59[i] = Interpreter::unknown_instruction;
		m_infoTable59[i] = 0;
	}

	for (int i = 0; i < 1024; i++)
	{
		Interpreter::m_opTable4 [i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable19[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable31[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable63[i] = Interpreter::unknown_instruction;
		m_infoTable4[i] = 0;		
		m_infoTable19[i] = 0;		
		m_infoTable31[i] = 0;		
		m_infoTable63[i] = 0;		
	}

	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
	{
		Interpreter::m_opTable[primarytable[i].opcode] = primarytable[i].Inst;
		m_infoTable[primarytable[i].opcode] = &primarytable[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_2[j].opcode;
			Interpreter::m_opTable4[op] = table4_2[j].Inst;
			m_infoTable4[op] = &table4_2[j].opinfo;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (int j = 0; j < (int)(sizeof(table4_3) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_3[j].opcode;
			Interpreter::m_opTable4[op] = table4_3[j].Inst;
			m_infoTable4[op] = &table4_3[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table4[i].opcode;
		Interpreter::m_opTable4[op] = table4[i].Inst;
		m_infoTable4[op] = &table4[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table31[i].opcode;
		Interpreter::m_opTable31[op] = table31[i].Inst;
		m_infoTable31[op] = &table31[i].opinfo;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (int j = 0; j < (int)(sizeof(table31_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table31_2[j].opcode;
			Interpreter::m_opTable31[op] = table31_2[j].Inst;
			m_infoTable31[op] = &table31_2[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table19[i].opcode;
		Interpreter::m_opTable19[op] = table19[i].Inst;
		m_infoTable19[op] = &table19[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table59[i].opcode;
		Interpreter::m_opTable59[op] = table59[i].Inst;
		m_infoTable59[op] = &table59[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table63[i].opcode;
		Interpreter::m_opTable63[op] = table63[i].Inst;
		m_infoTable63[op] = &table63[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table63_2[j].opcode;
			Interpreter::m_opTable63[op] = table63_2[j].Inst;
			m_infoTable63[op] = &table63_2[j].opinfo;
		}
	}

	m_numInstructions = 0;
	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &primarytable[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table4_2[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table4_3) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table4_3[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table4[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table31[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table31_2) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table31_2[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table19[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table59[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table63[i].opinfo;
	for (int i = 0; i < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); i++)
		m_allInstructions[m_numInstructions++] = &table63_2[i].opinfo;
	if (m_numInstructions >= 512) {
		PanicAlert("m_allInstructions underdimensioned");
	}
}

}
//remove
