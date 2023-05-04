// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCTables.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/TypeUtils.h"

#include "Core/PowerPC/PowerPC.h"

namespace PPCTables
{
namespace
{
struct GekkoOPTemplate
{
  u32 opcode;
  const char* opname;
  OpType type;
  u32 num_cycles;
  u64 flags;
};

constexpr GekkoOPTemplate s_unknown_op_info = {0, "unknown_instruction", OpType::Unknown, 0,
                                               FL_ENDBLOCK};

constexpr std::array<GekkoOPTemplate, 54> s_primary_table{{
    {4, "RunTable4", OpType::Subtable, 0, 0},
    {19, "RunTable19", OpType::Subtable, 0, 0},
    {31, "RunTable31", OpType::Subtable, 0, 0},
    {59, "RunTable59", OpType::Subtable, 0, 0},
    {63, "RunTable63", OpType::Subtable, 0, 0},

    {16, "bcx", OpType::Branch, 1, FL_ENDBLOCK | FL_READ_CR_BI},
    {18, "bx", OpType::Branch, 1, FL_ENDBLOCK},

    {3, "twi", OpType::System, 1, FL_IN_A | FL_ENDBLOCK},
    {17, "sc", OpType::System, 2, FL_ENDBLOCK},

    {7, "mulli", OpType::Integer, 3, FL_OUT_D | FL_IN_A},
    {8, "subfic", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_SET_CA},
    {10, "cmpli", OpType::Integer, 1, FL_IN_A | FL_SET_CRn},
    {11, "cmpi", OpType::Integer, 1, FL_IN_A | FL_SET_CRn},
    {12, "addic", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_SET_CA},
    {13, "addic_rc", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_SET_CA | FL_SET_CR0},
    {14, "addi", OpType::Integer, 1, FL_OUT_D | FL_IN_A0},
    {15, "addis", OpType::Integer, 1, FL_OUT_D | FL_IN_A0},

    {20, "rlwimix", OpType::Integer, 1, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT},
    {21, "rlwinmx", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_RC_BIT},
    {23, "rlwnmx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},

    {24, "ori", OpType::Integer, 1, FL_OUT_A | FL_IN_S},
    {25, "oris", OpType::Integer, 1, FL_OUT_A | FL_IN_S},
    {26, "xori", OpType::Integer, 1, FL_OUT_A | FL_IN_S},
    {27, "xoris", OpType::Integer, 1, FL_OUT_A | FL_IN_S},
    {28, "andi_rc", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_SET_CR0},
    {29, "andis_rc", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_SET_CR0},

    {32, "lwz", OpType::Load, 1, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE},
    {33, "lwzu", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE},
    {34, "lbz", OpType::Load, 1, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE},
    {35, "lbzu", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE},
    {40, "lhz", OpType::Load, 1, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE},
    {41, "lhzu", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE},

    {42, "lha", OpType::Load, 1, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE},
    {43, "lhau", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE},

    {44, "sth", OpType::Store, 1, FL_IN_A0 | FL_IN_S | FL_LOADSTORE},
    {45, "sthu", OpType::Store, 1, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE},
    {36, "stw", OpType::Store, 1, FL_IN_A0 | FL_IN_S | FL_LOADSTORE},
    {37, "stwu", OpType::Store, 1, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE},
    {38, "stb", OpType::Store, 1, FL_IN_A0 | FL_IN_S | FL_LOADSTORE},
    {39, "stbu", OpType::Store, 1, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE},

    {46, "lmw", OpType::System, 11, FL_EVIL | FL_IN_A0 | FL_LOADSTORE},
    {47, "stmw", OpType::System, 11, FL_EVIL | FL_IN_A0 | FL_LOADSTORE},

    {48, "lfs", OpType::LoadFP, 1, FL_OUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},
    {49, "lfsu", OpType::LoadFP, 1,
     FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},
    {50, "lfd", OpType::LoadFP, 1, FL_INOUT_FLOAT_D | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},
    {51, "lfdu", OpType::LoadFP, 1,
     FL_INOUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},

    {52, "stfs", OpType::StoreFP, 1, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE},
    {53, "stfsu", OpType::StoreFP, 1,
     FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},
    {54, "stfd", OpType::StoreFP, 1, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE},
    {55, "stfdu", OpType::StoreFP, 1,
     FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE},

    {56, "psq_l", OpType::LoadPS, 1,
     FL_OUT_FLOAT_D | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE | FL_PROGRAMEXCEPTION},
    {57, "psq_lu", OpType::LoadPS, 1,
     FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE | FL_PROGRAMEXCEPTION},
    {60, "psq_st", OpType::StorePS, 1,
     FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE | FL_PROGRAMEXCEPTION},
    {61, "psq_stu", OpType::StorePS, 1,
     FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE | FL_PROGRAMEXCEPTION},

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

constexpr std::array<GekkoOPTemplate, 13> s_table4{{
    // SUBOP10
    {0, "ps_cmpu0", OpType::PS, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION},
    {32, "ps_cmpo0", OpType::PS, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION},
    {40, "ps_neg", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {136, "ps_nabs", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {264, "ps_abs", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {64, "ps_cmpu1", OpType::PS, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION},
    {72, "ps_mr", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {96, "ps_cmpo1", OpType::PS, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION},
    {528, "ps_merge00", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_IN_FLOAT_AB_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {560, "ps_merge01", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_IN_FLOAT_AB_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {592, "ps_merge10", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_IN_FLOAT_AB_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {624, "ps_merge11", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_IN_FLOAT_AB_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},

    {1014, "dcbz_l", OpType::System, 1, FL_IN_A0B | FL_LOADSTORE | FL_PROGRAMEXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 17> s_table4_2{{
    {10, "ps_sum0", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {11, "ps_sum1", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {12, "ps_muls0", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {13, "ps_muls1", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {14, "ps_madds0", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {15, "ps_madds1", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {18, "ps_div", OpType::PS, 17,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION | FL_FLOAT_DIV},
    {20, "ps_sub", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {21, "ps_add", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {23, "ps_sel", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_IN_FLOAT_BC_BITEXACT | FL_RC_BIT_F | FL_USE_FPU |
         FL_PROGRAMEXCEPTION},
    {24, "ps_res", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION | FL_FLOAT_DIV},
    {25, "ps_mul", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {26, "ps_rsqrte", OpType::PS, 2,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_PROGRAMEXCEPTION |
         FL_FLOAT_EXCEPTION | FL_FLOAT_DIV},
    {28, "ps_msub", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {29, "ps_madd", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {30, "ps_nmsub", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {31, "ps_nmadd", OpType::PS, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 4> s_table4_3{{
    {6, "psq_lx", OpType::LoadPS, 1, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {7, "psq_stx", OpType::StorePS, 1, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {38, "psq_lux", OpType::LoadPS, 1,
     FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE},
    {39, "psq_stux", OpType::StorePS, 1,
     FL_IN_FLOAT_S | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE},
}};

constexpr std::array<GekkoOPTemplate, 13> s_table19{{
    {528, "bcctrx", OpType::Branch, 1, FL_ENDBLOCK | FL_READ_CR_BI},
    {16, "bclrx", OpType::Branch, 1, FL_ENDBLOCK | FL_READ_CR_BI},
    {257, "crand", OpType::CR, 1, FL_EVIL},
    {129, "crandc", OpType::CR, 1, FL_EVIL},
    {289, "creqv", OpType::CR, 1, FL_EVIL},
    {225, "crnand", OpType::CR, 1, FL_EVIL},
    {33, "crnor", OpType::CR, 1, FL_EVIL},
    {449, "cror", OpType::CR, 1, FL_EVIL},
    {417, "crorc", OpType::CR, 1, FL_EVIL},
    {193, "crxor", OpType::CR, 1, FL_EVIL},

    {150, "isync", OpType::InstructionCache, 1, FL_EVIL},
    {0, "mcrf", OpType::System, 1, FL_EVIL | FL_SET_CRn | FL_READ_CRn},

    {50, "rfi", OpType::System, 2, FL_ENDBLOCK | FL_CHECKEXCEPTIONS | FL_PROGRAMEXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 107> s_table31{{
    {266, "addx", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {778, "addox", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE},
    {10, "addcx", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT},
    {522, "addcox", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {138, "addex", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {650, "addeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {234, "addmex", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {746, "addmeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {202, "addzex", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {714, "addzeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {491, "divwx", OpType::Integer, 40, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {1003, "divwox", OpType::Integer, 40, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE},
    {459, "divwux", OpType::Integer, 40, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {971, "divwuox", OpType::Integer, 40, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE},
    {75, "mulhwx", OpType::Integer, 5, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {11, "mulhwux", OpType::Integer, 5, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {235, "mullwx", OpType::Integer, 5, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {747, "mullwox", OpType::Integer, 5, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE},
    {104, "negx", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_RC_BIT},
    {616, "negox", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_RC_BIT | FL_SET_OE},
    {40, "subfx", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_RC_BIT},
    {552, "subfox", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_SET_OE},
    {8, "subfcx", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT},
    {520, "subfcox", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {136, "subfex", OpType::Integer, 1, FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {648, "subfeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {232, "subfmex", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {744, "subfmeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},
    {200, "subfzex", OpType::Integer, 1, FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT},
    {712, "subfzeox", OpType::Integer, 1,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_SET_OE},

    {28, "andx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {60, "andcx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {444, "orx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {124, "norx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {316, "xorx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {412, "orcx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {476, "nandx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {284, "eqvx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {0, "cmp", OpType::Integer, 1, FL_IN_AB | FL_SET_CRn},
    {32, "cmpl", OpType::Integer, 1, FL_IN_AB | FL_SET_CRn},
    {26, "cntlzwx", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_RC_BIT},
    {922, "extshx", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_RC_BIT},
    {954, "extsbx", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_RC_BIT},
    {536, "srwx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},
    {792, "srawx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_SET_CA | FL_RC_BIT},
    {824, "srawix", OpType::Integer, 1, FL_OUT_A | FL_IN_S | FL_SET_CA | FL_RC_BIT},
    {24, "slwx", OpType::Integer, 1, FL_OUT_A | FL_IN_SB | FL_RC_BIT},

    {54, "dcbst", OpType::DataCache, 5, FL_IN_A0B | FL_LOADSTORE},
    {86, "dcbf", OpType::DataCache, 5, FL_IN_A0B | FL_LOADSTORE},
    {246, "dcbtst", OpType::DataCache, 2, 0},
    {278, "dcbt", OpType::DataCache, 2, 0},
    {470, "dcbi", OpType::DataCache, 5, FL_IN_A0B | FL_LOADSTORE | FL_PROGRAMEXCEPTION},
    {758, "dcba", OpType::DataCache, 5, 0},
    {1014, "dcbz", OpType::DataCache, 5, FL_IN_A0B | FL_LOADSTORE},

    // load word
    {23, "lwzx", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},
    {55, "lwzux", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // load halfword
    {279, "lhzx", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},
    {311, "lhzux", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // load halfword signextend
    {343, "lhax", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},
    {375, "lhaux", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // load byte
    {87, "lbzx", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},
    {119, "lbzux", OpType::Load, 1, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // load byte reverse
    {534, "lwbrx", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},
    {790, "lhbrx", OpType::Load, 1, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE},

    // Conditional load/store (Wii SMP)
    {150, "stwcxd", OpType::Store, 1, FL_EVIL | FL_IN_S | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE},
    {20, "lwarx", OpType::Load, 1, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE},

    // load string (Inst these)
    {533, "lswx", OpType::Load, 1, FL_EVIL | FL_IN_A0B | FL_OUT_D | FL_LOADSTORE},
    {597, "lswi", OpType::Load, 1, FL_EVIL | FL_IN_A0 | FL_OUT_D | FL_LOADSTORE},

    // store word
    {151, "stwx", OpType::Store, 1, FL_IN_S | FL_IN_A0B | FL_LOADSTORE},
    {183, "stwux", OpType::Store, 1, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // store halfword
    {407, "sthx", OpType::Store, 1, FL_IN_S | FL_IN_A0B | FL_LOADSTORE},
    {439, "sthux", OpType::Store, 1, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // store byte
    {215, "stbx", OpType::Store, 1, FL_IN_S | FL_IN_A0B | FL_LOADSTORE},
    {247, "stbux", OpType::Store, 1, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE},

    // store bytereverse
    {662, "stwbrx", OpType::Store, 1, FL_IN_S | FL_IN_A0B | FL_LOADSTORE},
    {918, "sthbrx", OpType::Store, 1, FL_IN_S | FL_IN_A0B | FL_LOADSTORE},

    {661, "stswx", OpType::Store, 1, FL_EVIL | FL_IN_A0B | FL_LOADSTORE},
    {725, "stswi", OpType::Store, 1, FL_EVIL | FL_IN_A0 | FL_LOADSTORE},

    // fp load/store
    {535, "lfsx", OpType::LoadFP, 1, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {567, "lfsux", OpType::LoadFP, 1,
     FL_OUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE},
    {599, "lfdx", OpType::LoadFP, 1, FL_INOUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {631, "lfdux", OpType::LoadFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE},

    {663, "stfsx", OpType::StoreFP, 1, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {695, "stfsux", OpType::StoreFP, 1,
     FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE},
    {727, "stfdx", OpType::StoreFP, 1, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},
    {759, "stfdux", OpType::StoreFP, 1,
     FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE},
    {983, "stfiwx", OpType::StoreFP, 1, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE},

    {19, "mfcr", OpType::System, 1, FL_OUT_D | FL_READ_ALL_CR},
    {83, "mfmsr", OpType::System, 1, FL_OUT_D | FL_PROGRAMEXCEPTION},
    {144, "mtcrf", OpType::System, 1, FL_IN_S | FL_SET_ALL_CR | FL_READ_ALL_CR},
    {146, "mtmsr", OpType::System, 1,
     FL_IN_S | FL_ENDBLOCK | FL_PROGRAMEXCEPTION | FL_FLOAT_EXCEPTION},
    {210, "mtsr", OpType::System, 1, FL_IN_S | FL_PROGRAMEXCEPTION},
    {242, "mtsrin", OpType::System, 1, FL_IN_SB | FL_PROGRAMEXCEPTION},
    {339, "mfspr", OpType::SPR, 1, FL_OUT_D | FL_PROGRAMEXCEPTION},
    {467, "mtspr", OpType::SPR, 2, FL_IN_S | FL_PROGRAMEXCEPTION},
    {371, "mftb", OpType::System, 1, FL_OUT_D | FL_TIMER | FL_PROGRAMEXCEPTION},
    {512, "mcrxr", OpType::System, 1, FL_SET_CRn | FL_READ_CA | FL_SET_CA},
    {595, "mfsr", OpType::System, 3, FL_OUT_D | FL_PROGRAMEXCEPTION},
    {659, "mfsrin", OpType::System, 3, FL_OUT_D | FL_IN_B | FL_PROGRAMEXCEPTION},

    {4, "tw", OpType::System, 2, FL_IN_AB | FL_ENDBLOCK},
    {598, "sync", OpType::System, 3, 0},
    {982, "icbi", OpType::System, 4, FL_IN_A0B | FL_ENDBLOCK | FL_LOADSTORE},

    // Unused instructions on GC
    {310, "eciwx", OpType::System, 1, FL_IN_A0B | FL_OUT_D | FL_LOADSTORE},
    {438, "ecowx", OpType::System, 1, FL_IN_A0B | FL_IN_S | FL_LOADSTORE},
    {854, "eieio", OpType::System, 1, 0},
    {306, "tlbie", OpType::System, 1, FL_IN_B | FL_PROGRAMEXCEPTION},
    {566, "tlbsync", OpType::System, 1, FL_PROGRAMEXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 9> s_table59{{
    {18, "fdivsx", OpType::SingleFP, 17,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION |
         FL_FLOAT_DIV},  // TODO
    {20, "fsubsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {21, "faddsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {24, "fresx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION |
         FL_FLOAT_DIV},
    {25, "fmulsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {28, "fmsubsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {29, "fmaddsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {30, "fnmsubsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {31, "fnmaddsx", OpType::SingleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 15> s_table63{{
    {264, "fabsx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU},

    // FIXME: fcmp modifies the FPRF flags, but if the flags are clobbered later,
    // we don't actually need to calculate or store them here. So FL_READ_FPRF and FL_SET_FPRF is
    // not an ideal representation of fcmp's effect on FPRF flags and might result in slightly
    // sub-optimal code.
    {32, "fcmpo", OpType::DoubleFP, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {0, "fcmpu", OpType::DoubleFP, 1,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_FLOAT_EXCEPTION},

    {14, "fctiwx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_FLOAT_EXCEPTION},
    {15, "fctiwzx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_FLOAT_EXCEPTION},
    {72, "fmrx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_IN_FLOAT_B_BITEXACT | FL_RC_BIT_F | FL_USE_FPU},
    {136, "fnabsx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_IN_FLOAT_B_BITEXACT | FL_USE_FPU},
    {40, "fnegx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_IN_FLOAT_B_BITEXACT | FL_USE_FPU},
    {12, "frspx", OpType::DoubleFP, 1,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF | FL_FLOAT_EXCEPTION},

    {64, "mcrfs", OpType::SystemFP, 1, FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF},
    {583, "mffsx", OpType::SystemFP, 1, FL_RC_BIT_F | FL_INOUT_FLOAT_D | FL_USE_FPU | FL_READ_FPRF},
    {70, "mtfsb0x", OpType::SystemFP, 3, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF},
    {38, "mtfsb1x", OpType::SystemFP, 3,
     FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {134, "mtfsfix", OpType::SystemFP, 3,
     FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
    {711, "mtfsfx", OpType::SystemFP, 3,
     FL_RC_BIT_F | FL_IN_FLOAT_B | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF | FL_FLOAT_EXCEPTION},
}};

constexpr std::array<GekkoOPTemplate, 10> s_table63_2{{
    {18, "fdivx", OpType::DoubleFP, 31,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION | FL_FLOAT_DIV},
    {20, "fsubx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {21, "faddx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {23, "fselx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_IN_FLOAT_BC_BITEXACT | FL_RC_BIT_F | FL_USE_FPU},
    {25, "fmulx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {26, "frsqrtex", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION | FL_FLOAT_DIV},
    {28, "fmsubx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {29, "fmaddx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {30, "fnmsubx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
    {31, "fnmaddx", OpType::DoubleFP, 1,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF |
         FL_FLOAT_EXCEPTION},
}};

constexpr size_t TOTAL_INSTRUCTION_COUNT =
    1 + s_primary_table.size() + s_table4_2.size() + s_table4_3.size() + s_table4.size() +
    s_table31.size() + s_table19.size() + s_table59.size() + s_table63.size() + s_table63_2.size();

struct Tables
{
  std::array<GekkoOPInfo, TOTAL_INSTRUCTION_COUNT> all_instructions{};
  u32 unknown_op_info;
  std::array<u32, 64> primary_table{};
  std::array<u32, 1024> table4{};
  std::array<u32, 1024> table19{};
  std::array<u32, 1024> table31{};
  std::array<u32, 32> table59{};
  std::array<u32, 1024> table63{};
};
}  // namespace

static std::array<GekkoOPStats, TOTAL_INSTRUCTION_COUNT> s_all_instructions_stats;

constexpr Tables s_tables = []() consteval
{
  Tables tables{};

  u32 counter = 0;
  auto make_info = [&](const GekkoOPTemplate& inst) consteval->u32
  {
    ASSERT(counter < TOTAL_INSTRUCTION_COUNT);
    GekkoOPInfo* info = &tables.all_instructions[counter];
    info->opname = inst.opname;
    info->flags = inst.flags;
    info->type = inst.type;
    info->num_cycles = inst.num_cycles;
    info->stats = &s_all_instructions_stats[counter];
    return counter++;
  };

  u32 unknown_op_info = make_info(s_unknown_op_info);
  tables.unknown_op_info = unknown_op_info;

  Common::Fill(tables.primary_table, unknown_op_info);
  for (auto& tpl : s_primary_table)
  {
    ASSERT(tables.primary_table[tpl.opcode] == unknown_op_info);
    tables.primary_table[tpl.opcode] = make_info(tpl);
  };

  Common::Fill(tables.table4, unknown_op_info);

  for (const auto& tpl : s_table4_2)
  {
    u32 info = make_info(tpl);
    for (u32 i = 0; i < 32; i++)
    {
      const u32 fill = i << 5;
      const u32 op = fill + tpl.opcode;
      ASSERT(tables.table4[op] == unknown_op_info);
      tables.table4[op] = info;
    }
  }

  for (const auto& tpl : s_table4_3)
  {
    u32 info = make_info(tpl);
    for (u32 i = 0; i < 16; i++)
    {
      const u32 fill = i << 6;
      const u32 op = fill + tpl.opcode;
      ASSERT(tables.table4[op] == unknown_op_info);
      tables.table4[op] = info;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(tables.table4[op] == unknown_op_info);
    tables.table4[op] = make_info(tpl);
  }

  Common::Fill(tables.table19, unknown_op_info);
  for (auto& tpl : s_table19)
  {
    ASSERT(tables.table19[tpl.opcode] == unknown_op_info);
    tables.table19[tpl.opcode] = make_info(tpl);
  };

  Common::Fill(tables.table31, unknown_op_info);
  for (auto& tpl : s_table31)
  {
    ASSERT(tables.table31[tpl.opcode] == unknown_op_info);
    tables.table31[tpl.opcode] = make_info(tpl);
  };

  Common::Fill(tables.table59, unknown_op_info);
  for (auto& tpl : s_table59)
  {
    ASSERT(tables.table59[tpl.opcode] == unknown_op_info);
    tables.table59[tpl.opcode] = make_info(tpl);
  };

  Common::Fill(tables.table63, unknown_op_info);
  for (auto& tpl : s_table63)
  {
    ASSERT(tables.table63[tpl.opcode] == unknown_op_info);
    tables.table63[tpl.opcode] = make_info(tpl);
  };

  for (const auto& tpl : s_table63_2)
  {
    u32 info = make_info(tpl);
    for (u32 i = 0; i < 32; i++)
    {
      const u32 fill = i << 5;
      const u32 op = fill + tpl.opcode;
      ASSERT(tables.table63[op] == unknown_op_info);
      tables.table63[op] = info;
    }
  }

  ASSERT(counter == TOTAL_INSTRUCTION_COUNT);
  return tables;
}
();

const GekkoOPInfo* GetOpInfo(UGeckoInstruction inst, u32 pc)
{
  const GekkoOPInfo* info = &s_tables.all_instructions[s_tables.primary_table[inst.OPCD]];
  if (info->type == OpType::Subtable)
  {
    switch (inst.OPCD)
    {
    case 4:
      return &s_tables.all_instructions[s_tables.table4[inst.SUBOP10]];
    case 19:
      return &s_tables.all_instructions[s_tables.table19[inst.SUBOP10]];
    case 31:
      return &s_tables.all_instructions[s_tables.table31[inst.SUBOP10]];
    case 59:
      return &s_tables.all_instructions[s_tables.table59[inst.SUBOP5]];
    case 63:
      return &s_tables.all_instructions[s_tables.table63[inst.SUBOP10]];
    default:
      ASSERT_MSG(POWERPC, 0, "GetOpInfo - invalid subtable op {:08x} @ {:08x}", inst.hex, pc);
      return &s_tables.all_instructions[s_tables.unknown_op_info];
    }
  }
  else
  {
    if (info->type == OpType::Invalid)
    {
      ASSERT_MSG(POWERPC, 0, "GetOpInfo - invalid op {:08x} @ {:08x}", inst.hex, pc);
      return &s_tables.all_instructions[s_tables.unknown_op_info];
    }
    return info;
  }
}

// #define OPLOG
// #define OP_TO_LOG "mtfsb0x"

#ifdef OPLOG
namespace
{
std::vector<u32> rsplocations;
}
#endif

const char* GetInstructionName(UGeckoInstruction inst, u32 pc)
{
  const GekkoOPInfo* info = GetOpInfo(inst, pc);
  return info->opname;
}

bool IsValidInstruction(UGeckoInstruction inst, u32 pc)
{
  const GekkoOPInfo* info = GetOpInfo(inst, pc);
  return info->type != OpType::Invalid && info->type != OpType::Unknown;
}

void CountInstruction(UGeckoInstruction inst, u32 pc)
{
  const GekkoOPInfo* info = GetOpInfo(inst, pc);
  info->stats->run_count++;
}

void CountInstructionCompile(const GekkoOPInfo* info, u32 pc)
{
  info->stats->compile_count++;
  info->stats->last_use = pc;

#ifdef OPLOG
  if (!strcmp(info->opname, OP_TO_LOG))
  {
    rsplocations.push_back(pc);
  }
#endif
}

void PrintInstructionRunCounts()
{
  typedef std::pair<const char*, u64> OpInfo;
  std::array<OpInfo, TOTAL_INSTRUCTION_COUNT> temp;
  for (size_t i = 0; i < TOTAL_INSTRUCTION_COUNT; i++)
  {
    const GekkoOPInfo& info = s_tables.all_instructions[i];
    temp[i] = std::make_pair(info.opname, info.stats->run_count);
  }
  std::sort(temp.begin(), temp.end(),
            [](const OpInfo& a, const OpInfo& b) { return a.second > b.second; });

  for (auto& inst : temp)
  {
    if (inst.second == 0)
      break;

    INFO_LOG_FMT(POWERPC, "{} : {}", inst.first, inst.second);
  }
}

void LogCompiledInstructions()
{
  static unsigned int time = 0;

  File::IOFile f(fmt::format("{}inst_log{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (size_t i = 0; i < TOTAL_INSTRUCTION_COUNT; i++)
  {
    const GekkoOPInfo& info = s_tables.all_instructions[i];
    if (info.stats->compile_count > 0)
    {
      f.WriteString(fmt::format("{0}\t{1}\t{2}\t{3:08x}\n", info.opname, info.stats->compile_count,
                                info.stats->run_count, info.stats->last_use));
    }
  }

  f.Open(fmt::format("{}inst_not{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (size_t i = 0; i < TOTAL_INSTRUCTION_COUNT; i++)
  {
    const GekkoOPInfo& info = s_tables.all_instructions[i];
    if (info.stats->compile_count == 0)
    {
      f.WriteString(fmt::format("{0}\t{1}\t{2}\n", info.opname, info.stats->compile_count,
                                info.stats->run_count));
    }
  }

#ifdef OPLOG
  f.Open(fmt::format("{}" OP_TO_LOG "_at{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (auto& rsplocation : rsplocations)
  {
    f.WriteString(fmt::format(OP_TO_LOG ": {0:08x}\n", rsplocation));
  }
#endif

  ++time;
}

}  // namespace PPCTables
