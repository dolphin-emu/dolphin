// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include <array>

#include "Common/Assert.h"
#include "Common/TypeUtils.h"
#include "Core/PowerPC/Gekko.h"

namespace
{
struct Jit64OpTemplate
{
  u32 opcode;
  Jit64::Instruction fn;
};

constexpr std::array<Jit64OpTemplate, 54> s_primary_table{{
    {4, &Jit64::DynaRunTable4},    // RunTable4
    {19, &Jit64::DynaRunTable19},  // RunTable19
    {31, &Jit64::DynaRunTable31},  // RunTable31
    {59, &Jit64::DynaRunTable59},  // RunTable59
    {63, &Jit64::DynaRunTable63},  // RunTable63

    {16, &Jit64::bcx},  // bcx
    {18, &Jit64::bx},   // bx

    {3, &Jit64::twX},  // twi
    {17, &Jit64::sc},  // sc

    {7, &Jit64::mulli},     // mulli
    {8, &Jit64::subfic},    // subfic
    {10, &Jit64::cmpXX},    // cmpli
    {11, &Jit64::cmpXX},    // cmpi
    {12, &Jit64::reg_imm},  // addic
    {13, &Jit64::reg_imm},  // addic_rc
    {14, &Jit64::reg_imm},  // addi
    {15, &Jit64::reg_imm},  // addis

    {20, &Jit64::rlwimix},  // rlwimix
    {21, &Jit64::rlwinmx},  // rlwinmx
    {23, &Jit64::rlwnmx},   // rlwnmx

    {24, &Jit64::reg_imm},  // ori
    {25, &Jit64::reg_imm},  // oris
    {26, &Jit64::reg_imm},  // xori
    {27, &Jit64::reg_imm},  // xoris
    {28, &Jit64::reg_imm},  // andi_rc
    {29, &Jit64::reg_imm},  // andis_rc

    {32, &Jit64::lXXx},  // lwz
    {33, &Jit64::lXXx},  // lwzu
    {34, &Jit64::lXXx},  // lbz
    {35, &Jit64::lXXx},  // lbzu
    {40, &Jit64::lXXx},  // lhz
    {41, &Jit64::lXXx},  // lhzu
    {42, &Jit64::lXXx},  // lha
    {43, &Jit64::lXXx},  // lhau

    {44, &Jit64::stX},  // sth
    {45, &Jit64::stX},  // sthu
    {36, &Jit64::stX},  // stw
    {37, &Jit64::stX},  // stwu
    {38, &Jit64::stX},  // stb
    {39, &Jit64::stX},  // stbu

    {46, &Jit64::lmw},   // lmw
    {47, &Jit64::stmw},  // stmw

    {48, &Jit64::lfXXX},  // lfs
    {49, &Jit64::lfXXX},  // lfsu
    {50, &Jit64::lfXXX},  // lfd
    {51, &Jit64::lfXXX},  // lfdu

    {52, &Jit64::stfXXX},  // stfs
    {53, &Jit64::stfXXX},  // stfsu
    {54, &Jit64::stfXXX},  // stfd
    {55, &Jit64::stfXXX},  // stfdu

    {56, &Jit64::psq_lXX},   // psq_l
    {57, &Jit64::psq_lXX},   // psq_lu
    {60, &Jit64::psq_stXX},  // psq_st
    {61, &Jit64::psq_stXX},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

constexpr std::array<Jit64OpTemplate, 13> s_table4{{
    // SUBOP10
    {0, &Jit64::ps_cmpXX},      // ps_cmpu0
    {32, &Jit64::ps_cmpXX},     // ps_cmpo0
    {40, &Jit64::fsign},        // ps_neg
    {136, &Jit64::fsign},       // ps_nabs
    {264, &Jit64::fsign},       // ps_abs
    {64, &Jit64::ps_cmpXX},     // ps_cmpu1
    {72, &Jit64::ps_mr},        // ps_mr
    {96, &Jit64::ps_cmpXX},     // ps_cmpo1
    {528, &Jit64::ps_mergeXX},  // ps_merge00
    {560, &Jit64::ps_mergeXX},  // ps_merge01
    {592, &Jit64::ps_mergeXX},  // ps_merge10
    {624, &Jit64::ps_mergeXX},  // ps_merge11

    {1014, &Jit64::FallBackToInterpreter},  // dcbz_l
}};

constexpr std::array<Jit64OpTemplate, 17> s_table4_2{{
    {10, &Jit64::ps_sum},     // ps_sum0
    {11, &Jit64::ps_sum},     // ps_sum1
    {12, &Jit64::ps_muls},    // ps_muls0
    {13, &Jit64::ps_muls},    // ps_muls1
    {14, &Jit64::fmaddXX},    // ps_madds0
    {15, &Jit64::fmaddXX},    // ps_madds1
    {18, &Jit64::fp_arith},   // ps_div
    {20, &Jit64::fp_arith},   // ps_sub
    {21, &Jit64::fp_arith},   // ps_add
    {23, &Jit64::fselx},      // ps_sel
    {24, &Jit64::ps_res},     // ps_res
    {25, &Jit64::fp_arith},   // ps_mul
    {26, &Jit64::ps_rsqrte},  // ps_rsqrte
    {28, &Jit64::fmaddXX},    // ps_msub
    {29, &Jit64::fmaddXX},    // ps_madd
    {30, &Jit64::fmaddXX},    // ps_nmsub
    {31, &Jit64::fmaddXX},    // ps_nmadd
}};

constexpr std::array<Jit64OpTemplate, 4> s_table4_3{{
    {6, &Jit64::psq_lXX},    // psq_lx
    {7, &Jit64::psq_stXX},   // psq_stx
    {38, &Jit64::psq_lXX},   // psq_lux
    {39, &Jit64::psq_stXX},  // psq_stux
}};

constexpr std::array<Jit64OpTemplate, 13> s_table19{{
    {528, &Jit64::bcctrx},  // bcctrx
    {16, &Jit64::bclrx},    // bclrx
    {257, &Jit64::crXXX},   // crand
    {129, &Jit64::crXXX},   // crandc
    {289, &Jit64::crXXX},   // creqv
    {225, &Jit64::crXXX},   // crnand
    {33, &Jit64::crXXX},    // crnor
    {449, &Jit64::crXXX},   // cror
    {417, &Jit64::crXXX},   // crorc
    {193, &Jit64::crXXX},   // crxor

    {150, &Jit64::DoNothing},  // isync
    {0, &Jit64::mcrf},         // mcrf

    {50, &Jit64::rfi},  // rfi
}};

constexpr std::array<Jit64OpTemplate, 107> s_table31{{
    {266, &Jit64::addx},      // addx
    {778, &Jit64::addx},      // addox
    {10, &Jit64::addx},       // addcx
    {522, &Jit64::addx},      // addcox
    {138, &Jit64::arithXex},  // addex
    {650, &Jit64::arithXex},  // addeox
    {234, &Jit64::arithXex},  // addmex
    {746, &Jit64::arithXex},  // addmeox
    {202, &Jit64::arithXex},  // addzex
    {714, &Jit64::arithXex},  // addzeox
    {491, &Jit64::divwx},     // divwx
    {1003, &Jit64::divwx},    // divwox
    {459, &Jit64::divwux},    // divwux
    {971, &Jit64::divwux},    // divwuox
    {75, &Jit64::mulhwXx},    // mulhwx
    {11, &Jit64::mulhwXx},    // mulhwux
    {235, &Jit64::mullwx},    // mullwx
    {747, &Jit64::mullwx},    // mullwox
    {104, &Jit64::negx},      // negx
    {616, &Jit64::negx},      // negox
    {40, &Jit64::subfx},      // subfx
    {552, &Jit64::subfx},     // subfox
    {8, &Jit64::subfx},       // subfcx
    {520, &Jit64::subfx},     // subfcox
    {136, &Jit64::arithXex},  // subfex
    {648, &Jit64::arithXex},  // subfeox
    {232, &Jit64::arithXex},  // subfmex
    {744, &Jit64::arithXex},  // subfmeox
    {200, &Jit64::arithXex},  // subfzex
    {712, &Jit64::arithXex},  // subfzeox

    {28, &Jit64::boolX},    // andx
    {60, &Jit64::boolX},    // andcx
    {444, &Jit64::boolX},   // orx
    {124, &Jit64::boolX},   // norx
    {316, &Jit64::boolX},   // xorx
    {412, &Jit64::boolX},   // orcx
    {476, &Jit64::boolX},   // nandx
    {284, &Jit64::boolX},   // eqvx
    {0, &Jit64::cmpXX},     // cmp
    {32, &Jit64::cmpXX},    // cmpl
    {26, &Jit64::cntlzwx},  // cntlzwx
    {922, &Jit64::extsXx},  // extshx
    {954, &Jit64::extsXx},  // extsbx
    {536, &Jit64::srwx},    // srwx
    {792, &Jit64::srawx},   // srawx
    {824, &Jit64::srawix},  // srawix
    {24, &Jit64::slwx},     // slwx

    {54, &Jit64::dcbx},        // dcbst
    {86, &Jit64::dcbx},        // dcbf
    {246, &Jit64::dcbt},       // dcbtst
    {278, &Jit64::dcbt},       // dcbt
    {470, &Jit64::dcbx},       // dcbi
    {758, &Jit64::DoNothing},  // dcba
    {1014, &Jit64::dcbz},      // dcbz

    // load word
    {23, &Jit64::lXXx},  // lwzx
    {55, &Jit64::lXXx},  // lwzux

    // load halfword
    {279, &Jit64::lXXx},  // lhzx
    {311, &Jit64::lXXx},  // lhzux

    // load halfword signextend
    {343, &Jit64::lXXx},  // lhax
    {375, &Jit64::lXXx},  // lhaux

    // load byte
    {87, &Jit64::lXXx},   // lbzx
    {119, &Jit64::lXXx},  // lbzux

    // load byte reverse
    {534, &Jit64::lXXx},  // lwbrx
    {790, &Jit64::lXXx},  // lhbrx

    // Conditional load/store (Wii SMP)
    {150, &Jit64::FallBackToInterpreter},  // stwcxd
    {20, &Jit64::FallBackToInterpreter},   // lwarx

    // load string (interpret these)
    {533, &Jit64::FallBackToInterpreter},  // lswx
    {597, &Jit64::FallBackToInterpreter},  // lswi

    // store word
    {151, &Jit64::stXx},  // stwx
    {183, &Jit64::stXx},  // stwux

    // store halfword
    {407, &Jit64::stXx},  // sthx
    {439, &Jit64::stXx},  // sthux

    // store byte
    {215, &Jit64::stXx},  // stbx
    {247, &Jit64::stXx},  // stbux

    // store bytereverse
    {662, &Jit64::stXx},  // stwbrx
    {918, &Jit64::stXx},  // sthbrx

    {661, &Jit64::FallBackToInterpreter},  // stswx
    {725, &Jit64::FallBackToInterpreter},  // stswi

    // fp load/store
    {535, &Jit64::lfXXX},  // lfsx
    {567, &Jit64::lfXXX},  // lfsux
    {599, &Jit64::lfXXX},  // lfdx
    {631, &Jit64::lfXXX},  // lfdux

    {663, &Jit64::stfXXX},  // stfsx
    {695, &Jit64::stfXXX},  // stfsux
    {727, &Jit64::stfXXX},  // stfdx
    {759, &Jit64::stfXXX},  // stfdux
    {983, &Jit64::stfiwx},  // stfiwx

    {19, &Jit64::mfcr},                    // mfcr
    {83, &Jit64::mfmsr},                   // mfmsr
    {144, &Jit64::mtcrf},                  // mtcrf
    {146, &Jit64::mtmsr},                  // mtmsr
    {210, &Jit64::FallBackToInterpreter},  // mtsr
    {242, &Jit64::FallBackToInterpreter},  // mtsrin
    {339, &Jit64::mfspr},                  // mfspr
    {467, &Jit64::mtspr},                  // mtspr
    {371, &Jit64::mftb},                   // mftb
    {512, &Jit64::mcrxr},                  // mcrxr
    {595, &Jit64::FallBackToInterpreter},  // mfsr
    {659, &Jit64::FallBackToInterpreter},  // mfsrin

    {4, &Jit64::twX},                      // tw
    {598, &Jit64::DoNothing},              // sync
    {982, &Jit64::FallBackToInterpreter},  // icbi

    // Unused instructions on GC
    {310, &Jit64::FallBackToInterpreter},  // eciwx
    {438, &Jit64::FallBackToInterpreter},  // ecowx
    {854, &Jit64::eieio},                  // eieio
    {306, &Jit64::FallBackToInterpreter},  // tlbie
    {566, &Jit64::DoNothing},              // tlbsync
}};

constexpr std::array<Jit64OpTemplate, 9> s_table59{{
    {18, &Jit64::fp_arith},  // fdivsx
    {20, &Jit64::fp_arith},  // fsubsx
    {21, &Jit64::fp_arith},  // faddsx
    {24, &Jit64::fresx},     // fresx
    {25, &Jit64::fp_arith},  // fmulsx
    {28, &Jit64::fmaddXX},   // fmsubsx
    {29, &Jit64::fmaddXX},   // fmaddsx
    {30, &Jit64::fmaddXX},   // fnmsubsx
    {31, &Jit64::fmaddXX},   // fnmaddsx
}};

constexpr std::array<Jit64OpTemplate, 15> s_table63{{
    {264, &Jit64::fsign},  // fabsx
    {32, &Jit64::fcmpX},   // fcmpo
    {0, &Jit64::fcmpX},    // fcmpu
    {14, &Jit64::fctiwx},  // fctiwx
    {15, &Jit64::fctiwx},  // fctiwzx
    {72, &Jit64::fmrx},    // fmrx
    {136, &Jit64::fsign},  // fnabsx
    {40, &Jit64::fsign},   // fnegx
    {12, &Jit64::frspx},   // frspx

    {64, &Jit64::mcrfs},     // mcrfs
    {583, &Jit64::mffsx},    // mffsx
    {70, &Jit64::mtfsb0x},   // mtfsb0x
    {38, &Jit64::mtfsb1x},   // mtfsb1x
    {134, &Jit64::mtfsfix},  // mtfsfix
    {711, &Jit64::mtfsfx},   // mtfsfx
}};

constexpr std::array<Jit64OpTemplate, 10> s_table63_2{{
    {18, &Jit64::fp_arith},  // fdivx
    {20, &Jit64::fp_arith},  // fsubx
    {21, &Jit64::fp_arith},  // faddx
    {23, &Jit64::fselx},     // fselx
    {25, &Jit64::fp_arith},  // fmulx
    {26, &Jit64::frsqrtex},  // frsqrtex
    {28, &Jit64::fmaddXX},   // fmsubx
    {29, &Jit64::fmaddXX},   // fmaddx
    {30, &Jit64::fmaddXX},   // fnmsubx
    {31, &Jit64::fmaddXX},   // fnmaddx
}};

constexpr std::array<Jit64::Instruction, 64> s_dyna_op_table = []() consteval
{
  std::array<Jit64::Instruction, 64> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (auto& tpl : s_primary_table)
  {
    ASSERT(table[tpl.opcode] == &Jit64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<Jit64::Instruction, 1024> s_dyna_op_table4 = []() consteval
{
  std::array<Jit64::Instruction, 1024> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table4_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &Jit64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (u32 i = 0; i < 16; i++)
  {
    const u32 fill = i << 6;
    for (const auto& tpl : s_table4_3)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &Jit64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(table[op] == &Jit64::FallBackToInterpreter);
    table[op] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<Jit64::Instruction, 1024> s_dyna_op_table19 = []() consteval
{
  std::array<Jit64::Instruction, 1024> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (const auto& tpl : s_table19)
  {
    ASSERT(table[tpl.opcode] == &Jit64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<Jit64::Instruction, 1024> s_dyna_op_table31 = []() consteval
{
  std::array<Jit64::Instruction, 1024> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (const auto& tpl : s_table31)
  {
    ASSERT(table[tpl.opcode] == &Jit64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<Jit64::Instruction, 32> s_dyna_op_table59 = []() consteval
{
  std::array<Jit64::Instruction, 32> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (const auto& tpl : s_table59)
  {
    ASSERT(table[tpl.opcode] == &Jit64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  return table;
}
();

constexpr std::array<Jit64::Instruction, 1024> s_dyna_op_table63 = []() consteval
{
  std::array<Jit64::Instruction, 1024> table{};
  Common::Fill(table, &Jit64::FallBackToInterpreter);

  for (const auto& tpl : s_table63)
  {
    ASSERT(table[tpl.opcode] == &Jit64::FallBackToInterpreter);
    table[tpl.opcode] = tpl.fn;
  }

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table63_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == &Jit64::FallBackToInterpreter);
      table[op] = tpl.fn;
    }
  }

  return table;
}
();

}  // Anonymous namespace

void Jit64::DynaRunTable4(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table4[inst.SUBOP10])(inst);
}

void Jit64::DynaRunTable19(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table19[inst.SUBOP10])(inst);
}

void Jit64::DynaRunTable31(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table31[inst.SUBOP10])(inst);
}

void Jit64::DynaRunTable59(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table59[inst.SUBOP5])(inst);
}

void Jit64::DynaRunTable63(UGeckoInstruction inst)
{
  (this->*s_dyna_op_table63[inst.SUBOP10])(inst);
}

void Jit64::CompileInstruction(PPCAnalyst::CodeOp& op)
{
  (this->*s_dyna_op_table[op.inst.OPCD])(op.inst);

  PPCTables::CountInstructionCompile(op.opinfo, js.compilerPC);
}
