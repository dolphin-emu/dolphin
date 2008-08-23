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

typedef void (*_recompilerInstruction) (UGeckoInstruction instCode);

struct GekkoOPTemplate
{
	int opcode;
	PPCTables::_interpreterInstruction interpret;
	PPCTables::_recompilerInstruction recompile;

	GekkoOPInfo opinfo;
	int runCount;
};

// The eventual goal is to be able to constify as much as possible in this file.
// Currently, the main obstacle is runCount above.
static GekkoOPInfo *m_infoTable[64];
static GekkoOPInfo *m_infoTable4[1024];
static GekkoOPInfo *m_infoTable19[1024];
static GekkoOPInfo *m_infoTable31[1024];
static GekkoOPInfo *m_infoTable59[32];
static GekkoOPInfo *m_infoTable63[1024];

static _recompilerInstruction dynaOpTable[64];
static _recompilerInstruction dynaOpTable4[1024];
static _recompilerInstruction dynaOpTable19[1024];
static _recompilerInstruction dynaOpTable31[1024];
static _recompilerInstruction dynaOpTable59[32];
static _recompilerInstruction dynaOpTable63[1024];

void DynaRunTable4(UGeckoInstruction _inst)  {dynaOpTable4 [_inst.SUBOP10](_inst);}
void DynaRunTable19(UGeckoInstruction _inst) {dynaOpTable19[_inst.SUBOP10](_inst);}
void DynaRunTable31(UGeckoInstruction _inst) {dynaOpTable31[_inst.SUBOP10](_inst);}
void DynaRunTable59(UGeckoInstruction _inst) {dynaOpTable59[_inst.SUBOP5 ](_inst);}
void DynaRunTable63(UGeckoInstruction _inst) {dynaOpTable63[_inst.SUBOP10](_inst);}

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

CInterpreter::_interpreterInstruction GetInterpreterOp(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch(table) 
		{
		case 4:  return CInterpreter::m_opTable4[_inst.SUBOP10];
		case 19: return CInterpreter::m_opTable19[_inst.SUBOP10];
		case 31: return CInterpreter::m_opTable31[_inst.SUBOP10];
		case 59: return CInterpreter::m_opTable59[_inst.SUBOP5];
		case 63: return CInterpreter::m_opTable63[_inst.SUBOP10];
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
		return CInterpreter::m_opTable[_inst.OPCD];
	}
}


