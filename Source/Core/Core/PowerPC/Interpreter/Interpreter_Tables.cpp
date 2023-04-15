// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <array>

#include "Common/Assert.h"
#include "Common/TypeUtils.h"
#include "Core/PowerPC/Gekko.h"

namespace
{
struct InterpreterOpTemplate
{
  u32 opcode;
  Interpreter::Instruction fn;
};
}  // namespace

constexpr std::array<InterpreterOpTemplate, 54> s_primary_table{{
    {4, Interpreter::RunTable4},    // RunTable4
    {19, Interpreter::RunTable19},  // RunTable19
    {31, Interpreter::RunTable31},  // RunTable31
    {59, Interpreter::RunTable59},  // RunTable59
    {63, Interpreter::RunTable63},  // RunTable63

    {16, Interpreter::bcx},  // bcx
    {18, Interpreter::bx},   // bx

    {3, Interpreter::twi},  // twi
    {17, Interpreter::sc},  // sc

    {7, Interpreter::mulli},      // mulli
    {8, Interpreter::subfic},     // subfic
    {10, Interpreter::cmpli},     // cmpli
    {11, Interpreter::cmpi},      // cmpi
    {12, Interpreter::addic},     // addic
    {13, Interpreter::addic_rc},  // addic_rc
    {14, Interpreter::addi},      // addi
    {15, Interpreter::addis},     // addis

    {20, Interpreter::rlwimix},  // rlwimix
    {21, Interpreter::rlwinmx},  // rlwinmx
    {23, Interpreter::rlwnmx},   // rlwnmx

    {24, Interpreter::ori},       // ori
    {25, Interpreter::oris},      // oris
    {26, Interpreter::xori},      // xori
    {27, Interpreter::xoris},     // xoris
    {28, Interpreter::andi_rc},   // andi_rc
    {29, Interpreter::andis_rc},  // andis_rc

    {32, Interpreter::lwz},   // lwz
    {33, Interpreter::lwzu},  // lwzu
    {34, Interpreter::lbz},   // lbz
    {35, Interpreter::lbzu},  // lbzu
    {40, Interpreter::lhz},   // lhz
    {41, Interpreter::lhzu},  // lhzu

    {42, Interpreter::lha},   // lha
    {43, Interpreter::lhau},  // lhau

    {44, Interpreter::sth},   // sth
    {45, Interpreter::sthu},  // sthu
    {36, Interpreter::stw},   // stw
    {37, Interpreter::stwu},  // stwu
    {38, Interpreter::stb},   // stb
    {39, Interpreter::stbu},  // stbu

    {46, Interpreter::lmw},   // lmw
    {47, Interpreter::stmw},  // stmw

    {48, Interpreter::lfs},   // lfs
    {49, Interpreter::lfsu},  // lfsu
    {50, Interpreter::lfd},   // lfd
    {51, Interpreter::lfdu},  // lfdu

    {52, Interpreter::stfs},   // stfs
    {53, Interpreter::stfsu},  // stfsu
    {54, Interpreter::stfd},   // stfd
    {55, Interpreter::stfdu},  // stfdu

    {56, Interpreter::psq_l},    // psq_l
    {57, Interpreter::psq_lu},   // psq_lu
    {60, Interpreter::psq_st},   // psq_st
    {61, Interpreter::psq_stu},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
}};

constexpr std::array<InterpreterOpTemplate, 13> s_table4{{
    // SUBOP10
    {0, Interpreter::ps_cmpu0},      // ps_cmpu0
    {32, Interpreter::ps_cmpo0},     // ps_cmpo0
    {40, Interpreter::ps_neg},       // ps_neg
    {136, Interpreter::ps_nabs},     // ps_nabs
    {264, Interpreter::ps_abs},      // ps_abs
    {64, Interpreter::ps_cmpu1},     // ps_cmpu1
    {72, Interpreter::ps_mr},        // ps_mr
    {96, Interpreter::ps_cmpo1},     // ps_cmpo1
    {528, Interpreter::ps_merge00},  // ps_merge00
    {560, Interpreter::ps_merge01},  // ps_merge01
    {592, Interpreter::ps_merge10},  // ps_merge10
    {624, Interpreter::ps_merge11},  // ps_merge11

    {1014, Interpreter::dcbz_l},  // dcbz_l
}};

