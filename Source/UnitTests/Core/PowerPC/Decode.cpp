// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cinttypes>
#include <cstring>

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PPCTables.h"

#include <gtest/gtest.h>

namespace
{
struct GekkoOPTemplate
{
  u32 opcode;
  const char* name;
  OpType type;
  int flags;
  int cycles;
};
}  // namespace

static constexpr std::array<GekkoOPTemplate, 49> primarytable = {{
    {16, "bcx", OpType::Branch, FL_ENDBLOCK, 1},
    {18, "bx", OpType::Branch, FL_ENDBLOCK, 1},

    {3, "twi", OpType::System, FL_ENDBLOCK, 1},
    {17, "sc", OpType::System, FL_ENDBLOCK, 2},

    {7, "mulli", OpType::Integer, FL_OUT_D | FL_IN_A, 3},
    {8, "subfic", OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA, 1},
    {10, "cmpli", OpType::Integer, FL_IN_A | FL_SET_CRn, 1},
    {11, "cmpi", OpType::Integer, FL_IN_A | FL_SET_CRn, 1},
    {12, "addic", OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA, 1},
    {13, "addic_rc", OpType::Integer, FL_OUT_D | FL_IN_A | FL_SET_CA | FL_SET_CR0, 1},
    {14, "addi", OpType::Integer, FL_OUT_D | FL_IN_A0, 1},
    {15, "addis", OpType::Integer, FL_OUT_D | FL_IN_A0, 1},

    {20, "rlwimix", OpType::Integer, FL_OUT_A | FL_IN_A | FL_IN_S | FL_RC_BIT, 1},
    {21, "rlwinmx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1},
    {23, "rlwnmx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},

    {24, "ori", OpType::Integer, FL_OUT_A | FL_IN_S, 1},
    {25, "oris", OpType::Integer, FL_OUT_A | FL_IN_S, 1},
    {26, "xori", OpType::Integer, FL_OUT_A | FL_IN_S, 1},
    {27, "xoris", OpType::Integer, FL_OUT_A | FL_IN_S, 1},
    {28, "andi_rc", OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1},
    {29, "andis_rc", OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CR0, 1},

    {32, "lwz", OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1},
    {33, "lwzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1},
    {34, "lbz", OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1},
    {35, "lbzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1},
    {40, "lhz", OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1},
    {41, "lhzu", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1},

    {42, "lha", OpType::Load, FL_OUT_D | FL_IN_A0 | FL_LOADSTORE, 1},
    {43, "lhau", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_A | FL_LOADSTORE, 1},

    {44, "sth", OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1},
    {45, "sthu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1},
    {36, "stw", OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1},
    {37, "stwu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1},
    {38, "stb", OpType::Store, FL_IN_A0 | FL_IN_S | FL_LOADSTORE, 1},
    {39, "stbu", OpType::Store, FL_OUT_A | FL_IN_A | FL_IN_S | FL_LOADSTORE, 1},

    {46, "lmw", OpType::System, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 11},
    {47, "stmw", OpType::System, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 11},

    {48, "lfs", OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {49, "lfsu", OpType::LoadFP, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},
    {50, "lfd", OpType::LoadFP, FL_INOUT_FLOAT_D | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {51, "lfdu", OpType::LoadFP, FL_INOUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},

    {52, "stfs", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {53, "stfsu", OpType::StoreFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},
    {54, "stfd", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {55, "stfdu", OpType::StoreFP, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},

    {56, "psq_l", OpType::LoadPS, FL_OUT_FLOAT_D | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {57, "psq_lu", OpType::LoadPS, FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},
    {60, "psq_st", OpType::StorePS, FL_IN_FLOAT_S | FL_IN_A0 | FL_USE_FPU | FL_LOADSTORE, 1},
    {61, "psq_stu", OpType::StorePS, FL_IN_FLOAT_S | FL_OUT_A | FL_IN_A | FL_USE_FPU | FL_LOADSTORE,
     1},

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

static constexpr std::array<GekkoOPTemplate, 34> table4 = {{
    {0, "ps_cmpu0", OpType::PS,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},
    {32, "ps_cmpo0", OpType::PS,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},
    {40, "ps_neg", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {136, "ps_nabs", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {264, "ps_abs", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {64, "ps_cmpu1", OpType::PS,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},
    {72, "ps_mr", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {96, "ps_cmpo1", OpType::PS,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},
    {528, "ps_merge00", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1},
    {560, "ps_merge01", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1},
    {592, "ps_merge10", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1},
    {624, "ps_merge11", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU, 1},
    {1014, "dcbz_l", OpType::System, FL_IN_A0B | FL_LOADSTORE, 1},
    {10, "ps_sum0", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {11, "ps_sum1", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {12, "ps_muls0", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {13, "ps_muls1", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {14, "ps_madds0", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {15, "ps_madds1", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {18, "ps_div", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17},
    {20, "ps_sub", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {21, "ps_add", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {23, "ps_sel", OpType::PS, FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU, 1},
    {24, "ps_res", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {25, "ps_mul", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {26, "ps_rsqrte", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 2},
    {28, "ps_msub", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {29, "ps_madd", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {30, "ps_nmsub", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {31, "ps_nmadd", OpType::PS,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {6, "psq_lx", OpType::LoadPS, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {7, "psq_stx", OpType::StorePS, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {38, "psq_lux", OpType::LoadPS,
     FL_OUT_FLOAT_D | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1},
    {39, "psq_stux", OpType::StorePS,
     FL_IN_FLOAT_S | FL_OUT_A | FL_IN_AB | FL_USE_FPU | FL_LOADSTORE, 1},
}};

static constexpr std::array<GekkoOPTemplate, 13> table19 = {{
    {528, "bcctrx", OpType::Branch, FL_ENDBLOCK, 1},
    {16, "bclrx", OpType::Branch, FL_ENDBLOCK, 1},
    {257, "crand", OpType::CR, FL_EVIL, 1},
    {129, "crandc", OpType::CR, FL_EVIL, 1},
    {289, "creqv", OpType::CR, FL_EVIL, 1},
    {225, "crnand", OpType::CR, FL_EVIL, 1},
    {33, "crnor", OpType::CR, FL_EVIL, 1},
    {449, "cror", OpType::CR, FL_EVIL, 1},
    {417, "crorc", OpType::CR, FL_EVIL, 1},
    {193, "crxor", OpType::CR, FL_EVIL, 1},

    {150, "isync", OpType::InstructionCache, FL_EVIL, 1},
    {0, "mcrf", OpType::System, FL_EVIL | FL_SET_CRn, 1},

    {50, "rfi", OpType::System, FL_ENDBLOCK | FL_CHECKEXCEPTIONS, 2},
}};

static constexpr std::array<GekkoOPTemplate, 93> table31 = {{
    {266, "addx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_OE_BIT, 1},
    {10, "addcx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {138, "addex", OpType::Integer,
     FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {234, "addmex", OpType::Integer,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {202, "addzex", OpType::Integer,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {491, "divwx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_OE_BIT, 40},
    {459, "divwux", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_OE_BIT, 40},
    {75, "mulhwx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5},
    {11, "mulhwux", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT, 5},
    {235, "mullwx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_OE_BIT, 5},
    {104, "negx", OpType::Integer, FL_OUT_D | FL_IN_A | FL_RC_BIT | FL_OE_BIT, 1},
    {40, "subfx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_RC_BIT | FL_OE_BIT, 1},
    {8, "subfcx", OpType::Integer, FL_OUT_D | FL_IN_AB | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {136, "subfex", OpType::Integer,
     FL_OUT_D | FL_IN_AB | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {232, "subfmex", OpType::Integer,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},
    {200, "subfzex", OpType::Integer,
     FL_OUT_D | FL_IN_A | FL_READ_CA | FL_SET_CA | FL_RC_BIT | FL_OE_BIT, 1},

    {28, "andx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {60, "andcx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {444, "orx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {124, "norx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {316, "xorx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {412, "orcx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {476, "nandx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {284, "eqvx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {0, "cmp", OpType::Integer, FL_IN_AB | FL_SET_CRn, 1},
    {32, "cmpl", OpType::Integer, FL_IN_AB | FL_SET_CRn, 1},
    {26, "cntlzwx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1},
    {922, "extshx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1},
    {954, "extsbx", OpType::Integer, FL_OUT_A | FL_IN_S | FL_RC_BIT, 1},
    {536, "srwx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},
    {792, "srawx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_SET_CA | FL_RC_BIT, 1},
    {824, "srawix", OpType::Integer, FL_OUT_A | FL_IN_S | FL_SET_CA | FL_RC_BIT, 1},
    {24, "slwx", OpType::Integer, FL_OUT_A | FL_IN_SB | FL_RC_BIT, 1},

    {54, "dcbst", OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5},
    {86, "dcbf", OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5},
    {246, "dcbtst", OpType::DataCache, 0, 2},
    {278, "dcbt", OpType::DataCache, 0, 2},
    {470, "dcbi", OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5},
    {758, "dcba", OpType::DataCache, 0, 5},
    {1014, "dcbz", OpType::DataCache, FL_IN_A0B | FL_LOADSTORE, 5},

    // load word
    {23, "lwzx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},
    {55, "lwzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // load halfword
    {279, "lhzx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},
    {311, "lhzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // load halfword signextend
    {343, "lhax", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},
    {375, "lhaux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // load byte
    {87, "lbzx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},
    {119, "lbzux", OpType::Load, FL_OUT_D | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // load byte reverse
    {534, "lwbrx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},
    {790, "lhbrx", OpType::Load, FL_OUT_D | FL_IN_A0B | FL_LOADSTORE, 1},

    // Conditional load/store (Wii SMP)
    {150, "stwcxd", OpType::Store, FL_EVIL | FL_IN_S | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1},
    {20, "lwarx", OpType::Load, FL_EVIL | FL_OUT_D | FL_IN_A0B | FL_SET_CR0 | FL_LOADSTORE, 1},

    // load string (Inst these)
    {533, "lswx", OpType::Load, FL_EVIL | FL_IN_A0B | FL_OUT_D | FL_LOADSTORE, 1},
    {597, "lswi", OpType::Load, FL_EVIL | FL_IN_A0 | FL_OUT_D | FL_LOADSTORE, 1},

    // store word
    {151, "stwx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1},
    {183, "stwux", OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // store halfword
    {407, "sthx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1},
    {439, "sthux", OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // store byte
    {215, "stbx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1},
    {247, "stbux", OpType::Store, FL_IN_S | FL_OUT_A | FL_IN_AB | FL_LOADSTORE, 1},

    // store bytereverse
    {662, "stwbrx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1},
    {918, "sthbrx", OpType::Store, FL_IN_S | FL_IN_A0B | FL_LOADSTORE, 1},

    {661, "stswx", OpType::Store, FL_EVIL | FL_IN_A0B | FL_LOADSTORE, 1},
    {725, "stswi", OpType::Store, FL_EVIL | FL_IN_A0 | FL_LOADSTORE, 1},

    // fp load/store
    {535, "lfsx", OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {567, "lfsux", OpType::LoadFP, FL_OUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE,
     1},
    {599, "lfdx", OpType::LoadFP, FL_INOUT_FLOAT_D | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {631, "lfdux", OpType::LoadFP,
     FL_INOUT_FLOAT_D | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1},

    {663, "stfsx", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {695, "stfsux", OpType::StoreFP,
     FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1},
    {727, "stfdx", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},
    {759, "stfdux", OpType::StoreFP,
     FL_IN_FLOAT_S | FL_IN_AB | FL_OUT_A | FL_USE_FPU | FL_LOADSTORE, 1},
    {983, "stfiwx", OpType::StoreFP, FL_IN_FLOAT_S | FL_IN_A0B | FL_USE_FPU | FL_LOADSTORE, 1},

    {19, "mfcr", OpType::System, FL_OUT_D, 1},
    {83, "mfmsr", OpType::System, FL_OUT_D, 1},
    {144, "mtcrf", OpType::System, FL_IN_S | FL_SET_CRn, 1},
    {146, "mtmsr", OpType::System, FL_IN_S | FL_ENDBLOCK, 1},
    {210, "mtsr", OpType::System, FL_IN_S, 1},
    {242, "mtsrin", OpType::System, FL_IN_SB, 1},
    {339, "mfspr", OpType::SPR, FL_OUT_D, 1},
    {467, "mtspr", OpType::SPR, FL_IN_S, 2},
    {371, "mftb", OpType::System, FL_OUT_D | FL_TIMER, 1},
    {512, "mcrxr", OpType::System, FL_SET_CRn | FL_READ_CA | FL_SET_CA, 1},
    {595, "mfsr", OpType::System, FL_OUT_D, 3},
    {659, "mfsrin", OpType::System, FL_OUT_D | FL_IN_B, 3},

    {4, "tw", OpType::System, FL_IN_AB | FL_ENDBLOCK, 2},
    {598, "sync", OpType::System, 0, 3},
    {982, "icbi", OpType::System, FL_IN_A0B | FL_ENDBLOCK | FL_LOADSTORE, 4},

    // Unused instructions on GC
    {310, "eciwx", OpType::System, FL_IN_A0B | FL_OUT_D | FL_LOADSTORE, 1},
    {438, "ecowx", OpType::System, FL_IN_A0B | FL_IN_S | FL_LOADSTORE, 1},
    {854, "eieio", OpType::System, 0, 1},
    {306, "tlbie", OpType::System, FL_IN_B, 1},
    {566, "tlbsync", OpType::System, 0, 1},
}};

static constexpr std::array<GekkoOPTemplate, 9> table59 = {{
    {18, "fdivsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 17},  // TODO
    {20, "fsubsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {21, "faddsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {24, "fresx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {25, "fmulsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {28, "fmsubsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {29, "fmaddsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {30, "fnmsubsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {31, "fnmaddsx", OpType::SingleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
}};

static constexpr std::array<GekkoOPTemplate, 25> table63 = {{
    {264, "fabsx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU,
     1},

    // FIXME: fcmp modifies the FPRF flags, but if the flags are clobbered later,
    // we don't actually need to calculate or store them here. So FL_READ_FPRF and FL_SET_FPRF is
    // not an ideal representation of fcmp's effect on FPRF flags and might result in slightly
    // sub-optimal code.
    {32, "fcmpo", OpType::DoubleFP,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},
    {0, "fcmpu", OpType::DoubleFP,
     FL_IN_FLOAT_AB | FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 1},

    {14, "fctiwx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU,
     1},
    {15, "fctiwzx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU,
     1},
    {72, "fmrx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {136, "fnabsx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU,
     1},
    {40, "fnegx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU, 1},
    {12, "frspx", OpType::DoubleFP,
     FL_OUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},

    {64, "mcrfs", OpType::SystemFP, FL_SET_CRn | FL_USE_FPU | FL_READ_FPRF, 1},
    {583, "mffsx", OpType::SystemFP, FL_RC_BIT_F | FL_INOUT_FLOAT_D | FL_USE_FPU | FL_READ_FPRF, 1},
    {70, "mtfsb0x", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3},
    {38, "mtfsb1x", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3},
    {134, "mtfsfix", OpType::SystemFP, FL_RC_BIT_F | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3},
    {711, "mtfsfx", OpType::SystemFP,
     FL_RC_BIT_F | FL_IN_FLOAT_B | FL_USE_FPU | FL_READ_FPRF | FL_SET_FPRF, 3},
    {18, "fdivx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 31},
    {20, "fsubx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {21, "faddx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AB | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {23, "fselx", OpType::DoubleFP, FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU,
     1},
    {25, "fmulx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_AC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {26, "frsqrtex", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_B | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {28, "fmsubx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {29, "fmaddx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {30, "fnmsubx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
    {31, "fnmaddx", OpType::DoubleFP,
     FL_INOUT_FLOAT_D | FL_IN_FLOAT_ABC | FL_RC_BIT_F | FL_USE_FPU | FL_SET_FPRF, 1},
}};

static void TestSubtable(const GekkoOPTemplate* templ, const GekkoOPTemplate* end, u32 opc)
{
  do
  {
    printf("Testing %" PRIxPTR " '%s'\n", (uintptr_t)templ, templ->name);
    const GekkoOPInfo* info = PPCTables::GetOpInfo((opc << 26) | (templ->opcode << 1));
    ASSERT_FALSE(!info);
    EXPECT_EQ(std::strcmp(templ->name, info->opname), 0);
    EXPECT_EQ(templ->type, info->type);
    EXPECT_EQ(templ->flags, info->flags);
    EXPECT_EQ(templ->cycles, info->numCycles);
    if (templ->flags & FL_OE_BIT)
    {
      EXPECT_EQ(info, PPCTables::GetOpInfo((opc << 26) | (templ->opcode << 1) | 1 << 10));
    }
  } while (++templ != end);
}

TEST(PPCTables, Decode)
{
  for (u8 i = 0; i < primarytable.size(); ++i)
  {
    auto& templ = primarytable[i];
    printf("Testing %" PRIxPTR " '%s'\n", (uintptr_t)&templ, templ.name);
    const GekkoOPInfo* info = PPCTables::GetOpInfo(templ.opcode << 26);
    EXPECT_EQ(std::strcmp(templ.name, info->opname), 0);
    EXPECT_EQ(templ.type, info->type);
    EXPECT_EQ(templ.flags, info->flags);
    EXPECT_EQ(templ.cycles, info->numCycles);
  }

  TestSubtable(&table4[0], &table4[0] + table4.size(), 4);
  TestSubtable(&table19[0], &table19[0] + table19.size(), 19);
  TestSubtable(&table31[0], &table31[0] + table31.size(), 31);
  TestSubtable(&table59[0], &table59[0] + table59.size(), 59);
  TestSubtable(&table63[0], &table63[0] + table63.size(), 63);
}
