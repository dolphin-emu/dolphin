// Copyright (C) 2003-2008 Dolphin Project.

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

#include <algorithm>
#include <vector>

#include "Common.h"
#include "PPCTables.h"
#include "StringUtil.h"
#include "Interpreter/Interpreter.h"

#if defined(_M_IX86) || defined(_M_X64)
#include "Jit64/Jit.h"
#include "Jit64/JitCache.h"
#else
#error Unknown architecture!
#endif

struct GekkoOPTemplate
{
	int opcode;
	PPCTables::_interpreterInstruction interpret;
	PPCTables::_recompilerInstruction recompile;

	GekkoOPInfo opinfo;
	int runCount;
};

struct inf
{
    const char *name;
    int count;
    bool operator < (const inf &o) const
    {
	return count > o.count;
    }
};

static GekkoOPInfo *m_infoTable[64];
static GekkoOPInfo *m_infoTable4[1024];
static GekkoOPInfo *m_infoTable19[1024];
static GekkoOPInfo *m_infoTable31[1024];
static GekkoOPInfo *m_infoTable59[32];
static GekkoOPInfo *m_infoTable63[1024];

static PPCTables::_recompilerInstruction dynaOpTable[64];
static PPCTables::_recompilerInstruction dynaOpTable4[1024];
static PPCTables::_recompilerInstruction dynaOpTable19[1024];
static PPCTables::_recompilerInstruction dynaOpTable31[1024];
static PPCTables::_recompilerInstruction dynaOpTable59[32];
static PPCTables::_recompilerInstruction dynaOpTable63[1024];

