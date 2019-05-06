// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/Jit.h"

#include <cstring>
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCTables.h"

// Should be moved in to the Jit class
typedef void (JitArm64::*_Instruction)(UGeckoInstruction instCode);

static _Instruction dynaOpTable[64];
static _Instruction dynaOpTable4[1024];
static _Instruction dynaOpTable19[1024];
static _Instruction dynaOpTable31[1024];
static _Instruction dynaOpTable59[32];
static _Instruction dynaOpTable63[1024];

void JitArm64::DynaRunTable4(UGeckoInstruction inst)
{
  (this->*dynaOpTable4[inst.SUBOP10])(inst);
}
void JitArm64::DynaRunTable19(UGeckoInstruction inst)
{
  (this->*dynaOpTable19[inst.SUBOP10])(inst);
}
void JitArm64::DynaRunTable31(UGeckoInstruction inst)
{
  (this->*dynaOpTable31[inst.SUBOP10])(inst);
}
void JitArm64::DynaRunTable59(UGeckoInstruction inst)
{
  (this->*dynaOpTable59[inst.SUBOP5])(inst);
}
void JitArm64::DynaRunTable63(UGeckoInstruction inst)
{
  (this->*dynaOpTable63[inst.SUBOP10])(inst);
}

namespace
{
struct GekkoOPTemplate
{
  int opcode;
  _Instruction Inst;
  // GekkoOPInfo opinfo; // Doesn't need opinfo, Interpreter fills it out
};
}  // namespace

constexpr GekkoOPTemplate primarytable[] = {
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

    {56, &JitArm64::psq_l},   // psq_l
    {57, &JitArm64::psq_l},   // psq_lu
    {60, &JitArm64::psq_st},  // psq_st
    {61, &JitArm64::psq_st},  // psq_stu

    // missing: 0, 1, 2, 5, 6, 9, 22, 30, 62, 58
};

constexpr GekkoOPTemplate table4[] = {
    // SUBOP10
    {0, &JitArm64::FallBackToInterpreter},   // ps_cmpu0
    {32, &JitArm64::FallBackToInterpreter},  // ps_cmpo0
    {40, &JitArm64::fp_logic},               // ps_neg
    {136, &JitArm64::fp_logic},              // ps_nabs
    {264, &JitArm64::fp_logic},              // ps_abs
    {64, &JitArm64::FallBackToInterpreter},  // ps_cmpu1
    {72, &JitArm64::fp_logic},               // ps_mr
    {96, &JitArm64::FallBackToInterpreter},  // ps_cmpo1
    {528, &JitArm64::ps_mergeXX},            // ps_merge00
    {560, &JitArm64::ps_mergeXX},            // ps_merge01
    {592, &JitArm64::ps_mergeXX},            // ps_merge10
    {624, &JitArm64::ps_mergeXX},            // ps_merge11

    {1014, &JitArm64::FallBackToInterpreter},  // dcbz_l
};

constexpr GekkoOPTemplate table4_2[] = {
    {10, &JitArm64::ps_sumX},                // ps_sum0
    {11, &JitArm64::ps_sumX},                // ps_sum1
    {12, &JitArm64::ps_mulsX},               // ps_muls0
    {13, &JitArm64::ps_mulsX},               // ps_muls1
    {14, &JitArm64::ps_maddXX},              // ps_madds0
    {15, &JitArm64::ps_maddXX},              // ps_madds1
    {18, &JitArm64::fp_arith},               // ps_div
    {20, &JitArm64::fp_arith},               // ps_sub
    {21, &JitArm64::fp_arith},               // ps_add
    {23, &JitArm64::ps_sel},                 // ps_sel
    {24, &JitArm64::FallBackToInterpreter},  // ps_res
    {25, &JitArm64::fp_arith},               // ps_mul
    {26, &JitArm64::FallBackToInterpreter},  // ps_rsqrte
    {28, &JitArm64::ps_maddXX},              // ps_msub
    {29, &JitArm64::ps_maddXX},              // ps_madd
    {30, &JitArm64::ps_maddXX},              // ps_nmsub
    {31, &JitArm64::ps_maddXX},              // ps_nmadd
};

