// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <array>

#include "Common/Assert.h"
#include "Common/TypeUtils.h"
#include "Core/PowerPC/Gekko.h"

namespace
{
struct JitArm64OpTemplate
{
  u32 opcode;
  JitArm64::Instruction fn;
};

constexpr std::array<JitArm64OpTemplate, 54> s_primary_table{{
    {4, &JitArm64::DynaRunTable4},    // RunTable4
    {19, &JitArm64::DynaRunTable19},  // RunTable19
    {31, &JitArm64::DynaRunTable31},  // RunTable31
    {59, &JitArm64::DynaRunTable59},  // RunTable59
    {63, &JitArm64::DynaRunTable63},  // RunTable63

    {16, &JitArm64::bcx},  // bcx
    {18, &JitArm64::bx},   // bx

    {3, &JitArm64::twx},  // twi
    {17, &JitArm64::sc},  // sc

    {7, &JitArm64::mulli},   // mulli
    {8, &JitArm64::subfic},  // subfic
    {10, &JitArm64::cmpli},  // cmpli
    {11, &JitArm64::cmpi},   // cmpi
    {12, &JitArm64::addic},  // addic
    {13, &JitArm64::addic},  // addic_rc
    {14, &JitArm64::addix},  // addi
    {15, &JitArm64::addix},  // addis

    {20, &JitArm64::rlwimix},  // rlwimix
    {21, &JitArm64::rlwinmx},  // rlwinmx
    {23, &JitArm64::rlwnmx},   // rlwnmx

    {24, &JitArm64::arith_imm},  // ori
    {25, &JitArm64::arith_imm},  // oris
    {26, &JitArm64::arith_imm},  // xori
    {27, &JitArm64::arith_imm},  // xoris
    {28, &JitArm64::arith_imm},  // andi_rc
    {29, &JitArm64::arith_imm},  // andis_rc

    {32, &JitArm64::lXX},  // lwz
    {33, &JitArm64::lXX},  // lwzu
    {34, &JitArm64::lXX},  // lbz
    {35, &JitArm64::lXX},  // lbzu
    {40, &JitArm64::lXX},  // lhz
    {41, &JitArm64::lXX},  // lhzu
    {42, &JitArm64::lXX},  // lha
    {43, &JitArm64::lXX},  // lhau

    {44, &JitArm64::stX},  // sth
    {45, &JitArm64::stX},  // sthu
    {36, &JitArm64::stX},  // stw
    {37, &JitArm64::stX},  // stwu
    {38, &JitArm64::stX},  // stb
    {39, &JitArm64::stX},  // stbu

    {46, &JitArm64::lmw},   // lmw
    {47, &JitArm64::stmw},  // stmw

    {48, &JitArm64::lfXX},  // lfs
    {49, &JitArm64::lfXX},  // lfsu
    {50, &JitArm64::lfXX},  // lfd
    {51, &JitArm64::lfXX},  // lfdu

    {52, &JitArm64::stfXX},  // stfs
    {53, &JitArm64::stfXX},  // stfsu
    {54, &JitArm64::stfXX},  // stfd
    {55, &JitArm64::stfXX},  // stfdu

    {56, &JitArm64::psq_lXX},   // psq_l
    {57, &JitArm64::psq_lXX},   // psq_lu
    {60, &JitArm64::psq_stXX},  // psq_st
    {61, &JitArm64::psq_stXX},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 58, 62
}};

constexpr std::array<JitArm64OpTemplate, 13> s_table4{{
    // SUBOP10
    {0, &JitArm64::ps_cmpXX},      // ps_cmpu0
    {32, &JitArm64::ps_cmpXX},     // ps_cmpo0
    {40, &JitArm64::fp_logic},     // ps_neg
    {136, &JitArm64::fp_logic},    // ps_nabs
    {264, &JitArm64::fp_logic},    // ps_abs
    {64, &JitArm64::ps_cmpXX},     // ps_cmpu1
    {72, &JitArm64::fp_logic},     // ps_mr
    {96, &JitArm64::ps_cmpXX},     // ps_cmpo1
    {528, &JitArm64::ps_mergeXX},  // ps_merge00
    {560, &JitArm64::ps_mergeXX},  // ps_merge01
    {592, &JitArm64::ps_mergeXX},  // ps_merge10
    {624, &JitArm64::ps_mergeXX},  // ps_merge11

    {1014, &JitArm64::FallBackToInterpreter},  // dcbz_l
}};

constexpr std::array<JitArm64OpTemplate, 17> s_table4_2{{
    {10, &JitArm64::ps_sumX},    // ps_sum0
    {11, &JitArm64::ps_sumX},    // ps_sum1
    {12, &JitArm64::ps_arith},   // ps_muls0
    {13, &JitArm64::ps_arith},   // ps_muls1
    {14, &JitArm64::ps_arith},   // ps_madds0
    {15, &JitArm64::ps_arith},   // ps_madds1
    {18, &JitArm64::ps_arith},   // ps_div
    {20, &JitArm64::ps_arith},   // ps_sub
    {21, &JitArm64::ps_arith},   // ps_add
    {23, &JitArm64::ps_sel},     // ps_sel
    {24, &JitArm64::ps_res},     // ps_res
    {25, &JitArm64::ps_arith},   // ps_mul
    {26, &JitArm64::ps_rsqrte},  // ps_rsqrte
    {28, &JitArm64::ps_arith},   // ps_msub
    {29, &JitArm64::ps_arith},   // ps_madd
    {30, &JitArm64::ps_arith},   // ps_nmsub
    {31, &JitArm64::ps_arith},   // ps_nmadd
}};

constexpr std::array<JitArm64OpTemplate, 4> s_table4_3{{
    {6, &JitArm64::psq_lXX},    // psq_lx
    {7, &JitArm64::psq_stXX},   // psq_stx
    {38, &JitArm64::psq_lXX},   // psq_lux
    {39, &JitArm64::psq_stXX},  // psq_stux
}};

constexpr std::array<JitArm64OpTemplate, 13> s_table19{{
    {528, &JitArm64::bcctrx},  // bcctrx
    {16, &JitArm64::bclrx},    // bclrx
    {257, &JitArm64::crXXX},   // crand
    {129, &JitArm64::crXXX},   // crandc
    {289, &JitArm64::crXXX},   // creqv
    {225, &JitArm64::crXXX},   // crnand
    {33, &JitArm64::crXXX},    // crnor
    {449, &JitArm64::crXXX},   // cror
    {417, &JitArm64::crXXX},   // crorc
    {193, &JitArm64::crXXX},   // crxor

    {150, &JitArm64::DoNothing},  // isync
    {0, &JitArm64::mcrf},         // mcrf

    {50, &JitArm64::rfi},  // rfi
}};

constexpr std::array<JitArm64OpTemplate, 107> s_table31{{
    {266, &JitArm64::addx},     // addx
    {778, &JitArm64::addx},     // addox
    {10, &JitArm64::addcx},     // addcx
    {522, &JitArm64::addcx},    // addcox
    {138, &JitArm64::addex},    // addex
    {650, &JitArm64::addex},    // addeox
    {234, &JitArm64::addex},    // addmex
    {746, &JitArm64::addex},    // addmeox
    {202, &JitArm64::addzex},   // addzex
    {714, &JitArm64::addzex},   // addzeox
    {491, &JitArm64::divwx},    // divwx
    {1003, &JitArm64::divwx},   // divwox
    {459, &JitArm64::divwux},   // divwux
    {971, &JitArm64::divwux},   // divwuox
    {75, &JitArm64::mulhwx},    // mulhwx
    {11, &JitArm64::mulhwux},   // mulhwux
    {235, &JitArm64::mullwx},   // mullwx
    {747, &JitArm64::mullwx},   // mullwox
    {104, &JitArm64::negx},     // negx
    {616, &JitArm64::negx},     // negox
    {40, &JitArm64::subfx},     // subfx
    {552, &JitArm64::subfx},    // subfox
    {8, &JitArm64::subfcx},     // subfcx
    {520, &JitArm64::subfcx},   // subfcox
    {136, &JitArm64::subfex},   // subfex
    {648, &JitArm64::subfex},   // subfeox
    {232, &JitArm64::subfex},   // subfmex
    {744, &JitArm64::subfex},   // subfmeox
    {200, &JitArm64::subfzex},  // subfzex
    {712, &JitArm64::subfzex},  // subfzeox

    {28, &JitArm64::boolX},    // andx
    {60, &JitArm64::boolX},    // andcx
    {444, &JitArm64::boolX},   // orx
    {124, &JitArm64::boolX},   // norx
    {316, &JitArm64::boolX},   // xorx
    {412, &JitArm64::boolX},   // orcx
    {476, &JitArm64::boolX},   // nandx
    {284, &JitArm64::boolX},   // eqvx
    {0, &JitArm64::cmp},       // cmp
    {32, &JitArm64::cmpl},     // cmpl
    {26, &JitArm64::cntlzwx},  // cntlzwx
    {922, &JitArm64::extsXx},  // extshx
    {954, &JitArm64::extsXx},  // extsbx
    {536, &JitArm64::srwx},    // srwx
    {792, &JitArm64::srawx},   // srawx
    {824, &JitArm64::srawix},  // srawix
    {24, &JitArm64::slwx},     // slwx

    {54, &JitArm64::dcbx},        // dcbst
    {86, &JitArm64::dcbx},        // dcbf
    {246, &JitArm64::dcbt},       // dcbtst
    {278, &JitArm64::dcbt},       // dcbt
    {470, &JitArm64::dcbx},       // dcbi
    {758, &JitArm64::DoNothing},  // dcba
    {1014, &JitArm64::dcbz},      // dcbz

    // load word
    {23, &JitArm64::lXX},  // lwzx
    {55, &JitArm64::lXX},  // lwzux

    // load halfword
    {279, &JitArm64::lXX},  // lhzx
    {311, &JitArm64::lXX},  // lhzux

    // load halfword signextend
    {343, &JitArm64::lXX},  // lhax
    {375, &JitArm64::lXX},  // lhaux

    // load byte
    {87, &JitArm64::lXX},   // lbzx
    {119, &JitArm64::lXX},  // lbzux

    // load byte reverse
    {534, &JitArm64::lXX},  // lwbrx
    {790, &JitArm64::lXX},  // lhbrx

    // Conditional load/store (Wii SMP)
    {150, &JitArm64::FallBackToInterpreter},  // stwcxd
    {20, &JitArm64::FallBackToInterpreter},   // lwarx

    // load string (interpret these)
    {533, &JitArm64::FallBackToInterpreter},  // lswx
    {597, &JitArm64::FallBackToInterpreter},  // lswi

    // store word
    {151, &JitArm64::stX},  // stwx
    {183, &JitArm64::stX},  // stwux

    // store halfword
    {407, &JitArm64::stX},  // sthx
    {439, &JitArm64::stX},  // sthux

    // store byte
    {215, &JitArm64::stX},  // stbx
    {247, &JitArm64::stX},  // stbux

    // store bytereverse
    {662, &JitArm64::stX},  // stwbrx
    {918, &JitArm64::stX},  // sthbrx

    {661, &JitArm64::FallBackToInterpreter},  // stswx
    {725, &JitArm64::FallBackToInterpreter},  // stswi

    // fp load/store
    {535, &JitArm64::lfXX},  // lfsx
    {567, &JitArm64::lfXX},  // lfsux
    {599, &JitArm64::lfXX},  // lfdx
    {631, &JitArm64::lfXX},  // lfdux

    {663, &JitArm64::stfXX},  // stfsx
    {695, &JitArm64::stfXX},  // stfsux
    {727, &JitArm64::stfXX},  // stfdx
    {759, &JitArm64::stfXX},  // stfdux
    {983, &JitArm64::stfXX},  // stfiwx

    {19, &JitArm64::mfcr},     // mfcr
    {83, &JitArm64::mfmsr},    // mfmsr
    {144, &JitArm64::mtcrf},   // mtcrf
    {146, &JitArm64::mtmsr},   // mtmsr
    {210, &JitArm64::mtsr},    // mtsr
    {242, &JitArm64::mtsrin},  // mtsrin
    {339, &JitArm64::mfspr},   // mfspr
    {467, &JitArm64::mtspr},   // mtspr
    {371, &JitArm64::mftb},    // mftb
    {512, &JitArm64::mcrxr},   // mcrxr
    {595, &JitArm64::mfsr},    // mfsr
    {659, &JitArm64::mfsrin},  // mfsrin

    {4, &JitArm64::twx},                      // tw
    {598, &JitArm64::DoNothing},              // sync
    {982, &JitArm64::FallBackToInterpreter},  // icbi

    // Unused instructions on GC
    {310, &JitArm64::FallBackToInterpreter},  // eciwx
    {438, &JitArm64::FallBackToInterpreter},  // ecowx
    {854, &JitArm64::eieio},                  // eieio
    {306, &JitArm64::FallBackToInterpreter},  // tlbie
    {566, &JitArm64::DoNothing},              // tlbsync
}};

constexpr std::array<JitArm64OpTemplate, 9> s_table59{{
    {18, &JitArm64::fp_arith},  // fdivsx
    {20, &JitArm64::fp_arith},  // fsubsx
    {21, &JitArm64::fp_arith},  // faddsx
    {24, &JitArm64::fresx},     // fresx
    {25, &JitArm64::fp_arith},  // fmulsx
    {28, &JitArm64::fp_arith},  // fmsubsx
    {29, &JitArm64::fp_arith},  // fmaddsx
    {30, &JitArm64::fp_arith},  // fnmsubsx
    {31, &JitArm64::fp_arith},  // fnmaddsx
}};

constexpr std::array<JitArm64OpTemplate, 15> s_table63{{
    {264, &JitArm64::fp_logic},  // fabsx
    {32, &JitArm64::fcmpX},      // fcmpo
    {0, &JitArm64::fcmpX},       // fcmpu
    {14, &JitArm64::fctiwx},     // fctiwx
    {15, &JitArm64::fctiwx},     // fctiwzx
    {72, &JitArm64::fp_logic},   // fmrx
    {136, &JitArm64::fp_logic},  // fnabsx
    {40, &JitArm64::fp_logic},   // fnegx
    {12, &JitArm64::frspx},      // frspx

    {64, &JitArm64::mcrfs},     // mcrfs
    {583, &JitArm64::mffsx},    // mffsx
    {70, &JitArm64::mtfsb0x},   // mtfsb0x
    {38, &JitArm64::mtfsb1x},   // mtfsb1x
    {134, &JitArm64::mtfsfix},  // mtfsfix
    {711, &JitArm64::mtfsfx},   // mtfsfx
}};

constexpr std::array<JitArm64OpTemplate, 10> s_table63_2{{
    {18, &JitArm64::fp_arith},  // fdivx
    {20, &JitArm64::fp_arith},  // fsubx
    {21, &JitArm64::fp_arith},  // faddx
    {23, &JitArm64::fselx},     // fselx
    {25, &JitArm64::fp_arith},  // fmulx
    {26, &JitArm64::frsqrtex},  // frsqrtex
    {28, &JitArm64::fp_arith},  // fmsubx
    {29, &JitArm64::fp_arith},  // fmaddx
    {30, &JitArm64::fp_arith},  // fnmsubx
    {31, &JitArm64::fp_arith},  // fnmaddx
}};

constexpr std::array<JitArm64::Instruction, 64> s_dyna_op_table = []() consteval
{
  std::array<JitArm64::Instruction, 64> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (auto& tpl : s_primary_table)
  {
    ASSERT(table[tpl.opcode] == &JitArm64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<JitArm64::Instruction, 1024> s_dyna_op_table4 = []() consteval
{
  std::array<JitArm64::Instruction, 1024> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table4_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitArm64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (u32 i = 0; i < 16; i++)
  {
    const u32 fill = i << 6;
    for (const auto& tpl : s_table4_3)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitArm64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(table[op] == &JitArm64::FallBackToInterpreter);
    table[op] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<JitArm64::Instruction, 1024> s_dyna_op_table19 = []() consteval
{
  std::array<JitArm64::Instruction, 1024> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (const auto& tpl : s_table19)
  {
    ASSERT(table[tpl.opcode] == &JitArm64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<JitArm64::Instruction, 1024> s_dyna_op_table31 = []() consteval
{
  std::array<JitArm64::Instruction, 1024> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (const auto& tpl : s_table31)
  {
    ASSERT(table[tpl.opcode] == &JitArm64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<JitArm64::Instruction, 32> s_dyna_op_table59 = []() consteval
{
  std::array<JitArm64::Instruction, 32> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (const auto& tpl : s_table59)
  {
    ASSERT(table[tpl.opcode] == &JitArm64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<JitArm64::Instruction, 1024> s_dyna_op_table63 = []() consteval
{
  std::array<JitArm64::Instruction, 1024> table{};
  Common::Fill(table, &JitArm64::FallBackToInterpreter);

  for (const auto& tpl : s_table63)
  {
    ASSERT(table[tpl.opcode] == &JitArm64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table63_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &JitArm64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  return table;
}
();

}  // Anonymous namespace

void JitArm64::DynaRunTable4(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table4[inst.SUBOP10])(inst);
}

void JitArm64::DynaRunTable19(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table19[inst.SUBOP10])(inst);
}

void JitArm64::DynaRunTable31(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table31[inst.SUBOP10])(inst);
}

void JitArm64::DynaRunTable59(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table59[inst.SUBOP5])(inst);
}

void JitArm64::DynaRunTable63(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table63[inst.SUBOP10])(inst);
}

void JitArm64::CompileInstruction(PPCAnalyst::CodeOp& op)
{
  (this->*s_dyna_op_table[op.inst.OPCD])(op.inst);

  PPCTables::CountInstructionCompile(op.opinfo, js.compilerPC);
}