constexpr std::array<InterpreterOpTemplate, 17> s_table4_2{{
    {10, Interpreter::ps_sum0},    // ps_sum0
    {11, Interpreter::ps_sum1},    // ps_sum1
    {12, Interpreter::ps_muls0},   // ps_muls0
    {13, Interpreter::ps_muls1},   // ps_muls1
    {14, Interpreter::ps_madds0},  // ps_madds0
    {15, Interpreter::ps_madds1},  // ps_madds1
    {18, Interpreter::ps_div},     // ps_div
    {20, Interpreter::ps_sub},     // ps_sub
    {21, Interpreter::ps_add},     // ps_add
    {23, Interpreter::ps_sel},     // ps_sel
    {24, Interpreter::ps_res},     // ps_res
    {25, Interpreter::ps_mul},     // ps_mul
    {26, Interpreter::ps_rsqrte},  // ps_rsqrte
    {28, Interpreter::ps_msub},    // ps_msub
    {29, Interpreter::ps_madd},    // ps_madd
    {30, Interpreter::ps_nmsub},   // ps_nmsub
    {31, Interpreter::ps_nmadd},   // ps_nmadd
}};

constexpr std::array<InterpreterOpTemplate, 4> s_table4_3{{
    {6, Interpreter::psq_lx},     // psq_lx
    {7, Interpreter::psq_stx},    // psq_stx
    {38, Interpreter::psq_lux},   // psq_lux
    {39, Interpreter::psq_stux},  // psq_stux
}};

constexpr std::array<InterpreterOpTemplate, 13> s_table19{{
    {528, Interpreter::bcctrx},  // bcctrx
    {16, Interpreter::bclrx},    // bclrx
    {257, Interpreter::crand},   // crand
    {129, Interpreter::crandc},  // crandc
    {289, Interpreter::creqv},   // creqv
    {225, Interpreter::crnand},  // crnand
    {33, Interpreter::crnor},    // crnor
    {449, Interpreter::cror},    // cror
    {417, Interpreter::crorc},   // crorc
    {193, Interpreter::crxor},   // crxor

    {150, Interpreter::isync},  // isync
    {0, Interpreter::mcrf},     // mcrf

    {50, Interpreter::rfi},  // rfi
}};