constexpr GekkoOPTemplate table4_3[] = {
    {6, &JitArm64::FallBackToInterpreter},   // psq_lx
    {7, &JitArm64::FallBackToInterpreter},   // psq_stx
    {38, &JitArm64::FallBackToInterpreter},  // psq_lux
    {39, &JitArm64::FallBackToInterpreter},  // psq_stux
};

constexpr GekkoOPTemplate table19[] = {
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
};

constexpr GekkoOPTemplate table31[] = {
    {266, &JitArm64::addx},                   // addx
    {778, &JitArm64::addx},                   // addox
    {10, &JitArm64::addcx},                   // addcx
    {522, &JitArm64::addcx},                  // addcox
    {138, &JitArm64::addex},                  // addex
    {650, &JitArm64::addex},                  // addeox
    {234, &JitArm64::FallBackToInterpreter},  // addmex
    {746, &JitArm64::FallBackToInterpreter},  // addmeox
    {202, &JitArm64::addzex},                 // addzex
    {714, &JitArm64::addzex},                 // addzeox
    {491, &JitArm64::divwx},                  // divwx
    {1003, &JitArm64::divwx},                 // divwox
    {459, &JitArm64::divwux},                 // divwux
    {971, &JitArm64::divwux},                 // divwuox
    {75, &JitArm64::mulhwx},                  // mulhwx
    {11, &JitArm64::mulhwux},                 // mulhwux
    {235, &JitArm64::mullwx},                 // mullwx
    {747, &JitArm64::mullwx},                 // mullwox
    {104, &JitArm64::negx},                   // negx
    {616, &JitArm64::negx},                   // negox
    {40, &JitArm64::subfx},                   // subfx
    {552, &JitArm64::subfx},                  // subfox
    {8, &JitArm64::subfcx},                   // subfcx
    {520, &JitArm64::subfcx},                 // subfcox
    {136, &JitArm64::subfex},                 // subfex
    {648, &JitArm64::subfex},                 // subfeox
    {232, &JitArm64::FallBackToInterpreter},  // subfmex
    {744, &JitArm64::FallBackToInterpreter},  // subfmeox
    {200, &JitArm64::subfzex},                // subfzex
    {712, &JitArm64::subfzex},                // subfzeox

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
    {662, &JitArm64::FallBackToInterpreter},  // stwbrx
    {918, &JitArm64::FallBackToInterpreter},  // sthbrx

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
};

constexpr GekkoOPTemplate table59[] = {
    {18, &JitArm64::fp_arith},               // fdivsx
    {20, &JitArm64::fp_arith},               // fsubsx
    {21, &JitArm64::fp_arith},               // faddsx
    {24, &JitArm64::FallBackToInterpreter},  // fresx
    {25, &JitArm64::fp_arith},               // fmulsx
    {28, &JitArm64::fp_arith},               // fmsubsx
    {29, &JitArm64::fp_arith},               // fmaddsx
    {30, &JitArm64::fp_arith},               // fnmsubsx
    {31, &JitArm64::fp_arith},               // fnmaddsx
};

constexpr GekkoOPTemplate table63[] = {
    {264, &JitArm64::fp_logic},              // fabsx
    {32, &JitArm64::fcmpX},                  // fcmpo
    {0, &JitArm64::fcmpX},                   // fcmpu
    {14, &JitArm64::FallBackToInterpreter},  // fctiwx
    {15, &JitArm64::fctiwzx},                // fctiwzx
    {72, &JitArm64::fp_logic},               // fmrx
    {136, &JitArm64::fp_logic},              // fnabsx
    {40, &JitArm64::fp_logic},               // fnegx
    {12, &JitArm64::frspx},                  // frspx

    {64, &JitArm64::FallBackToInterpreter},   // mcrfs
    {583, &JitArm64::FallBackToInterpreter},  // mffsx
    {70, &JitArm64::FallBackToInterpreter},   // mtfsb0x
    {38, &JitArm64::FallBackToInterpreter},   // mtfsb1x
    {134, &JitArm64::FallBackToInterpreter},  // mtfsfix
    {711, &JitArm64::FallBackToInterpreter},  // mtfsfx
};

