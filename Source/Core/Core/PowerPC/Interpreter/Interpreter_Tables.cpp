// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter_Tables.h"

typedef void (*_Instruction) (UGeckoInstruction instCode);

struct GekkoOPTemplate
{
	int opcode;
	_Instruction Inst;
	GekkoOPInfo opinfo;
};

static GekkoOPTemplate primarytable[] =
{
	{4,  Interpreter::RunTable4,    {"RunTable4",  OPTYPE_SUBTABLE | (4<<24), 0, 0, 0, 0, 0}},
	{19, Interpreter::RunTable19,   {"RunTable19", OPTYPE_SUBTABLE | (19<<24), 0, 0, 0, 0, 0}},
	{31, Interpreter::RunTable31,   {"RunTable31", OPTYPE_SUBTABLE | (31<<24), 0, 0, 0, 0, 0}},
	{59, Interpreter::RunTable59,   {"RunTable59", OPTYPE_SUBTABLE | (59<<24), 0, 0, 0, 0, 0}},
	{63, Interpreter::RunTable63,   {"RunTable63", OPTYPE_SUBTABLE | (63<<24), 0, 0, 0, 0, 0}},

	{16, Interpreter::bcx,          {"bcx", OPTYPE_SYSTEM, FL_ENDBLOCK, 1, 0, 0, 0}},
	{18, Interpreter::bx,           {"bx",  OPTYPE_SYSTEM, FL_ENDBLOCK, 1, 0, 0, 0}},

	{1,  Interpreter::HLEFunction,  {"HLEFunction", OPTYPE_SYSTEM, FL_ENDBLOCK, 1, 0, 0, 0}},
	{2,  Interpreter::CompiledBlock,{"DynaBlock",   OPTYPE_SYSTEM, 1, 0, 0, 0, 0}},
	{3,  Interpreter::twi,          {"twi",         OPTYPE_SYSTEM, FL_ENDBLOCK, 1, 0, 0, 0}},
	{17, Interpreter::sc,           {"sc",          OPTYPE_SYSTEM, FL_ENDBLOCK, 2, 0, 0, 0}},

	{7,  Interpreter::mulli,        {"mulli",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_RC_BIT, 3, 0, 0, 0}},
	{8,  Interpreter::subfic,       {"subfic",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA, 1, 0, 0, 0}},
	{10, Interpreter::cmpli,        {"cmpli",    OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn, 1, 0, 0, 0}},
	{11, Interpreter::cmpi,         {"cmpi",     OPTYPE_INTEGER, FL_IN_A | FL_SET_CRn, 1, 0, 0, 0}},
	{12, Interpreter::addic,        {"addic",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA, 1, 0, 0, 0}},
	{13, Interpreter::addic_rc,     {"addic_rc", OPTYPE_INTEGER, FL_OUT_D | FL_IN_A | FL_SET_CA | FL_SET_CR0, 1, 0, 0, 0}},
	{14, Interpreter::addi,         {"addi",     OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0, 1, 0, 0, 0}},
	{15, Interpreter::addis,        {"addis",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_A0, 1, 0, 0, 0}},

	{20, Interpreter::rlwimix,      {"rlwimix",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{21, Interpreter::rlwinmx,      {"rlwinmx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{23, Interpreter::rlwnmx,       {"rlwnmx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},

	{24, Interpreter::ori,          {"ori",      OPTYPE_INTEGER, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{25, Interpreter::oris,         {"oris",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{26, Interpreter::xori,         {"xori",     OPTYPE_INTEGER, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{27, Interpreter::xoris,        {"xoris",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{28, Interpreter::andi_rc,      {"andi_rc",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1, 0, 0, 0}},
	{29, Interpreter::andis_rc,     {"andis_rc", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1, 0, 0, 0}},

	{32, Interpreter::lwz,          {"lwz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{33, Interpreter::lwzu,         {"lwzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{34, Interpreter::lbz,          {"lbz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{35, Interpreter::lbzu,         {"lbzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{40, Interpreter::lhz,          {"lhz",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{41, Interpreter::lhzu,         {"lhzu", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},

	{42, Interpreter::lha,          {"lha",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{43, Interpreter::lhau,         {"lhau", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},

	{44, Interpreter::sth,          {"sth",  OPTYPE_STORE, FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{45, Interpreter::sthu,         {"sthu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{36, Interpreter::stw,          {"stw",  OPTYPE_STORE, FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{37, Interpreter::stwu,         {"stwu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{38, Interpreter::stb,          {"stb",  OPTYPE_STORE, FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{39, Interpreter::stbu,         {"stbu", OPTYPE_STORE, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},

	{46, Interpreter::lmw,          {"lmw",   OPTYPE_SYSTEM, FL_EVIL | FL_LOADSTORE, 11, 0, 0, 0}},
	{47, Interpreter::stmw,         {"stmw",  OPTYPE_SYSTEM, FL_EVIL | FL_LOADSTORE, 11, 0, 0, 0}},

	{48, Interpreter::lfs,          {"lfs",  OPTYPE_LOADFP, FL_OUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{49, Interpreter::lfsu,         {"lfsu", OPTYPE_LOADFP, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{50, Interpreter::lfd,          {"lfd",  OPTYPE_LOADFP, FL_INOUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{51, Interpreter::lfdu,         {"lfdu", OPTYPE_LOADFP, FL_INOUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{52, Interpreter::stfs,         {"stfs",  OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{53, Interpreter::stfsu,        {"stfsu", OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{54, Interpreter::stfd,         {"stfd",  OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{55, Interpreter::stfdu,        {"stfdu", OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{56, Interpreter::psq_l,        {"psq_l",   OPTYPE_LOADPS, FL_OUT_FLOAT_S | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{57, Interpreter::psq_lu,       {"psq_lu",  OPTYPE_LOADPS, FL_OUT_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{60, Interpreter::psq_st,       {"psq_st",  OPTYPE_STOREPS, FL_IN_FLOAT_S | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{61, Interpreter::psq_stu,      {"psq_stu", OPTYPE_STOREPS, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	//missing: 0, 5, 6, 9, 22, 30, 62, 58
	{0,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{5,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{6,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{9,  Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{22, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{30, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{62, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
	{58, Interpreter::unknown_instruction,   {"unknown_instruction", OPTYPE_UNKNOWN, 0, 0, 0, 0, 0}},
};

static GekkoOPTemplate table4[] =
{    //SUBOP10
	{0,    Interpreter::ps_cmpu0,   {"ps_cmpu0",   OPTYPE_PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{32,   Interpreter::ps_cmpo0,   {"ps_cmpo0",   OPTYPE_PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{40,   Interpreter::ps_neg,     {"ps_neg",     OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{136,  Interpreter::ps_nabs,    {"ps_nabs",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{264,  Interpreter::ps_abs,     {"ps_abs",     OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{64,   Interpreter::ps_cmpu1,   {"ps_cmpu1",   OPTYPE_PS, FL_IN_FLOAT_AB | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{72,   Interpreter::ps_mr,      {"ps_mr",      OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{96,   Interpreter::ps_cmpo1,   {"ps_cmpo1",   OPTYPE_PS, FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{528, Interpreter::ps_merge00, { "ps_merge00", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{560,  Interpreter::ps_merge01, {"ps_merge01", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{592, Interpreter::ps_merge10, {"ps_merge10",  OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{624,  Interpreter::ps_merge11, {"ps_merge11", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},

	{1014, Interpreter::dcbz_l,     {"dcbz_l",     OPTYPE_SYSTEM, FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
};

static GekkoOPTemplate table4_2[] =
{
	{10, Interpreter::ps_sum0,      {"ps_sum0",   OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{11, Interpreter::ps_sum1,      {"ps_sum1",   OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{12, Interpreter::ps_muls0,     {"ps_muls0",  OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{13, Interpreter::ps_muls1,     {"ps_muls1",  OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{14, Interpreter::ps_madds0,    {"ps_madds0", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{15, Interpreter::ps_madds1,    {"ps_madds1", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{18, Interpreter::ps_div,       {"ps_div",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17, 0, 0, 0}},
	{20, Interpreter::ps_sub,       {"ps_sub",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::ps_add,       {"ps_add",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{23, Interpreter::ps_sel,       {"ps_sel",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{24, Interpreter::ps_res,       {"ps_res",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{25, Interpreter::ps_mul,       {"ps_mul",    OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{26, Interpreter::ps_rsqrte,    {"ps_rsqrte", OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 2, 0, 0, 0}},
	{28, Interpreter::ps_msub,      {"ps_msub",   OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::ps_madd,      {"ps_madd",   OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::ps_nmsub,     {"ps_nmsub",  OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::ps_nmadd,     {"ps_nmadd",  OPTYPE_PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
};


static GekkoOPTemplate table4_3[] =
{
	{6,  Interpreter::psq_lx,       {"psq_lx",   OPTYPE_PS, FL_OUT_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{7,  Interpreter::psq_stx,      {"psq_stx",  OPTYPE_PS, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{38, Interpreter::psq_lux,      {"psq_lux",  OPTYPE_PS, FL_OUT_FLOAT_S | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{39, Interpreter::psq_stux,     {"psq_stux", OPTYPE_PS, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
};

static GekkoOPTemplate table19[] =
{
	{528, Interpreter::bcctrx,      {"bcctrx", OPTYPE_BRANCH, FL_ENDBLOCK, 1, 0, 0, 0}},
	{16,  Interpreter::bclrx,       {"bclrx",  OPTYPE_BRANCH, FL_ENDBLOCK, 1, 0, 0, 0}},
	{257, Interpreter::crand,       {"crand",  OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{129, Interpreter::crandc,      {"crandc", OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{289, Interpreter::creqv,       {"creqv",  OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{225, Interpreter::crnand,      {"crnand", OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{33,  Interpreter::crnor,       {"crnor",  OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{449, Interpreter::cror,        {"cror",   OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{417, Interpreter::crorc,       {"crorc",  OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},
	{193, Interpreter::crxor,       {"crxor",  OPTYPE_CR, FL_EVIL, 1, 0, 0, 0}},

	{150, Interpreter::isync,       {"isync",  OPTYPE_ICACHE, FL_EVIL, 1, 0, 0, 0}},
	{0,   Interpreter::mcrf,        {"mcrf",   OPTYPE_SYSTEM, FL_EVIL | FL_SET_CRn, 1, 0, 0, 0}},

	{50,  Interpreter::rfi,         {"rfi",    OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 2, 0, 0, 0}},
	{18,  Interpreter::rfid,        {"rfid",   OPTYPE_SYSTEM, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 1, 0, 0, 0}}
};


static GekkoOPTemplate table31[] =
{
	{28,  Interpreter::andx,        {"andx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{60,  Interpreter::andcx,       {"andcx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{444, Interpreter::orx,         {"orx",    OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{124, Interpreter::norx,        {"norx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{316, Interpreter::xorx,        {"xorx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{412, Interpreter::orcx,        {"orcx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{476, Interpreter::nandx,       {"nandx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{284, Interpreter::eqvx,        {"eqvx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{0,   Interpreter::cmp,         {"cmp",    OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn, 1, 0, 0, 0}},
	{32,  Interpreter::cmpl,        {"cmpl",   OPTYPE_INTEGER, FL_IN_AB | FL_SET_CRn, 1, 0, 0, 0}},
	{26,  Interpreter::cntlzwx,     {"cntlzwx",OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{922, Interpreter::extshx,      {"extshx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{954, Interpreter::extsbx,      {"extsbx", OPTYPE_INTEGER, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{536, Interpreter::srwx,        {"srwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{792, Interpreter::srawx,       {"srawx",  OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{824, Interpreter::srawix,      {"srawix", OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{24,  Interpreter::slwx,        {"slwx",   OPTYPE_INTEGER, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},

	{54,   Interpreter::dcbst,      {"dcbst",  OPTYPE_DCACHE, 0, 5, 0, 0, 0}},
	{86,   Interpreter::dcbf,       {"dcbf",   OPTYPE_DCACHE, 0, 5, 0, 0, 0}},
	{246,  Interpreter::dcbtst,     {"dcbtst", OPTYPE_DCACHE, 0, 2, 0, 0, 0}},
	{278,  Interpreter::dcbt,       {"dcbt",   OPTYPE_DCACHE, 0, 2, 0, 0, 0}},
	{470,  Interpreter::dcbi,       {"dcbi",   OPTYPE_DCACHE, 0, 5, 0, 0, 0}},
	{758,  Interpreter::dcba,       {"dcba",   OPTYPE_DCACHE, 0, 5, 0, 0, 0}},
	{1014, Interpreter::dcbz,       {"dcbz",   OPTYPE_DCACHE, FL_LOADSTORE, 5, 0, 0, 0}},

	//load word
	{23,  Interpreter::lwzx,        {"lwzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{55,  Interpreter::lwzux,       {"lwzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load halfword
	{279, Interpreter::lhzx,        {"lhzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{311, Interpreter::lhzux,       {"lhzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load halfword signextend
	{343, Interpreter::lhax,        {"lhax",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{375, Interpreter::lhaux,       {"lhaux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load byte
	{87,  Interpreter::lbzx,        {"lbzx",  OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{119, Interpreter::lbzux,       {"lbzux", OPTYPE_LOAD, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load byte reverse
	{534, Interpreter::lwbrx,       {"lwbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{790, Interpreter::lhbrx,       {"lhbrx", OPTYPE_LOAD, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},

	// Conditional load/store (Wii SMP)
	{150, Interpreter::stwcxd,      {"stwcxd", OPTYPE_STORE, FL_EVIL | FL_IN_S | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{20,  Interpreter::lwarx,       {"lwarx",  OPTYPE_LOAD, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1, 0, 0, 0}},

	//load string (Inst these)
	{533, Interpreter::lswx,        {"lswx",  OPTYPE_LOAD, FL_EVIL | FL_IN_A0B | FL_OUT_D | FL_LOADSTORE, 1, 0, 0, 0}},
	{597, Interpreter::lswi,        {"lswi",  OPTYPE_LOAD, FL_EVIL | FL_IN_A | FL_OUT_D | FL_LOADSTORE, 1, 0, 0, 0}},

	//store word
	{151, Interpreter::stwx,        {"stwx",   OPTYPE_STORE, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{183, Interpreter::stwux,       {"stwux",  OPTYPE_STORE, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store halfword
	{407, Interpreter::sthx,        {"sthx",   OPTYPE_STORE, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{439, Interpreter::sthux,       {"sthux",  OPTYPE_STORE, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store byte
	{215, Interpreter::stbx,        {"stbx",   OPTYPE_STORE, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{247, Interpreter::stbux,       {"stbux",  OPTYPE_STORE, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store bytereverse
	{662, Interpreter::stwbrx,      {"stwbrx", OPTYPE_STORE, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{918, Interpreter::sthbrx,      {"sthbrx", OPTYPE_STORE, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},

	{661, Interpreter::stswx,       {"stswx",  OPTYPE_STORE, FL_EVIL | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{725, Interpreter::stswi,       {"stswi",  OPTYPE_STORE, FL_EVIL | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},

	// fp load/store
	{535, Interpreter::lfsx,        {"lfsx",  OPTYPE_LOADFP, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{567, Interpreter::lfsux,       {"lfsux", OPTYPE_LOADFP, FL_OUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{599, Interpreter::lfdx,        {"lfdx",  OPTYPE_LOADFP, FL_INOUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{631, Interpreter::lfdux,       {"lfdux", OPTYPE_LOADFP, FL_INOUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{663, Interpreter::stfsx,       {"stfsx",  OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{695, Interpreter::stfsux,      {"stfsux", OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{727, Interpreter::stfdx,       {"stfdx",  OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{759, Interpreter::stfdux,      {"stfdux", OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{983, Interpreter::stfiwx,      {"stfiwx", OPTYPE_STOREFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{19,  Interpreter::mfcr,        {"mfcr",   OPTYPE_SYSTEM, FL_OUT_D, 1, 0, 0, 0}},
	{83,  Interpreter::mfmsr,       {"mfmsr",  OPTYPE_SYSTEM, FL_OUT_D, 1, 0, 0, 0}},
	{144, Interpreter::mtcrf,       {"mtcrf",  OPTYPE_SYSTEM, FL_IN_S | FL_SET_CRn, 1, 0, 0, 0}},
	{146, Interpreter::mtmsr,       {"mtmsr",  OPTYPE_SYSTEM, FL_IN_S | FL_ENDBLOCK, 1, 0, 0, 0}},
	{210, Interpreter::mtsr,        {"mtsr",   OPTYPE_SYSTEM, FL_IN_S, 1, 0, 0, 0}},
	{242, Interpreter::mtsrin,      {"mtsrin", OPTYPE_SYSTEM, FL_IN_SB, 1, 0, 0, 0}},
	{339, Interpreter::mfspr,       {"mfspr",  OPTYPE_SPR, FL_OUT_D, 1, 0, 0, 0}},
	{467, Interpreter::mtspr,       {"mtspr",  OPTYPE_SPR, FL_IN_S, 2, 0, 0, 0}},
	{371, Interpreter::mftb,        {"mftb",   OPTYPE_SYSTEM, FL_OUT_D | FL_TIMER, 1, 0, 0, 0}},
	{512, Interpreter::mcrxr,       {"mcrxr",  OPTYPE_SYSTEM, FL_READ_CA | FL_SET_CA, 1, 0, 0, 0}},
	{595, Interpreter::mfsr,        {"mfsr",   OPTYPE_SYSTEM, FL_OUT_D, 3, 0, 0, 0}},
	{659, Interpreter::mfsrin,      {"mfsrin", OPTYPE_SYSTEM, FL_OUT_D | FL_IN_B, 3, 0, 0, 0}},

	{4,   Interpreter::tw,          {"tw",     OPTYPE_SYSTEM, FL_ENDBLOCK, 2, 0, 0, 0}},
	{598, Interpreter::sync,        {"sync",   OPTYPE_SYSTEM, 0, 3, 0, 0, 0}},
	{982, Interpreter::icbi,        {"icbi",   OPTYPE_SYSTEM, FL_ENDBLOCK, 4, 0, 0, 0}},

	// Unused instructions on GC
	{310, Interpreter::eciwx,       {"eciwx",   OPTYPE_SYSTEM, FL_IN_AB | FL_OUT_S, 1, 0, 0, 0}},
	{438, Interpreter::ecowx,       {"ecowx",   OPTYPE_SYSTEM, FL_IN_AB | FL_IN_S, 1, 0, 0, 0}},
	{854, Interpreter::eieio,       {"eieio",   OPTYPE_SYSTEM, 0, 1, 0, 0, 0}},
	{306, Interpreter::tlbie,       {"tlbie",   OPTYPE_SYSTEM, 0, 1, 0, 0, 0}},
	{370, Interpreter::tlbia,       {"tlbia",   OPTYPE_SYSTEM, 0, 1, 0, 0, 0}},
	{566, Interpreter::tlbsync,     {"tlbsync", OPTYPE_SYSTEM, 0, 1, 0, 0, 0}},
};

static GekkoOPTemplate table31_2[] =
{
	{266,  Interpreter::addx,        {"addx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{778,  Interpreter::addx,        {"addox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{10,   Interpreter::addcx,       {"addcx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{522,  Interpreter::addcx,       {"addcox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{138,  Interpreter::addex,       {"addex",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{650,  Interpreter::addex,       {"addeox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{234,  Interpreter::addmex,      {"addmex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{202,  Interpreter::addzex,      {"addzex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{491,  Interpreter::divwx,       {"divwx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 40, 0, 0, 0}},
	{1003, Interpreter::divwx,       {"divwox",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 40, 0, 0, 0}},
	{459,  Interpreter::divwux,      {"divwux",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 40, 0, 0, 0}},
	{971,  Interpreter::divwux,      {"divwuox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 40, 0, 0, 0}},
	{75,   Interpreter::mulhwx,      {"mulhwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{11,   Interpreter::mulhwux,     {"mulhwux", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{235,  Interpreter::mullwx,      {"mullwx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{747,  Interpreter::mullwx,      {"mullwox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 5, 0, 0, 0}},
	{104,  Interpreter::negx,        {"negx",    OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{40,   Interpreter::subfx,       {"subfx",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{552,  Interpreter::subfx,       {"subox",   OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{8,    Interpreter::subfcx,      {"subfcx",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{520,  Interpreter::subfcx,      {"subfcox", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{136,  Interpreter::subfex,      {"subfex",  OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{232,  Interpreter::subfmex,     {"subfmex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{200,  Interpreter::subfzex,     {"subfzex", OPTYPE_INTEGER, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
};

static GekkoOPTemplate table59[] =
{
	{18, Interpreter::fdivsx,       {"fdivsx",   OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17, 0, 0, 0}}, // TODO
	{20, Interpreter::fsubsx,       {"fsubsx",   OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::faddsx,       {"faddsx",   OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	//{22, Interpreter::fsqrtsx,      {"fsqrtsx",  OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}}, // Not implemented on gekko
	{24, Interpreter::fresx,        {"fresx",    OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{25, Interpreter::fmulsx,       {"fmulsx",   OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{28, Interpreter::fmsubsx,      {"fmsubsx",  OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::fmaddsx,      {"fmaddsx",  OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::fnmsubsx,     {"fnmsubsx", OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::fnmaddsx,     {"fnmaddsx", OPTYPE_SINGLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
};

static GekkoOPTemplate table63[] =
{
	{264, Interpreter::fabsx,       {"fabsx",   OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{32,  Interpreter::fcmpo,       {"fcmpo",   OPTYPE_DOUBLEFP, FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{0,   Interpreter::fcmpu,       {"fcmpu",   OPTYPE_DOUBLEFP, FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{14,  Interpreter::fctiwx,      {"fctiwx",  OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{15,  Interpreter::fctiwzx,     {"fctiwzx", OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{72,  Interpreter::fmrx,        {"fmrx",    OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{136, Interpreter::fnabsx,      {"fnabsx",  OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{40,  Interpreter::fnegx,       {"fnegx",   OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{12,  Interpreter::frspx,       {"frspx",   OPTYPE_DOUBLEFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},

	{64,  Interpreter::mcrfs,       {"mcrfs",   OPTYPE_SYSTEMFP, FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF, 1, 0, 0, 0}},
	{583, Interpreter::mffsx,       {"mffsx",   OPTYPE_SYSTEMFP, FL_OUT_D | FL_USE_FPU | FL_READ_FPRF, 1, 0, 0, 0}},
	{70,  Interpreter::mtfsb0x,     {"mtfsb0x", OPTYPE_SYSTEMFP, FL_USE_FPU | FL_READ_FPRF, 3, 0, 0, 0}},
	{38,  Interpreter::mtfsb1x,     {"mtfsb1x", OPTYPE_SYSTEMFP, FL_USE_FPU | FL_READ_FPRF, 3, 0, 0, 0}},
	{134, Interpreter::mtfsfix,     {"mtfsfix", OPTYPE_SYSTEMFP, FL_USE_FPU | FL_READ_FPRF, 3, 0, 0, 0}},
	{711, Interpreter::mtfsfx,      {"mtfsfx",  OPTYPE_SYSTEMFP, FL_IN_B | FL_USE_FPU | FL_READ_FPRF, 3, 0, 0, 0}},
};

static GekkoOPTemplate table63_2[] =
{
	{18, Interpreter::fdivx,        {"fdivx",    OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 31, 0, 0, 0}},
	{20, Interpreter::fsubx,        {"fsubx",    OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::faddx,        {"faddx",    OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{22, Interpreter::fsqrtx,       {"fsqrtx",   OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{23, Interpreter::fselx,        {"fselx",    OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{25, Interpreter::fmulx,        {"fmulx",    OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{26, Interpreter::frsqrtex,     {"frsqrtex", OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{28, Interpreter::fmsubx,       {"fmsubx",   OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::fmaddx,       {"fmaddx",   OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::fnmsubx,      {"fnmsubx",  OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::fnmaddx,      {"fnmaddx",  OPTYPE_DOUBLEFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
};
namespace InterpreterTables
{

void InitTables()
{
	// once initialized, tables are read-only
	static bool initialized = false;
	if (initialized)
		return;

	//clear
	for (int i = 0; i < 32; i++)
	{
		Interpreter::m_opTable59[i] = Interpreter::unknown_instruction;
		m_infoTable59[i] = nullptr;
	}

	for (int i = 0; i < 1024; i++)
	{
		Interpreter::m_opTable4 [i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable19[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable31[i] = Interpreter::unknown_instruction;
		Interpreter::m_opTable63[i] = Interpreter::unknown_instruction;
		m_infoTable4[i] = nullptr;
		m_infoTable19[i] = nullptr;
		m_infoTable31[i] = nullptr;
		m_infoTable63[i] = nullptr;
	}

	for (auto& tpl : primarytable)
	{
		Interpreter::m_opTable[tpl.opcode] = tpl.Inst;
		m_infoTable[tpl.opcode] = &tpl.opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table4_2)
		{
			int op = fill+tpl.opcode;
			Interpreter::m_opTable4[op] = tpl.Inst;
			m_infoTable4[op] = &tpl.opinfo;
		}
	}

	for (int i = 0; i < 16; i++)
	{
		int fill = i << 6;
		for (auto& tpl : table4_3)
		{
			int op = fill+tpl.opcode;
			Interpreter::m_opTable4[op] = tpl.Inst;
			m_infoTable4[op] = &tpl.opinfo;
		}
	}

	for (auto& tpl : table4)
	{
		int op = tpl.opcode;
		Interpreter::m_opTable4[op] = tpl.Inst;
		m_infoTable4[op] = &tpl.opinfo;
	}

	for (auto& tpl : table31)
	{
		int op = tpl.opcode;
		Interpreter::m_opTable31[op] = tpl.Inst;
		m_infoTable31[op] = &tpl.opinfo;
	}

	for (int i = 0; i < 1; i++)
	{
		int fill = i << 9;
		for (auto& tpl : table31_2)
		{
			int op = fill + tpl.opcode;
			Interpreter::m_opTable31[op] = tpl.Inst;
			m_infoTable31[op] = &tpl.opinfo;
		}
	}

	for (auto& tpl : table19)
	{
		int op = tpl.opcode;
		Interpreter::m_opTable19[op] = tpl.Inst;
		m_infoTable19[op] = &tpl.opinfo;
	}

	for (auto& tpl : table59)
	{
		int op = tpl.opcode;
		Interpreter::m_opTable59[op] = tpl.Inst;
		m_infoTable59[op] = &tpl.opinfo;
	}

	for (auto& tpl : table63)
	{
		int op = tpl.opcode;
		Interpreter::m_opTable63[op] = tpl.Inst;
		m_infoTable63[op] = &tpl.opinfo;
	}

	for (int i = 0; i < 32; i++)
	{
		int fill = i << 5;
		for (auto& tpl : table63_2)
		{
			int op = fill + tpl.opcode;
			Interpreter::m_opTable63[op] = tpl.Inst;
			m_infoTable63[op] = &tpl.opinfo;
		}
	}

	m_numInstructions = 0;
	for (auto& tpl : primarytable)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table4_2)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table4_3)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table4)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table31)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table31_2)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table19)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table59)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table63)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;
	for (auto& tpl : table63_2)
		m_allInstructions[m_numInstructions++] = &tpl.opinfo;

	if (m_numInstructions >= 512)
	{
		PanicAlert("m_allInstructions underdimensioned");
	}

	initialized = true;
}

}
//remove