GekkoOPTemplate primarytable[] = 
{
	{4,  CInterpreter::RunTable4,     DynaRunTable4,  {"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0}},
	{19, CInterpreter::RunTable19,    DynaRunTable19, {"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0}},
	{31, CInterpreter::RunTable31,    DynaRunTable31, {"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0}},
	{59, CInterpreter::RunTable59,    DynaRunTable59, {"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0}},
	{63, CInterpreter::RunTable63,    DynaRunTable63, {"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0}},

	{16, CInterpreter::bcx,           Jit64::bcx,         {"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{18, CInterpreter::bx,            Jit64::bx,          {"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK}},

	{1,  CInterpreter::HLEFunction,   Jit64::HLEFunction, {"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{2,  CInterpreter::CompiledBlock, Jit64::Default,     {"DynaBlock",   OPTYPE_SYSTEM, 0}},
	{3,  CInterpreter::twi,           Jit64::Default,     {"twi",         OPTYPE_SYSTEM, 0}},
	{17, CInterpreter::sc,            Jit64::sc,          {"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 1}},

	{7,  CInterpreter::mulli,     Jit64::mulli,        {"mulli",    OPTYPE_INTEGER, FL_RC_BIT, 2}},
	{8,  CInterpreter::subfic,    Jit64::subfic,       {"subfic",   OPTYPE_INTEGER, FL_SET_CA}},
	{10, CInterpreter::cmpli,     Jit64::cmpli,        {"cmpli",    OPTYPE_INTEGER, FL_SET_CRn}},
	{11, CInterpreter::cmpi,      Jit64::cmpi,         {"cmpi",     OPTYPE_INTEGER, FL_SET_CRn}},
	{12, CInterpreter::addic,     Jit64::reg_imm,      {"addic",    OPTYPE_INTEGER, FL_SET_CA}},
	{13, CInterpreter::addic_rc,  Jit64::reg_imm,      {"addic_rc", OPTYPE_INTEGER, FL_SET_CR0}},
	{14, CInterpreter::addi,      Jit64::reg_imm,      {"addi",     OPTYPE_INTEGER, 0}},
	{15, CInterpreter::addis,     Jit64::reg_imm,      {"addis",    OPTYPE_INTEGER, 0}},

	{20, CInterpreter::rlwimix,   Jit64::rlwimix,      {"rlwimix",  OPTYPE_INTEGER, FL_RC_BIT}},
	{21, CInterpreter::rlwinmx,   Jit64::rlwinmx,      {"rlwinmx",  OPTYPE_INTEGER, FL_RC_BIT}},
	{23, CInterpreter::rlwnmx,    Jit64::rlwnmx,       {"rlwnmx",   OPTYPE_INTEGER, FL_RC_BIT}},

	{24, CInterpreter::ori,       Jit64::reg_imm,      {"ori",      OPTYPE_INTEGER, 0}},
	{25, CInterpreter::oris,      Jit64::reg_imm,      {"oris",     OPTYPE_INTEGER, 0}},
	{26, CInterpreter::xori,      Jit64::reg_imm,      {"xori",     OPTYPE_INTEGER, 0}},
	{27, CInterpreter::xoris,     Jit64::reg_imm,      {"xoris",    OPTYPE_INTEGER, 0}},
	{28, CInterpreter::andi_rc,   Jit64::reg_imm,      {"andi_rc",  OPTYPE_INTEGER, FL_SET_CR0}},
	{29, CInterpreter::andis_rc,  Jit64::reg_imm,      {"andis_rc", OPTYPE_INTEGER, FL_SET_CR0}},

	{32, CInterpreter::lwz,       Jit64::lXz,      {"lwz",  OPTYPE_LOAD, 0}},
	{33, CInterpreter::lwzu,      Jit64::Default,  {"lwzu", OPTYPE_LOAD, 0}},
	{34, CInterpreter::lbz,       Jit64::lXz,      {"lbz",  OPTYPE_LOAD, 0}},
	{35, CInterpreter::lbzu,      Jit64::Default,  {"lbzu", OPTYPE_LOAD, 0}},
	{40, CInterpreter::lhz,       Jit64::lXz,      {"lhz",  OPTYPE_LOAD, 0}},
	{41, CInterpreter::lhzu,      Jit64::Default,  {"lhzu", OPTYPE_LOAD, 0}},
	{42, CInterpreter::lha,       Jit64::lha,      {"lha",  OPTYPE_LOAD, 0}},
	{43, CInterpreter::lhau,      Jit64::Default,  {"lhau", OPTYPE_LOAD, 0}},

	{48, CInterpreter::lfs,       Jit64::lfs,      {"lfs",  OPTYPE_LOADFP, 0}},
	{49, CInterpreter::lfsu,      Jit64::Default,  {"lfsu", OPTYPE_LOADFP, 0}},
	{50, CInterpreter::lfd,       Jit64::lfd,      {"lfd",  OPTYPE_LOADFP, 0}},
	{51, CInterpreter::lfdu,      Jit64::Default,  {"lfdu", OPTYPE_LOADFP, 0}},

	{44, CInterpreter::sth,       Jit64::stX,      {"sth",  OPTYPE_STORE, 0}},
	{45, CInterpreter::sthu,      Jit64::stX,      {"sthu", OPTYPE_STORE, 0}},
	{36, CInterpreter::stw,       Jit64::stX,      {"stw",  OPTYPE_STORE, 0}},
	{37, CInterpreter::stwu,      Jit64::stX,      {"stwu", OPTYPE_STORE, 0}},
	{38, CInterpreter::stb,	      Jit64::stX,      {"stb",  OPTYPE_STORE, 0}},
	{39, CInterpreter::stbu,      Jit64::stX,      {"stbu", OPTYPE_STORE, 0}},

	{52, CInterpreter::stfs,      Jit64::stfs,     {"stfs",  OPTYPE_STOREFP, 0}},
	{53, CInterpreter::stfsu,     Jit64::stfs,     {"stfsu", OPTYPE_STOREFP, 0}},
	{54, CInterpreter::stfd,      Jit64::stfd,     {"stfd",  OPTYPE_STOREFP, 0}},
	{55, CInterpreter::stfdu,     Jit64::Default,  {"stfdu", OPTYPE_STOREFP, 0}},

	{46, CInterpreter::lmw,       Jit64::lmw,      {"lmw",   OPTYPE_SYSTEM, 0, 10}},
	{47, CInterpreter::stmw,      Jit64::stmw,     {"stmw",  OPTYPE_SYSTEM, 0, 10}},

	{56, CInterpreter::psq_l,     Jit64::psq_l,    {"psq_l",   OPTYPE_PS, 0}},
	{57, CInterpreter::psq_lu,    Jit64::psq_l,    {"psq_lu",  OPTYPE_PS, 0}},
	{60, CInterpreter::psq_st,    Jit64::psq_st,   {"psq_st",  OPTYPE_PS, 0}},
	{61, CInterpreter::psq_stu,   Jit64::psq_st,   {"psq_stu", OPTYPE_PS, 0}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{5,  CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{6,  CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{9,  CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{22, CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{30, CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{62, CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
	{58, CInterpreter::unknown_instruction, Jit64::Default,  {"unknown_instruction", OPTYPE_UNKNOWN, 0}},
};

GekkoOPTemplate table4[] = 
{
	{0,    CInterpreter::ps_cmpu0,   Jit64::Default,     {"ps_cmpu0",   OPTYPE_PS, FL_SET_CRn}},
	{32,   CInterpreter::ps_cmpo0,   Jit64::Default,     {"ps_cmpo0",   OPTYPE_PS, FL_SET_CRn}},
	{40,   CInterpreter::ps_neg,     Jit64::ps_sign,     {"ps_neg",     OPTYPE_PS, FL_RC_BIT}},
	{136,  CInterpreter::ps_nabs,    Jit64::ps_sign,     {"ps_nabs",    OPTYPE_PS, FL_RC_BIT}},
	{264,  CInterpreter::ps_abs,     Jit64::ps_sign,     {"ps_abs",     OPTYPE_PS, FL_RC_BIT}},
	{64,   CInterpreter::ps_cmpu1,   Jit64::Default,     {"ps_cmpu1",   OPTYPE_PS, FL_RC_BIT}},
	{72,   CInterpreter::ps_mr,      Jit64::ps_mr,       {"ps_mr",      OPTYPE_PS, FL_RC_BIT}},
	{96,   CInterpreter::ps_cmpo1,   Jit64::Default,     {"ps_cmpo1",   OPTYPE_PS, FL_RC_BIT}},
	{528,  CInterpreter::ps_merge00, Jit64::ps_mergeXX,  {"ps_merge00", OPTYPE_PS, FL_RC_BIT}},
	{560,  CInterpreter::ps_merge01, Jit64::ps_mergeXX,  {"ps_merge01", OPTYPE_PS, FL_RC_BIT}},
	{592,  CInterpreter::ps_merge10, Jit64::ps_mergeXX,  {"ps_merge10", OPTYPE_PS, FL_RC_BIT}},
	{624,  CInterpreter::ps_merge11, Jit64::ps_mergeXX,  {"ps_merge11", OPTYPE_PS, FL_RC_BIT}},

	{1014, CInterpreter::dcbz_l,     Jit64::Default,     {"dcbz_l", OPTYPE_SYSTEM, 0}},
};		

GekkoOPTemplate table4_2[] = 
{
	{10, CInterpreter::ps_sum0,   Jit64::Default,   {"ps_sum0",   OPTYPE_PS, 0}},
	{11, CInterpreter::ps_sum1,   Jit64::Default,   {"ps_sum1",   OPTYPE_PS, 0}},
	{12, CInterpreter::ps_muls0,  Jit64::Default,   {"ps_muls0",  OPTYPE_PS, 0}},
	{13, CInterpreter::ps_muls1,  Jit64::Default,   {"ps_muls1",  OPTYPE_PS, 0}},
	{14, CInterpreter::ps_madds0, Jit64::Default,   {"ps_madds0", OPTYPE_PS, 0}},
	{15, CInterpreter::ps_madds1, Jit64::Default,   {"ps_madds1", OPTYPE_PS, 0}},
	{18, CInterpreter::ps_div,    Jit64::ps_arith,  {"ps_div",    OPTYPE_PS, 0, 16}},
	{20, CInterpreter::ps_sub,    Jit64::ps_arith,  {"ps_sub",    OPTYPE_PS, 0}},
	{21, CInterpreter::ps_add,    Jit64::ps_arith,  {"ps_add",    OPTYPE_PS, 0}},
	{23, CInterpreter::ps_sel,    Jit64::ps_sel,    {"ps_sel",    OPTYPE_PS, 0}},
	{24, CInterpreter::ps_res,    Jit64::Default,   {"ps_res",    OPTYPE_PS, 0}},
	{25, CInterpreter::ps_mul,    Jit64::ps_arith,  {"ps_mul",    OPTYPE_PS, 0}},
	{26, CInterpreter::ps_rsqrte, Jit64::ps_rsqrte, {"ps_rsqrte", OPTYPE_PS, 0}},
	{28, CInterpreter::ps_msub,   Jit64::ps_maddXX, {"ps_msub",   OPTYPE_PS, 0}},
	{29, CInterpreter::ps_madd,   Jit64::ps_maddXX, {"ps_madd",   OPTYPE_PS, 0}},
	{30, CInterpreter::ps_nmsub,  Jit64::ps_maddXX, {"ps_nmsub",  OPTYPE_PS, 0}},
	{31, CInterpreter::ps_nmadd,  Jit64::ps_maddXX, {"ps_nmadd",  OPTYPE_PS, 0}},
};

GekkoOPTemplate table4_3[] = 
{
	{6,  CInterpreter::psq_lx,   Jit64::Default, {"psq_lx",   OPTYPE_PS, 0}},
	{7,  CInterpreter::psq_stx,  Jit64::Default, {"psq_stx",  OPTYPE_PS, 0}},
	{38, CInterpreter::psq_lux,  Jit64::Default, {"psq_lux",  OPTYPE_PS, 0}},
	{39, CInterpreter::psq_stux, Jit64::Default, {"psq_stux", OPTYPE_PS, 0}}, 
};

GekkoOPTemplate table19[] = 
{
	{528, CInterpreter::bcctrx, Jit64::bcctrx,    {"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK}},
	{16,  CInterpreter::bclrx,  Jit64::bclrx,     {"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK}},
	{257, CInterpreter::crand,  Jit64::Default,   {"crand",  OPTYPE_CR, 0}},
	{129, CInterpreter::crandc, Jit64::Default,   {"crandc", OPTYPE_CR, 0}},
	{289, CInterpreter::creqv,  Jit64::Default,   {"creqv",  OPTYPE_CR, 0}},
	{225, CInterpreter::crnand, Jit64::Default,   {"crnand", OPTYPE_CR, 0}},
	{33,  CInterpreter::crnor,  Jit64::Default,   {"crnor",  OPTYPE_CR, 0}},
	{449, CInterpreter::cror,   Jit64::Default,   {"cror",   OPTYPE_CR, 0}},
	{417, CInterpreter::crorc,  Jit64::Default,   {"crorc",  OPTYPE_CR, 0}},
	{193, CInterpreter::crxor,  Jit64::Default,   {"crxor",  OPTYPE_CR, 0}},
												   
	{150, CInterpreter::isync,  Jit64::DoNothing, {"isync",  OPTYPE_ICACHE, 0	}},
	{0,   CInterpreter::mcrf,   Jit64::Default,   {"mcrf",   OPTYPE_SYSTEM, 0}},
												   
	{50,  CInterpreter::rfi,    Jit64::rfi,       {"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1}},
	{18,  CInterpreter::rfid,   Jit64::Default,   {"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS}}
};


GekkoOPTemplate table31[] = 
{
	{28,  CInterpreter::andx,    Jit64::andx,     {"andx",   OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{60,  CInterpreter::andcx,   Jit64::Default,  {"andcx",  OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{444, CInterpreter::orx,     Jit64::orx,      {"orx",    OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{124, CInterpreter::norx,    Jit64::Default,  {"norx",   OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{316, CInterpreter::xorx,    Jit64::Default,  {"xorx",   OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{412, CInterpreter::orcx,    Jit64::Default,  {"orcx",   OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{476, CInterpreter::nandx,   Jit64::Default,  {"nandx",  OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{284, CInterpreter::eqvx,    Jit64::Default,  {"eqvx",   OPTYPE_INTEGER, FL_IN_AB | FL_OUT_S | FL_RC_BIT}},
	{0,   CInterpreter::cmp,     Jit64::cmp,      {"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{32,  CInterpreter::cmpl,    Jit64::cmpl,     {"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn}},
	{26,  CInterpreter::cntlzwx, Jit64::cntlzwx,  {"cntlzwx",OPTYPE_INTEGER, FL_IN_A  | FL_OUT_S | FL_RC_BIT}},
	{922, CInterpreter::extshx,  Jit64::extshx,   {"extshx", OPTYPE_INTEGER, FL_IN_A  | FL_OUT_S | FL_RC_BIT}},
	{954, CInterpreter::extsbx,  Jit64::extsbx,   {"extsbx", OPTYPE_INTEGER, FL_IN_A  | FL_OUT_S | FL_RC_BIT}},
	{536, CInterpreter::srwx,    Jit64::srwx,     {"srwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{792, CInterpreter::srawx,   Jit64::Default,  {"srawx",  OPTYPE_INTEGER, FL_RC_BIT}},
	{824, CInterpreter::srawix,  Jit64::srawix,   {"srawix", OPTYPE_INTEGER, FL_RC_BIT}},
	{24,  CInterpreter::slwx,    Jit64::slwx,     {"slwx",   OPTYPE_INTEGER, FL_RC_BIT}},

	{54,   CInterpreter::dcbst,  Jit64::Default,  {"dcbst",  OPTYPE_DCACHE, 0, 4}},
	{86,   CInterpreter::dcbf,   Jit64::Default,  {"dcbf",   OPTYPE_DCACHE, 0, 4}},
	{246,  CInterpreter::dcbtst, Jit64::Default,  {"dcbtst", OPTYPE_DCACHE, 0, 1}},
	{278,  CInterpreter::dcbt,   Jit64::Default,  {"dcbt",   OPTYPE_DCACHE, 0, 1}},
	{470,  CInterpreter::dcbi,   Jit64::Default,  {"dcbi",   OPTYPE_DCACHE, 0, 4}},
	{758,  CInterpreter::dcba,   Jit64::Default,  {"dcba",   OPTYPE_DCACHE, 0, 4}},
	{1014, CInterpreter::dcbz,   Jit64::dcbz,     {"dcbz",   OPTYPE_DCACHE, 0, 4}},

	//load word
	{23,  CInterpreter::lwzx,  Jit64::Default,   {"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{55,  CInterpreter::lwzux, Jit64::Default,   {"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_IN_B}},

	//load halfword
	{279, CInterpreter::lhzx,  Jit64::Default,   {"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{311, CInterpreter::lhzux, Jit64::Default,   {"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_IN_B}},

	//load halfword signextend
	{343, CInterpreter::lhax,  Jit64::Default,   {"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{375, CInterpreter::lhaux, Jit64::Default,   {"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_IN_B}},

	//load byte
	{87,  CInterpreter::lbzx,  Jit64::lbzx,      {"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{119, CInterpreter::lbzux, Jit64::Default,   {"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_IN_B}},

	//load byte reverse
	{534, CInterpreter::lwbrx, Jit64::Default,  {"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},
	{790, CInterpreter::lhbrx, Jit64::Default,  {"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0 | FL_IN_B}},

	{20,  CInterpreter::lwarx, Jit64::Default,  {"lwarx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B}},

	//load string (interpret these)
	{533, CInterpreter::lswx,  Jit64::Default,   {"lswx",  OPTYPE_LOAD, FL_IN_A | FL_OUT_D}},
	{597, CInterpreter::lswi,  Jit64::Default,   {"lswi",  OPTYPE_LOAD, FL_IN_AB | FL_OUT_D}},

	{535, CInterpreter::lfsx,  Jit64::lfsx,      {"lfsx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{567, CInterpreter::lfsux, Jit64::Default,   {"lfsux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},
	{599, CInterpreter::lfdx,  Jit64::Default,   {"lfdx",  OPTYPE_LOADFP, FL_IN_A0 | FL_IN_B}},
	{631, CInterpreter::lfdux, Jit64::Default,   {"lfdux", OPTYPE_LOADFP, FL_IN_A | FL_IN_B}},

	//store word
	{151, CInterpreter::stwx,   Jit64::Default,    {"stwx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{183, CInterpreter::stwux,  Jit64::Default,    {"stwux",  OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	//store halfword
	{407, CInterpreter::sthx,   Jit64::Default,    {"sthx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{439, CInterpreter::sthux,  Jit64::Default,    {"sthux",  OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	//store byte
	{215, CInterpreter::stbx,   Jit64::Default,    {"stbx",   OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{247, CInterpreter::stbux,  Jit64::Default,    {"stbux",  OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	//store bytereverse
	{662, CInterpreter::stwbrx, Jit64::Default,   {"stwbrx", OPTYPE_STORE, FL_IN_A0 | FL_IN_B}},
	{918, CInterpreter::sthbrx, Jit64::Default,   {"sthbrx", OPTYPE_STORE, FL_IN_A | FL_IN_B}},

	{661, CInterpreter::stswx,  Jit64::Default,   {"stswx",  OPTYPE_STORE, 0}},
	{725, CInterpreter::stswi,  Jit64::Default,   {"stswi",  OPTYPE_STORE, 0}},

	{663, CInterpreter::stfsx,  Jit64::Default,   {"stfsx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{695, CInterpreter::stfsux, Jit64::Default,   {"stfsux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{727, CInterpreter::stfdx,  Jit64::Default,   {"stfdx",  OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},
	{759, CInterpreter::stfdux, Jit64::Default,   {"stfdux", OPTYPE_STOREFP, FL_IN_A | FL_IN_B}},
	{983, CInterpreter::stfiwx, Jit64::Default,   {"stfiwx", OPTYPE_STOREFP, FL_IN_A0 | FL_IN_B}},

	{19,  CInterpreter::mfcr,   Jit64::Default,    {"mfcr",   OPTYPE_SYSTEM, 0}},
	{83,  CInterpreter::mfmsr,  Jit64::mfmsr,      {"mfmsr",  OPTYPE_SYSTEM, 0}},
	{144, CInterpreter::mtcrf,  Jit64::Default,    {"mtcrf",  OPTYPE_SYSTEM, 0}},
	{146, CInterpreter::mtmsr,  Jit64::mtmsr,      {"mtmsr",  OPTYPE_SYSTEM, FL_ENDBLOCK}},
	{210, CInterpreter::mtsr,   Jit64::Default,    {"mtsr",   OPTYPE_SYSTEM, 0}},
	{242, CInterpreter::mtsrin, Jit64::Default,    {"mtsrin", OPTYPE_SYSTEM, 0}},
	{339, CInterpreter::mfspr,  Jit64::mfspr,      {"mfspr",  OPTYPE_SYSTEM, 0}},
	{467, CInterpreter::mtspr,  Jit64::mtspr,      {"mtspr",  OPTYPE_SYSTEM, 0, 2}},
	{371, CInterpreter::mftb,   Jit64::mftb,       {"mftb",   OPTYPE_SYSTEM, FL_TIMER}},
	{512, CInterpreter::mcrxr,  Jit64::Default,    {"mcrxr",  OPTYPE_SYSTEM, 0}},
	{595, CInterpreter::mfsr,   Jit64::Default,    {"mfsr",   OPTYPE_SYSTEM, 0, 2}},
	{659, CInterpreter::mfsrin, Jit64::Default,    {"mfsrin", OPTYPE_SYSTEM, 0, 2}},

	{4,   CInterpreter::tw,      Jit64::Default,     {"tw",     OPTYPE_SYSTEM, 0, 1}},
	{598, CInterpreter::sync,    Jit64::Default,     {"sync",   OPTYPE_SYSTEM, 0, 2}},
	{982, CInterpreter::icbi,    Jit64::Default,     {"icbi",   OPTYPE_SYSTEM, 0, 3}},

	//Unused instructions on GC
	{310, CInterpreter::eciwx,   Jit64::Default,    {"eciwx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{438, CInterpreter::ecowx,   Jit64::Default,    {"ecowx",   OPTYPE_INTEGER, FL_RC_BIT}},
	{854, CInterpreter::eieio,   Jit64::Default,    {"eieio",   OPTYPE_INTEGER, FL_RC_BIT}},
	{306, CInterpreter::tlbie,   Jit64::Default,    {"tlbie",   OPTYPE_SYSTEM, 0}},
	{370, CInterpreter::tlbia,   Jit64::Default,    {"tlbia",   OPTYPE_SYSTEM, 0}},
	{566, CInterpreter::tlbsync, Jit64::Default,    {"tlbsync", OPTYPE_SYSTEM, 0}},
	{150, CInterpreter::stwcxd,  Jit64::Default,    {"stwcxd",  OPTYPE_STORE, 0}},
};

GekkoOPTemplate table31_2[] = 
{	
	{266, CInterpreter::addx,    Jit64::addx,         {"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT}},
	{10,  CInterpreter::addcx,   Jit64::Default,      {"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_SET_CA | FL_RC_BIT}},
	{138, CInterpreter::addex,   Jit64::addex,        {"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{234, CInterpreter::addmex,  Jit64::Default,      {"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{202, CInterpreter::addzex,  Jit64::Default,      {"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{491, CInterpreter::divwx,   Jit64::Default,      {"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT, 39}},
	{459, CInterpreter::divwux,  Jit64::divwux,       {"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT, 39}},
	{75,  CInterpreter::mulhwx,  Jit64::Default,      {"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT, 4}},
	{11,  CInterpreter::mulhwux, Jit64::mulhwux,      {"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT, 4}},
	{235, CInterpreter::mullwx,  Jit64::mullwx,       {"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT, 4}},
	{104, CInterpreter::negx,    Jit64::negx,         {"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT}},
	{40,  CInterpreter::subfx,   Jit64::subfx,        {"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_RC_BIT}},
	{8,   CInterpreter::subfcx,  Jit64::subfcx,       {"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_SET_CA | FL_RC_BIT}},
	{136, CInterpreter::subfex,  Jit64::Default,      {"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{232, CInterpreter::subfmex, Jit64::Default,      {"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
	{200, CInterpreter::subfzex, Jit64::Default,      {"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_IN_B | FL_READ_CA | FL_SET_CA | FL_RC_BIT}},
};

GekkoOPTemplate table59[] = 
{
	{18, CInterpreter::fdivsx,   Jit64::fp_arith_s,    {"fdivsx",   OPTYPE_FPU, FL_RC_BIT_F, 16}}, 
	{20, CInterpreter::fsubsx,   Jit64::fp_arith_s,    {"fsubsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{21, CInterpreter::faddsx,   Jit64::fp_arith_s,    {"faddsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
//	{22, CInterpreter::fsqrtsx,  Jit64::Default,       {"fsqrtsx",  OPTYPE_FPU, FL_RC_BIT_F}}, // Not implemented on gekko
	{24, CInterpreter::fresx,    Jit64::Default,       {"fresx",    OPTYPE_FPU, FL_RC_BIT_F}}, 
	{25, CInterpreter::fmulsx,   Jit64::fp_arith_s,    {"fmulsx",   OPTYPE_FPU, FL_RC_BIT_F}}, 
	{28, CInterpreter::fmsubsx,  Jit64::fmaddXX,       {"fmsubsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{29, CInterpreter::fmaddsx,  Jit64::fmaddXX,       {"fmaddsx",  OPTYPE_FPU, FL_RC_BIT_F}}, 
	{30, CInterpreter::fnmsubsx, Jit64::fmaddXX,       {"fnmsubsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
	{31, CInterpreter::fnmaddsx, Jit64::fmaddXX,       {"fnmaddsx", OPTYPE_FPU, FL_RC_BIT_F}}, 
};							    

GekkoOPTemplate table63[] = 
{
	{264, CInterpreter::fabsx,   Jit64::Default,    {"fabsx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{32,  CInterpreter::fcmpo,   Jit64::fcmpx,      {"fcmpo",   OPTYPE_FPU, FL_RC_BIT_F}},
	{0,   CInterpreter::fcmpu,   Jit64::fcmpx,      {"fcmpu",   OPTYPE_FPU, FL_RC_BIT_F}},
	{14,  CInterpreter::fctiwx,  Jit64::Default,    {"fctiwx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{15,  CInterpreter::fctiwzx, Jit64::Default,    {"fctiwzx", OPTYPE_FPU, FL_RC_BIT_F}},
	{72,  CInterpreter::fmrx,    Jit64::fmrx,       {"fmrx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{136, CInterpreter::fnabsx,  Jit64::Default,    {"fnabsx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{40,  CInterpreter::fnegx,   Jit64::Default,    {"fnegx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{12,  CInterpreter::frspx,   Jit64::Default,    {"frspx",   OPTYPE_FPU, FL_RC_BIT_F}},

	{64,  CInterpreter::mcrfs,   Jit64::Default,    {"mcrfs",   OPTYPE_SYSTEMFP, 0}},
	{583, CInterpreter::mffsx,   Jit64::Default,    {"mffsx",   OPTYPE_SYSTEMFP, 0}},
	{70,  CInterpreter::mtfsb0x, Jit64::Default,    {"mtfsb0x", OPTYPE_SYSTEMFP, 0, 2}},
	{38,  CInterpreter::mtfsb1x, Jit64::Default,    {"mtfsb1x", OPTYPE_SYSTEMFP, 0, 2}},
	{134, CInterpreter::mtfsfix, Jit64::Default,    {"mtfsfix", OPTYPE_SYSTEMFP, 0, 2}},
	{711, CInterpreter::mtfsfx,  Jit64::Default,    {"mtfsfx",  OPTYPE_SYSTEMFP, 0, 2}},
};

GekkoOPTemplate table63_2[] = 
{
	{18, CInterpreter::fdivx,   Jit64::Default,     {"fdivx",    OPTYPE_FPU, FL_RC_BIT_F, 30}},
	{20, CInterpreter::fsubx,   Jit64::Default,     {"fsubx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{21, CInterpreter::faddx,   Jit64::Default,     {"faddx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{22, CInterpreter::fsqrtx,  Jit64::Default,     {"fsqrtx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{23, CInterpreter::fselx,   Jit64::Default,     {"fselx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{25, CInterpreter::fmulx,   Jit64::Default,     {"fmulx",    OPTYPE_FPU, FL_RC_BIT_F}},
	{26, CInterpreter::frsqrtex,Jit64::Default,     {"frsqrtex", OPTYPE_FPU, FL_RC_BIT_F}},
	{28, CInterpreter::fmsubx,  Jit64::Default,     {"fmsubx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{29, CInterpreter::fmaddx,  Jit64::Default,     {"fmaddx",   OPTYPE_FPU, FL_RC_BIT_F}},
	{30, CInterpreter::fnmsubx, Jit64::Default,     {"fnmsubx",  OPTYPE_FPU, FL_RC_BIT_F}},
	{31, CInterpreter::fnmaddx, Jit64::Default,     {"fnmaddx",  OPTYPE_FPU, FL_RC_BIT_F}},
};

bool PPCTables::UsesFPU(UGeckoInstruction _inst)
{
	switch (_inst.OPCD)
	{
	case 04:	// PS
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

void PPCTables::InitTables()
{
	//clear
	for (int i = 0; i < 32; i++) 
	{
		CInterpreter::m_opTable59[i] = CInterpreter::unknown_instruction;
		dynaOpTable59[i] = Jit64::unknown_instruction;
		m_infoTable59[i] = 0;
	}

	for (int i = 0; i < 1024; i++)
	{
		CInterpreter::m_opTable4 [i] = CInterpreter::unknown_instruction;
		CInterpreter::m_opTable19[i] = CInterpreter::unknown_instruction;
		CInterpreter::m_opTable31[i] = CInterpreter::unknown_instruction;
		CInterpreter::m_opTable63[i] = CInterpreter::unknown_instruction;
		dynaOpTable4 [i] = Jit64::unknown_instruction;
		dynaOpTable19[i] = Jit64::unknown_instruction;
		dynaOpTable31[i] = Jit64::unknown_instruction;
		dynaOpTable63[i] = Jit64::unknown_instruction;
		m_infoTable4[i] = 0;		
		m_infoTable19[i] = 0;		
		m_infoTable31[i] = 0;		
		m_infoTable63[i] = 0;		
	}

	for (int i = 0; i < (int)(sizeof(primarytable) / sizeof(GekkoOPTemplate)); i++)
	{
		CInterpreter::m_opTable[primarytable[i].opcode] = primarytable[i].interpret;
		dynaOpTable[primarytable[i].opcode] = primarytable[i].recompile;
		m_infoTable[primarytable[i].opcode] = &primarytable[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table4_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill+table4_2[j].opcode;
			CInterpreter::m_opTable4[op] = table4_2[j].interpret;
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
			CInterpreter::m_opTable4[op] = table4_3[j].interpret;
			dynaOpTable4[op] = table4_3[j].recompile;
			m_infoTable4[op] = &table4_3[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table4) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table4[i].opcode;
		CInterpreter::m_opTable4[op] = table4[i].interpret;
		dynaOpTable4[op] = table4[i].recompile;
		m_infoTable4[op] = &table4[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table31) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table31[i].opcode;
		CInterpreter::m_opTable31[op] = table31[i].interpret;
		dynaOpTable31[op] = table31[i].recompile;
		m_infoTable31[op] = &table31[i].opinfo;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (int j = 0; j < (int)(sizeof(table31_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table31_2[j].opcode;
			CInterpreter::m_opTable31[op] = table31_2[j].interpret;
			dynaOpTable31[op] = table31_2[j].recompile;
			m_infoTable31[op] = &table31_2[j].opinfo;
		}
	}

	for (int i = 0; i < (int)(sizeof(table19) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table19[i].opcode;
		CInterpreter::m_opTable19[op] = table19[i].interpret;
		dynaOpTable19[op] = table19[i].recompile;
		m_infoTable19[op] = &table19[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table59) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table59[i].opcode;
		CInterpreter::m_opTable59[op] = table59[i].interpret;
		dynaOpTable59[op] = table59[i].recompile;
		m_infoTable59[op] = &table59[i].opinfo;
	}

	for (int i = 0; i < (int)(sizeof(table63) / sizeof(GekkoOPTemplate)); i++)
	{
		int op = table63[i].opcode;
		CInterpreter::m_opTable63[op] = table63[i].interpret;
		dynaOpTable63[op] = table63[i].recompile;
		m_infoTable63[op] = &table63[i].opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (int j = 0; j < (int)(sizeof(table63_2) / sizeof(GekkoOPTemplate)); j++)
		{
			int op = fill + table63_2[j].opcode;
			CInterpreter::m_opTable63[op] = table63_2[j].interpret;
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

namespace {
	std::vector<u32> rsplocations;
}

void PPCTables::CompileInstruction(UGeckoInstruction _inst)
{
	dynaOpTable[_inst.OPCD](_inst);	
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info) {
		if (!strcmp(info->opname, "mcrfs")) {
			rsplocations.push_back(Jit64::js.compilerPC);
		}
		info->compileCount++;
		info->lastUse = Jit64::js.compilerPC;
	}
}

bool PPCTables::IsValidInstruction(UGeckoInstruction _instCode)
{
	const GekkoOPInfo *info = GetOpInfo(_instCode);
	return info != 0;
}

void PPCTables::CountInstruction(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info)
		info->runCount++;
}

struct inf
{
	const char *name;
	int count;
	bool operator < (const inf &o) const
	{
		return count > o.count;
	}
};

void PPCTables::PrintInstructionRunCounts()
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

void PPCTables::LogCompiledInstructions()
{
	static int time = 0;
	FILE *f = fopen(StringFromFormat("inst_log%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount > 0) {
			fprintf(f, "%s\t%i\t%i\t%08x\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount, m_allInstructions[i]->lastUse);
		}
	}
	fclose(f);
	f = fopen(StringFromFormat("inst_not%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount == 0) {
			fprintf(f, "%s\t%i\t%i\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount);
		}
	}
	fclose(f);
	f = fopen(StringFromFormat("mcrfs_at.txt", time).c_str(), "w");
	for (size_t i = 0; i < rsplocations.size(); i++) {
		fprintf(f, "mcrfs: %08x\n", rsplocations[i]);
	}
	fclose(f);
	time++;
}
