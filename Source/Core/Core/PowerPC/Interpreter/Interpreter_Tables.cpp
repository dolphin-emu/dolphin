// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PPCTables.h"

namespace
{
struct GekkoOPTemplate
{
  int opcode;
  Interpreter::Instruction Inst;
  GekkoOPInfo opinfo;
};
}  // namespace

// clang-format off
static GekkoOPInfo unknownopinfo = { "unknown_instruction", OpType::Unknown, FL_ENDBLOCK, 0, 0, 0, 0 };

static std::array<GekkoOPTemplate, 54> primarytable =
{{
	{4,  Interpreter::RunTable4,    {"RunTable4",  OpType::Subtable, 0, 0, 0, 0, 0}},
	{19, Interpreter::RunTable19,   {"RunTable19", OpType::Subtable, 0, 0, 0, 0, 0}},
	{31, Interpreter::RunTable31,   {"RunTable31", OpType::Subtable, 0, 0, 0, 0, 0}},
	{59, Interpreter::RunTable59,   {"RunTable59", OpType::Subtable, 0, 0, 0, 0, 0}},
	{63, Interpreter::RunTable63,   {"RunTable63", OpType::Subtable, 0, 0, 0, 0, 0}},

	{16, Interpreter::bcx,          {"bcx", OpType::Branch, FL_ENDBLOCK, 1, 0, 0, 0}},
	{18, Interpreter::bx,           {"bx",  OpType::Branch, FL_ENDBLOCK, 1, 0, 0, 0}},

	{3,  Interpreter::twi,          {"twi", OpType::System, FL_ENDBLOCK, 1, 0, 0, 0}},
	{17, Interpreter::sc,           {"sc",  OpType::System, FL_ENDBLOCK, 2, 0, 0, 0}},

	{7,  Interpreter::mulli,        {"mulli",    OpType::Integer, FL_OUT_D | FL_IN_A, 3, 0, 0, 0}},
	{8,  Interpreter::subfic,       {"subfic",   OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA, 1, 0, 0, 0}},
	{10, Interpreter::cmpli,        {"cmpli",    OpType::Integer, FL_IN_A | FL_SET_CRn, 1, 0, 0, 0}},
	{11, Interpreter::cmpi,         {"cmpi",     OpType::Integer, FL_IN_A | FL_SET_CRn, 1, 0, 0, 0}},
	{12, Interpreter::addic,        {"addic",    OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA, 1, 0, 0, 0}},
	{13, Interpreter::addic_rc,     {"addic_rc", OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA | FL_SET_CR0, 1, 0, 0, 0}},
	{14, Interpreter::addi,         {"addi",     OpType::Integer, FL_OUT_D | FL_IN_A0, 1, 0, 0, 0}},
	{15, Interpreter::addis,        {"addis",    OpType::Integer, FL_OUT_D | FL_IN_A0, 1, 0, 0, 0}},

	{20, Interpreter::rlwimix,      {"rlwimix",  OpType::Integer, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{21, Interpreter::rlwinmx,      {"rlwinmx",  OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{23, Interpreter::rlwnmx,       {"rlwnmx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},

	{24, Interpreter::ori,          {"ori",      OpType::Integer, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{25, Interpreter::oris,         {"oris",     OpType::Integer, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{26, Interpreter::xori,         {"xori",     OpType::Integer, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{27, Interpreter::xoris,        {"xoris",    OpType::Integer, FL_OUT_A | FL_IN_S, 1, 0, 0, 0}},
	{28, Interpreter::andi_rc,      {"andi_rc",  OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1, 0, 0, 0}},
	{29, Interpreter::andis_rc,     {"andis_rc", OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1, 0, 0, 0}},

	{32, Interpreter::lwz,          {"lwz",  OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{33, Interpreter::lwzu,         {"lwzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{34, Interpreter::lbz,          {"lbz",  OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{35, Interpreter::lbzu,         {"lbzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},
	{40, Interpreter::lhz,          {"lhz",  OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{41, Interpreter::lhzu,         {"lhzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},

	{42, Interpreter::lha,          {"lha",  OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{43, Interpreter::lhau,         {"lhau", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1, 0, 0, 0}},

	{44, Interpreter::sth,          {"sth",  OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{45, Interpreter::sthu,         {"sthu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{36, Interpreter::stw,          {"stw",  OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{37, Interpreter::stwu,         {"stwu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{38, Interpreter::stb,          {"stb",  OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{39, Interpreter::stbu,         {"stbu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},

	{46, Interpreter::lmw,          {"lmw",   OpType::System, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 11, 0, 0, 0}},
	{47, Interpreter::stmw,         {"stmw",  OpType::System, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 11, 0, 0, 0}},

	{48, Interpreter::lfs,          {"lfs",  OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{49, Interpreter::lfsu,         {"lfsu", OpType::LoadFP, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{50, Interpreter::lfd,          {"lfd",  OpType::LoadFP, FL_INOUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{51, Interpreter::lfdu,         {"lfdu", OpType::LoadFP, FL_INOUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{52, Interpreter::stfs,         {"stfs",  OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{53, Interpreter::stfsu,        {"stfsu", OpType::StoreFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{54, Interpreter::stfd,         {"stfd",  OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{55, Interpreter::stfdu,        {"stfdu", OpType::StoreFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{56, Interpreter::psq_l,        {"psq_l",   OpType::LoadPS, FL_OUT_FLOAT_D | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{57, Interpreter::psq_lu,       {"psq_lu",  OpType::LoadPS, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{60, Interpreter::psq_st,       {"psq_st",  OpType::StorePS, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{61, Interpreter::psq_stu,      {"psq_stu", OpType::StorePS, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	//missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

static std::array<GekkoOPTemplate, 13> table4 =
{{    //SUBOP10
	{0,    Interpreter::ps_cmpu0,   {"ps_cmpu0",   OpType::PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},
	{32,   Interpreter::ps_cmpo0,   {"ps_cmpo0",   OpType::PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},
	{40,   Interpreter::ps_neg,     {"ps_neg",     OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{136,  Interpreter::ps_nabs,    {"ps_nabs",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{264,  Interpreter::ps_abs,     {"ps_abs",     OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{64,   Interpreter::ps_cmpu1,   {"ps_cmpu1",   OpType::PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},
	{72,   Interpreter::ps_mr,      {"ps_mr",      OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{96,   Interpreter::ps_cmpo1,   {"ps_cmpo1",   OpType::PS, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},
	{528,  Interpreter::ps_merge00, {"ps_merge00", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{560,  Interpreter::ps_merge01, {"ps_merge01", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{592,  Interpreter::ps_merge10, {"ps_merge10", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{624,  Interpreter::ps_merge11, {"ps_merge11", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},

	{1014, Interpreter::dcbz_l,     {"dcbz_l",     OpType::System, FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 17> table4_2 =
{{
	{10, Interpreter::ps_sum0,      {"ps_sum0",   OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{11, Interpreter::ps_sum1,      {"ps_sum1",   OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{12, Interpreter::ps_muls0,     {"ps_muls0",  OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{13, Interpreter::ps_muls1,     {"ps_muls1",  OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{14, Interpreter::ps_madds0,    {"ps_madds0", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{15, Interpreter::ps_madds1,    {"ps_madds1", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{18, Interpreter::ps_div,       {"ps_div",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17, 0, 0, 0}},
	{20, Interpreter::ps_sub,       {"ps_sub",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::ps_add,       {"ps_add",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{23, Interpreter::ps_sel,       {"ps_sel",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{24, Interpreter::ps_res,       {"ps_res",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{25, Interpreter::ps_mul,       {"ps_mul",    OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{26, Interpreter::ps_rsqrte,    {"ps_rsqrte", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 2, 0, 0, 0}},
	{28, Interpreter::ps_msub,      {"ps_msub",   OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::ps_madd,      {"ps_madd",   OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::ps_nmsub,     {"ps_nmsub",  OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::ps_nmadd,     {"ps_nmadd",  OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
}};


static std::array<GekkoOPTemplate, 4> table4_3 =
{{
	{6,  Interpreter::psq_lx,       {"psq_lx",   OpType::LoadPS, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{7,  Interpreter::psq_stx,      {"psq_stx",  OpType::StorePS, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{38, Interpreter::psq_lux,      {"psq_lux",  OpType::LoadPS, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{39, Interpreter::psq_stux,     {"psq_stux", OpType::StorePS, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 13> table19 =
{{
	{528, Interpreter::bcctrx,      {"bcctrx", OpType::Branch, FL_ENDBLOCK, 1, 0, 0, 0}},
	{16,  Interpreter::bclrx,       {"bclrx",  OpType::Branch, FL_ENDBLOCK, 1, 0, 0, 0}},
	{257, Interpreter::crand,       {"crand",  OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{129, Interpreter::crandc,      {"crandc", OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{289, Interpreter::creqv,       {"creqv",  OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{225, Interpreter::crnand,      {"crnand", OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{33,  Interpreter::crnor,       {"crnor",  OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{449, Interpreter::cror,        {"cror",   OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{417, Interpreter::crorc,       {"crorc",  OpType::CR, FL_EVIL, 1, 0, 0, 0}},
	{193, Interpreter::crxor,       {"crxor",  OpType::CR, FL_EVIL, 1, 0, 0, 0}},

	{150, Interpreter::isync,       {"isync",  OpType::InstructionCache, FL_EVIL, 1, 0, 0, 0}},
	{0,   Interpreter::mcrf,        {"mcrf",   OpType::System, FL_EVIL | FL_SET_CRn, 1, 0, 0, 0}},

	{50,  Interpreter::rfi,         {"rfi",    OpType::System, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 2, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 107> table31 =
{{
	{266,  Interpreter::addx,       {"addx",    OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{778,  Interpreter::addx,       {"addox",   OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{10,   Interpreter::addcx,      {"addcx",   OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{522,  Interpreter::addcx,      {"addcox",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{138,  Interpreter::addex,      {"addex",   OpType::Integer, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{650,  Interpreter::addex,      {"addeox",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{234,  Interpreter::addmex,     {"addmex",  OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{746,  Interpreter::addmex,     {"addmeox", OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{202,  Interpreter::addzex,     {"addzex",  OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{714,  Interpreter::addzex,     {"addzeox", OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{491,  Interpreter::divwx,      {"divwx",   OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 40, 0, 0, 0}},
	{1003, Interpreter::divwx,      {"divwox",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 40, 0, 0, 0}},
	{459,  Interpreter::divwux,     {"divwux",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 40, 0, 0, 0}},
	{971,  Interpreter::divwux,     {"divwuox", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 40, 0, 0, 0}},
	{75,   Interpreter::mulhwx,     {"mulhwx",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{11,   Interpreter::mulhwux,    {"mulhwux", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{235,  Interpreter::mullwx,     {"mullwx",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5, 0, 0, 0}},
	{747,  Interpreter::mullwx,     {"mullwox", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 5, 0, 0, 0}},
	{104,  Interpreter::negx,       {"negx",    OpType::Integer, FL_OUT_D | FL_IN_A | FL_RC_BIT, 1, 0, 0, 0}},
	{616,  Interpreter::negx,       {"negox",   OpType::Integer, FL_OUT_D | FL_IN_A | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{40,   Interpreter::subfx,      {"subfx",   OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 1, 0, 0, 0}},
	{552,  Interpreter::subfx,      {"subfox",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{8,    Interpreter::subfcx,     {"subfcx",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{520,  Interpreter::subfcx,     {"subfcox", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{136,  Interpreter::subfex,     {"subfex",  OpType::Integer, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{648,  Interpreter::subfex,     {"subfeox", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{232,  Interpreter::subfmex,    {"subfmex", OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{744,  Interpreter::subfmex,    {"subfmeox",OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},
	{200,  Interpreter::subfzex,    {"subfzex", OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{712,  Interpreter::subfzex,    {"subfzeox",OpType::Integer, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE, 1, 0, 0, 0}},

	{28,  Interpreter::andx,        {"andx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{60,  Interpreter::andcx,       {"andcx",  OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{444, Interpreter::orx,         {"orx",    OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{124, Interpreter::norx,        {"norx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{316, Interpreter::xorx,        {"xorx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{412, Interpreter::orcx,        {"orcx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{476, Interpreter::nandx,       {"nandx",  OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{284, Interpreter::eqvx,        {"eqvx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{0,   Interpreter::cmp,         {"cmp",    OpType::Integer, FL_IN_AB | FL_SET_CRn, 1, 0, 0, 0}},
	{32,  Interpreter::cmpl,        {"cmpl",   OpType::Integer, FL_IN_AB | FL_SET_CRn, 1, 0, 0, 0}},
	{26,  Interpreter::cntlzwx,     {"cntlzwx",OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{922, Interpreter::extshx,      {"extshx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{954, Interpreter::extsbx,      {"extsbx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1, 0, 0, 0}},
	{536, Interpreter::srwx,        {"srwx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},
	{792, Interpreter::srawx,       {"srawx",  OpType::Integer, FL_OUT_A | FL_IN_SB | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{824, Interpreter::srawix,      {"srawix", OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CA | FL_RC_BIT, 1, 0, 0, 0}},
	{24,  Interpreter::slwx,        {"slwx",   OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1, 0, 0, 0}},

	{54,   Interpreter::dcbst,      {"dcbst",  OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5, 0, 0, 0}},
	{86,   Interpreter::dcbf,       {"dcbf",   OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5, 0, 0, 0}},
	{246,  Interpreter::dcbtst,     {"dcbtst", OpType::DataCache, 0, 2, 0, 0, 0}},
	{278,  Interpreter::dcbt,       {"dcbt",   OpType::DataCache, 0, 2, 0, 0, 0}},
	{470,  Interpreter::dcbi,       {"dcbi",   OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5, 0, 0, 0}},
	{758,  Interpreter::dcba,       {"dcba",   OpType::DataCache, 0, 5, 0, 0, 0}},
	{1014, Interpreter::dcbz,       {"dcbz",   OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5, 0, 0, 0}},

	//load word
	{23,  Interpreter::lwzx,        {"lwzx",  OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{55,  Interpreter::lwzux,       {"lwzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load halfword
	{279, Interpreter::lhzx,        {"lhzx",  OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{311, Interpreter::lhzux,       {"lhzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load halfword signextend
	{343, Interpreter::lhax,        {"lhax",  OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{375, Interpreter::lhaux,       {"lhaux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load byte
	{87,  Interpreter::lbzx,        {"lbzx",  OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{119, Interpreter::lbzux,       {"lbzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//load byte reverse
	{534, Interpreter::lwbrx,       {"lwbrx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{790, Interpreter::lhbrx,       {"lhbrx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},

	// Conditional load/store (Wii SMP)
	{150, Interpreter::stwcxd,      {"stwcxd", OpType::Store, FL_EVIL | FL_IN_S | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1, 0, 0, 0}},
	{20,  Interpreter::lwarx,       {"lwarx",  OpType::Load, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1, 0, 0, 0}},

	//load string (Inst these)
	{533, Interpreter::lswx,        {"lswx",  OpType::Load, FL_EVIL | FL_IN_A0B | FL_OUT_D | FL_LOADSTORE, 1, 0, 0, 0}},
	{597, Interpreter::lswi,        {"lswi",  OpType::Load, FL_EVIL | FL_IN_A0 | FL_OUT_D | FL_LOADSTORE, 1, 0, 0, 0}},

	//store word
	{151, Interpreter::stwx,        {"stwx",   OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{183, Interpreter::stwux,       {"stwux",  OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store halfword
	{407, Interpreter::sthx,        {"sthx",   OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{439, Interpreter::sthux,       {"sthux",  OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store byte
	{215, Interpreter::stbx,        {"stbx",   OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{247, Interpreter::stbux,       {"stbux",  OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1, 0, 0, 0}},

	//store bytereverse
	{662, Interpreter::stwbrx,      {"stwbrx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{918, Interpreter::sthbrx,      {"sthbrx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},

	{661, Interpreter::stswx,       {"stswx",  OpType::Store, FL_EVIL | FL_IN_A0B | FL_LOADSTORE, 1, 0, 0, 0}},
	{725, Interpreter::stswi,       {"stswi",  OpType::Store, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 1, 0, 0, 0}},

	// fp load/store
	{535, Interpreter::lfsx,        {"lfsx",  OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{567, Interpreter::lfsux,       {"lfsux", OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{599, Interpreter::lfdx,        {"lfdx",  OpType::LoadFP, FL_INOUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{631, Interpreter::lfdux,       {"lfdux", OpType::LoadFP, FL_INOUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{663, Interpreter::stfsx,       {"stfsx",  OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{695, Interpreter::stfsux,      {"stfsux", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{727, Interpreter::stfdx,       {"stfdx",  OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{759, Interpreter::stfdux,      {"stfdux", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},
	{983, Interpreter::stfiwx,      {"stfiwx", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1, 0, 0, 0}},

	{19,  Interpreter::mfcr,        {"mfcr",   OpType::System, FL_OUT_D, 1, 0, 0, 0}},
	{83,  Interpreter::mfmsr,       {"mfmsr",  OpType::System, FL_OUT_D, 1, 0, 0, 0}},
	{144, Interpreter::mtcrf,       {"mtcrf",  OpType::System, FL_IN_S | FL_SET_CRn, 1, 0, 0, 0}},
	{146, Interpreter::mtmsr,       {"mtmsr",  OpType::System, FL_IN_S | FL_ENDBLOCK, 1, 0, 0, 0}},
	{210, Interpreter::mtsr,        {"mtsr",   OpType::System, FL_IN_S, 1, 0, 0, 0}},
	{242, Interpreter::mtsrin,      {"mtsrin", OpType::System, FL_IN_SB, 1, 0, 0, 0}},
	{339, Interpreter::mfspr,       {"mfspr",  OpType::SPR, FL_OUT_D, 1, 0, 0, 0}},
	{467, Interpreter::mtspr,       {"mtspr",  OpType::SPR, FL_IN_S, 2, 0, 0, 0}},
	{371, Interpreter::mftb,        {"mftb",   OpType::System, FL_OUT_D | FL_TIMER, 1, 0, 0, 0}},
	{512, Interpreter::mcrxr,       {"mcrxr",  OpType::System, FL_SET_CRn | FL_READ_CA | FL_SET_CA, 1, 0, 0, 0}},
	{595, Interpreter::mfsr,        {"mfsr",   OpType::System, FL_OUT_D, 3, 0, 0, 0}},
	{659, Interpreter::mfsrin,      {"mfsrin", OpType::System, FL_OUT_D | FL_IN_B, 3, 0, 0, 0}},

	{4,   Interpreter::tw,          {"tw",     OpType::System, FL_IN_AB | FL_ENDBLOCK, 2, 0, 0, 0}},
	{598, Interpreter::sync,        {"sync",   OpType::System, 0, 3, 0, 0, 0}},
	{982, Interpreter::icbi,        {"icbi",   OpType::System, FL_IN_A0B | FL_ENDBLOCK | FL_LOADSTORE, 4, 0, 0, 0}},

	// Unused instructions on GC
	{310, Interpreter::eciwx,       {"eciwx",   OpType::System, FL_IN_A0B | FL_OUT_D | FL_LOADSTORE, 1, 0, 0, 0}},
	{438, Interpreter::ecowx,       {"ecowx",   OpType::System, FL_IN_A0B | FL_IN_S | FL_LOADSTORE, 1, 0, 0, 0}},
	{854, Interpreter::eieio,       {"eieio",   OpType::System, 0, 1, 0, 0, 0}},
	{306, Interpreter::tlbie,       {"tlbie",   OpType::System, FL_IN_B, 1, 0, 0, 0}},
	{566, Interpreter::tlbsync,     {"tlbsync", OpType::System, 0, 1, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 9> table59 =
{{
	{18, Interpreter::fdivsx,       {"fdivsx",   OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17, 0, 0, 0}}, // TODO
	{20, Interpreter::fsubsx,       {"fsubsx",   OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::faddsx,       {"faddsx",   OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{24, Interpreter::fresx,        {"fresx",    OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{25, Interpreter::fmulsx,       {"fmulsx",   OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{28, Interpreter::fmsubsx,      {"fmsubsx",  OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::fmaddsx,      {"fmaddsx",  OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::fnmsubsx,     {"fnmsubsx", OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::fnmaddsx,     {"fnmaddsx", OpType::SingleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 15> table63 =
{{
	{264, Interpreter::fabsx,       {"fabsx",   OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},

	// FIXME: fcmp modifies the FPRF flags, but if the flags are clobbered later,
	// we don't actually need to calculate or store them here. So FL_READ_FPRF and FL_SET_FPRF is not
	// an ideal representation of fcmp's effect on FPRF flags and might result in
	// slightly sub-optimal code.
	{32,  Interpreter::fcmpo,       {"fcmpo",   OpType::DoubleFP, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},
	{0,   Interpreter::fcmpu,       {"fcmpu",   OpType::DoubleFP, FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1, 0, 0, 0}},

	{14,  Interpreter::fctiwx,      {"fctiwx",  OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{15,  Interpreter::fctiwzx,     {"fctiwzx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{72,  Interpreter::fmrx,        {"fmrx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{136, Interpreter::fnabsx,      {"fnabsx",  OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{40,  Interpreter::fnegx,       {"fnegx",   OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{12,  Interpreter::frspx,       {"frspx",   OpType::DoubleFP, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},

	{64,  Interpreter::mcrfs,       {"mcrfs",   OpType::SystemFP, FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF, 1, 0, 0, 0}},
	{583, Interpreter::mffsx,       {"mffsx",   OpType::SystemFP, FL_RC_BIT_F | FL_INOUT_FLOAT_D | FL_USE_FPU | FL_READ_FPRF, 1, 0, 0, 0}},
	{70,  Interpreter::mtfsb0x,     {"mtfsb0x", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3, 0, 0, 0}},
	{38,  Interpreter::mtfsb1x,     {"mtfsb1x", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3, 0, 0, 0}},
	{134, Interpreter::mtfsfix,     {"mtfsfix", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3, 0, 0, 0}},
	{711, Interpreter::mtfsfx,      {"mtfsfx",  OpType::SystemFP, FL_RC_BIT_F | FL_IN_FLOAT_B | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3, 0, 0, 0}},
}};

static std::array<GekkoOPTemplate, 10> table63_2 =
{{
	{18, Interpreter::fdivx,        {"fdivx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 31, 0, 0, 0}},
	{20, Interpreter::fsubx,        {"fsubx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{21, Interpreter::faddx,        {"faddx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{23, Interpreter::fselx,        {"fselx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU, 1, 0, 0, 0}},
	{25, Interpreter::fmulx,        {"fmulx",    OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{26, Interpreter::frsqrtex,     {"frsqrtex", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{28, Interpreter::fmsubx,       {"fmsubx",   OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{29, Interpreter::fmaddx,       {"fmaddx",   OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{30, Interpreter::fnmsubx,      {"fnmsubx",  OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
	{31, Interpreter::fnmaddx,      {"fnmaddx",  OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1, 0, 0, 0}},
}};
// clang-format on

constexpr size_t TotalInstructionFunctionCount()
{
  return primarytable.size() + table4_2.size() + table4_3.size() + table4.size() + table31.size() +
         table19.size() + table59.size() + table63.size() + table63_2.size();
}

static_assert(TotalInstructionFunctionCount() < m_allInstructions.size(),
              "m_allInstructions is too small");

void Interpreter::InitializeInstructionTables()
{
  // once initialized, tables are read-only
  static bool initialized = false;
  if (initialized)
    return;

  // clear
  for (int i = 0; i < 64; i++)
  {
    m_op_table[i] = Interpreter::unknown_instruction;
    m_infoTable[i] = &unknownopinfo;
  }

  for (int i = 0; i < 32; i++)
  {
    m_op_table59[i] = Interpreter::unknown_instruction;
    m_infoTable59[i] = &unknownopinfo;
  }

  for (int i = 0; i < 1024; i++)
  {
    m_op_table4[i] = Interpreter::unknown_instruction;
    m_op_table19[i] = Interpreter::unknown_instruction;
    m_op_table31[i] = Interpreter::unknown_instruction;
    m_op_table63[i] = Interpreter::unknown_instruction;
    m_infoTable4[i] = &unknownopinfo;
    m_infoTable19[i] = &unknownopinfo;
    m_infoTable31[i] = &unknownopinfo;
    m_infoTable63[i] = &unknownopinfo;
  }

  for (auto& tpl : primarytable)
  {
    m_op_table[tpl.opcode] = tpl.Inst;
    m_infoTable[tpl.opcode] = &tpl.opinfo;
  }

  for (int i = 0; i < 32; i++)
  {
    int fill = i << 5;
    for (auto& tpl : table4_2)
    {
      int op = fill + tpl.opcode;
      m_op_table4[op] = tpl.Inst;
      m_infoTable4[op] = &tpl.opinfo;
    }
  }

  for (int i = 0; i < 16; i++)
  {
    int fill = i << 6;
    for (auto& tpl : table4_3)
    {
      int op = fill + tpl.opcode;
      m_op_table4[op] = tpl.Inst;
      m_infoTable4[op] = &tpl.opinfo;
    }
  }

  for (auto& tpl : table4)
  {
    int op = tpl.opcode;
    m_op_table4[op] = tpl.Inst;
    m_infoTable4[op] = &tpl.opinfo;
  }

  for (auto& tpl : table31)
  {
    int op = tpl.opcode;
    m_op_table31[op] = tpl.Inst;
    m_infoTable31[op] = &tpl.opinfo;
  }

  for (auto& tpl : table19)
  {
    int op = tpl.opcode;
    m_op_table19[op] = tpl.Inst;
    m_infoTable19[op] = &tpl.opinfo;
  }

  for (auto& tpl : table59)
  {
    int op = tpl.opcode;
    m_op_table59[op] = tpl.Inst;
    m_infoTable59[op] = &tpl.opinfo;
  }

  for (auto& tpl : table63)
  {
    int op = tpl.opcode;
    m_op_table63[op] = tpl.Inst;
    m_infoTable63[op] = &tpl.opinfo;
  }

  for (int i = 0; i < 32; i++)
  {
    int fill = i << 5;
    for (auto& tpl : table63_2)
    {
      int op = fill + tpl.opcode;
      m_op_table63[op] = tpl.Inst;
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
  for (auto& tpl : table19)
    m_allInstructions[m_numInstructions++] = &tpl.opinfo;
  for (auto& tpl : table59)
    m_allInstructions[m_numInstructions++] = &tpl.opinfo;
  for (auto& tpl : table63)
    m_allInstructions[m_numInstructions++] = &tpl.opinfo;
  for (auto& tpl : table63_2)
    m_allInstructions[m_numInstructions++] = &tpl.opinfo;

  initialized = true;
}