constexpr std::array<InterpreterOpTemplate, 107> s_table31{{
    {266, Interpreter::addx},     // addx
    {778, Interpreter::addx},     // addox
    {10, Interpreter::addcx},     // addcx
    {522, Interpreter::addcx},    // addcox
    {138, Interpreter::addex},    // addex
    {650, Interpreter::addex},    // addeox
    {234, Interpreter::addmex},   // addmex
    {746, Interpreter::addmex},   // addmeox
    {202, Interpreter::addzex},   // addzex
    {714, Interpreter::addzex},   // addzeox
    {491, Interpreter::divwx},    // divwx
    {1003, Interpreter::divwx},   // divwox
    {459, Interpreter::divwux},   // divwux
    {971, Interpreter::divwux},   // divwuox
    {75, Interpreter::mulhwx},    // mulhwx
    {11, Interpreter::mulhwux},   // mulhwux
    {235, Interpreter::mullwx},   // mullwx
    {747, Interpreter::mullwx},   // mullwox
    {104, Interpreter::negx},     // negx
    {616, Interpreter::negx},     // negox
    {40, Interpreter::subfx},     // subfx
    {552, Interpreter::subfx},    // subfox
    {8, Interpreter::subfcx},     // subfcx
    {520, Interpreter::subfcx},   // subfcox
    {136, Interpreter::subfex},   // subfex
    {648, Interpreter::subfex},   // subfeox
    {232, Interpreter::subfmex},  // subfmex
    {744, Interpreter::subfmex},  // subfmeox
    {200, Interpreter::subfzex},  // subfzex
    {712, Interpreter::subfzex},  // subfzeox

    {28, Interpreter::andx},     // andx
    {60, Interpreter::andcx},    // andcx
    {444, Interpreter::orx},     // orx
    {124, Interpreter::norx},    // norx
    {316, Interpreter::xorx},    // xorx
    {412, Interpreter::orcx},    // orcx
    {476, Interpreter::nandx},   // nandx
    {284, Interpreter::eqvx},    // eqvx
    {0, Interpreter::cmp},       // cmp
    {32, Interpreter::cmpl},     // cmpl
    {26, Interpreter::cntlzwx},  // cntlzwx
    {922, Interpreter::extshx},  // extshx
    {954, Interpreter::extsbx},  // extsbx
    {536, Interpreter::srwx},    // srwx
    {792, Interpreter::srawx},   // srawx
    {824, Interpreter::srawix},  // srawix
    {24, Interpreter::slwx},     // slwx

    {54, Interpreter::dcbst},    // dcbst
    {86, Interpreter::dcbf},     // dcbf
    {246, Interpreter::dcbtst},  // dcbtst
    {278, Interpreter::dcbt},    // dcbt
    {470, Interpreter::dcbi},    // dcbi
    {758, Interpreter::dcba},    // dcba
    {1014, Interpreter::dcbz},   // dcbz

    // load word
    {23, Interpreter::lwzx},   // lwzx
    {55, Interpreter::lwzux},  // lwzux

    // load halfword
    {279, Interpreter::lhzx},   // lhzx
    {311, Interpreter::lhzux},  // lhzux

    // load halfword signextend
    {343, Interpreter::lhax},   // lhax
    {375, Interpreter::lhaux},  // lhaux

    // load byte
    {87, Interpreter::lbzx},    // lbzx
    {119, Interpreter::lbzux},  // lbzux

    // load byte reverse
    {534, Interpreter::lwbrx},  // lwbrx
    {790, Interpreter::lhbrx},  // lhbrx

    // Conditional load/store (Wii SMP)
    {150, Interpreter::stwcxd},  // stwcxd
    {20, Interpreter::lwarx},    // lwarx

    // load string (Inst these)
    {533, Interpreter::lswx},  // lswx
    {597, Interpreter::lswi},  // lswi

    // store word
    {151, Interpreter::stwx},   // stwx
    {183, Interpreter::stwux},  // stwux

    // store halfword
    {407, Interpreter::sthx},   // sthx
    {439, Interpreter::sthux},  // sthux

    // store byte
    {215, Interpreter::stbx},   // stbx
    {247, Interpreter::stbux},  // stbux

    // store bytereverse
    {662, Interpreter::stwbrx},  // stwbrx
    {918, Interpreter::sthbrx},  // sthbrx

    {661, Interpreter::stswx},  // stswx
    {725, Interpreter::stswi},  // stswi

    // fp load/store
    {535, Interpreter::lfsx},   // lfsx
    {567, Interpreter::lfsux},  // lfsux
    {599, Interpreter::lfdx},   // lfdx
    {631, Interpreter::lfdux},  // lfdux

    {663, Interpreter::stfsx},   // stfsx
    {695, Interpreter::stfsux},  // stfsux
    {727, Interpreter::stfdx},   // stfdx
    {759, Interpreter::stfdux},  // stfdux
    {983, Interpreter::stfiwx},  // stfiwx

    {19, Interpreter::mfcr},     // mfcr
    {83, Interpreter::mfmsr},    // mfmsr
    {144, Interpreter::mtcrf},   // mtcrf
    {146, Interpreter::mtmsr},   // mtmsr
    {210, Interpreter::mtsr},    // mtsr
    {242, Interpreter::mtsrin},  // mtsrin
    {339, Interpreter::mfspr},   // mfspr
    {467, Interpreter::mtspr},   // mtspr
    {371, Interpreter::mftb},    // mftb
    {512, Interpreter::mcrxr},   // mcrxr
    {595, Interpreter::mfsr},    // mfsr
    {659, Interpreter::mfsrin},  // mfsrin

    {4, Interpreter::tw},      // tw
    {598, Interpreter::sync},  // sync
    {982, Interpreter::icbi},  // icbi

    // Unused instructions on GC
    {310, Interpreter::eciwx},    // eciwx
    {438, Interpreter::ecowx},    // ecowx
    {854, Interpreter::eieio},    // eieio
    {306, Interpreter::tlbie},    // tlbie
    {566, Interpreter::tlbsync},  // tlbsync
}};

