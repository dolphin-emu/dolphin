// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/Jit.h"

#include <array>

#include "Common/Assert.h"
#include "Core/PowerPC/Gekko.h"

namespace
{
struct JitPPC64OpTemplate
{
  u32 opcode;
  JitPPC64::Instruction fn;
};

constexpr std::array<JitPPC64OpTemplate, 54> s_primary_table{{
    {4, &JitPPC64::DynaRunTable4},    // RunTable4
    {19, &JitPPC64::DynaRunTable19},  // RunTable19
    {31, &JitPPC64::DynaRunTable31},  // RunTable31
    {59, &JitPPC64::DynaRunTable59},  // RunTable59
    {63, &JitPPC64::DynaRunTable63},  // RunTable63

    {16, &JitPPC64::bcx},  // bcx
    {18, &JitPPC64::bx},   // bx

    {3, &JitPPC64::twx},  // twi
    {17, &JitPPC64::sc},  // sc

    {7, &JitPPC64::mulli},   // mulli
    {8, &JitPPC64::subfic},  // subfic
    {10, &JitPPC64::cmpli},  // cmpli
    {11, &JitPPC64::cmpi},   // cmpi
    {12, &JitPPC64::addic},  // addic
    {13, &JitPPC64::addic},  // addic_rc
    {14, &JitPPC64::addix},  // addi
    {15, &JitPPC64::addix},  // addis

    {20, &JitPPC64::rlwimix},  // rlwimix
    {21, &JitPPC64::rlwinmx},  // rlwinmx
    {23, &JitPPC64::rlwnmx},   // rlwnmx

    {24, &JitPPC64::arith_imm},  // ori
    {25, &JitPPC64::arith_imm},  // oris
    {26, &JitPPC64::arith_imm},  // xori
    {27, &JitPPC64::arith_imm},  // xoris
    {28, &JitPPC64::arith_imm},  // andi_rc
    {29, &JitPPC64::arith_imm},  // andis_rc

    {32, &JitPPC64::lXX},  // lwz
    {33, &JitPPC64::lXX},  // lwzu
    {34, &JitPPC64::lXX},  // lbz
    {35, &JitPPC64::lXX},  // lbzu
    {40, &JitPPC64::lXX},  // lhz
    {41, &JitPPC64::lXX},  // lhzu
    {42, &JitPPC64::lXX},  // lha
    {43, &JitPPC64::lXX},  // lhau

    {44, &JitPPC64::stX},  // sth
    {45, &JitPPC64::stX},  // sthu
    {36, &JitPPC64::stX},  // stw
    {37, &JitPPC64::stX},  // stwu
    {38, &JitPPC64::stX},  // stb
    {39, &JitPPC64::stX},  // stbu

    {46, &JitPPC64::lmw_stmw},  // lmw
    {47, &JitPPC64::lmw_stmw},  // stmw

    {48, &JitPPC64::lfXX},  // lfs
    {49, &JitPPC64::lfXX},  // lfsu
    {50, &JitPPC64::lfXX},  // lfd
    {51, &JitPPC64::lfXX},  // lfdu

    {52, &JitPPC64::stfXX},  // stfs
    {53, &JitPPC64::stfXX},  // stfsu
    {54, &JitPPC64::stfXX},  // stfd
    {55, &JitPPC64::stfXX},  // stfdu

    {56, &JitPPC64::psq_lXX},   // psq_l
    {57, &JitPPC64::psq_lXX},   // psq_lu
    {60, &JitPPC64::psq_stXX},  // psq_st
    {61, &JitPPC64::psq_stXX},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 58, 62
}};

constexpr std::array<JitPPC64OpTemplate, 13> s_table4{{
    // SUBOP10
    {0, &JitPPC64::ps_cmpXX},      // ps_cmpu0
    {32, &JitPPC64::ps_cmpXX},     // ps_cmpo0
    {40, &JitPPC64::fp_logic},     // ps_neg
    {136, &JitPPC64::fp_logic},    // ps_nabs
    {264, &JitPPC64::fp_logic},    // ps_abs
    {64, &JitPPC64::ps_cmpXX},     // ps_cmpu1
    {72, &JitPPC64::fp_logic},     // ps_mr
    {96, &JitPPC64::ps_cmpXX},     // ps_cmpo1
    {528, &JitPPC64::ps_mergeXX},  // ps_merge00
    {560, &JitPPC64::ps_mergeXX},  // ps_merge01
    {592, &JitPPC64::ps_mergeXX},  // ps_merge10
    {624, &JitPPC64::ps_mergeXX},  // ps_merge11

    {1014, &JitPPC64::dcbz},  // dcbz_l
}};

constexpr std::array<JitPPC64OpTemplate, 17> s_table4_2{{
    {10, &JitPPC64::ps_sumX},    // ps_sum0
    {11, &JitPPC64::ps_sumX},    // ps_sum1
    {12, &JitPPC64::ps_arith},   // ps_muls0
    {13, &JitPPC64::ps_arith},   // ps_muls1
    {14, &JitPPC64::ps_arith},   // ps_madds0
    {15, &JitPPC64::ps_arith},   // ps_madds1
    {18, &JitPPC64::ps_arith},   // ps_div
    {20, &JitPPC64::ps_arith},   // ps_sub
    {21, &JitPPC64::ps_arith},   // ps_add
    {23, &JitPPC64::ps_sel},     // ps_sel
    {24, &JitPPC64::ps_res},     // ps_res
    {25, &JitPPC64::ps_arith},   // ps_mul
    {26, &JitPPC64::ps_rsqrte},  // ps_rsqrte
    {28, &JitPPC64::ps_arith},   // ps_msub
    {29, &JitPPC64::ps_arith},   // ps_madd
    {30, &JitPPC64::ps_arith},   // ps_nmsub
    {31, &JitPPC64::ps_arith},   // ps_nmadd
}};

constexpr std::array<JitPPC64OpTemplate, 4> s_table4_3{{
    {6, &JitPPC64::psq_lXX},    // psq_lx
    {7, &JitPPC64::psq_stXX},   // psq_stx
    {38, &JitPPC64::psq_lXX},   // psq_lux
    {39, &JitPPC64::psq_stXX},  // psq_stux
}};

constexpr std::array<JitPPC64OpTemplate, 13> s_table19{{
    {528, &JitPPC64::bcctrx},  // bcctrx
    {16, &JitPPC64::bclrx},    // bclrx
    {257, &JitPPC64::crXXX},   // crand
    {129, &JitPPC64::crXXX},   // crandc
    {289, &JitPPC64::crXXX},   // creqv
    {225, &JitPPC64::crXXX},   // crnand
    {33, &JitPPC64::crXXX},    // crnor
    {449, &JitPPC64::crXXX},   // cror
    {417, &JitPPC64::crXXX},   // crorc
    {193, &JitPPC64::crXXX},   // crxor

    {150, &JitPPC64::DoNothing},  // isync
    {0, &JitPPC64::mcrf},         // mcrf

    {50, &JitPPC64::rfi},  // rfi
}};

constexpr std::array<JitPPC64OpTemplate, 107> s_table31{{
    {266, &JitPPC64::addx},     // addx
    {778, &JitPPC64::addx},     // addox
    {10, &JitPPC64::addcx},     // addcx
    {522, &JitPPC64::addcx},    // addcox
    {138, &JitPPC64::addex},    // addex
    {650, &JitPPC64::addex},    // addeox
    {234, &JitPPC64::addex},    // addmex
    {746, &JitPPC64::addex},    // addmeox
    {202, &JitPPC64::addzex},   // addzex
    {714, &JitPPC64::addzex},   // addzeox
    {491, &JitPPC64::divwx},    // divwx
    {1003, &JitPPC64::divwx},   // divwox
    {459, &JitPPC64::divwux},   // divwux
    {971, &JitPPC64::divwux},   // divwuox
    {75, &JitPPC64::mulhwx},    // mulhwx
    {11, &JitPPC64::mulhwux},   // mulhwux
    {235, &JitPPC64::mullwx},   // mullwx
    {747, &JitPPC64::mullwx},   // mullwox
    {104, &JitPPC64::negx},     // negx
    {616, &JitPPC64::negx},     // negox
    {40, &JitPPC64::subfx},     // subfx
    {552, &JitPPC64::subfx},    // subfox
    {8, &JitPPC64::subfx},      // subfcx
    {520, &JitPPC64::subfx},    // subfcox
    {136, &JitPPC64::subfex},   // subfex
    {648, &JitPPC64::subfex},   // subfeox
    {232, &JitPPC64::subfex},   // subfmex
    {744, &JitPPC64::subfex},   // subfmeox
    {200, &JitPPC64::subfzex},  // subfzex
    {712, &JitPPC64::subfzex},  // subfzeox

    {28, &JitPPC64::boolX},    // andx
    {60, &JitPPC64::boolX},    // andcx
    {444, &JitPPC64::boolX},   // orx
    {124, &JitPPC64::boolX},   // norx
    {316, &JitPPC64::boolX},   // xorx
    {412, &JitPPC64::boolX},   // orcx
    {476, &JitPPC64::boolX},   // nandx
    {284, &JitPPC64::boolX},   // eqvx
    {0, &JitPPC64::cmp},       // cmp
    {32, &JitPPC64::cmpl},     // cmpl
    {26, &JitPPC64::cntlzwx},  // cntlzwx
    {922, &JitPPC64::extsXx},  // extshx
    {954, &JitPPC64::extsXx},  // extsbx
    {536, &JitPPC64::srwx},    // srwx
    {792, &JitPPC64::srawx},   // srawx
    {824, &JitPPC64::srawix},  // srawix
    {24, &JitPPC64::slwx},     // slwx

    {54, &JitPPC64::dcbx},        // dcbst
    {86, &JitPPC64::dcbx},        // dcbf
    {246, &JitPPC64::DoNothing},  // dcbtst
    {278, &JitPPC64::DoNothing},  // dcbt
    {470, &JitPPC64::dcbx},       // dcbi
    {758, &JitPPC64::DoNothing},  // dcba
    {1014, &JitPPC64::dcbz},      // dcbz

    // load word
    {23, &JitPPC64::lXX},  // lwzx
    {55, &JitPPC64::lXX},  // lwzux

    // load halfword
    {279, &JitPPC64::lXX},  // lhzx
    {311, &JitPPC64::lXX},  // lhzux

    // load halfword signextend
    {343, &JitPPC64::lXX},  // lhax
    {375, &JitPPC64::lXX},  // lhaux

    // load byte
    {87, &JitPPC64::lXX},   // lbzx
    {119, &JitPPC64::lXX},  // lbzux

    // load byte reverse
    {534, &JitPPC64::lXX},  // lwbrx
    {790, &JitPPC64::lXX},  // lhbrx

    // Conditional load/store (Wii SMP)
    {150, &JitPPC64::InterpretInstruction},  // stwcxd
    {20, &JitPPC64::InterpretInstruction},   // lwarx

    // load string (interpret these)
    {533, &JitPPC64::FallBackToInterpreter},  // lswx
    {597, &JitPPC64::FallBackToInterpreter},  // lswi

    // store word
    {151, &JitPPC64::stX},  // stwx
    {183, &JitPPC64::stX},  // stwux

    // store halfword
    {407, &JitPPC64::stX},  // sthx
    {439, &JitPPC64::stX},  // sthux

    // store byte
    {215, &JitPPC64::stX},  // stbx
    {247, &JitPPC64::stX},  // stbux

    // store bytereverse
    {662, &JitPPC64::stX},  // stwbrx
    {918, &JitPPC64::stX},  // sthbrx

    {661, &JitPPC64::FallBackToInterpreter},  // stswx
    {725, &JitPPC64::FallBackToInterpreter},  // stswi

    // fp load/store
    {535, &JitPPC64::lfXX},  // lfsx
    {567, &JitPPC64::lfXX},  // lfsux
    {599, &JitPPC64::lfXX},  // lfdx
    {631, &JitPPC64::lfXX},  // lfdux

    {663, &JitPPC64::stfXX},  // stfsx
    {695, &JitPPC64::stfXX},  // stfsux
    {727, &JitPPC64::stfXX},  // stfdx
    {759, &JitPPC64::stfXX},  // stfdux
    {983, &JitPPC64::stfXX},  // stfiwx

    {19, &JitPPC64::mfcr},                   // mfcr
    {83, &JitPPC64::mfmsr},                  // mfmsr
    {144, &JitPPC64::mtcrf},                 // mtcrf
    {146, &JitPPC64::mtmsr},                 // mtmsr
    {210, &JitPPC64::InterpretInstruction},  // mtsr
    {242, &JitPPC64::InterpretInstruction},  // mtsrin
    {339, &JitPPC64::mfspr},                 // mfspr
    {467, &JitPPC64::mtspr},                 // mtspr
    {371, &JitPPC64::mfspr},                 // mftb
    {512, &JitPPC64::mcrxr},                 // mcrxr
    {595, &JitPPC64::mfsr},                  // mfsr
    {659, &JitPPC64::mfsr},                  // mfsrin

    {4, &JitPPC64::twx},                      // tw
    {598, &JitPPC64::DoNothing},              // sync
    {982, &JitPPC64::FallBackToInterpreter},  // icbi

    // Unused instructions on GC
    {310, &JitPPC64::InterpretInstruction},  // eciwx
    {438, &JitPPC64::InterpretInstruction},  // ecowx
    {854, &JitPPC64::DoNothing},             // eieio
    {306, &JitPPC64::InterpretInstruction},  // tlbie
    {566, &JitPPC64::DoNothing},             // tlbsync
}};

constexpr std::array<JitPPC64OpTemplate, 9> s_table59{{
    {18, &JitPPC64::fp_arith},  // fdivsx
    {20, &JitPPC64::fp_arith},  // fsubsx
    {21, &JitPPC64::fp_arith},  // faddsx
    {24, &JitPPC64::fresx},     // fresx
    {25, &JitPPC64::fp_arith},  // fmulsx
    {28, &JitPPC64::fp_arith},  // fmsubsx
    {29, &JitPPC64::fp_arith},  // fmaddsx
    {30, &JitPPC64::fp_arith},  // fnmsubsx
    {31, &JitPPC64::fp_arith},  // fnmaddsx
}};

constexpr std::array<JitPPC64OpTemplate, 15> s_table63{{
    {264, &JitPPC64::fp_logic},  // fabsx
    {32, &JitPPC64::fcmpX},      // fcmpo
    {0, &JitPPC64::fcmpX},       // fcmpu
    {14, &JitPPC64::fctiwx},     // fctiwx
    {15, &JitPPC64::fctiwx},     // fctiwzx
    {72, &JitPPC64::fp_logic},   // fmrx
    {136, &JitPPC64::fp_logic},  // fnabsx
    {40, &JitPPC64::fp_logic},   // fnegx
    {12, &JitPPC64::frspx},      // frspx

    {64, &JitPPC64::mcrfs},     // mcrfs
    {583, &JitPPC64::mffsx},    // mffsx
    {70, &JitPPC64::mtfsb0x},   // mtfsb0x
    {38, &JitPPC64::mtfsb1x},   // mtfsb1x
    {134, &JitPPC64::mtfsfix},  // mtfsfix
    {711, &JitPPC64::mtfsfx},   // mtfsfx
}};

constexpr std::array<JitPPC64OpTemplate, 10> s_table63_2{{
    {18, &JitPPC64::fp_arith},  // fdivx
    {20, &JitPPC64::fp_arith},  // fsubx
    {21, &JitPPC64::fp_arith},  // faddx
    {23, &JitPPC64::fselx},     // fselx
    {25, &JitPPC64::fp_arith},  // fmulx
    {26, &JitPPC64::frsqrtex},  // frsqrtex
    {28, &JitPPC64::fp_arith},  // fmsubx
    {29, &JitPPC64::fp_arith},  // fmaddx
    {30, &JitPPC64::fp_arith},  // fnmsubx
    {31, &JitPPC64::fp_arith},  // fnmaddx
}};

constexpr std::array<JitPPC64::Instruction, 64> s_dyna_op_table = []() consteval {
  std::array<JitPPC64::Instruction, 64> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (auto& tpl : s_primary_table)
  {
    ASSERT(table[tpl.opcode] == &JitPPC64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<JitPPC64::Instruction, 1024> s_dyna_op_table4 = []() consteval {
  std::array<JitPPC64::Instruction, 1024> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table4_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitPPC64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (u32 i = 0; i < 16; i++)
  {
    const u32 fill = i << 6;
    for (const auto& tpl : s_table4_3)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitPPC64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(table[op] == &JitPPC64::FallBackToInterpreter);
    table[op] = tpl.fn;
  }

  return table;
}();

constexpr std::array<JitPPC64::Instruction, 1024> s_dyna_op_table19 = []() consteval {
  std::array<JitPPC64::Instruction, 1024> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (const auto& tpl : s_table19)
  {
    ASSERT(table[tpl.opcode] == &JitPPC64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<JitPPC64::Instruction, 1024> s_dyna_op_table31 = []() consteval {
  std::array<JitPPC64::Instruction, 1024> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (const auto& tpl : s_table31)
  {
    ASSERT(table[tpl.opcode] == &JitPPC64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<JitPPC64::Instruction, 32> s_dyna_op_table59 = []() consteval {
  std::array<JitPPC64::Instruction, 32> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (const auto& tpl : s_table59)
  {
    ASSERT(table[tpl.opcode] == &JitPPC64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}();

constexpr std::array<JitPPC64::Instruction, 1024> s_dyna_op_table63 = []() consteval {
  std::array<JitPPC64::Instruction, 1024> table{};
  table.fill(&JitPPC64::FallBackToInterpreter);

  for (const auto& tpl : s_table63)
  {
    ASSERT(table[tpl.opcode] == &JitPPC64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table63_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitPPC64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  return table;
}();

}  // Anonymous namespace

void JitPPC64::DynaRunTable4(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table4[inst.SUBOP10])(inst);
}

void JitPPC64::DynaRunTable19(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table19[inst.SUBOP10])(inst);
}

void JitPPC64::DynaRunTable31(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table31[inst.SUBOP10])(inst);
}

void JitPPC64::DynaRunTable59(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table59[inst.SUBOP5])(inst);
}

void JitPPC64::DynaRunTable63(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table63[inst.SUBOP10])(inst);
}

void JitPPC64::CompileInstruction(PPCAnalyst::CodeOp& op)
{
  (this->*s_dyna_op_table[op.inst.OPCD])(op.inst);

  PPCTables::CountInstructionCompile(op.opinfo, js.compilerPC);
}