void Jit64::DynaRunTable4(UGeckoInstruction _inst)  {(this->*dynaOpTable4 [_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable19(UGeckoInstruction _inst) {(this->*dynaOpTable19[_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable31(UGeckoInstruction _inst) {(this->*dynaOpTable31[_inst.SUBOP10])(_inst);}
void Jit64::DynaRunTable59(UGeckoInstruction _inst) {(this->*dynaOpTable59[_inst.SUBOP5 ])(_inst);}
void Jit64::DynaRunTable63(UGeckoInstruction _inst) {(this->*dynaOpTable63[_inst.SUBOP10])(_inst);}

static GekkoOPInfo *m_allInstructions[2048];
static int m_numInstructions;

GekkoOPInfo *GetOpInfo(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch(table) 
		{
		case 4:  return m_infoTable4[_inst.SUBOP10];
		case 19: return m_infoTable19[_inst.SUBOP10];
		case 31: return m_infoTable31[_inst.SUBOP10];
		case 59: return m_infoTable59[_inst.SUBOP5];
		case 63: return m_infoTable63[_inst.SUBOP10];
		default:
			_assert_msg_(GEKKO,0,"GetOpInfo - invalid subtable op %08x @ %08x", _inst, PC);
			return 0;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(GEKKO,0,"GetOpInfo - invalid op %08x @ %08x", _inst, PC);
			return 0;
		}
		return m_infoTable[_inst.OPCD];
	}
}

Interpreter::_interpreterInstruction GetInterpreterOp(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch(table) 
		{
		case 4:  return Interpreter::m_opTable4[_inst.SUBOP10];
		case 19: return Interpreter::m_opTable19[_inst.SUBOP10];
		case 31: return Interpreter::m_opTable31[_inst.SUBOP10];
		case 59: return Interpreter::m_opTable59[_inst.SUBOP5];
		case 63: return Interpreter::m_opTable63[_inst.SUBOP10];
		default:
			_assert_msg_(GEKKO,0,"GetInterpreterOp - invalid subtable op %08x @ %08x", _inst, PC);
			return 0;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(GEKKO,0,"GetInterpreterOp - invalid op %08x @ %08x", _inst, PC);
			return 0;
		}
		return Interpreter::m_opTable[_inst.OPCD];
	}
}

static GekkoOPTemplate primarytable[] = 
{
	{4,  Interpreter::RunTable4,     &Jit64::DynaRunTable4,  {"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, Interpreter::RunTable19,    &Jit64::DynaRunTable19, {"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, Interpreter::RunTable31,    &Jit64::DynaRunTable31, {"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, Interpreter::RunTable59,    &Jit64::DynaRunTable59, {"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, Interpreter::RunTable63,    &Jit64::DynaRunTable63, {"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, Interpreter::bcx,           &Jit64::bcx,         {"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, Interpreter::bx,            &Jit64::bx,          {"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  Interpreter::HLEFunction,   &Jit64::HLEFunction, {"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  Interpreter::CompiledBlock, &Jit64::Default,     {"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  Interpreter::twi,           &Jit64::Default,     {"twi",         OPTYPE_SYSTEM, 0}},
	{17, Interpreter::sc,            &Jit64::sc,          {"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  Interpreter::mulli,     &Jit64::mulli,        {"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 2}},
	{8,  Interpreter::subfic,    &Jit64::subfic,       {"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A |	FL_SET_CA}},
	{10, Interpreter::cmpli,     &Jit64::cmpXX,        {"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{11, Interpreter::cmpi,      &Jit64::cmpXX,        {"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn}},
	{12, Interpreter::addic,     &Jit64::reg_imm,      {"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA}},
	{13, Interpreter::addic_rc,  &Jit64::reg_imm,      {"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CR0}},
	{14, Interpreter::addi,      &Jit64::reg_imm,      {"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},
	{15, Interpreter::addis,     &Jit64::reg_imm,      {"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0}},

	{20, Interpreter::rlwimix,   &Jit64::rlwimix,      {"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT}},
	{21, Interpreter::rlwinmx,   &Jit64::rlwinmx,      {"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{23, Interpreter::rlwnmx,    &Jit64::rlwnmx,       {"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_IN_B | FL_RC_BIT}},

	{24, Interpreter::ori,       &Jit64::reg_imm,      {"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{25, Interpreter::oris,      &Jit64::reg_imm,      {"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{26, Interpreter::xori,      &Jit64::reg_imm,      {"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{27, Interpreter::xoris,     &Jit64::reg_imm,      {"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S}},
	{28, Interpreter::andi_rc,   &Jit64::reg_imm,      {"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},
	{29, Interpreter::andis_rc,  &Jit64::reg_imm,      {"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0}},

	{32, Interpreter::lwz,       &Jit64::lXz,      {"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{33, Interpreter::lwzu,      &Jit64::Default,  {"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{34, Interpreter::lbz,       &Jit64::lXz,      {"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{35, Interpreter::lbzu,      &Jit64::Default,  {"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{40, Interpreter::lhz,       &Jit64::lXz,      {"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{41, Interpreter::lhzu,      &Jit64::Default,  {"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},
	{42, Interpreter::lha,       &Jit64::lha,      {"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A}},
	{43, Interpreter::lhau,      &Jit64::Default,  {"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A}},

	{44, Interpreter::sth,       &Jit64::stX,      {"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{45, Interpreter::sthu,      &Jit64::stX,      {"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{36, Interpreter::stw,       &Jit64::stX,      {"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{37, Interpreter::stwu,      &Jit64::stX,      {"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},
	{38, Interpreter::stb,	     &Jit64::stX,      {"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S}},
	{39, Interpreter::stbu,      &Jit64::stX,      {"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S}},

	{46, Interpreter::lmw,       &Jit64::lmw,      {"lmw",   OPTYPE_SYSTEM, FL_EVIL, 10}},
	{47, Interpreter::stmw,      &Jit64::stmw,     {"stmw",  OPTYPE_SYSTEM, FL_EVIL, 10}},

	{48, Interpreter::lfs,       &Jit64::lfs,      {"lfs",  OPTYPE_LOADFP, FL_IN_A}},
	{49, Interpreter::lfsu,      &Jit64::Default,  {"lfsu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},
	{50, Interpreter::lfd,       &Jit64::lfd,      {"lfd",  OPTYPE_LOADFP, FL_IN_A}},
	{51, Interpreter::lfdu,      &Jit64::Default,  {"lfdu", OPTYPE_LOADFP, FL_OUT_A | FL_IN_A}},

	{52, Interpreter::stfs,      &Jit64::stfs,     {"stfs",  OPTYPE_STOREFP, FL_IN_A}},
	{53, Interpreter::stfsu,     &Jit64::stfs,     {"stfsu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},
	{54, Interpreter::stfd,      &Jit64::stfd,     {"stfd",  OPTYPE_STOREFP, FL_IN_A}},
	{55, Interpreter::stfdu,     &Jit64::Default,  {"stfdu", OPTYPE_STOREFP, FL_OUT_A | FL_IN_A}},

	{56, Interpreter::psq_l,     &Jit64::psq_l,    {"psq_l",   OPTYPE_PS, FL_IN_A}},
	{57, Interpreter::psq_lu,    &Jit64::psq_l,    {"psq_lu",  OPTYPE_PS, FL_OUT_A | FL_IN_A}},
	{60, Interpreter::psq_st,    &Jit64::psq_st,   {"psq_st",  OPTYPE_PS, FL_IN_A}},
	{61, Interpreter::psq_stu,   &Jit64::psq_st,   {"psq_stu", OPTYPE_PS, FL_OUT_A | FL_IN_A}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, Interpreter::unknown_instruction, &Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

static GekkoOPTemplate table4[] = 
{    //SUBOP10
	{0,    Interpreter::ps_cmpu0,   &Jit64::Default,     {"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   Interpreter::ps_cmpo0,   &Jit64::Default,     {"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   Interpreter::ps_neg,     &Jit64::ps_sign,     {"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  Interpreter::ps_nabs,    &Jit64::ps_sign,     {"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  Interpreter::ps_abs,     &Jit64::ps_sign,     {"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   Interpreter::ps_cmpu1,   &Jit64::Default,     {"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   Interpreter::ps_mr,      &Jit64::ps_mr,       {"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   Interpreter::ps_cmpo1,   &Jit64::Default,     {"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  Interpreter::ps_merge00, &Jit64::ps_mergeXX,  {"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  Interpreter::ps_merge01, &Jit64::ps_mergeXX,  {"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  Interpreter::ps_merge10, &Jit64::ps_mergeXX,  {"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  Interpreter::ps_merge11, &Jit64::ps_mergeXX,  {"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, Interpreter::dcbz_l,     &Jit64::Default,     {"dcbz_l",     OPTYPE_SYSTEM, 0}},
};		

static GekkoOPTemplate table4_2[] = 
{
	{10, Interpreter::ps_sum0,   &Jit64::ps_sum,    {"ps_sum0",   OPTYPE_PS, 0}},
	{11, Interpreter::ps_sum1,   &Jit64::ps_sum,    {"ps_sum1",   OPTYPE_PS, 0}},
	{12, Interpreter::ps_muls0,  &Jit64::ps_muls,   {"ps_muls0",  OPTYPE_PS, 0}},
	{13, Interpreter::ps_muls1,  &Jit64::ps_muls,   {"ps_muls1",  OPTYPE_PS, 0}},
	{14, Interpreter::ps_madds0, &Jit64::ps_maddXX, {"ps_madds0", OPTYPE_PS, 0}},
	{15, Interpreter::ps_madds1, &Jit64::ps_maddXX, {"ps_madds1", OPTYPE_PS, 0}},
	{18, Interpreter::ps_div,    &Jit64::ps_arith,  {"ps_div",    OPTYPE_PS, 0, 16}},
	{20, Interpreter::ps_sub,    &Jit64::ps_arith,  {"ps_sub",    OPTYPE_PS, 0}},
	{21, Interpreter::ps_add,    &Jit64::ps_arith,  {"ps_add",    OPTYPE_PS, 0}},
	{23, Interpreter::ps_sel,    &Jit64::ps_sel,    {"ps_sel",    OPTYPE_PS, 0}},
	{24, Interpreter::ps_res,    &Jit64::Default,   {"ps_res",    OPTYPE_PS, 0}},
	{25, Interpreter::ps_mul,    &Jit64::ps_arith,  {"ps_mul",    OPTYPE_PS, 0}},
	{26, Interpreter::ps_rsqrte, &Jit64::ps_rsqrte, {"ps_rsqrte", OPTYPE_PS, 0, 1}},
	{28, Interpreter::ps_msub,   &Jit64::ps_maddXX, {"ps_msub",   OPTYPE_PS, 0}},
	{29, Interpreter::ps_madd,   &Jit64::ps_maddXX, {"ps_madd",   OPTYPE_PS, 0}},
	{30, Interpreter::ps_nmsub,  &Jit64::ps_maddXX, {"ps_nmsub",  OPTYPE_PS, 0}},
	{31, Interpreter::ps_nmadd,  &Jit64::ps_maddXX, {"ps_nmadd",  OPTYPE_PS, 0}},
};


static GekkoOPTemplate table4_3[] = 
{
	{6,  Interpreter::psq_lx,   &Jit64::Default, {"psq_lx",   OPTYPE_PS, 0}},
	{7,  Interpreter::psq_stx,  &Jit64::Default, {"psq_stx",  OPTYPE_PS, 0}},
	{38, Interpreter::psq_lux,  &Jit64::Default, {"psq_lux",  OPTYPE_PS, 0}},
	{39, Interpreter::psq_stux, &Jit64::Default, {"psq_stux", OPTYPE_PS, 0}}, 
};

static GekkoOPTemplate table19[] = 
{
	{528, Interpreter::bcctrx, &Jit64::bcctrx,    {"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  Interpreter::bclrx,  &Jit64::bclrx,     {"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, Interpreter::crand,  &Jit64::Default,   {"crand",  OPTYPE_CR, FL_EVIL}},
	{129, Interpreter::crandc, &Jit64::Default,   {"crandc", OPTYPE_CR, FL_EVIL}},
	{289, Interpreter::creqv,  &Jit64::Default,   {"creqv",  OPTYPE_CR, FL_EVIL}},
	{225, Interpreter::crnand, &Jit64::Default,   {"crnand", OPTYPE_CR, FL_EVIL}},
	{33,  Interpreter::crnor,  &Jit64::Default,   {"crnor",  OPTYPE_CR, FL_EVIL}},
	{449, Interpreter::cror,   &Jit64::Default,   {"cror",   OPTYPE_CR, FL_EVIL}},
	{417, Interpreter::crorc,  &Jit64::Default,   {"crorc",  OPTYPE_CR, FL_EVIL}},
	{193, Interpreter::crxor,  &Jit64::Default,   {"crxor",  OPTYPE_CR, FL_EVIL}},
												   
	{150, Interpreter::isync,  &Jit64::DoNothing, {"isync",  OPTYPE_ICACHE, FL_EVIL}},
	{0,   Interpreter::mcrf,   &Jit64::Default,   {"mcrf",   OPTYPE_SYSTEM, FL_EVIL}},
												   
	{50,  Interpreter::rfi,    &Jit64::rfi,       {"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  Interpreter::rfid,   &Jit64::Default,   {"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


static GekkoOPTemplate table31[] = 
{
	{28,  Interpreter::andx,    &Jit64::andx,     {"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{60,  Interpreter::andcx,   &Jit64::Default,  {"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{444, Interpreter::orx,     &Jit64::orx,      {"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{124, Interpreter::norx,    &Jit64::Default,  {"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{316, Interpreter::xorx,    &Jit64::xorx,     {"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{412, Interpreter::orcx,    &Jit64::Default,  {"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{476, Interpreter::nandx,   &Jit64::Default,  {"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{284, Interpreter::eqvx,    &Jit64::Default,  {"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT}},
	{0,   Interpreter::cmp,     &Jit64::cmpXX,    {"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  Interpreter::cmpl,    &Jit64::cmpXX,    {"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  Interpreter::cntlzwx, &Jit64::cntlzwx,  {"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{922, Interpreter::extshx,  &Jit64::extshx,   {"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{954, Interpreter::extsbx,  &Jit64::extsbx,   {"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT}},
	{536, Interpreter::srwx,    &Jit64::srwx,     {"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{792, Interpreter::srawx,   &Jit64::srawx,    {"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{824, Interpreter::srawix,  &Jit64::srawix,   {"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},
	{24,  Interpreter::slwx,    &Jit64::slwx,     {"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_B | FL_IN_S | FL_RC_BIT}},

	{54,   Interpreter::dcbst,  &Jit64::Default,  {"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   Interpreter::dcbf,   &Jit64::Default,  {"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  Interpreter::dcbtst, &Jit64::Default,  {"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  Interpreter::dcbt,   &Jit64::Default,  {"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  Interpreter::dcbi,   &Jit64::Default,  {"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  Interpreter::dcba,   &Jit64::Default,  {"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, Interpreter::dcbz,   &Jit64::dcbz,     {"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  Interpreter::lwzx,  &Jit64::lwzx,      {"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  Interpreter::lwzux, &Jit64::lwzux,     {"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, Interpreter::lhzx,  &Jit64::Default,   {"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, Interpreter::lhzux, &Jit64::Default,   {"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, Interpreter::lhax,  &Jit64::lhax,      {"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, Interpreter::lhaux, &Jit64::Default,   {"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  Interpreter::lbzx,  &Jit64::lbzx,      {"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, Interpreter::lbzux, &Jit64::Default,   {"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, Interpreter::lwbrx, &Jit64::Default,  {"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, Interpreter::lhbrx, &Jit64::Default,  {"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	// Conditional load/store (Wii SMP)
	{150, Interpreter::stwcxd,  &Jit64::Default,    {"stwcxd", OPTYPE_STORE, FL_EVIL}},
	{20,  Interpreter::lwarx,   &Jit64::Default,    {"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B}},

	//load string (interpret these)
	{533, Interpreter::lswx,  &Jit64::Default,   {"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D}},
	{597, Interpreter::lswi,  &Jit64::Default,   {"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_AB | FL_OUT_D}},

	//store word
	{151, Interpreter::stwx,   &Jit64::Default,    {"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, Interpreter::stwux,  &Jit64::stwux,      {"stwux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store halfword
	{407, Interpreter::sthx,   &Jit64::Default,    {"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, Interpreter::sthux,  &Jit64::Default,    {"sthux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store byte
	{215, Interpreter::stbx,   &Jit64::Default,    {"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, Interpreter::stbux,  &Jit64::Default,    {"stbux",  OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, Interpreter::stwbrx, &Jit64::Default,   {"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, Interpreter::sthbrx, &Jit64::Default,   {"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, Interpreter::stswx,  &Jit64::Default,   {"stswx",  OPTYPE_STORE, FL_EVIL}},
	{725, Interpreter::stswi,  &Jit64::Default,   {"stswi",  OPTYPE_STORE, FL_EVIL}},

	// fp load/store	
	{535, Interpreter::lfsx,  &Jit64::lfsx,      {"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, Interpreter::lfsux, &Jit64::Default,   {"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, Interpreter::lfdx,  &Jit64::Default,   {"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, Interpreter::lfdux, &Jit64::Default,   {"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	{663, Interpreter::stfsx,  &Jit64::stfsx,     {"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, Interpreter::stfsux, &Jit64::Default,   {"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, Interpreter::stfdx,  &Jit64::Default,   {"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, Interpreter::stfdux, &Jit64::Default,   {"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, Interpreter::stfiwx, &Jit64::Default,   {"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  Interpreter::mfcr,   &Jit64::mfcr,       {"mfcr",   OPTYPE_SYSTEM, FL_OUT_D}},
	{83,  Interpreter::mfmsr,  &Jit64::mfmsr,      {"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D}},
	{144, Interpreter::mtcrf,  &Jit64::mtcrf,      {"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, Interpreter::mtmsr,  &Jit64::mtmsr,      {"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, Interpreter::mtsr,   &Jit64::Default,    {"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, Interpreter::mtsrin, &Jit64::Default,    {"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, Interpreter::mfspr,  &Jit64::mfspr,      {"mfspr",  OPTYPE_SPR, FL_OUT_D}},
	{467, Interpreter::mtspr,  &Jit64::mtspr,      {"mtspr",  OPTYPE_SPR, 0, 2}},
	{371, Interpreter::mftb,   &Jit64::mftb,       {"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER}},
	{512, Interpreter::mcrxr,  &Jit64::Default,    {"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, Interpreter::mfsr,   &Jit64::Default,    {"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 2}},
	{659, Interpreter::mfsrin, &Jit64::Default,    {"mfsrin", OPTYPE_SYSTEM, FL_OUT_D, 2}},

	{4,   Interpreter::tw,      &Jit64::Default,     {"tw",     OPTYPE_SYSTEM, 0, 1}},
	{598, Interpreter::sync,    &Jit64::DoNothing,   {"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, Interpreter::icbi,    &Jit64::Default,     {"icbi",   OPTYPE_SYSTEM, 0, 3}},

	// Unused instructions on GC
	{310, Interpreter::eciwx,   &Jit64::Default,    {"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, Interpreter::ecowx,   &Jit64::Default,    {"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, Interpreter::eieio,   &Jit64::Default,    {"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, Interpreter::tlbie,   &Jit64::Default,    {"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, Interpreter::tlbia,   &Jit64::Default,    {"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, Interpreter::tlbsync, &Jit64::Default,    {"tlbsync", OPTYPE_SYSTEM, 0}},
};

static GekkoOPTemplate table31_2[] = 
{	
	{266, Interpreter::addx,    &Jit64::addx,         {"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{10,  Interpreter::addcx,   &Jit64::Default,      {"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{138, Interpreter::addex,   &Jit64::addex,        {"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234, Interpreter::addmex,  &Jit64::Default,      {"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202, Interpreter::addzex,  &Jit64::Default,      {"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491, Interpreter::divwx,   &Jit64::Default,      {"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{459, Interpreter::divwux,  &Jit64::divwux,       {"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 39}},
	{75,  Interpreter::mulhwx,  &Jit64::Default,      {"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{11,  Interpreter::mulhwux, &Jit64::mulhwux,      {"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{235, Interpreter::mullwx,  &Jit64::mullwx,       {"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 4}},
	{104, Interpreter::negx,    &Jit64::negx,         {"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{40,  Interpreter::subfx,   &Jit64::subfx,        {"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT}},
	{8,   Interpreter::subfcx,  &Jit64::subfcx,       {"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT}},
	{136, Interpreter::subfex,  &Jit64::subfex,       {"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232, Interpreter::subfmex, &Jit64::Default,      {"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200, Interpreter::subfzex, &Jit64::Default,      {"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

static GekkoOPTemplate table59[] = 
{
	{18, Interpreter::fdivsx,   &Jit64::fp_arith_s,    {"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, Interpreter::fsubsx,   &Jit64::fp_arith_s,    {"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, Interpreter::faddsx,   &Jit64::fp_arith_s,    {"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, Interpreter::fsqrtsx,  &Jit64::Default,       {"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, Interpreter::fresx,    &Jit64::Default,       {"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, Interpreter::fmulsx,   &Jit64::fp_arith_s,    {"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, Interpreter::fmsubsx,  &Jit64::fmaddXX,       {"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, Interpreter::fmaddsx,  &Jit64::fmaddXX,       {"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, Interpreter::fnmsubsx, &Jit64::fmaddXX,       {"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, Interpreter::fnmaddsx, &Jit64::fmaddXX,       {"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

static GekkoOPTemplate table63[] = 
{
	{264, Interpreter::fabsx,   &Jit64::Default,    {"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  Interpreter::fcmpo,   &Jit64::fcmpx,      {"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   Interpreter::fcmpu,   &Jit64::fcmpx,      {"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  Interpreter::fctiwx,  &Jit64::Default,    {"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  Interpreter::fctiwzx, &Jit64::Default,    {"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  Interpreter::fmrx,    &Jit64::fmrx,       {"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, Interpreter::fnabsx,  &Jit64::Default,    {"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  Interpreter::fnegx,   &Jit64::Default,    {"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  Interpreter::frspx,   &Jit64::Default,    {"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  Interpreter::mcrfs,   &Jit64::Default,    {"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, Interpreter::mffsx,   &Jit64::Default,    {"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  Interpreter::mtfsb0x, &Jit64::Default,    {"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  Interpreter::mtfsb1x, &Jit64::Default,    {"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, Interpreter::mtfsfix, &Jit64::Default,    {"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, Interpreter::mtfsfx,  &Jit64::Default,    {"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

static GekkoOPTemplate table63_2[] = 
{
	{18, Interpreter::fdivx,   &Jit64::Default,     {"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, Interpreter::fsubx,   &Jit64::Default,     {"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, Interpreter::faddx,   &Jit64::Default,     {"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, Interpreter::fsqrtx,  &Jit64::Default,     {"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, Interpreter::fselx,   &Jit64::Default,     {"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, Interpreter::fmulx,   &Jit64::fp_arith_s,  {"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, Interpreter::frsqrtex,&Jit64::Default,     {"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, Interpreter::fmsubx,  &Jit64::fmaddXX,     {"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, Interpreter::fmaddx,  &Jit64::fmaddXX,     {"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, Interpreter::fnmsubx, &Jit64::fmaddXX,     {"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, Interpreter::fnmaddx, &Jit64::fmaddXX,     {"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};

namespace PPCTables
{

bool UsesFPU(UGeckoInstruction _inst)
{
	switch (_inst.OPCD)
	{
	case 04:	// PS
		return _inst.SUBOP10 != 1014;

	case 48:	// lfs
	case 49:	// lfsu
	case 50:	// lfd
	case 51:	// lfdu
	case 52:	// stfs
	case 53:	// stfsu
	case 54:	// stfd
	case 55:	// stfdu
	case 56:	// psq_l
	case 57:	// psq_lu

	case 59:	// FPU-sgl
	case 60:	// psq_st
	case 61:	// psq_stu
	case 63:	// FPU-dbl
		return true;

	case 31:
		switch (_inst.SUBOP10)
		{
		case 535:
		case 567:
		case 599:
		case 631:
		case 663:
		case 695:
		case 727:
		case 759:
		case 983:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}

void InitTables()
{
	//clear
	for (int i = 0; i < 32; i++) 
	{
		Interpreter::m_opTable59[i] = Interpreter::unknown_instruction;
		dynaOpTable59[i] = &Jit64::unknown_instruction;
		m_infoTable59[i] = 0;
	}

	for (int i = 0; i < 1024; i++)
	{
		Interpreter::m_opTable4 [i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable19[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable31[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable63[i] = Interpreter::unknown_instruction;
		dynaOpTable4 [i] = &Jit64::unknown_instruction;
		dynaOpTable19[i] = &Jit64::unknown_instruction;
		dynaOpTable31[i] = &Jit64::unknown_instruction;
		dynaOpTable63[i] = &Jit64::unknown_instruction;
		m_infoTable4[i] = 0;		
		m_infoTable19[i] = 0;		
		m_infoTable31[i] = 0;		
		m_infoTable63[i] = 0;		
	}

	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
	{
		Interpreter::m_opTable[primarytable[i].opcode] = primarytable[i].interpret;
		dynaOpTable[primarytable[i].opcode] = primarytable[i].recompile;
		m_infoTable[primarytable[i].opcode] = &primarytable[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_2[j].opcode;
			Interpreter::m_opTable4[op] = table4_2[j].interpret;
			dynaOpTable4[op] = table4_2[j].recompile;
			m_infoTable4[op] = &table4_2[j].opinfo;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (int j = 0; j < (int)(sizeof(table4_3) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_3[j].opcode;
			Interpreter::m_opTable4[op] = table4_3[j].interpret;
			dynaOpTable4[op] = table4_3[j].recompile;
			m_infoTable4[op] = &table4_3[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table4[i].opcode;
		Interpreter::m_opTable4[op] = table4[i].interpret;
		dynaOpTable4[op] = table4[i].recompile;
		m_infoTable4[op] = &table4[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table31[i].opcode;
		Interpreter::m_opTable31[op] = table31[i].interpret;
		dynaOpTable31[op] = table31[i].recompile;
		m_infoTable31[op] = &table31[i].opinfo;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (int j = 0; j < (int)(sizeof(table31_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table31_2[j].opcode;
			Interpreter::m_opTable31[op] = table31_2[j].interpret;
			dynaOpTable31[op] = table31_2[j].recompile;
			m_infoTable31[op] = &table31_2[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table19[i].opcode;
		Interpreter::m_opTable19[op] = table19[i].interpret;
		dynaOpTable19[op] = table19[i].recompile;
		m_infoTable19[op] = &table19[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table59[i].opcode;
		Interpreter::m_opTable59[op] = table59[i].interpret;
		dynaOpTable59[op] = table59[i].recompile;
		m_infoTable59[op] = &table59[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table63[i].opcode;
		Interpreter::m_opTable63[op] = table63[i].interpret;
		dynaOpTable63[op] = table63[i].recompile;
		m_infoTable63[op] = &table63[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table63_2[j].opcode;
			Interpreter::m_opTable63[op] = table63_2[j].interpret;
			dynaOpTable63[op] = table63_2[j].recompile;
			m_infoTable63[op] = &table63_2[j].opinfo;
		}
	}

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
	if (m_numInstructions >= 2048) {
		PanicAlert("m_allInstructions underdimensioned");
	}
}

// #define OPLOG

#ifdef OPLOG
namespace {
	std::vector<u32> rsplocations;
}
#endif

void CompileInstruction(UGeckoInstruction _inst)
{
	(jit.*dynaOpTable[_inst.OPCD])(_inst);	
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info) {
#ifdef OPLOG
		if (!strcmp(info->opname, "mcrfs")) {
			rsplocations.push_back(Jit64::js.compilerPC);
		}
#endif
		info->compileCount++;
		info->lastUse = jit.js.compilerPC;
	} else {
		PanicAlert("Tried to compile illegal (or unknown) instruction %08x, at %08x", _inst.hex, jit.js.compilerPC);
	}
}

bool IsValidInstruction(UGeckoInstruction _instCode)
{
	const GekkoOPInfo *info = GetOpInfo(_instCode);
	return info != 0;
}

void CountInstruction(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info)
		info->runCount++;
}

void PrintInstructionRunCounts()
{

	std::vector<inf> temp;
	for (int i = 0; i < m_numInstructions; i++)
	{
		inf x;
		x.name = m_allInstructions[i]->opname;
		x.count = m_allInstructions[i]->runCount;
		temp.push_back(x);
	}
	std::sort(temp.begin(), temp.end());
	for (int i = 0; i < m_numInstructions; i++)
	{
        LOG(GEKKO, "%s : %i", temp[i].name,temp[i].count);
	}
}

//TODO move to LogManager
void LogCompiledInstructions()
{
	static int time = 0;
	FILE *f = fopen(StringFromFormat(FULL_LOGS_DIR "inst_log%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount > 0) {
			fprintf(f, "%s\t%i\t%i\t%08x\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount, m_allInstructions[i]->lastUse);
		}
	}
	fclose(f);
	f = fopen(StringFromFormat(FULL_LOGS_DIR "inst_not%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount == 0) {
			fprintf(f, "%s\t%i\t%i\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount);
		}
	}
	fclose(f);
#ifdef OPLOG
	f = fopen(StringFromFormat(FULL_LOGS_DIR "mcrfs_at.txt", time).c_str(), "w");
	for (size_t i = 0; i < rsplocations.size(); i++) {
		fprintf(f, "mcrfs: %08x\n", rsplocations[i]);
	}
	fclose(f);
#endif
	time++;
}

}  // namespace