constexpr GekkoOPTemplate table63_2[] = {
    {18, &JitArm64::fp_arith},               // fdivx
    {20, &JitArm64::fp_arith},               // fsubx
    {21, &JitArm64::fp_arith},               // faddx
    {23, &JitArm64::fselx},                  // fselx
    {25, &JitArm64::fp_arith},               // fmulx
    {26, &JitArm64::FallBackToInterpreter},  // frsqrtex
    {28, &JitArm64::fp_arith},               // fmsubx
    {29, &JitArm64::fp_arith},               // fmaddx
    {30, &JitArm64::fp_arith},               // fnmsubx
    {31, &JitArm64::fp_arith},               // fnmaddx
};

void JitArm64::CompileInstruction(PPCAnalyst::CodeOp& op)
{
  (this->*dynaOpTable[op.inst.OPCD])(op.inst);

  GekkoOPInfo* info = op.opinfo;
  if (info)
  {
#ifdef OPLOG
    if (!strcmp(info->opname, OP_TO_LOG))
    {  ///"mcrfs"
      rsplocations.push_back(js.compilerPC);
    }
#endif
    info->compileCount++;
    info->lastUse = js.compilerPC;
  }
}

void JitArm64::InitializeInstructionTables()
{
  // once initialized, tables are read-only
  static bool initialized = false;
  if (initialized)
    return;

  // clear
  for (auto& tpl : dynaOpTable)
  {
    tpl = &JitArm64::FallBackToInterpreter;
  }

  for (auto& tpl : dynaOpTable59)
  {
    tpl = &JitArm64::FallBackToInterpreter;
  }

  for (int i = 0; i < 1024; i++)
  {
    dynaOpTable4[i] = &JitArm64::FallBackToInterpreter;
    dynaOpTable19[i] = &JitArm64::FallBackToInterpreter;
    dynaOpTable31[i] = &JitArm64::FallBackToInterpreter;
    dynaOpTable63[i] = &JitArm64::FallBackToInterpreter;
  }

  for (const auto& tpl : primarytable)
  {
    dynaOpTable[tpl.opcode] = tpl.Inst;
  }

  for (int i = 0; i < 32; i++)
  {
    int fill = i << 5;
    for (const auto& tpl : table4_2)
    {
      int op = fill + tpl.opcode;
      dynaOpTable4[op] = tpl.Inst;
    }
  }

  for (int i = 0; i < 16; i++)
  {
    int fill = i << 6;
    for (const auto& tpl : table4_3)
    {
      int op = fill + tpl.opcode;
      dynaOpTable4[op] = tpl.Inst;
    }
  }

  for (const auto& tpl : table4)
  {
    dynaOpTable4[tpl.opcode] = tpl.Inst;
  }

  for (const auto& tpl : table31)
  {
    dynaOpTable31[tpl.opcode] = tpl.Inst;
  }

  for (const auto& tpl : table19)
  {
    dynaOpTable19[tpl.opcode] = tpl.Inst;
  }

  for (const auto& tpl : table59)
  {
    dynaOpTable59[tpl.opcode] = tpl.Inst;
  }

  for (const auto& tpl : table63)
  {
    dynaOpTable63[tpl.opcode] = tpl.Inst;
  }

  for (int i = 0; i < 32; i++)
  {
    int fill = i << 5;
    for (const auto& tpl : table63_2)
    {
      int op = fill + tpl.opcode;
      dynaOpTable63[op] = tpl.Inst;
    }
  }

  initialized = true;
}