constexpr std::array<InterpreterOpTemplate, 9> s_table59{{
    {18, Interpreter::fdivsx},    // fdivsx // TODO
    {20, Interpreter::fsubsx},    // fsubsx
    {21, Interpreter::faddsx},    // faddsx
    {24, Interpreter::fresx},     // fresx
    {25, Interpreter::fmulsx},    // fmulsx
    {28, Interpreter::fmsubsx},   // fmsubsx
    {29, Interpreter::fmaddsx},   // fmaddsx
    {30, Interpreter::fnmsubsx},  // fnmsubsx
    {31, Interpreter::fnmaddsx},  // fnmaddsx
}};

constexpr std::array<InterpreterOpTemplate, 15> s_table63{{
    {264, Interpreter::fabsx},   // fabsx
    {32, Interpreter::fcmpo},    // fcmpo
    {0, Interpreter::fcmpu},     // fcmpu
    {14, Interpreter::fctiwx},   // fctiwx
    {15, Interpreter::fctiwzx},  // fctiwzx
    {72, Interpreter::fmrx},     // fmrx
    {136, Interpreter::fnabsx},  // fnabsx
    {40, Interpreter::fnegx},    // fnegx
    {12, Interpreter::frspx},    // frspx

    {64, Interpreter::mcrfs},     // mcrfs
    {583, Interpreter::mffsx},    // mffsx
    {70, Interpreter::mtfsb0x},   // mtfsb0x
    {38, Interpreter::mtfsb1x},   // mtfsb1x
    {134, Interpreter::mtfsfix},  // mtfsfix
    {711, Interpreter::mtfsfx},   // mtfsfx
}};

constexpr std::array<InterpreterOpTemplate, 10> s_table63_2{{
    {18, Interpreter::fdivx},     // fdivx
    {20, Interpreter::fsubx},     // fsubx
    {21, Interpreter::faddx},     // faddx
    {23, Interpreter::fselx},     // fselx
    {25, Interpreter::fmulx},     // fmulx
    {26, Interpreter::frsqrtex},  // frsqrtex
    {28, Interpreter::fmsubx},    // fmsubx
    {29, Interpreter::fmaddx},    // fmaddx
    {30, Interpreter::fnmsubx},   // fnmsubx
    {31, Interpreter::fnmaddx},   // fnmaddx
}};

constexpr std::array<Interpreter::Instruction, 64> s_interpreter_op_table = []() consteval
{
  std::array<Interpreter::Instruction, 64> table{};
  Common::Fill(table, Interpreter::unknown_instruction);
  for (auto& tpl : s_primary_table)
  {
    ASSERT(table[tpl.opcode] == Interpreter::unknown_instruction);
    table[tpl.opcode] = tpl.fn;
  };
  return table;
}
();
constexpr std::array<Interpreter::Instruction, 1024> s_interpreter_op_table4 = []() consteval
{
  std::array<Interpreter::Instruction, 1024> table{};
  Common::Fill(table, Interpreter::unknown_instruction);

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table4_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == Interpreter::unknown_instruction);
      table[op] = tpl.fn;
    }
  }

  for (u32 i = 0; i < 16; i++)
  {
    const u32 fill = i << 6;
    for (const auto& tpl : s_table4_3)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == Interpreter::unknown_instruction);
      table[op] = tpl.fn;
    }
  }

  for (const auto& tpl : s_table4)
  {
    const u32 op = tpl.opcode;
    ASSERT(table[op] == Interpreter::unknown_instruction);
    table[op] = tpl.fn;
  }

  return table;
}
();
constexpr std::array<Interpreter::Instruction, 1024> s_interpreter_op_table19 = []() consteval
{
  std::array<Interpreter::Instruction, 1024> table{};
  Common::Fill(table, Interpreter::unknown_instruction);
  for (auto& tpl : s_table19)
  {
    ASSERT(table[tpl.opcode] == Interpreter::unknown_instruction);
    table[tpl.opcode] = tpl.fn;
  };
  return table;
}
();
constexpr std::array<Interpreter::Instruction, 1024> s_interpreter_op_table31 = []() consteval
{
  std::array<Interpreter::Instruction, 1024> table{};
  Common::Fill(table, Interpreter::unknown_instruction);
  for (auto& tpl : s_table31)
  {
    ASSERT(table[tpl.opcode] == Interpreter::unknown_instruction);
    table[tpl.opcode] = tpl.fn;
  };
  return table;
}
();
constexpr std::array<Interpreter::Instruction, 32> s_interpreter_op_table59 = []() consteval
{
  std::array<Interpreter::Instruction, 32> table{};
  Common::Fill(table, Interpreter::unknown_instruction);
  for (auto& tpl : s_table59)
  {
    ASSERT(table[tpl.opcode] == Interpreter::unknown_instruction);
    table[tpl.opcode] = tpl.fn;
  };
  return table;
}
();
constexpr std::array<Interpreter::Instruction, 1024> s_interpreter_op_table63 = []() consteval
{
  std::array<Interpreter::Instruction, 1024> table{};
  Common::Fill(table, Interpreter::unknown_instruction);
  for (auto& tpl : s_table63)
  {
    ASSERT(table[tpl.opcode] == Interpreter::unknown_instruction);
    table[tpl.opcode] = tpl.fn;
  };

  for (u32 i = 0; i < 32; i++)
  {
    const u32 fill = i << 5;
    for (const auto& tpl : s_table63_2)
    {
      const u32 op = fill + tpl.opcode;
      ASSERT(table[op] == Interpreter::unknown_instruction);
      table[op] = tpl.fn;
    }
  }

  return table;
}
();

Interpreter::Instruction Interpreter::GetInterpreterOp(UGeckoInstruction inst)
{
  // Check for the appropriate subtable ahead of time.
  // (This is used by the cached interpreter and JIT, and called once per instruction, so spending a
  // bit of extra time to optimise is worthwhile)
  Interpreter::Instruction result = s_interpreter_op_table[inst.OPCD];
  if (result == Interpreter::RunTable4)
    return s_interpreter_op_table4[inst.SUBOP10];
  else if (result == Interpreter::RunTable19)
    return s_interpreter_op_table19[inst.SUBOP10];
  else if (result == Interpreter::RunTable31)
    return s_interpreter_op_table31[inst.SUBOP10];
  else if (result == Interpreter::RunTable59)
    return s_interpreter_op_table59[inst.SUBOP5];
  else if (result == Interpreter::RunTable63)
    return s_interpreter_op_table63[inst.SUBOP10];
  else
    return result;
}

void Interpreter::RunInterpreterOp(Interpreter& interpreter, UGeckoInstruction inst)
{
  // Will handle subtables using RunTable4 etc.
  s_interpreter_op_table[inst.OPCD](interpreter, inst);
}

void Interpreter::RunTable4(Interpreter& interpreter, UGeckoInstruction inst)
{
  s_interpreter_op_table4[inst.SUBOP10](interpreter, inst);
}
void Interpreter::RunTable19(Interpreter& interpreter, UGeckoInstruction inst)
{
  s_interpreter_op_table19[inst.SUBOP10](interpreter, inst);
}
void Interpreter::RunTable31(Interpreter& interpreter, UGeckoInstruction inst)
{
  s_interpreter_op_table31[inst.SUBOP10](interpreter, inst);
}
void Interpreter::RunTable59(Interpreter& interpreter, UGeckoInstruction inst)
{
  s_interpreter_op_table59[inst.SUBOP5](interpreter, inst);
}
void Interpreter::RunTable63(Interpreter& interpreter, UGeckoInstruction inst)
{
  s_interpreter_op_table63[inst.SUBOP10](interpreter, inst);
}
