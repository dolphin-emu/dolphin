// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

#include "Common/Assembler/GekkoAssembler.h"

using namespace Common::GekkoAssembler;

constexpr char instructions[] = "add r3, r4, r5\n"
                                "add. r3, r4, r5\n"
                                "addo r3, r4, r5\n"
                                "addo. r3, r4, r5\n"
                                "addc r3, r4, r5\n"
                                "addc. r3, r4, r5\n"
                                "addco r3, r4, r5\n"
                                "addco. r3, r4, r5\n"
                                "adde r3, r4, r5\n"
                                "adde. r3, r4, r5\n"
                                "addeo r3, r4, r5\n"
                                "addeo. r3, r4, r5\n"
                                "addme r3, r4\n"
                                "addme. r3, r4\n"
                                "addmeo r3, r4\n"
                                "addmeo. r3, r4\n"
                                "addze r3, r4\n"
                                "addze. r3, r4\n"
                                "addzeo r3, r4\n"
                                "addzeo. r3, r4\n"
                                "divw r3, r4, r5\n"
                                "divw. r3, r4, r5\n"
                                "divwo r3, r4, r5\n"
                                "divwo. r3, r4, r5\n"
                                "divwu r3, r4, r5\n"
                                "divwu. r3, r4, r5\n"
                                "divwuo r3, r4, r5\n"
                                "divwuo. r3, r4, r5\n"
                                "mullw r3, r4, r5\n"
                                "mullw. r3, r4, r5\n"
                                "mullwo r3, r4, r5\n"
                                "mullwo. r3, r4, r5\n"
                                "neg r3, r4\n"
                                "neg. r3, r4\n"
                                "nego r3, r4\n"
                                "nego. r3, r4\n"
                                "subf r3, r4, r5\n"
                                "subf. r3, r4, r5\n"
                                "subfo r3, r4, r5\n"
                                "subfo. r3, r4, r5\n"
                                "subfc r3, r4, r5\n"
                                "subfc. r3, r4, r5\n"
                                "subfco r3, r4, r5\n"
                                "subfco. r3, r4, r5\n"
                                "subfe r3, r4, r5\n"
                                "subfe. r3, r4, r5\n"
                                "subfeo r3, r4, r5\n"
                                "subfeo. r3, r4, r5\n"
                                "subfme r3, r4\n"
                                "subfme. r3, r4\n"
                                "subfmeo r3, r4\n"
                                "subfmeo. r3, r4\n"
                                "subfze r3, r4\n"
                                "subfze. r3, r4\n"
                                "subfzeo r3, r4\n"
                                "subfzeo. r3, r4\n"
                                "addi r3, r4, 1000\n"
                                "addic r3, r4, 1000\n"
                                "addic. r3, r4, 1000\n"
                                "addis r3, r4, 1000\n"
                                "mulli r3, r4, 1000\n"
                                "subfic r3, r4, 1000\n"
                                "cmpi cr1, 0, r4, 1000\n"
                                "cmpli cr1, 0, r4, 1000\n"
                                "andi. r4, r6, 1000\n"
                                "andis. r4, r6, 1000\n"
                                "ori r4, r6, 1000\n"
                                "oris r4, r6, 1000\n"
                                "xori r4, r6, 1000\n"
                                "xoris r4, r6, 1000\n"
                                "lbz r3, 100(r4)\n"
                                "lbzu r3, 100( r4)\n"
                                "lha r3, 100( r4)\n"
                                "lhau r3, 100( r4)\n"
                                "lhz r3, 100( r4)\n"
                                "lhzu r3, 100( r4)\n"
                                "lwz r3, 100( r4)\n"
                                "lwzu r3, 100( r4)\n"
                                "stb r6, 100( r4)\n"
                                "stbu r6, 100( r4)\n"
                                "sth r6, 100( r4)\n"
                                "sthu r6, 100( r4)\n"
                                "stw r6, 100( r4)\n"
                                "stwu r6, 100( r4)\n"
                                "lmw r6, 100( r4)\n"
                                "stmw r6, 100( r4)\n"
                                "lfd r3, 100( r4)\n"
                                "lfdu r3, 100( r4)\n"
                                "lfs r3, 100( r4)\n"
                                "lfsu r3, 100( r4)\n"
                                "stfd r6, 100( r4)\n"
                                "stfdu r6, 100( r4)\n"
                                "stfs r6, 100( r4)\n"
                                "stfsu r6, 100( r4)\n"
                                "twi 8, r4, 1000\n"
                                "psq_l r3, 200( r4), 0, 2\n"
                                "psq_lu r3, 200( r4), 0, 2\n"
                                "psq_st r6, 200( r4), 0, 2\n"
                                "psq_stu r6, 200( r4), 0, 2\n"
                                "mulhw r3, r4, r5\n"
                                "mulhw. r3, r4, r5\n"
                                "mulhwu r3, r4, r5\n"
                                "mulhwu. r3, r4, r5\n"
                                "and r4, r6, r5\n"
                                "and. r4, r6, r5\n"
                                "andc r4, r6, r5\n"
                                "andc. r4, r6, r5\n"
                                "cntlzw r4, r6\n"
                                "cntlzw. r4, r6\n"
                                "eqv r4, r6, r5\n"
                                "eqv. r4, r6, r5\n"
                                "extsb r4, r6\n"
                                "extsb. r4, r6\n"
                                "extsh r4, r6\n"
                                "extsh. r4, r6\n"
                                "nand r4, r6, r5\n"
                                "nand. r4, r6, r5\n"
                                "nor r4, r6, r5\n"
                                "nor. r4, r6, r5\n"
                                "or r4, r6, r5\n"
                                "or. r4, r6, r5\n"
                                "orc r4, r6, r5\n"
                                "orc. r4, r6, r5\n"
                                "xor r4, r6, r5\n"
                                "xor. r4, r6, r5\n"
                                "rlwimi r4, r6, 0, 10, 15\n"
                                "rlwimi. r4, r6, 0, 10, 15\n"
                                "rlwinm r4, r6, 0, 10, 15\n"
                                "rlwinm. r4, r6, 0, 10, 15\n"
                                "rlwnm r4, r6, r5, 10, 15\n"
                                "rlwnm. r4, r6, r5, 10, 15\n"
                                "slw r4, r6, r5\n"
                                "slw. r4, r6, r5\n"
                                "sraw r4, r6, r5\n"
                                "sraw. r4, r6, r5\n"
                                "srawi r4, r6, 0\n"
                                "srawi. r4, r6, 0\n"
                                "srw r4, r6, r5\n"
                                "srw. r4, r6, r5\n"
                                "fadd r3, r4, r5\n"
                                "fadd. r3, r4, r5\n"
                                "fadds r3, r4, r5\n"
                                "fadds. r3, r4, r5\n"
                                "fdiv r3, r4, r5\n"
                                "fdiv. r3, r4, r5\n"
                                "fdivs r3, r4, r5\n"
                                "fdivs. r3, r4, r5\n"
                                "fmul r3, r4, r7\n"
                                "fmul. r3, r4, r7\n"
                                "fmuls r3, r4, r7\n"
                                "fmuls. r3, r4, r7\n"
                                "fres r3, r5\n"
                                "fres. r3, r5\n"
                                "frsqrte r3, r5\n"
                                "frsqrte. r3, r5\n"
                                "fsub r3, r4, r5\n"
                                "fsub. r3, r4, r5\n"
                                "fsubs r3, r4, r5\n"
                                "fsubs. r3, r4, r5\n"
                                "fsel r3, r4, r7, r5\n"
                                "fsel. r3, r4, r7, r5\n"
                                "fmadd r3, r4, r7, r5\n"
                                "fmadd. r3, r4, r7, r5\n"
                                "fmadds r3, r4, r7, r5\n"
                                "fmadds. r3, r4, r7, r5\n"
                                "fmsub r3, r4, r7, r5\n"
                                "fmsub. r3, r4, r7, r5\n"
                                "fmsubs r3, r4, r7, r5\n"
                                "fmsubs. r3, r4, r7, r5\n"
                                "fnmadd r3, r4, r7, r5\n"
                                "fnmadd. r3, r4, r7, r5\n"
                                "fnmadds r3, r4, r7, r5\n"
                                "fnmadds. r3, r4, r7, r5\n"
                                "fnmsub r3, r4, r7, r5\n"
                                "fnmsub. r3, r4, r7, r5\n"
                                "fnmsubs r3, r4, r7, r5\n"
                                "fnmsubs. r3, r4, r7, r5\n"
                                "fctiw r3, r5\n"
                                "fctiw. r3, r5\n"
                                "fctiwz r3, r5\n"
                                "fctiwz. r3, r5\n"
                                "frsp r3, r5\n"
                                "frsp. r3, r5\n"
                                "mffs r3\n"
                                "mffs. r3\n"
                                "mtfsb0 21\n"
                                "mtfsb0. 21\n"
                                "mtfsb1 21\n"
                                "mtfsb1. 21\n"
                                "mtfsf 255, r5\n"
                                "mtfsf. 255, r5\n"
                                "mtfsfi cr1, 5\n"
                                "mtfsfi. cr1, 5\n"
                                "fabs r3, r5\n"
                                "fabs. r3, r5\n"
                                "fmr r3, r5\n"
                                "fmr. r3, r5\n"
                                "fnabs r3, r5\n"
                                "fnabs. r3, r5\n"
                                "fneg r3, r5\n"
                                "fneg. r3, r5\n"
                                "ps_div r3, r4, r5\n"
                                "ps_div. r3, r4, r5\n"
                                "ps_sub r3, r4, r5\n"
                                "ps_sub. r3, r4, r5\n"
                                "ps_add r3, r4, r5\n"
                                "ps_add. r3, r4, r5\n"
                                "ps_sel r3, r4, r7, r5\n"
                                "ps_sel. r3, r4, r7, r5\n"
                                "ps_res r3, r5\n"
                                "ps_res. r3, r5\n"
                                "ps_mul r3, r4, r7\n"
                                "ps_mul. r3, r4, r7\n"
                                "ps_rsqrte r3, r5\n"
                                "ps_rsqrte. r3, r5\n"
                                "ps_msub r3, r4, r7, r5\n"
                                "ps_msub. r3, r4, r7, r5\n"
                                "ps_madd r3, r4, r7, r5\n"
                                "ps_madd. r3, r4, r7, r5\n"
                                "ps_nmsub r3, r4, r7, r5\n"
                                "ps_nmsub. r3, r4, r7, r5\n"
                                "ps_nmadd r3, r4, r7, r5\n"
                                "ps_nmadd. r3, r4, r7, r5\n"
                                "ps_neg r3, r5\n"
                                "ps_neg. r3, r5\n"
                                "ps_mr r3, r5\n"
                                "ps_mr. r3, r5\n"
                                "ps_nabs r3, r5\n"
                                "ps_nabs. r3, r5\n"
                                "ps_abs r3, r5\n"
                                "ps_abs. r3, r5\n"
                                "ps_sum0 r3, r4, r7, r5\n"
                                "ps_sum0. r3, r4, r7, r5\n"
                                "ps_sum1 r3, r4, r7, r5\n"
                                "ps_sum1. r3, r4, r7, r5\n"
                                "ps_muls0 r3, r4, r7\n"
                                "ps_muls0. r3, r4, r7\n"
                                "ps_muls1 r3, r4, r7\n"
                                "ps_muls1. r3, r4, r7\n"
                                "ps_madds0 r3, r4, r7, r5\n"
                                "ps_madds0. r3, r4, r7, r5\n"
                                "ps_madds1 r3, r4, r7, r5\n"
                                "ps_madds1. r3, r4, r7, r5\n"
                                "ps_merge00 r3, r4, r5\n"
                                "ps_merge00. r3, r4, r5\n"
                                "ps_merge01 r3, r4, r5\n"
                                "ps_merge01. r3, r4, r5\n"
                                "ps_merge10 r3, r4, r5\n"
                                "ps_merge10. r3, r4, r5\n"
                                "ps_merge11 r3, r4, r5\n"
                                "ps_merge11. r3, r4, r5\n"
                                "cmp cr1, 0, r4, r5\n"
                                "cmpl cr1, 0, r4, r5\n"
                                "fcmpo cr1, r4, r5\n"
                                "fcmpu cr1, r4, r5\n"
                                "mcrfs cr1, 7\n"
                                "lbzux r3, r4, r5\n"
                                "lbzx r3, r4, r5\n"
                                "lhaux r3, r4, r5\n"
                                "lhax r3, r4, r5\n"
                                "lhzux r3, r4, r5\n"
                                "lhzx r3, r4, r5\n"
                                "lwzux r3, r4, r5\n"
                                "lwzx r3, r4, r5\n"
                                "stbux r6, r4, r5\n"
                                "stbx r6, r4, r5\n"
                                "sthux r6, r4, r5\n"
                                "sthx r6, r4, r5\n"
                                "stwux r6, r4, r5\n"
                                "stwx r6, r4, r5\n"
                                "lhbrx r3, r4, r5\n"
                                "lwbrx r3, r4, r5\n"
                                "sthbrx r6, r4, r5\n"
                                "stwbrx r6, r4, r5\n"
                                "lswi r5, r4, 1\n"
                                "lswx r3, r4, r5\n"
                                "stswi r6, r4, 1\n"
                                "stswx r6, r4, r5\n"
                                "lwarx r3, r4, r5\n"
                                "stwcx. r6, r4, r5\n"
                                "lfdux r3, r4, r5\n"
                                "lfdx r3, r4, r5\n"
                                "lfsux r3, r4, r5\n"
                                "lfsx r3, r4, r5\n"
                                "stfdux r6, r4, r5\n"
                                "stfdx r6, r4, r5\n"
                                "stfiwx r6, r4, r5\n"
                                "stfsux r6, r4, r5\n"
                                "stfsx r6, r4, r5\n"
                                "crand 21, 22, 23\n"
                                "crandc 21, 22, 23\n"
                                "creqv 21, 22, 23\n"
                                "crnand 21, 22, 23\n"
                                "crnor 21, 22, 23\n"
                                "cror 21, 22, 23\n"
                                "crorc 21, 22, 23\n"
                                "crxor 21, 22, 23\n"
                                "mcrf cr1, 7\n"
                                "tw 8, r4, r5\n"
                                "mcrxr cr1\n"
                                "mfcr r3\n"
                                "mfmsr r3\n"
                                "mfspr r3, LR\n"
                                "mftb r3, 268\n"
                                "mtcrf 255, r6\n"
                                "mtmsr r6\n"
                                "mtspr LR, r3\n"
                                "dcbf r4, r5\n"
                                "dcbi r4, r5\n"
                                "dcbst r4, r5\n"
                                "dcbt r4, r5\n"
                                "dcbtst r4, r5\n"
                                "dcbz r4, r5\n"
                                "icbi r4, r5\n"
                                "mfsr r3, 0\n"
                                "mfsrin r3, r5\n"
                                "mtsr 0, r6\n"
                                "mtsrin r6, r5\n"
                                "tlbie r5\n"
                                "eciwx r3, r4, r5\n"
                                "ecowx r6, r4, r5\n"
                                "psq_lx r3, r4, r5, 0, 2\n"
                                "psq_stx r6, r4, r5, 0, 2\n"
                                "psq_lux r3, r4, r5, 0, 2\n"
                                "psq_stux r6, r4, r5, 0, 2\n"
                                "ps_cmpu0 cr1, r4, r5\n"
                                "ps_cmpo0 cr1, r4, r5\n"
                                "ps_cmpu1 cr1, r4, r5\n"
                                "ps_cmpo1 cr1, r4, r5\n"
                                "dcbz_l r4, r5\n"
                                "b 0x1000\n"
                                "ba 0x1000\n"
                                "bl 0x1000\n"
                                "bla 0x1000\n"
                                "bc 12, 2, -0xc\n"
                                "bca 12, 2, -0xc\n"
                                "bcl 12, 2, -0xc\n"
                                "bcla 12, 2, -0xc\n"
                                "bcctr 12, 2\n"
                                "bcctrl 12, 2\n"
                                "bclr 12, 2\n"
                                "bclrl 12, 2\n";

constexpr u8 expected_instructions[] = {
    0x7c, 0x64, 0x2a, 0x14, 0x7c, 0x64, 0x2a, 0x15, 0x7c, 0x64, 0x2e, 0x14, 0x7c, 0x64, 0x2e, 0x15,
    0x7c, 0x64, 0x28, 0x14, 0x7c, 0x64, 0x28, 0x15, 0x7c, 0x64, 0x2c, 0x14, 0x7c, 0x64, 0x2c, 0x15,
    0x7c, 0x64, 0x29, 0x14, 0x7c, 0x64, 0x29, 0x15, 0x7c, 0x64, 0x2d, 0x14, 0x7c, 0x64, 0x2d, 0x15,
    0x7c, 0x64, 0x01, 0xd4, 0x7c, 0x64, 0x01, 0xd5, 0x7c, 0x64, 0x05, 0xd4, 0x7c, 0x64, 0x05, 0xd5,
    0x7c, 0x64, 0x01, 0x94, 0x7c, 0x64, 0x01, 0x95, 0x7c, 0x64, 0x05, 0x94, 0x7c, 0x64, 0x05, 0x95,
    0x7c, 0x64, 0x2b, 0xd6, 0x7c, 0x64, 0x2b, 0xd7, 0x7c, 0x64, 0x2f, 0xd6, 0x7c, 0x64, 0x2f, 0xd7,
    0x7c, 0x64, 0x2b, 0x96, 0x7c, 0x64, 0x2b, 0x97, 0x7c, 0x64, 0x2f, 0x96, 0x7c, 0x64, 0x2f, 0x97,
    0x7c, 0x64, 0x29, 0xd6, 0x7c, 0x64, 0x29, 0xd7, 0x7c, 0x64, 0x2d, 0xd6, 0x7c, 0x64, 0x2d, 0xd7,
    0x7c, 0x64, 0x00, 0xd0, 0x7c, 0x64, 0x00, 0xd1, 0x7c, 0x64, 0x04, 0xd0, 0x7c, 0x64, 0x04, 0xd1,
    0x7c, 0x64, 0x28, 0x50, 0x7c, 0x64, 0x28, 0x51, 0x7c, 0x64, 0x2c, 0x50, 0x7c, 0x64, 0x2c, 0x51,
    0x7c, 0x64, 0x28, 0x10, 0x7c, 0x64, 0x28, 0x11, 0x7c, 0x64, 0x2c, 0x10, 0x7c, 0x64, 0x2c, 0x11,
    0x7c, 0x64, 0x29, 0x10, 0x7c, 0x64, 0x29, 0x11, 0x7c, 0x64, 0x2d, 0x10, 0x7c, 0x64, 0x2d, 0x11,
    0x7c, 0x64, 0x01, 0xd0, 0x7c, 0x64, 0x01, 0xd1, 0x7c, 0x64, 0x05, 0xd0, 0x7c, 0x64, 0x05, 0xd1,
    0x7c, 0x64, 0x01, 0x90, 0x7c, 0x64, 0x01, 0x91, 0x7c, 0x64, 0x05, 0x90, 0x7c, 0x64, 0x05, 0x91,
    0x38, 0x64, 0x03, 0xe8, 0x30, 0x64, 0x03, 0xe8, 0x34, 0x64, 0x03, 0xe8, 0x3c, 0x64, 0x03, 0xe8,
    0x1c, 0x64, 0x03, 0xe8, 0x20, 0x64, 0x03, 0xe8, 0x2c, 0x84, 0x03, 0xe8, 0x28, 0x84, 0x03, 0xe8,
    0x70, 0xc4, 0x03, 0xe8, 0x74, 0xc4, 0x03, 0xe8, 0x60, 0xc4, 0x03, 0xe8, 0x64, 0xc4, 0x03, 0xe8,
    0x68, 0xc4, 0x03, 0xe8, 0x6c, 0xc4, 0x03, 0xe8, 0x88, 0x64, 0x00, 0x64, 0x8c, 0x64, 0x00, 0x64,
    0xa8, 0x64, 0x00, 0x64, 0xac, 0x64, 0x00, 0x64, 0xa0, 0x64, 0x00, 0x64, 0xa4, 0x64, 0x00, 0x64,
    0x80, 0x64, 0x00, 0x64, 0x84, 0x64, 0x00, 0x64, 0x98, 0xc4, 0x00, 0x64, 0x9c, 0xc4, 0x00, 0x64,
    0xb0, 0xc4, 0x00, 0x64, 0xb4, 0xc4, 0x00, 0x64, 0x90, 0xc4, 0x00, 0x64, 0x94, 0xc4, 0x00, 0x64,
    0xb8, 0xc4, 0x00, 0x64, 0xbc, 0xc4, 0x00, 0x64, 0xc8, 0x64, 0x00, 0x64, 0xcc, 0x64, 0x00, 0x64,
    0xc0, 0x64, 0x00, 0x64, 0xc4, 0x64, 0x00, 0x64, 0xd8, 0xc4, 0x00, 0x64, 0xdc, 0xc4, 0x00, 0x64,
    0xd0, 0xc4, 0x00, 0x64, 0xd4, 0xc4, 0x00, 0x64, 0x0d, 0x04, 0x03, 0xe8, 0xe0, 0x64, 0x20, 0xc8,
    0xe4, 0x64, 0x20, 0xc8, 0xf0, 0xc4, 0x20, 0xc8, 0xf4, 0xc4, 0x20, 0xc8, 0x7c, 0x64, 0x28, 0x96,
    0x7c, 0x64, 0x28, 0x97, 0x7c, 0x64, 0x28, 0x16, 0x7c, 0x64, 0x28, 0x17, 0x7c, 0xc4, 0x28, 0x38,
    0x7c, 0xc4, 0x28, 0x39, 0x7c, 0xc4, 0x28, 0x78, 0x7c, 0xc4, 0x28, 0x79, 0x7c, 0xc4, 0x00, 0x34,
    0x7c, 0xc4, 0x00, 0x35, 0x7c, 0xc4, 0x2a, 0x38, 0x7c, 0xc4, 0x2a, 0x39, 0x7c, 0xc4, 0x07, 0x74,
    0x7c, 0xc4, 0x07, 0x75, 0x7c, 0xc4, 0x07, 0x34, 0x7c, 0xc4, 0x07, 0x35, 0x7c, 0xc4, 0x2b, 0xb8,
    0x7c, 0xc4, 0x2b, 0xb9, 0x7c, 0xc4, 0x28, 0xf8, 0x7c, 0xc4, 0x28, 0xf9, 0x7c, 0xc4, 0x2b, 0x78,
    0x7c, 0xc4, 0x2b, 0x79, 0x7c, 0xc4, 0x2b, 0x38, 0x7c, 0xc4, 0x2b, 0x39, 0x7c, 0xc4, 0x2a, 0x78,
    0x7c, 0xc4, 0x2a, 0x79, 0x50, 0xc4, 0x02, 0x9e, 0x50, 0xc4, 0x02, 0x9f, 0x54, 0xc4, 0x02, 0x9e,
    0x54, 0xc4, 0x02, 0x9f, 0x5c, 0xc4, 0x2a, 0x9e, 0x5c, 0xc4, 0x2a, 0x9f, 0x7c, 0xc4, 0x28, 0x30,
    0x7c, 0xc4, 0x28, 0x31, 0x7c, 0xc4, 0x2e, 0x30, 0x7c, 0xc4, 0x2e, 0x31, 0x7c, 0xc4, 0x06, 0x70,
    0x7c, 0xc4, 0x06, 0x71, 0x7c, 0xc4, 0x2c, 0x30, 0x7c, 0xc4, 0x2c, 0x31, 0xfc, 0x64, 0x28, 0x2a,
    0xfc, 0x64, 0x28, 0x2b, 0xec, 0x64, 0x28, 0x2a, 0xec, 0x64, 0x28, 0x2b, 0xfc, 0x64, 0x28, 0x24,
    0xfc, 0x64, 0x28, 0x25, 0xec, 0x64, 0x28, 0x24, 0xec, 0x64, 0x28, 0x25, 0xfc, 0x64, 0x01, 0xf2,
    0xfc, 0x64, 0x01, 0xf3, 0xec, 0x64, 0x01, 0xf2, 0xec, 0x64, 0x01, 0xf3, 0xec, 0x60, 0x28, 0x30,
    0xec, 0x60, 0x28, 0x31, 0xfc, 0x60, 0x28, 0x34, 0xfc, 0x60, 0x28, 0x35, 0xfc, 0x64, 0x28, 0x28,
    0xfc, 0x64, 0x28, 0x29, 0xec, 0x64, 0x28, 0x28, 0xec, 0x64, 0x28, 0x29, 0xfc, 0x64, 0x29, 0xee,
    0xfc, 0x64, 0x29, 0xef, 0xfc, 0x64, 0x29, 0xfa, 0xfc, 0x64, 0x29, 0xfb, 0xec, 0x64, 0x29, 0xfa,
    0xec, 0x64, 0x29, 0xfb, 0xfc, 0x64, 0x29, 0xf8, 0xfc, 0x64, 0x29, 0xf9, 0xec, 0x64, 0x29, 0xf8,
    0xec, 0x64, 0x29, 0xf9, 0xfc, 0x64, 0x29, 0xfe, 0xfc, 0x64, 0x29, 0xff, 0xec, 0x64, 0x29, 0xfe,
    0xec, 0x64, 0x29, 0xff, 0xfc, 0x64, 0x29, 0xfc, 0xfc, 0x64, 0x29, 0xfd, 0xec, 0x64, 0x29, 0xfc,
    0xec, 0x64, 0x29, 0xfd, 0xfc, 0x60, 0x28, 0x1c, 0xfc, 0x60, 0x28, 0x1d, 0xfc, 0x60, 0x28, 0x1e,
    0xfc, 0x60, 0x28, 0x1f, 0xfc, 0x60, 0x28, 0x18, 0xfc, 0x60, 0x28, 0x19, 0xfc, 0x60, 0x04, 0x8e,
    0xfc, 0x60, 0x04, 0x8f, 0xfe, 0xa0, 0x00, 0x8c, 0xfe, 0xa0, 0x00, 0x8d, 0xfe, 0xa0, 0x00, 0x4c,
    0xfe, 0xa0, 0x00, 0x4d, 0xfd, 0xfe, 0x2d, 0x8e, 0xfd, 0xfe, 0x2d, 0x8f, 0xfc, 0x80, 0x51, 0x0c,
    0xfc, 0x80, 0x51, 0x0d, 0xfc, 0x60, 0x2a, 0x10, 0xfc, 0x60, 0x2a, 0x11, 0xfc, 0x60, 0x28, 0x90,
    0xfc, 0x60, 0x28, 0x91, 0xfc, 0x60, 0x29, 0x10, 0xfc, 0x60, 0x29, 0x11, 0xfc, 0x60, 0x28, 0x50,
    0xfc, 0x60, 0x28, 0x51, 0x10, 0x64, 0x28, 0x24, 0x10, 0x64, 0x28, 0x25, 0x10, 0x64, 0x28, 0x28,
    0x10, 0x64, 0x28, 0x29, 0x10, 0x64, 0x28, 0x2a, 0x10, 0x64, 0x28, 0x2b, 0x10, 0x64, 0x29, 0xee,
    0x10, 0x64, 0x29, 0xef, 0x10, 0x60, 0x28, 0x30, 0x10, 0x60, 0x28, 0x31, 0x10, 0x64, 0x01, 0xf2,
    0x10, 0x64, 0x01, 0xf3, 0x10, 0x60, 0x28, 0x34, 0x10, 0x60, 0x28, 0x35, 0x10, 0x64, 0x29, 0xf8,
    0x10, 0x64, 0x29, 0xf9, 0x10, 0x64, 0x29, 0xfa, 0x10, 0x64, 0x29, 0xfb, 0x10, 0x64, 0x29, 0xfc,
    0x10, 0x64, 0x29, 0xfd, 0x10, 0x64, 0x29, 0xfe, 0x10, 0x64, 0x29, 0xff, 0x10, 0x60, 0x28, 0x50,
    0x10, 0x60, 0x28, 0x51, 0x10, 0x60, 0x28, 0x90, 0x10, 0x60, 0x28, 0x91, 0x10, 0x60, 0x29, 0x10,
    0x10, 0x60, 0x29, 0x11, 0x10, 0x60, 0x2a, 0x10, 0x10, 0x60, 0x2a, 0x11, 0x10, 0x64, 0x29, 0xd4,
    0x10, 0x64, 0x29, 0xd5, 0x10, 0x64, 0x29, 0xd6, 0x10, 0x64, 0x29, 0xd7, 0x10, 0x64, 0x01, 0xd8,
    0x10, 0x64, 0x01, 0xd9, 0x10, 0x64, 0x01, 0xda, 0x10, 0x64, 0x01, 0xdb, 0x10, 0x64, 0x29, 0xdc,
    0x10, 0x64, 0x29, 0xdd, 0x10, 0x64, 0x29, 0xde, 0x10, 0x64, 0x29, 0xdf, 0x10, 0x64, 0x2c, 0x20,
    0x10, 0x64, 0x2c, 0x21, 0x10, 0x64, 0x2c, 0x60, 0x10, 0x64, 0x2c, 0x61, 0x10, 0x64, 0x2c, 0xa0,
    0x10, 0x64, 0x2c, 0xa1, 0x10, 0x64, 0x2c, 0xe0, 0x10, 0x64, 0x2c, 0xe1, 0x7c, 0x84, 0x28, 0x00,
    0x7c, 0x84, 0x28, 0x40, 0xfc, 0x84, 0x28, 0x40, 0xfc, 0x84, 0x28, 0x00, 0xfc, 0x9c, 0x00, 0x80,
    0x7c, 0x64, 0x28, 0xee, 0x7c, 0x64, 0x28, 0xae, 0x7c, 0x64, 0x2a, 0xee, 0x7c, 0x64, 0x2a, 0xae,
    0x7c, 0x64, 0x2a, 0x6e, 0x7c, 0x64, 0x2a, 0x2e, 0x7c, 0x64, 0x28, 0x6e, 0x7c, 0x64, 0x28, 0x2e,
    0x7c, 0xc4, 0x29, 0xee, 0x7c, 0xc4, 0x29, 0xae, 0x7c, 0xc4, 0x2b, 0x6e, 0x7c, 0xc4, 0x2b, 0x2e,
    0x7c, 0xc4, 0x29, 0x6e, 0x7c, 0xc4, 0x29, 0x2e, 0x7c, 0x64, 0x2e, 0x2c, 0x7c, 0x64, 0x2c, 0x2c,
    0x7c, 0xc4, 0x2f, 0x2c, 0x7c, 0xc4, 0x2d, 0x2c, 0x7c, 0xa4, 0x0c, 0xaa, 0x7c, 0x64, 0x2c, 0x2a,
    0x7c, 0xc4, 0x0d, 0xaa, 0x7c, 0xc4, 0x2d, 0x2a, 0x7c, 0x64, 0x28, 0x28, 0x7c, 0xc4, 0x29, 0x2d,
    0x7c, 0x64, 0x2c, 0xee, 0x7c, 0x64, 0x2c, 0xae, 0x7c, 0x64, 0x2c, 0x6e, 0x7c, 0x64, 0x2c, 0x2e,
    0x7c, 0xc4, 0x2d, 0xee, 0x7c, 0xc4, 0x2d, 0xae, 0x7c, 0xc4, 0x2f, 0xae, 0x7c, 0xc4, 0x2d, 0x6e,
    0x7c, 0xc4, 0x2d, 0x2e, 0x4e, 0xb6, 0xba, 0x02, 0x4e, 0xb6, 0xb9, 0x02, 0x4e, 0xb6, 0xba, 0x42,
    0x4e, 0xb6, 0xb9, 0xc2, 0x4e, 0xb6, 0xb8, 0x42, 0x4e, 0xb6, 0xbb, 0x82, 0x4e, 0xb6, 0xbb, 0x42,
    0x4e, 0xb6, 0xb9, 0x82, 0x4c, 0x9c, 0x00, 0x00, 0x7d, 0x04, 0x28, 0x08, 0x7c, 0x80, 0x04, 0x00,
    0x7c, 0x60, 0x00, 0x26, 0x7c, 0x60, 0x00, 0xa6, 0x7c, 0x68, 0x02, 0xa6, 0x7c, 0x6c, 0x42, 0xe6,
    0x7c, 0xcf, 0xf1, 0x20, 0x7c, 0xc0, 0x01, 0x24, 0x7c, 0x68, 0x03, 0xa6, 0x7c, 0x04, 0x28, 0xac,
    0x7c, 0x04, 0x2b, 0xac, 0x7c, 0x04, 0x28, 0x6c, 0x7c, 0x04, 0x2a, 0x2c, 0x7c, 0x04, 0x29, 0xec,
    0x7c, 0x04, 0x2f, 0xec, 0x7c, 0x04, 0x2f, 0xac, 0x7c, 0x60, 0x04, 0xa6, 0x7c, 0x60, 0x2d, 0x26,
    0x7c, 0xc0, 0x01, 0xa4, 0x7c, 0xc0, 0x29, 0xe4, 0x7c, 0x00, 0x2a, 0x64, 0x7c, 0x64, 0x2a, 0x6c,
    0x7c, 0xc4, 0x2b, 0x6c, 0x10, 0x64, 0x29, 0x0c, 0x10, 0xc4, 0x29, 0x0e, 0x10, 0x64, 0x29, 0x4c,
    0x10, 0xc4, 0x29, 0x4e, 0x10, 0x84, 0x28, 0x00, 0x10, 0x84, 0x28, 0x40, 0x10, 0x84, 0x28, 0x80,
    0x10, 0x84, 0x28, 0xc0, 0x10, 0x04, 0x2f, 0xec, 0x48, 0x00, 0x10, 0x00, 0x48, 0x00, 0x10, 0x02,
    0x48, 0x00, 0x10, 0x01, 0x48, 0x00, 0x10, 0x03, 0x41, 0x82, 0xff, 0xf4, 0x41, 0x82, 0xff, 0xf6,
    0x41, 0x82, 0xff, 0xf5, 0x41, 0x82, 0xff, 0xf7, 0x4d, 0x82, 0x04, 0x20, 0x4d, 0x82, 0x04, 0x21,
    0x4d, 0x82, 0x00, 0x20, 0x4d, 0x82, 0x00, 0x21,
};

TEST(Assembler, AllInstructions)
{
  auto res = Assemble(instructions, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expected_instructions));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expected_instructions[i]) << " -> i=" << i;
  }
}

constexpr char extended_instructions[] = "subi 0, 4, 8\n"
                                         "subis 0, 4, 8\n"
                                         "subic 0, 4, 8\n"
                                         "subic. 0, 4, 8\n"
                                         "cmpwi 0, 4\n"
                                         "cmpwi 0, 4, 8\n"
                                         "cmpw 0, 4\n"
                                         "cmpw 0, 4, 8\n"
                                         "cmplwi 0, 4\n"
                                         "cmplwi 0, 4, 8\n"
                                         "cmplw 0, 4\n"
                                         "cmplw 0, 4, 8\n"
                                         "crset 0\n"
                                         "crclr 0\n"
                                         "crmove 0, 4\n"
                                         "crnot 0, 4\n"
                                         "twlt 0, 4\n"
                                         "twlti 0, 4\n"
                                         "twle 0, 4\n"
                                         "twlei 0, 4\n"
                                         "tweq 0, 4\n"
                                         "tweqi 0, 4\n"
                                         "twge 0, 4\n"
                                         "twgei 0, 4\n"
                                         "twgt 0, 4\n"
                                         "twgti 0, 4\n"
                                         "twnl 0, 4\n"
                                         "twnli 0, 4\n"
                                         "twne 0, 4\n"
                                         "twnei 0, 4\n"
                                         "twng 0, 4\n"
                                         "twngi 0, 4\n"
                                         "twllt 0, 4\n"
                                         "twllti 0, 4\n"
                                         "twlle 0, 4\n"
                                         "twllei 0, 4\n"
                                         "twlge 0, 4\n"
                                         "twlgei 0, 4\n"
                                         "twlgt 0, 4\n"
                                         "twlgti 0, 4\n"
                                         "twlnl 0, 4\n"
                                         "twlnli 0, 4\n"
                                         "twlng 0, 4\n"
                                         "twlngi 0, 4\n"
                                         "trap \n"
                                         "mtxer 0\n"
                                         "mfxer 0\n"
                                         "mtlr 0\n"
                                         "mflr 0\n"
                                         "mtctr 0\n"
                                         "mfctr 0\n"
                                         "mtdsisr 0\n"
                                         "mfdsisr 0\n"
                                         "mtdar 0\n"
                                         "mfdar 0\n"
                                         "mtdec 0\n"
                                         "mfdec 0\n"
                                         "mtsdr1 0\n"
                                         "mfsdr1 0\n"
                                         "mtsrr0 0\n"
                                         "mfsrr0 0\n"
                                         "mtsrr1 0\n"
                                         "mfsrr1 0\n"
                                         "mtear 0\n"
                                         "mfear 0\n"
                                         "mttbl 0\n"
                                         "mftbl 0\n"
                                         "mttbu 0\n"
                                         "mftbu 0\n"
                                         "mtsprg 0, 4\n"
                                         "mfsprg 0, 1\n"
                                         "mtibatu 0, 1\n"
                                         "mfibatu 0, 1\n"
                                         "mtibatl 0, 1\n"
                                         "mfibatl 0, 1\n"
                                         "mtdbatu 0, 1\n"
                                         "mfdbatu 0, 1\n"
                                         "mtdbatl 0, 1\n"
                                         "mfdbatl 0, 1\n"
                                         "nop \n"
                                         "li 0, 4\n"
                                         "lis 0, 4\n"
                                         "la 0, 4(8)\n"
                                         "mtcr 0\n"
                                         "mfspr 0, 4\n"
                                         "mftb 0, 268\n"
                                         "mtspr 0, 4\n"
                                         "sub 0, 4, 8\n"
                                         "sub. 0, 4, 8\n"
                                         "subo 0, 4, 8\n"
                                         "subo. 0, 4, 8\n"
                                         "subc 0, 4, 8\n"
                                         "subc. 0, 4, 8\n"
                                         "subco 0, 4, 8\n"
                                         "subco. 0, 4, 8\n"
                                         "extlwi 0, 4, 8, 12\n"
                                         "extlwi. 0, 4, 8, 12\n"
                                         "extrwi 0, 4, 8, 12\n"
                                         "extrwi. 0, 4, 8, 12\n"
                                         "inslwi 0, 4, 8, 12\n"
                                         "inslwi. 0, 4, 8, 12\n"
                                         "insrwi 0, 4, 8, 12\n"
                                         "insrwi. 0, 4, 8, 12\n"
                                         "rotlwi 0, 4, 8\n"
                                         "rotlwi. 0, 4, 8\n"
                                         "rotrwi 0, 4, 8\n"
                                         "rotrwi. 0, 4, 8\n"
                                         "rotlw 0, 4, 8\n"
                                         "rotlw. 0, 4, 8\n"
                                         "slwi 0, 4, 8\n"
                                         "slwi. 0, 4, 8\n"
                                         "srwi 0, 4, 8\n"
                                         "srwi. 0, 4, 8\n"
                                         "clrlwi 0, 4, 8\n"
                                         "clrlwi. 0, 4, 8\n"
                                         "clrrwi 0, 4, 8\n"
                                         "clrrwi. 0, 4, 8\n"
                                         "clrlslwi 0, 4, 12, 8\n"
                                         "clrlslwi. 0, 4, 12, 8\n"
                                         "mr 0, 4\n"
                                         "mr. 0, 4\n"
                                         "not 0, 4\n"
                                         "not. 0, 4\n"
                                         "bt 0, 4\n"
                                         "btl 0, 4\n"
                                         "bta 0, 4\n"
                                         "btla 0, 4\n"
                                         "bt- 0, 4\n"
                                         "btl- 0, 4\n"
                                         "bta- 0, 4\n"
                                         "btla- 0, 4\n"
                                         "bt+ 0, 4\n"
                                         "btl+ 0, 4\n"
                                         "bta+ 0, 4\n"
                                         "btla+ 0, 4\n"
                                         "bf 0, 4\n"
                                         "bfl 0, 4\n"
                                         "bfa 0, 4\n"
                                         "bfla 0, 4\n"
                                         "bf- 0, 4\n"
                                         "bfl- 0, 4\n"
                                         "bfa- 0, 4\n"
                                         "bfla- 0, 4\n"
                                         "bf+ 0, 4\n"
                                         "bfl+ 0, 4\n"
                                         "bfa+ 0, 4\n"
                                         "bfla+ 0, 4\n"
                                         "bdnz 0\n"
                                         "bdnzl 0\n"
                                         "bdnza 0\n"
                                         "bdnzla 0\n"
                                         "bdnz- 0\n"
                                         "bdnzl- 0\n"
                                         "bdnza- 0\n"
                                         "bdnzla- 0\n"
                                         "bdnz+ 0\n"
                                         "bdnzl+ 0\n"
                                         "bdnza+ 0\n"
                                         "bdnzla+ 0\n"
                                         "bdnzt 0, 4\n"
                                         "bdnztl 0, 4\n"
                                         "bdnzta 0, 4\n"
                                         "bdnztla 0, 4\n"
                                         "bdnzt- 0, 4\n"
                                         "bdnztl- 0, 4\n"
                                         "bdnzta- 0, 4\n"
                                         "bdnztla- 0, 4\n"
                                         "bdnzt+ 0, 4\n"
                                         "bdnztl+ 0, 4\n"
                                         "bdnzta+ 0, 4\n"
                                         "bdnztla+ 0, 4\n"
                                         "bdnzf 0, 4\n"
                                         "bdnzfl 0, 4\n"
                                         "bdnzfa 0, 4\n"
                                         "bdnzfla 0, 4\n"
                                         "bdnzf- 0, 4\n"
                                         "bdnzfl- 0, 4\n"
                                         "bdnzfa- 0, 4\n"
                                         "bdnzfla- 0, 4\n"
                                         "bdnzf+ 0, 4\n"
                                         "bdnzfl+ 0, 4\n"
                                         "bdnzfa+ 0, 4\n"
                                         "bdnzfla+ 0, 4\n"
                                         "bdz 0\n"
                                         "bdzl 0\n"
                                         "bdza 0\n"
                                         "bdzla 0\n"
                                         "bdz- 0\n"
                                         "bdzl- 0\n"
                                         "bdza- 0\n"
                                         "bdzla- 0\n"
                                         "bdz+ 0\n"
                                         "bdzl+ 0\n"
                                         "bdza+ 0\n"
                                         "bdzla+ 0\n"
                                         "bdzt 0, 4\n"
                                         "bdztl 0, 4\n"
                                         "bdzta 0, 4\n"
                                         "bdztla 0, 4\n"
                                         "bdzt- 0, 4\n"
                                         "bdztl- 0, 4\n"
                                         "bdzta- 0, 4\n"
                                         "bdztla- 0, 4\n"
                                         "bdzt+ 0, 4\n"
                                         "bdztl+ 0, 4\n"
                                         "bdzta+ 0, 4\n"
                                         "bdztla+ 0, 4\n"
                                         "bdzf 0, 4\n"
                                         "bdzfl 0, 4\n"
                                         "bdzfa 0, 4\n"
                                         "bdzfla 0, 4\n"
                                         "bdzf- 0, 4\n"
                                         "bdzfl- 0, 4\n"
                                         "bdzfa- 0, 4\n"
                                         "bdzfla- 0, 4\n"
                                         "bdzf+ 0, 4\n"
                                         "bdzfl+ 0, 4\n"
                                         "bdzfa+ 0, 4\n"
                                         "bdzfla+ 0, 4\n"
                                         "blt 0 \n"
                                         "blt 0, 4\n"
                                         "bltl 0 \n"
                                         "bltl 0, 4\n"
                                         "blta 0 \n"
                                         "blta 0, 4\n"
                                         "bltla 0 \n"
                                         "bltla 0, 4\n"
                                         "blt- 0 \n"
                                         "blt- 0, 4\n"
                                         "bltl- 0 \n"
                                         "bltl- 0, 4\n"
                                         "blta- 0 \n"
                                         "blta- 0, 4\n"
                                         "bltla- 0 \n"
                                         "bltla- 0, 4\n"
                                         "blt+ 0 \n"
                                         "blt+ 0, 4\n"
                                         "bltl+ 0 \n"
                                         "bltl+ 0, 4\n"
                                         "blta+ 0 \n"
                                         "blta+ 0, 4\n"
                                         "bltla+ 0 \n"
                                         "bltla+ 0, 4\n"
                                         "ble 0 \n"
                                         "ble 0, 4\n"
                                         "blel 0 \n"
                                         "blel 0, 4\n"
                                         "blea 0 \n"
                                         "blea 0, 4\n"
                                         "blela 0 \n"
                                         "blela 0, 4\n"
                                         "ble- 0 \n"
                                         "ble- 0, 4\n"
                                         "blel- 0 \n"
                                         "blel- 0, 4\n"
                                         "blea- 0 \n"
                                         "blea- 0, 4\n"
                                         "blela- 0 \n"
                                         "blela- 0, 4\n"
                                         "ble+ 0 \n"
                                         "ble+ 0, 4\n"
                                         "blel+ 0 \n"
                                         "blel+ 0, 4\n"
                                         "blea+ 0 \n"
                                         "blea+ 0, 4\n"
                                         "blela+ 0 \n"
                                         "blela+ 0, 4\n"
                                         "beq 0 \n"
                                         "beq 0, 4\n"
                                         "beql 0 \n"
                                         "beql 0, 4\n"
                                         "beqa 0 \n"
                                         "beqa 0, 4\n"
                                         "beqla 0 \n"
                                         "beqla 0, 4\n"
                                         "beq- 0 \n"
                                         "beq- 0, 4\n"
                                         "beql- 0 \n"
                                         "beql- 0, 4\n"
                                         "beqa- 0 \n"
                                         "beqa- 0, 4\n"
                                         "beqla- 0 \n"
                                         "beqla- 0, 4\n"
                                         "beq+ 0 \n"
                                         "beq+ 0, 4\n"
                                         "beql+ 0 \n"
                                         "beql+ 0, 4\n"
                                         "beqa+ 0 \n"
                                         "beqa+ 0, 4\n"
                                         "beqla+ 0 \n"
                                         "beqla+ 0, 4\n"
                                         "bge 0 \n"
                                         "bge 0, 4\n"
                                         "bgel 0 \n"
                                         "bgel 0, 4\n"
                                         "bgea 0 \n"
                                         "bgea 0, 4\n"
                                         "bgela 0 \n"
                                         "bgela 0, 4\n"
                                         "bge- 0 \n"
                                         "bge- 0, 4\n"
                                         "bgel- 0 \n"
                                         "bgel- 0, 4\n"
                                         "bgea- 0 \n"
                                         "bgea- 0, 4\n"
                                         "bgela- 0 \n"
                                         "bgela- 0, 4\n"
                                         "bge+ 0 \n"
                                         "bge+ 0, 4\n"
                                         "bgel+ 0 \n"
                                         "bgel+ 0, 4\n"
                                         "bgea+ 0 \n"
                                         "bgea+ 0, 4\n"
                                         "bgela+ 0 \n"
                                         "bgela+ 0, 4\n"
                                         "bgt 0 \n"
                                         "bgt 0, 4\n"
                                         "bgtl 0 \n"
                                         "bgtl 0, 4\n"
                                         "bgta 0 \n"
                                         "bgta 0, 4\n"
                                         "bgtla 0 \n"
                                         "bgtla 0, 4\n"
                                         "bgt- 0 \n"
                                         "bgt- 0, 4\n"
                                         "bgtl- 0 \n"
                                         "bgtl- 0, 4\n"
                                         "bgta- 0 \n"
                                         "bgta- 0, 4\n"
                                         "bgtla- 0 \n"
                                         "bgtla- 0, 4\n"
                                         "bgt+ 0 \n"
                                         "bgt+ 0, 4\n"
                                         "bgtl+ 0 \n"
                                         "bgtl+ 0, 4\n"
                                         "bgta+ 0 \n"
                                         "bgta+ 0, 4\n"
                                         "bgtla+ 0 \n"
                                         "bgtla+ 0, 4\n"
                                         "bnl 0 \n"
                                         "bnl 0, 4\n"
                                         "bnll 0 \n"
                                         "bnll 0, 4\n"
                                         "bnla 0 \n"
                                         "bnla 0, 4\n"
                                         "bnlla 0 \n"
                                         "bnlla 0, 4\n"
                                         "bnl- 0 \n"
                                         "bnl- 0, 4\n"
                                         "bnll- 0 \n"
                                         "bnll- 0, 4\n"
                                         "bnla- 0 \n"
                                         "bnla- 0, 4\n"
                                         "bnlla- 0 \n"
                                         "bnlla- 0, 4\n"
                                         "bnl+ 0 \n"
                                         "bnl+ 0, 4\n"
                                         "bnll+ 0 \n"
                                         "bnll+ 0, 4\n"
                                         "bnla+ 0 \n"
                                         "bnla+ 0, 4\n"
                                         "bnlla+ 0 \n"
                                         "bnlla+ 0, 4\n"
                                         "bne 0 \n"
                                         "bne 0, 4\n"
                                         "bnel 0 \n"
                                         "bnel 0, 4\n"
                                         "bnea 0 \n"
                                         "bnea 0, 4\n"
                                         "bnela 0 \n"
                                         "bnela 0, 4\n"
                                         "bne- 0 \n"
                                         "bne- 0, 4\n"
                                         "bnel- 0 \n"
                                         "bnel- 0, 4\n"
                                         "bnea- 0 \n"
                                         "bnea- 0, 4\n"
                                         "bnela- 0 \n"
                                         "bnela- 0, 4\n"
                                         "bne+ 0 \n"
                                         "bne+ 0, 4\n"
                                         "bnel+ 0 \n"
                                         "bnel+ 0, 4\n"
                                         "bnea+ 0 \n"
                                         "bnea+ 0, 4\n"
                                         "bnela+ 0 \n"
                                         "bnela+ 0, 4\n"
                                         "bng 0 \n"
                                         "bng 0, 4\n"
                                         "bngl 0 \n"
                                         "bngl 0, 4\n"
                                         "bnga 0 \n"
                                         "bnga 0, 4\n"
                                         "bngla 0 \n"
                                         "bngla 0, 4\n"
                                         "bng- 0 \n"
                                         "bng- 0, 4\n"
                                         "bngl- 0 \n"
                                         "bngl- 0, 4\n"
                                         "bnga- 0 \n"
                                         "bnga- 0, 4\n"
                                         "bngla- 0 \n"
                                         "bngla- 0, 4\n"
                                         "bng+ 0 \n"
                                         "bng+ 0, 4\n"
                                         "bngl+ 0 \n"
                                         "bngl+ 0, 4\n"
                                         "bnga+ 0 \n"
                                         "bnga+ 0, 4\n"
                                         "bngla+ 0 \n"
                                         "bngla+ 0, 4\n"
                                         "bso 0 \n"
                                         "bso 0, 4\n"
                                         "bsol 0 \n"
                                         "bsol 0, 4\n"
                                         "bsoa 0 \n"
                                         "bsoa 0, 4\n"
                                         "bsola 0 \n"
                                         "bsola 0, 4\n"
                                         "bso- 0 \n"
                                         "bso- 0, 4\n"
                                         "bsol- 0 \n"
                                         "bsol- 0, 4\n"
                                         "bsoa- 0 \n"
                                         "bsoa- 0, 4\n"
                                         "bsola- 0 \n"
                                         "bsola- 0, 4\n"
                                         "bso+ 0 \n"
                                         "bso+ 0, 4\n"
                                         "bsol+ 0 \n"
                                         "bsol+ 0, 4\n"
                                         "bsoa+ 0 \n"
                                         "bsoa+ 0, 4\n"
                                         "bsola+ 0 \n"
                                         "bsola+ 0, 4\n"
                                         "bns 0 \n"
                                         "bns 0, 4\n"
                                         "bnsl 0 \n"
                                         "bnsl 0, 4\n"
                                         "bnsa 0 \n"
                                         "bnsa 0, 4\n"
                                         "bnsla 0 \n"
                                         "bnsla 0, 4\n"
                                         "bns- 0 \n"
                                         "bns- 0, 4\n"
                                         "bnsl- 0 \n"
                                         "bnsl- 0, 4\n"
                                         "bnsa- 0 \n"
                                         "bnsa- 0, 4\n"
                                         "bnsla- 0 \n"
                                         "bnsla- 0, 4\n"
                                         "bns+ 0 \n"
                                         "bns+ 0, 4\n"
                                         "bnsl+ 0 \n"
                                         "bnsl+ 0, 4\n"
                                         "bnsa+ 0 \n"
                                         "bnsa+ 0, 4\n"
                                         "bnsla+ 0 \n"
                                         "bnsla+ 0, 4\n"
                                         "bun 0 \n"
                                         "bun 0, 4\n"
                                         "bunl 0 \n"
                                         "bunl 0, 4\n"
                                         "buna 0 \n"
                                         "buna 0, 4\n"
                                         "bunla 0 \n"
                                         "bunla 0, 4\n"
                                         "bun- 0 \n"
                                         "bun- 0, 4\n"
                                         "bunl- 0 \n"
                                         "bunl- 0, 4\n"
                                         "buna- 0 \n"
                                         "buna- 0, 4\n"
                                         "bunla- 0 \n"
                                         "bunla- 0, 4\n"
                                         "bun+ 0 \n"
                                         "bun+ 0, 4\n"
                                         "bunl+ 0 \n"
                                         "bunl+ 0, 4\n"
                                         "buna+ 0 \n"
                                         "buna+ 0, 4\n"
                                         "bunla+ 0 \n"
                                         "bunla+ 0, 4\n"
                                         "bnu 0 \n"
                                         "bnu 0, 4\n"
                                         "bnul 0 \n"
                                         "bnul 0, 4\n"
                                         "bnua 0 \n"
                                         "bnua 0, 4\n"
                                         "bnula 0 \n"
                                         "bnula 0, 4\n"
                                         "bnu- 0 \n"
                                         "bnu- 0, 4\n"
                                         "bnul- 0 \n"
                                         "bnul- 0, 4\n"
                                         "bnua- 0 \n"
                                         "bnua- 0, 4\n"
                                         "bnula- 0 \n"
                                         "bnula- 0, 4\n"
                                         "bnu+ 0 \n"
                                         "bnu+ 0, 4\n"
                                         "bnul+ 0 \n"
                                         "bnul+ 0, 4\n"
                                         "bnua+ 0 \n"
                                         "bnua+ 0, 4\n"
                                         "bnula+ 0 \n"
                                         "bnula+ 0, 4\n"
                                         "blr \n"
                                         "blrl \n"
                                         "bctr \n"
                                         "bctrl \n"
                                         "btlr 0\n"
                                         "btlrl 0\n"
                                         "btlr- 0\n"
                                         "btlrl- 0\n"
                                         "btlr+ 0\n"
                                         "btlrl+ 0\n"
                                         "btctr 0\n"
                                         "btctrl 0\n"
                                         "btctr- 0\n"
                                         "btctrl- 0\n"
                                         "btctr+ 0\n"
                                         "btctrl+ 0\n"
                                         "bflr 0\n"
                                         "bflrl 0\n"
                                         "bflr- 0\n"
                                         "bflrl- 0\n"
                                         "bflr+ 0\n"
                                         "bflrl+ 0\n"
                                         "bfctr 0\n"
                                         "bfctrl 0\n"
                                         "bfctr- 0\n"
                                         "bfctrl- 0\n"
                                         "bfctr+ 0\n"
                                         "bfctrl+ 0\n"
                                         "bdnzlr \n"
                                         "bdnzlrl \n"
                                         "bdnzlr- \n"
                                         "bdnzlrl- \n"
                                         "bdnzlr+ \n"
                                         "bdnzlrl+ \n"
                                         "bdnztlr 0\n"
                                         "bdnztlrl 0\n"
                                         "bdnztlr- 0\n"
                                         "bdnztlrl- 0\n"
                                         "bdnztlr+ 0\n"
                                         "bdnztlrl+ 0\n"
                                         "bdnzflr 0\n"
                                         "bdnzflrl 0\n"
                                         "bdnzflr- 0\n"
                                         "bdnzflrl- 0\n"
                                         "bdnzflr+ 0\n"
                                         "bdnzflrl+ 0\n"
                                         "bdzlr \n"
                                         "bdzlrl \n"
                                         "bdzlr- \n"
                                         "bdzlrl- \n"
                                         "bdzlr+ \n"
                                         "bdzlrl+ \n"
                                         "bdztlr 0\n"
                                         "bdztlrl 0\n"
                                         "bdztlr- 0\n"
                                         "bdztlrl- 0\n"
                                         "bdztlr+ 0\n"
                                         "bdztlrl+ 0\n"
                                         "bdzflr 0\n"
                                         "bdzflrl 0\n"
                                         "bdzflr- 0\n"
                                         "bdzflrl- 0\n"
                                         "bdzflr+ 0\n"
                                         "bdzflrl+ 0\n"
                                         "bltlr\n"
                                         "bltlr 0\n"
                                         "bltlrl\n"
                                         "bltlrl 0\n"
                                         "bltlr-\n"
                                         "bltlr- 0\n"
                                         "bltlrl-\n"
                                         "bltlrl- 0\n"
                                         "bltlr+\n"
                                         "bltlr+ 0\n"
                                         "bltlrl+\n"
                                         "bltlrl+ 0\n"
                                         "bltctr\n"
                                         "bltctr 0\n"
                                         "bltctrl\n"
                                         "bltctrl 0\n"
                                         "bltctr-\n"
                                         "bltctr- 0\n"
                                         "bltctrl-\n"
                                         "bltctrl- 0\n"
                                         "bltctr+\n"
                                         "bltctr+ 0\n"
                                         "bltctrl+\n"
                                         "bltctrl+ 0\n"
                                         "blelr\n"
                                         "blelr 0\n"
                                         "blelrl\n"
                                         "blelrl 0\n"
                                         "blelr-\n"
                                         "blelr- 0\n"
                                         "blelrl-\n"
                                         "blelrl- 0\n"
                                         "blelr+\n"
                                         "blelr+ 0\n"
                                         "blelrl+\n"
                                         "blelrl+ 0\n"
                                         "blectr\n"
                                         "blectr 0\n"
                                         "blectrl\n"
                                         "blectrl 0\n"
                                         "blectr-\n"
                                         "blectr- 0\n"
                                         "blectrl-\n"
                                         "blectrl- 0\n"
                                         "blectr+\n"
                                         "blectr+ 0\n"
                                         "blectrl+\n"
                                         "blectrl+ 0\n"
                                         "beqlr\n"
                                         "beqlr 0\n"
                                         "beqlrl\n"
                                         "beqlrl 0\n"
                                         "beqlr-\n"
                                         "beqlr- 0\n"
                                         "beqlrl-\n"
                                         "beqlrl- 0\n"
                                         "beqlr+\n"
                                         "beqlr+ 0\n"
                                         "beqlrl+\n"
                                         "beqlrl+ 0\n"
                                         "beqctr\n"
                                         "beqctr 0\n"
                                         "beqctrl\n"
                                         "beqctrl 0\n"
                                         "beqctr-\n"
                                         "beqctr- 0\n"
                                         "beqctrl-\n"
                                         "beqctrl- 0\n"
                                         "beqctr+\n"
                                         "beqctr+ 0\n"
                                         "beqctrl+\n"
                                         "beqctrl+ 0\n"
                                         "bgelr\n"
                                         "bgelr 0\n"
                                         "bgelrl\n"
                                         "bgelrl 0\n"
                                         "bgelr-\n"
                                         "bgelr- 0\n"
                                         "bgelrl-\n"
                                         "bgelrl- 0\n"
                                         "bgelr+\n"
                                         "bgelr+ 0\n"
                                         "bgelrl+\n"
                                         "bgelrl+ 0\n"
                                         "bgectr\n"
                                         "bgectr 0\n"
                                         "bgectrl\n"
                                         "bgectrl 0\n"
                                         "bgectr-\n"
                                         "bgectr- 0\n"
                                         "bgectrl-\n"
                                         "bgectrl- 0\n"
                                         "bgectr+\n"
                                         "bgectr+ 0\n"
                                         "bgectrl+\n"
                                         "bgectrl+ 0\n"
                                         "bgtlr\n"
                                         "bgtlr 0\n"
                                         "bgtlrl\n"
                                         "bgtlrl 0\n"
                                         "bgtlr-\n"
                                         "bgtlr- 0\n"
                                         "bgtlrl-\n"
                                         "bgtlrl- 0\n"
                                         "bgtlr+\n"
                                         "bgtlr+ 0\n"
                                         "bgtlrl+\n"
                                         "bgtlrl+ 0\n"
                                         "bgtctr\n"
                                         "bgtctr 0\n"
                                         "bgtctrl\n"
                                         "bgtctrl 0\n"
                                         "bgtctr-\n"
                                         "bgtctr- 0\n"
                                         "bgtctrl-\n"
                                         "bgtctrl- 0\n"
                                         "bgtctr+\n"
                                         "bgtctr+ 0\n"
                                         "bgtctrl+\n"
                                         "bgtctrl+ 0\n"
                                         "bnllr\n"
                                         "bnllr 0\n"
                                         "bnllrl\n"
                                         "bnllrl 0\n"
                                         "bnllr-\n"
                                         "bnllr- 0\n"
                                         "bnllrl-\n"
                                         "bnllrl- 0\n"
                                         "bnllr+\n"
                                         "bnllr+ 0\n"
                                         "bnllrl+\n"
                                         "bnllrl+ 0\n"
                                         "bnlctr\n"
                                         "bnlctr 0\n"
                                         "bnlctrl\n"
                                         "bnlctrl 0\n"
                                         "bnlctr-\n"
                                         "bnlctr- 0\n"
                                         "bnlctrl-\n"
                                         "bnlctrl- 0\n"
                                         "bnlctr+\n"
                                         "bnlctr+ 0\n"
                                         "bnlctrl+\n"
                                         "bnlctrl+ 0\n"
                                         "bnelr\n"
                                         "bnelr 0\n"
                                         "bnelrl\n"
                                         "bnelrl 0\n"
                                         "bnelr-\n"
                                         "bnelr- 0\n"
                                         "bnelrl-\n"
                                         "bnelrl- 0\n"
                                         "bnelr+\n"
                                         "bnelr+ 0\n"
                                         "bnelrl+\n"
                                         "bnelrl+ 0\n"
                                         "bnectr\n"
                                         "bnectr 0\n"
                                         "bnectrl\n"
                                         "bnectrl 0\n"
                                         "bnectr-\n"
                                         "bnectr- 0\n"
                                         "bnectrl-\n"
                                         "bnectrl- 0\n"
                                         "bnectr+\n"
                                         "bnectr+ 0\n"
                                         "bnectrl+\n"
                                         "bnectrl+ 0\n"
                                         "bnglr\n"
                                         "bnglr 0\n"
                                         "bnglrl\n"
                                         "bnglrl 0\n"
                                         "bnglr-\n"
                                         "bnglr- 0\n"
                                         "bnglrl-\n"
                                         "bnglrl- 0\n"
                                         "bnglr+\n"
                                         "bnglr+ 0\n"
                                         "bnglrl+\n"
                                         "bnglrl+ 0\n"
                                         "bngctr\n"
                                         "bngctr 0\n"
                                         "bngctrl\n"
                                         "bngctrl 0\n"
                                         "bngctr-\n"
                                         "bngctr- 0\n"
                                         "bngctrl-\n"
                                         "bngctrl- 0\n"
                                         "bngctr+\n"
                                         "bngctr+ 0\n"
                                         "bngctrl+\n"
                                         "bngctrl+ 0\n"
                                         "bsolr\n"
                                         "bsolr 0\n"
                                         "bsolrl\n"
                                         "bsolrl 0\n"
                                         "bsolr-\n"
                                         "bsolr- 0\n"
                                         "bsolrl-\n"
                                         "bsolrl- 0\n"
                                         "bsolr+\n"
                                         "bsolr+ 0\n"
                                         "bsolrl+\n"
                                         "bsolrl+ 0\n"
                                         "bsoctr\n"
                                         "bsoctr 0\n"
                                         "bsoctrl\n"
                                         "bsoctrl 0\n"
                                         "bsoctr-\n"
                                         "bsoctr- 0\n"
                                         "bsoctrl-\n"
                                         "bsoctrl- 0\n"
                                         "bsoctr+\n"
                                         "bsoctr+ 0\n"
                                         "bsoctrl+\n"
                                         "bsoctrl+ 0\n"
                                         "bnslr\n"
                                         "bnslr 0\n"
                                         "bnslrl\n"
                                         "bnslrl 0\n"
                                         "bnslr-\n"
                                         "bnslr- 0\n"
                                         "bnslrl-\n"
                                         "bnslrl- 0\n"
                                         "bnslr+\n"
                                         "bnslr+ 0\n"
                                         "bnslrl+\n"
                                         "bnslrl+ 0\n"
                                         "bnsctr\n"
                                         "bnsctr 0\n"
                                         "bnsctrl\n"
                                         "bnsctrl 0\n"
                                         "bnsctr-\n"
                                         "bnsctr- 0\n"
                                         "bnsctrl-\n"
                                         "bnsctrl- 0\n"
                                         "bnsctr+\n"
                                         "bnsctr+ 0\n"
                                         "bnsctrl+\n"
                                         "bnsctrl+ 0\n"
                                         "bunlr\n"
                                         "bunlr 0\n"
                                         "bunlrl\n"
                                         "bunlrl 0\n"
                                         "bunlr-\n"
                                         "bunlr- 0\n"
                                         "bunlrl-\n"
                                         "bunlrl- 0\n"
                                         "bunlr+\n"
                                         "bunlr+ 0\n"
                                         "bunlrl+\n"
                                         "bunlrl+ 0\n"
                                         "bunctr\n"
                                         "bunctr 0\n"
                                         "bunctrl\n"
                                         "bunctrl 0\n"
                                         "bunctr-\n"
                                         "bunctr- 0\n"
                                         "bunctrl-\n"
                                         "bunctrl- 0\n"
                                         "bunctr+\n"
                                         "bunctr+ 0\n"
                                         "bunctrl+\n"
                                         "bunctrl+ 0\n"
                                         "bnulr\n"
                                         "bnulr 0\n"
                                         "bnulrl\n"
                                         "bnulrl 0\n"
                                         "bnulr-\n"
                                         "bnulr- 0\n"
                                         "bnulrl-\n"
                                         "bnulrl- 0\n"
                                         "bnulr+\n"
                                         "bnulr+ 0\n"
                                         "bnulrl+\n"
                                         "bnulrl+ 0\n"
                                         "bnuctr\n"
                                         "bnuctr 0\n"
                                         "bnuctrl\n"
                                         "bnuctrl 0\n"
                                         "bnuctr-\n"
                                         "bnuctr- 0\n"
                                         "bnuctrl-\n"
                                         "bnuctrl- 0\n"
                                         "bnuctr+\n"
                                         "bnuctr+ 0\n"
                                         "bnuctrl+\n"
                                         "bnuctrl+ 0\n";

constexpr u8 extended_expect[] = {
    0x38, 0x04, 0xff, 0xf8, 0x3c, 0x04, 0xff, 0xf8, 0x30, 0x04, 0xff, 0xf8, 0x34, 0x04, 0xff, 0xf8,
    0x2c, 0x00, 0x00, 0x04, 0x2c, 0x04, 0x00, 0x08, 0x7c, 0x00, 0x20, 0x00, 0x7c, 0x04, 0x40, 0x00,
    0x28, 0x00, 0x00, 0x04, 0x28, 0x04, 0x00, 0x08, 0x7c, 0x00, 0x20, 0x40, 0x7c, 0x04, 0x40, 0x40,
    0x4c, 0x00, 0x02, 0x42, 0x4c, 0x00, 0x01, 0x82, 0x4c, 0x04, 0x23, 0x82, 0x4c, 0x04, 0x20, 0x42,
    0x7e, 0x00, 0x20, 0x08, 0x0e, 0x00, 0x00, 0x04, 0x7e, 0x80, 0x20, 0x08, 0x0e, 0x80, 0x00, 0x04,
    0x7c, 0x80, 0x20, 0x08, 0x0c, 0x80, 0x00, 0x04, 0x7d, 0x80, 0x20, 0x08, 0x0d, 0x80, 0x00, 0x04,
    0x7d, 0x00, 0x20, 0x08, 0x0d, 0x00, 0x00, 0x04, 0x7d, 0x80, 0x20, 0x08, 0x0d, 0x80, 0x00, 0x04,
    0x7f, 0x00, 0x20, 0x08, 0x0f, 0x00, 0x00, 0x04, 0x7e, 0x80, 0x20, 0x08, 0x0e, 0x80, 0x00, 0x04,
    0x7c, 0x40, 0x20, 0x08, 0x0c, 0x40, 0x00, 0x04, 0x7c, 0xc0, 0x20, 0x08, 0x0c, 0xc0, 0x00, 0x04,
    0x7c, 0xa0, 0x20, 0x08, 0x0c, 0xa0, 0x00, 0x04, 0x7c, 0x20, 0x20, 0x08, 0x0c, 0x20, 0x00, 0x04,
    0x7c, 0xa0, 0x20, 0x08, 0x0c, 0xa0, 0x00, 0x04, 0x7c, 0xc0, 0x20, 0x08, 0x0c, 0xc0, 0x00, 0x04,
    0x7f, 0xe0, 0x00, 0x08, 0x7c, 0x01, 0x03, 0xa6, 0x7c, 0x01, 0x02, 0xa6, 0x7c, 0x08, 0x03, 0xa6,
    0x7c, 0x08, 0x02, 0xa6, 0x7c, 0x09, 0x03, 0xa6, 0x7c, 0x09, 0x02, 0xa6, 0x7c, 0x12, 0x03, 0xa6,
    0x7c, 0x12, 0x02, 0xa6, 0x7c, 0x13, 0x03, 0xa6, 0x7c, 0x13, 0x02, 0xa6, 0x7c, 0x16, 0x03, 0xa6,
    0x7c, 0x16, 0x02, 0xa6, 0x7c, 0x19, 0x03, 0xa6, 0x7c, 0x19, 0x02, 0xa6, 0x7c, 0x1a, 0x03, 0xa6,
    0x7c, 0x1a, 0x02, 0xa6, 0x7c, 0x1b, 0x03, 0xa6, 0x7c, 0x1b, 0x02, 0xa6, 0x7c, 0x1a, 0x43, 0xa6,
    0x7c, 0x1a, 0x42, 0xa6, 0x7c, 0x1c, 0x43, 0xa6, 0x7c, 0x0c, 0x42, 0xe6, 0x7c, 0x1d, 0x43, 0xa6,
    0x7c, 0x0d, 0x42, 0xe6, 0x7c, 0x90, 0x43, 0xa6, 0x7c, 0x11, 0x42, 0xa6, 0x7c, 0x30, 0x83, 0xa6,
    0x7c, 0x12, 0x82, 0xa6, 0x7c, 0x31, 0x83, 0xa6, 0x7c, 0x13, 0x82, 0xa6, 0x7c, 0x38, 0x83, 0xa6,
    0x7c, 0x1a, 0x82, 0xa6, 0x7c, 0x39, 0x83, 0xa6, 0x7c, 0x1b, 0x82, 0xa6, 0x60, 0x00, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x04, 0x3c, 0x00, 0x00, 0x04, 0x38, 0x08, 0x00, 0x04, 0x7c, 0x0f, 0xf1, 0x20,
    0x7c, 0x04, 0x02, 0xa6, 0x7c, 0x0c, 0x42, 0xe6, 0x7c, 0x80, 0x03, 0xa6, 0x7c, 0x08, 0x20, 0x50,
    0x7c, 0x08, 0x20, 0x51, 0x7c, 0x08, 0x24, 0x50, 0x7c, 0x08, 0x24, 0x51, 0x7c, 0x08, 0x20, 0x10,
    0x7c, 0x08, 0x20, 0x11, 0x7c, 0x08, 0x24, 0x10, 0x7c, 0x08, 0x24, 0x11, 0x54, 0x80, 0x60, 0x0e,
    0x54, 0x80, 0x60, 0x0f, 0x54, 0x80, 0xa6, 0x3e, 0x54, 0x80, 0xa6, 0x3f, 0x50, 0x80, 0xa3, 0x26,
    0x50, 0x80, 0xa3, 0x27, 0x50, 0x80, 0x63, 0x26, 0x50, 0x80, 0x63, 0x27, 0x54, 0x80, 0x40, 0x3e,
    0x54, 0x80, 0x40, 0x3f, 0x54, 0x80, 0xc0, 0x3e, 0x54, 0x80, 0xc0, 0x3f, 0x5c, 0x80, 0x40, 0x3e,
    0x5c, 0x80, 0x40, 0x3f, 0x54, 0x80, 0x40, 0x2e, 0x54, 0x80, 0x40, 0x2f, 0x54, 0x80, 0xc2, 0x3e,
    0x54, 0x80, 0xc2, 0x3f, 0x54, 0x80, 0x02, 0x3e, 0x54, 0x80, 0x02, 0x3f, 0x54, 0x80, 0x00, 0x2e,
    0x54, 0x80, 0x00, 0x2f, 0x54, 0x80, 0x41, 0x2e, 0x54, 0x80, 0x41, 0x2f, 0x7c, 0x80, 0x23, 0x78,
    0x7c, 0x80, 0x23, 0x79, 0x7c, 0x80, 0x20, 0xf8, 0x7c, 0x80, 0x20, 0xf9, 0x41, 0x80, 0x00, 0x04,
    0x41, 0x80, 0x00, 0x05, 0x41, 0x80, 0x00, 0x06, 0x41, 0x80, 0x00, 0x07, 0x41, 0x80, 0x00, 0x04,
    0x41, 0x80, 0x00, 0x05, 0x41, 0x80, 0x00, 0x06, 0x41, 0x80, 0x00, 0x07, 0x41, 0xa0, 0x00, 0x04,
    0x41, 0xa0, 0x00, 0x05, 0x41, 0xa0, 0x00, 0x06, 0x41, 0xa0, 0x00, 0x07, 0x40, 0x80, 0x00, 0x04,
    0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x07, 0x40, 0x80, 0x00, 0x04,
    0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x07, 0x40, 0xa0, 0x00, 0x04,
    0x40, 0xa0, 0x00, 0x05, 0x40, 0xa0, 0x00, 0x06, 0x40, 0xa0, 0x00, 0x07, 0x42, 0x00, 0x00, 0x00,
    0x42, 0x00, 0x00, 0x01, 0x42, 0x00, 0x00, 0x02, 0x42, 0x00, 0x00, 0x03, 0x42, 0x00, 0x00, 0x00,
    0x42, 0x00, 0x00, 0x01, 0x42, 0x00, 0x00, 0x02, 0x42, 0x00, 0x00, 0x03, 0x42, 0x20, 0x00, 0x00,
    0x42, 0x20, 0x00, 0x01, 0x42, 0x20, 0x00, 0x02, 0x42, 0x20, 0x00, 0x03, 0x41, 0x00, 0x00, 0x04,
    0x41, 0x00, 0x00, 0x05, 0x41, 0x00, 0x00, 0x06, 0x41, 0x00, 0x00, 0x07, 0x41, 0x00, 0x00, 0x04,
    0x41, 0x00, 0x00, 0x05, 0x41, 0x00, 0x00, 0x06, 0x41, 0x00, 0x00, 0x07, 0x41, 0x20, 0x00, 0x04,
    0x41, 0x20, 0x00, 0x05, 0x41, 0x20, 0x00, 0x06, 0x41, 0x20, 0x00, 0x07, 0x40, 0x00, 0x00, 0x04,
    0x40, 0x00, 0x00, 0x05, 0x40, 0x00, 0x00, 0x06, 0x40, 0x00, 0x00, 0x07, 0x40, 0x00, 0x00, 0x04,
    0x40, 0x00, 0x00, 0x05, 0x40, 0x00, 0x00, 0x06, 0x40, 0x00, 0x00, 0x07, 0x40, 0x20, 0x00, 0x04,
    0x40, 0x20, 0x00, 0x05, 0x40, 0x20, 0x00, 0x06, 0x40, 0x20, 0x00, 0x07, 0x42, 0x40, 0x00, 0x00,
    0x42, 0x40, 0x00, 0x01, 0x42, 0x40, 0x00, 0x02, 0x42, 0x40, 0x00, 0x03, 0x42, 0x40, 0x00, 0x00,
    0x42, 0x40, 0x00, 0x01, 0x42, 0x40, 0x00, 0x02, 0x42, 0x40, 0x00, 0x03, 0x42, 0x60, 0x00, 0x00,
    0x42, 0x60, 0x00, 0x01, 0x42, 0x60, 0x00, 0x02, 0x42, 0x60, 0x00, 0x03, 0x41, 0x40, 0x00, 0x04,
    0x41, 0x40, 0x00, 0x05, 0x41, 0x40, 0x00, 0x06, 0x41, 0x40, 0x00, 0x07, 0x41, 0x40, 0x00, 0x04,
    0x41, 0x40, 0x00, 0x05, 0x41, 0x40, 0x00, 0x06, 0x41, 0x40, 0x00, 0x07, 0x41, 0x60, 0x00, 0x04,
    0x41, 0x60, 0x00, 0x05, 0x41, 0x60, 0x00, 0x06, 0x41, 0x60, 0x00, 0x07, 0x40, 0x40, 0x00, 0x04,
    0x40, 0x40, 0x00, 0x05, 0x40, 0x40, 0x00, 0x06, 0x40, 0x40, 0x00, 0x07, 0x40, 0x40, 0x00, 0x04,
    0x40, 0x40, 0x00, 0x05, 0x40, 0x40, 0x00, 0x06, 0x40, 0x40, 0x00, 0x07, 0x40, 0x60, 0x00, 0x04,
    0x40, 0x60, 0x00, 0x05, 0x40, 0x60, 0x00, 0x06, 0x40, 0x60, 0x00, 0x07, 0x41, 0x80, 0x00, 0x00,
    0x41, 0x80, 0x00, 0x04, 0x41, 0x80, 0x00, 0x01, 0x41, 0x80, 0x00, 0x05, 0x41, 0x80, 0x00, 0x02,
    0x41, 0x80, 0x00, 0x06, 0x41, 0x80, 0x00, 0x03, 0x41, 0x80, 0x00, 0x07, 0x41, 0x80, 0x00, 0x00,
    0x41, 0x80, 0x00, 0x04, 0x41, 0x80, 0x00, 0x01, 0x41, 0x80, 0x00, 0x05, 0x41, 0x80, 0x00, 0x02,
    0x41, 0x80, 0x00, 0x06, 0x41, 0x80, 0x00, 0x03, 0x41, 0x80, 0x00, 0x07, 0x41, 0xa0, 0x00, 0x00,
    0x41, 0xa0, 0x00, 0x04, 0x41, 0xa0, 0x00, 0x01, 0x41, 0xa0, 0x00, 0x05, 0x41, 0xa0, 0x00, 0x02,
    0x41, 0xa0, 0x00, 0x06, 0x41, 0xa0, 0x00, 0x03, 0x41, 0xa0, 0x00, 0x07, 0x40, 0x81, 0x00, 0x00,
    0x40, 0x81, 0x00, 0x04, 0x40, 0x81, 0x00, 0x01, 0x40, 0x81, 0x00, 0x05, 0x40, 0x81, 0x00, 0x02,
    0x40, 0x81, 0x00, 0x06, 0x40, 0x81, 0x00, 0x03, 0x40, 0x81, 0x00, 0x07, 0x40, 0x81, 0x00, 0x00,
    0x40, 0x81, 0x00, 0x04, 0x40, 0x81, 0x00, 0x01, 0x40, 0x81, 0x00, 0x05, 0x40, 0x81, 0x00, 0x02,
    0x40, 0x81, 0x00, 0x06, 0x40, 0x81, 0x00, 0x03, 0x40, 0x81, 0x00, 0x07, 0x40, 0xa1, 0x00, 0x00,
    0x40, 0xa1, 0x00, 0x04, 0x40, 0xa1, 0x00, 0x01, 0x40, 0xa1, 0x00, 0x05, 0x40, 0xa1, 0x00, 0x02,
    0x40, 0xa1, 0x00, 0x06, 0x40, 0xa1, 0x00, 0x03, 0x40, 0xa1, 0x00, 0x07, 0x41, 0x82, 0x00, 0x00,
    0x41, 0x82, 0x00, 0x04, 0x41, 0x82, 0x00, 0x01, 0x41, 0x82, 0x00, 0x05, 0x41, 0x82, 0x00, 0x02,
    0x41, 0x82, 0x00, 0x06, 0x41, 0x82, 0x00, 0x03, 0x41, 0x82, 0x00, 0x07, 0x41, 0x82, 0x00, 0x00,
    0x41, 0x82, 0x00, 0x04, 0x41, 0x82, 0x00, 0x01, 0x41, 0x82, 0x00, 0x05, 0x41, 0x82, 0x00, 0x02,
    0x41, 0x82, 0x00, 0x06, 0x41, 0x82, 0x00, 0x03, 0x41, 0x82, 0x00, 0x07, 0x41, 0xa2, 0x00, 0x00,
    0x41, 0xa2, 0x00, 0x04, 0x41, 0xa2, 0x00, 0x01, 0x41, 0xa2, 0x00, 0x05, 0x41, 0xa2, 0x00, 0x02,
    0x41, 0xa2, 0x00, 0x06, 0x41, 0xa2, 0x00, 0x03, 0x41, 0xa2, 0x00, 0x07, 0x40, 0x80, 0x00, 0x00,
    0x40, 0x80, 0x00, 0x04, 0x40, 0x80, 0x00, 0x01, 0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x02,
    0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x03, 0x40, 0x80, 0x00, 0x07, 0x40, 0x80, 0x00, 0x00,
    0x40, 0x80, 0x00, 0x04, 0x40, 0x80, 0x00, 0x01, 0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x02,
    0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x03, 0x40, 0x80, 0x00, 0x07, 0x40, 0xa0, 0x00, 0x00,
    0x40, 0xa0, 0x00, 0x04, 0x40, 0xa0, 0x00, 0x01, 0x40, 0xa0, 0x00, 0x05, 0x40, 0xa0, 0x00, 0x02,
    0x40, 0xa0, 0x00, 0x06, 0x40, 0xa0, 0x00, 0x03, 0x40, 0xa0, 0x00, 0x07, 0x41, 0x81, 0x00, 0x00,
    0x41, 0x81, 0x00, 0x04, 0x41, 0x81, 0x00, 0x01, 0x41, 0x81, 0x00, 0x05, 0x41, 0x81, 0x00, 0x02,
    0x41, 0x81, 0x00, 0x06, 0x41, 0x81, 0x00, 0x03, 0x41, 0x81, 0x00, 0x07, 0x41, 0x81, 0x00, 0x00,
    0x41, 0x81, 0x00, 0x04, 0x41, 0x81, 0x00, 0x01, 0x41, 0x81, 0x00, 0x05, 0x41, 0x81, 0x00, 0x02,
    0x41, 0x81, 0x00, 0x06, 0x41, 0x81, 0x00, 0x03, 0x41, 0x81, 0x00, 0x07, 0x41, 0xa1, 0x00, 0x00,
    0x41, 0xa1, 0x00, 0x04, 0x41, 0xa1, 0x00, 0x01, 0x41, 0xa1, 0x00, 0x05, 0x41, 0xa1, 0x00, 0x02,
    0x41, 0xa1, 0x00, 0x06, 0x41, 0xa1, 0x00, 0x03, 0x41, 0xa1, 0x00, 0x07, 0x40, 0x80, 0x00, 0x00,
    0x40, 0x80, 0x00, 0x04, 0x40, 0x80, 0x00, 0x01, 0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x02,
    0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x03, 0x40, 0x80, 0x00, 0x07, 0x40, 0x80, 0x00, 0x00,
    0x40, 0x80, 0x00, 0x04, 0x40, 0x80, 0x00, 0x01, 0x40, 0x80, 0x00, 0x05, 0x40, 0x80, 0x00, 0x02,
    0x40, 0x80, 0x00, 0x06, 0x40, 0x80, 0x00, 0x03, 0x40, 0x80, 0x00, 0x07, 0x40, 0xa0, 0x00, 0x00,
    0x40, 0xa0, 0x00, 0x04, 0x40, 0xa0, 0x00, 0x01, 0x40, 0xa0, 0x00, 0x05, 0x40, 0xa0, 0x00, 0x02,
    0x40, 0xa0, 0x00, 0x06, 0x40, 0xa0, 0x00, 0x03, 0x40, 0xa0, 0x00, 0x07, 0x40, 0x82, 0x00, 0x00,
    0x40, 0x82, 0x00, 0x04, 0x40, 0x82, 0x00, 0x01, 0x40, 0x82, 0x00, 0x05, 0x40, 0x82, 0x00, 0x02,
    0x40, 0x82, 0x00, 0x06, 0x40, 0x82, 0x00, 0x03, 0x40, 0x82, 0x00, 0x07, 0x40, 0x82, 0x00, 0x00,
    0x40, 0x82, 0x00, 0x04, 0x40, 0x82, 0x00, 0x01, 0x40, 0x82, 0x00, 0x05, 0x40, 0x82, 0x00, 0x02,
    0x40, 0x82, 0x00, 0x06, 0x40, 0x82, 0x00, 0x03, 0x40, 0x82, 0x00, 0x07, 0x40, 0xa2, 0x00, 0x00,
    0x40, 0xa2, 0x00, 0x04, 0x40, 0xa2, 0x00, 0x01, 0x40, 0xa2, 0x00, 0x05, 0x40, 0xa2, 0x00, 0x02,
    0x40, 0xa2, 0x00, 0x06, 0x40, 0xa2, 0x00, 0x03, 0x40, 0xa2, 0x00, 0x07, 0x40, 0x81, 0x00, 0x00,
    0x40, 0x81, 0x00, 0x04, 0x40, 0x81, 0x00, 0x01, 0x40, 0x81, 0x00, 0x05, 0x40, 0x81, 0x00, 0x02,
    0x40, 0x81, 0x00, 0x06, 0x40, 0x81, 0x00, 0x03, 0x40, 0x81, 0x00, 0x07, 0x40, 0x81, 0x00, 0x00,
    0x40, 0x81, 0x00, 0x04, 0x40, 0x81, 0x00, 0x01, 0x40, 0x81, 0x00, 0x05, 0x40, 0x81, 0x00, 0x02,
    0x40, 0x81, 0x00, 0x06, 0x40, 0x81, 0x00, 0x03, 0x40, 0x81, 0x00, 0x07, 0x40, 0xa1, 0x00, 0x00,
    0x40, 0xa1, 0x00, 0x04, 0x40, 0xa1, 0x00, 0x01, 0x40, 0xa1, 0x00, 0x05, 0x40, 0xa1, 0x00, 0x02,
    0x40, 0xa1, 0x00, 0x06, 0x40, 0xa1, 0x00, 0x03, 0x40, 0xa1, 0x00, 0x07, 0x41, 0x83, 0x00, 0x00,
    0x41, 0x83, 0x00, 0x04, 0x41, 0x83, 0x00, 0x01, 0x41, 0x83, 0x00, 0x05, 0x41, 0x83, 0x00, 0x02,
    0x41, 0x83, 0x00, 0x06, 0x41, 0x83, 0x00, 0x03, 0x41, 0x83, 0x00, 0x07, 0x41, 0x83, 0x00, 0x00,
    0x41, 0x83, 0x00, 0x04, 0x41, 0x83, 0x00, 0x01, 0x41, 0x83, 0x00, 0x05, 0x41, 0x83, 0x00, 0x02,
    0x41, 0x83, 0x00, 0x06, 0x41, 0x83, 0x00, 0x03, 0x41, 0x83, 0x00, 0x07, 0x41, 0xa3, 0x00, 0x00,
    0x41, 0xa3, 0x00, 0x04, 0x41, 0xa3, 0x00, 0x01, 0x41, 0xa3, 0x00, 0x05, 0x41, 0xa3, 0x00, 0x02,
    0x41, 0xa3, 0x00, 0x06, 0x41, 0xa3, 0x00, 0x03, 0x41, 0xa3, 0x00, 0x07, 0x40, 0x83, 0x00, 0x00,
    0x40, 0x83, 0x00, 0x04, 0x40, 0x83, 0x00, 0x01, 0x40, 0x83, 0x00, 0x05, 0x40, 0x83, 0x00, 0x02,
    0x40, 0x83, 0x00, 0x06, 0x40, 0x83, 0x00, 0x03, 0x40, 0x83, 0x00, 0x07, 0x40, 0x83, 0x00, 0x00,
    0x40, 0x83, 0x00, 0x04, 0x40, 0x83, 0x00, 0x01, 0x40, 0x83, 0x00, 0x05, 0x40, 0x83, 0x00, 0x02,
    0x40, 0x83, 0x00, 0x06, 0x40, 0x83, 0x00, 0x03, 0x40, 0x83, 0x00, 0x07, 0x40, 0xa3, 0x00, 0x00,
    0x40, 0xa3, 0x00, 0x04, 0x40, 0xa3, 0x00, 0x01, 0x40, 0xa3, 0x00, 0x05, 0x40, 0xa3, 0x00, 0x02,
    0x40, 0xa3, 0x00, 0x06, 0x40, 0xa3, 0x00, 0x03, 0x40, 0xa3, 0x00, 0x07, 0x41, 0x83, 0x00, 0x00,
    0x41, 0x83, 0x00, 0x04, 0x41, 0x83, 0x00, 0x01, 0x41, 0x83, 0x00, 0x05, 0x41, 0x83, 0x00, 0x02,
    0x41, 0x83, 0x00, 0x06, 0x41, 0x83, 0x00, 0x03, 0x41, 0x83, 0x00, 0x07, 0x41, 0x83, 0x00, 0x00,
    0x41, 0x83, 0x00, 0x04, 0x41, 0x83, 0x00, 0x01, 0x41, 0x83, 0x00, 0x05, 0x41, 0x83, 0x00, 0x02,
    0x41, 0x83, 0x00, 0x06, 0x41, 0x83, 0x00, 0x03, 0x41, 0x83, 0x00, 0x07, 0x41, 0xa3, 0x00, 0x00,
    0x41, 0xa3, 0x00, 0x04, 0x41, 0xa3, 0x00, 0x01, 0x41, 0xa3, 0x00, 0x05, 0x41, 0xa3, 0x00, 0x02,
    0x41, 0xa3, 0x00, 0x06, 0x41, 0xa3, 0x00, 0x03, 0x41, 0xa3, 0x00, 0x07, 0x40, 0x83, 0x00, 0x00,
    0x40, 0x83, 0x00, 0x04, 0x40, 0x83, 0x00, 0x01, 0x40, 0x83, 0x00, 0x05, 0x40, 0x83, 0x00, 0x02,
    0x40, 0x83, 0x00, 0x06, 0x40, 0x83, 0x00, 0x03, 0x40, 0x83, 0x00, 0x07, 0x40, 0x83, 0x00, 0x00,
    0x40, 0x83, 0x00, 0x04, 0x40, 0x83, 0x00, 0x01, 0x40, 0x83, 0x00, 0x05, 0x40, 0x83, 0x00, 0x02,
    0x40, 0x83, 0x00, 0x06, 0x40, 0x83, 0x00, 0x03, 0x40, 0x83, 0x00, 0x07, 0x40, 0xa3, 0x00, 0x00,
    0x40, 0xa3, 0x00, 0x04, 0x40, 0xa3, 0x00, 0x01, 0x40, 0xa3, 0x00, 0x05, 0x40, 0xa3, 0x00, 0x02,
    0x40, 0xa3, 0x00, 0x06, 0x40, 0xa3, 0x00, 0x03, 0x40, 0xa3, 0x00, 0x07, 0x4e, 0x80, 0x00, 0x20,
    0x4e, 0x80, 0x00, 0x21, 0x4e, 0x80, 0x04, 0x20, 0x4e, 0x80, 0x04, 0x21, 0x4d, 0x80, 0x00, 0x20,
    0x4d, 0x80, 0x00, 0x21, 0x4d, 0x80, 0x00, 0x20, 0x4d, 0x80, 0x00, 0x21, 0x4d, 0xa0, 0x00, 0x20,
    0x4d, 0xa0, 0x00, 0x21, 0x4d, 0x80, 0x04, 0x20, 0x4d, 0x80, 0x04, 0x21, 0x4d, 0x80, 0x04, 0x20,
    0x4d, 0x80, 0x04, 0x21, 0x4d, 0xa0, 0x04, 0x20, 0x4d, 0xa0, 0x04, 0x21, 0x4c, 0x80, 0x00, 0x20,
    0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x20, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0xa0, 0x00, 0x20,
    0x4c, 0xa0, 0x00, 0x21, 0x4c, 0x80, 0x04, 0x20, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x20,
    0x4c, 0x80, 0x04, 0x21, 0x4c, 0xa0, 0x04, 0x20, 0x4c, 0xa0, 0x04, 0x21, 0x4e, 0x00, 0x00, 0x20,
    0x4e, 0x00, 0x00, 0x21, 0x4e, 0x00, 0x00, 0x20, 0x4e, 0x00, 0x00, 0x21, 0x4e, 0x20, 0x00, 0x20,
    0x4e, 0x20, 0x00, 0x21, 0x4d, 0x00, 0x00, 0x20, 0x4d, 0x00, 0x00, 0x21, 0x4d, 0x00, 0x00, 0x20,
    0x4d, 0x00, 0x00, 0x21, 0x4d, 0x20, 0x00, 0x20, 0x4d, 0x20, 0x00, 0x21, 0x4c, 0x00, 0x00, 0x20,
    0x4c, 0x00, 0x00, 0x21, 0x4c, 0x00, 0x00, 0x20, 0x4c, 0x00, 0x00, 0x21, 0x4c, 0x20, 0x00, 0x20,
    0x4c, 0x20, 0x00, 0x21, 0x4e, 0x40, 0x00, 0x20, 0x4e, 0x40, 0x00, 0x21, 0x4e, 0x40, 0x00, 0x20,
    0x4e, 0x40, 0x00, 0x21, 0x4e, 0x60, 0x00, 0x20, 0x4e, 0x60, 0x00, 0x21, 0x4d, 0x40, 0x00, 0x20,
    0x4d, 0x40, 0x00, 0x21, 0x4d, 0x40, 0x00, 0x20, 0x4d, 0x40, 0x00, 0x21, 0x4d, 0x60, 0x00, 0x20,
    0x4d, 0x60, 0x00, 0x21, 0x4c, 0x40, 0x00, 0x20, 0x4c, 0x40, 0x00, 0x21, 0x4c, 0x40, 0x00, 0x20,
    0x4c, 0x40, 0x00, 0x21, 0x4c, 0x60, 0x00, 0x20, 0x4c, 0x60, 0x00, 0x21, 0x4d, 0x80, 0x00, 0x20,
    0x4d, 0x80, 0x00, 0x20, 0x4d, 0x80, 0x00, 0x21, 0x4d, 0x80, 0x00, 0x21, 0x4d, 0x80, 0x00, 0x20,
    0x4d, 0x80, 0x00, 0x20, 0x4d, 0x80, 0x00, 0x21, 0x4d, 0x80, 0x00, 0x21, 0x4d, 0xa0, 0x00, 0x20,
    0x4d, 0xa0, 0x00, 0x20, 0x4d, 0xa0, 0x00, 0x21, 0x4d, 0xa0, 0x00, 0x21, 0x4d, 0x80, 0x04, 0x20,
    0x4d, 0x80, 0x04, 0x20, 0x4d, 0x80, 0x04, 0x21, 0x4d, 0x80, 0x04, 0x21, 0x4d, 0x80, 0x04, 0x20,
    0x4d, 0x80, 0x04, 0x20, 0x4d, 0x80, 0x04, 0x21, 0x4d, 0x80, 0x04, 0x21, 0x4d, 0xa0, 0x04, 0x20,
    0x4d, 0xa0, 0x04, 0x20, 0x4d, 0xa0, 0x04, 0x21, 0x4d, 0xa0, 0x04, 0x21, 0x4c, 0x81, 0x00, 0x20,
    0x4c, 0x81, 0x00, 0x20, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x20,
    0x4c, 0x81, 0x00, 0x20, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0xa1, 0x00, 0x20,
    0x4c, 0xa1, 0x00, 0x20, 0x4c, 0xa1, 0x00, 0x21, 0x4c, 0xa1, 0x00, 0x21, 0x4c, 0x81, 0x04, 0x20,
    0x4c, 0x81, 0x04, 0x20, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x20,
    0x4c, 0x81, 0x04, 0x20, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0xa1, 0x04, 0x20,
    0x4c, 0xa1, 0x04, 0x20, 0x4c, 0xa1, 0x04, 0x21, 0x4c, 0xa1, 0x04, 0x21, 0x4d, 0x82, 0x00, 0x20,
    0x4d, 0x82, 0x00, 0x20, 0x4d, 0x82, 0x00, 0x21, 0x4d, 0x82, 0x00, 0x21, 0x4d, 0x82, 0x00, 0x20,
    0x4d, 0x82, 0x00, 0x20, 0x4d, 0x82, 0x00, 0x21, 0x4d, 0x82, 0x00, 0x21, 0x4d, 0xa2, 0x00, 0x20,
    0x4d, 0xa2, 0x00, 0x20, 0x4d, 0xa2, 0x00, 0x21, 0x4d, 0xa2, 0x00, 0x21, 0x4d, 0x82, 0x04, 0x20,
    0x4d, 0x82, 0x04, 0x20, 0x4d, 0x82, 0x04, 0x21, 0x4d, 0x82, 0x04, 0x21, 0x4d, 0x82, 0x04, 0x20,
    0x4d, 0x82, 0x04, 0x20, 0x4d, 0x82, 0x04, 0x21, 0x4d, 0x82, 0x04, 0x21, 0x4d, 0xa2, 0x04, 0x20,
    0x4d, 0xa2, 0x04, 0x20, 0x4d, 0xa2, 0x04, 0x21, 0x4d, 0xa2, 0x04, 0x21, 0x4c, 0x80, 0x00, 0x20,
    0x4c, 0x80, 0x00, 0x20, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x20,
    0x4c, 0x80, 0x00, 0x20, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0xa0, 0x00, 0x20,
    0x4c, 0xa0, 0x00, 0x20, 0x4c, 0xa0, 0x00, 0x21, 0x4c, 0xa0, 0x00, 0x21, 0x4c, 0x80, 0x04, 0x20,
    0x4c, 0x80, 0x04, 0x20, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x20,
    0x4c, 0x80, 0x04, 0x20, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0xa0, 0x04, 0x20,
    0x4c, 0xa0, 0x04, 0x20, 0x4c, 0xa0, 0x04, 0x21, 0x4c, 0xa0, 0x04, 0x21, 0x4d, 0x81, 0x00, 0x20,
    0x4d, 0x81, 0x00, 0x20, 0x4d, 0x81, 0x00, 0x21, 0x4d, 0x81, 0x00, 0x21, 0x4d, 0x81, 0x00, 0x20,
    0x4d, 0x81, 0x00, 0x20, 0x4d, 0x81, 0x00, 0x21, 0x4d, 0x81, 0x00, 0x21, 0x4d, 0xa1, 0x00, 0x20,
    0x4d, 0xa1, 0x00, 0x20, 0x4d, 0xa1, 0x00, 0x21, 0x4d, 0xa1, 0x00, 0x21, 0x4d, 0x81, 0x04, 0x20,
    0x4d, 0x81, 0x04, 0x20, 0x4d, 0x81, 0x04, 0x21, 0x4d, 0x81, 0x04, 0x21, 0x4d, 0x81, 0x04, 0x20,
    0x4d, 0x81, 0x04, 0x20, 0x4d, 0x81, 0x04, 0x21, 0x4d, 0x81, 0x04, 0x21, 0x4d, 0xa1, 0x04, 0x20,
    0x4d, 0xa1, 0x04, 0x20, 0x4d, 0xa1, 0x04, 0x21, 0x4d, 0xa1, 0x04, 0x21, 0x4c, 0x80, 0x00, 0x20,
    0x4c, 0x80, 0x00, 0x20, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x20,
    0x4c, 0x80, 0x00, 0x20, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0x80, 0x00, 0x21, 0x4c, 0xa0, 0x00, 0x20,
    0x4c, 0xa0, 0x00, 0x20, 0x4c, 0xa0, 0x00, 0x21, 0x4c, 0xa0, 0x00, 0x21, 0x4c, 0x80, 0x04, 0x20,
    0x4c, 0x80, 0x04, 0x20, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x20,
    0x4c, 0x80, 0x04, 0x20, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0x80, 0x04, 0x21, 0x4c, 0xa0, 0x04, 0x20,
    0x4c, 0xa0, 0x04, 0x20, 0x4c, 0xa0, 0x04, 0x21, 0x4c, 0xa0, 0x04, 0x21, 0x4c, 0x82, 0x00, 0x20,
    0x4c, 0x82, 0x00, 0x20, 0x4c, 0x82, 0x00, 0x21, 0x4c, 0x82, 0x00, 0x21, 0x4c, 0x82, 0x00, 0x20,
    0x4c, 0x82, 0x00, 0x20, 0x4c, 0x82, 0x00, 0x21, 0x4c, 0x82, 0x00, 0x21, 0x4c, 0xa2, 0x00, 0x20,
    0x4c, 0xa2, 0x00, 0x20, 0x4c, 0xa2, 0x00, 0x21, 0x4c, 0xa2, 0x00, 0x21, 0x4c, 0x82, 0x04, 0x20,
    0x4c, 0x82, 0x04, 0x20, 0x4c, 0x82, 0x04, 0x21, 0x4c, 0x82, 0x04, 0x21, 0x4c, 0x82, 0x04, 0x20,
    0x4c, 0x82, 0x04, 0x20, 0x4c, 0x82, 0x04, 0x21, 0x4c, 0x82, 0x04, 0x21, 0x4c, 0xa2, 0x04, 0x20,
    0x4c, 0xa2, 0x04, 0x20, 0x4c, 0xa2, 0x04, 0x21, 0x4c, 0xa2, 0x04, 0x21, 0x4c, 0x81, 0x00, 0x20,
    0x4c, 0x81, 0x00, 0x20, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x20,
    0x4c, 0x81, 0x00, 0x20, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0x81, 0x00, 0x21, 0x4c, 0xa1, 0x00, 0x20,
    0x4c, 0xa1, 0x00, 0x20, 0x4c, 0xa1, 0x00, 0x21, 0x4c, 0xa1, 0x00, 0x21, 0x4c, 0x81, 0x04, 0x20,
    0x4c, 0x81, 0x04, 0x20, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x20,
    0x4c, 0x81, 0x04, 0x20, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0x81, 0x04, 0x21, 0x4c, 0xa1, 0x04, 0x20,
    0x4c, 0xa1, 0x04, 0x20, 0x4c, 0xa1, 0x04, 0x21, 0x4c, 0xa1, 0x04, 0x21, 0x4d, 0x83, 0x00, 0x20,
    0x4d, 0x83, 0x00, 0x20, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x20,
    0x4d, 0x83, 0x00, 0x20, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0xa3, 0x00, 0x20,
    0x4d, 0xa3, 0x00, 0x20, 0x4d, 0xa3, 0x00, 0x21, 0x4d, 0xa3, 0x00, 0x21, 0x4d, 0x83, 0x04, 0x20,
    0x4d, 0x83, 0x04, 0x20, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x20,
    0x4d, 0x83, 0x04, 0x20, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0xa3, 0x04, 0x20,
    0x4d, 0xa3, 0x04, 0x20, 0x4d, 0xa3, 0x04, 0x21, 0x4d, 0xa3, 0x04, 0x21, 0x4c, 0x83, 0x00, 0x20,
    0x4c, 0x83, 0x00, 0x20, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x20,
    0x4c, 0x83, 0x00, 0x20, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0xa3, 0x00, 0x20,
    0x4c, 0xa3, 0x00, 0x20, 0x4c, 0xa3, 0x00, 0x21, 0x4c, 0xa3, 0x00, 0x21, 0x4c, 0x83, 0x04, 0x20,
    0x4c, 0x83, 0x04, 0x20, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x20,
    0x4c, 0x83, 0x04, 0x20, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0xa3, 0x04, 0x20,
    0x4c, 0xa3, 0x04, 0x20, 0x4c, 0xa3, 0x04, 0x21, 0x4c, 0xa3, 0x04, 0x21, 0x4d, 0x83, 0x00, 0x20,
    0x4d, 0x83, 0x00, 0x20, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x20,
    0x4d, 0x83, 0x00, 0x20, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0x83, 0x00, 0x21, 0x4d, 0xa3, 0x00, 0x20,
    0x4d, 0xa3, 0x00, 0x20, 0x4d, 0xa3, 0x00, 0x21, 0x4d, 0xa3, 0x00, 0x21, 0x4d, 0x83, 0x04, 0x20,
    0x4d, 0x83, 0x04, 0x20, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x20,
    0x4d, 0x83, 0x04, 0x20, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0x83, 0x04, 0x21, 0x4d, 0xa3, 0x04, 0x20,
    0x4d, 0xa3, 0x04, 0x20, 0x4d, 0xa3, 0x04, 0x21, 0x4d, 0xa3, 0x04, 0x21, 0x4c, 0x83, 0x00, 0x20,
    0x4c, 0x83, 0x00, 0x20, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x20,
    0x4c, 0x83, 0x00, 0x20, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0x83, 0x00, 0x21, 0x4c, 0xa3, 0x00, 0x20,
    0x4c, 0xa3, 0x00, 0x20, 0x4c, 0xa3, 0x00, 0x21, 0x4c, 0xa3, 0x00, 0x21, 0x4c, 0x83, 0x04, 0x20,
    0x4c, 0x83, 0x04, 0x20, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x20,
    0x4c, 0x83, 0x04, 0x20, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0x83, 0x04, 0x21, 0x4c, 0xa3, 0x04, 0x20,
    0x4c, 0xa3, 0x04, 0x20, 0x4c, 0xa3, 0x04, 0x21, 0x4c, 0xa3, 0x04, 0x21,
};

TEST(Assembler, AllExtendedInstructions)
{
  auto res = Assemble(extended_instructions, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(extended_expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], extended_expect[i]) << "->i=" << i;
  }
}

TEST(Assembler, ByteDirectivesSimple)
{
  constexpr char assembly[] = ".byte 0\n"
                              ".Byte 0xff\n"
                              ".bYte 0x100\n"
                              ".2bYTe 0\n"
                              ".2bytE 0xff\n"
                              ".2BYte 0x100\n"
                              ".2bYTe 0xffff\n"
                              ".2byTE 0x10000\n"
                              ".4BytE 0\n"
                              ".4BYTe 0xff\n"
                              ".4bYTE 0x100\n"
                              ".4ByTE 0xffff\n"
                              ".4BYtE 0x10000\n"
                              ".4BYTE 0xffffffff\n"
                              ".4ByTe 0x100000000\n"
                              ".8bYtE 0\n"
                              ".8byte 0xff\n"
                              ".8byte 0x100\n"
                              ".8byte 0xffff\n"
                              ".8byte 0x10000\n"
                              ".8byte 0xffffffff\n"
                              ".8byte 0x100000000\n"
                              ".8byte 0xffffffffffffffff\n"
                              ".8byte 0x10000000000000000\n";
  constexpr u8 expect[] = {
      0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x01, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00,
      0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00,
      0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, MultiOperandDirectives)
{
  constexpr char assembly[] = ".byte 0, 1, 2\n"
                              ".2byte 3, 4, 5\n"
                              ".4byte 6, 7, 8\n"
                              ".8byte 9, 10, 11\n";
  constexpr u8 expect[] = {
      0, 1, 2, 0, 3, 0, 4, 0, 5, 0, 0, 0, 6, 0,  0, 0, 7, 0, 0, 0, 8, 0,  0,
      0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 11,
  };
  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, OperandExpressionDirectives)
{
  constexpr char assembly[] = ".byte 0 + 1, 1 * 4, 2 * 8\n"
                              ".2byte 3*6*9, 5*5*12, 81/9\n"
                              ".4byte 1<<12, 5>>3, 8^8\n"
                              ".8byte 0b1010 & 0b1101, 0b1010 | 0b0101, 0x12 + 010\n";
  constexpr u8 expect[] = {
      1, 4, 16, 0, 162, 1, 44, 0, 9, 0, 0, 16, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,          0,
      0, 0, 0,  0, 0,   8, 0,  0, 0, 0, 0, 0,  0, 15, 0, 0, 0, 0, 0, 0, 0, 0x12 + 010,
  };
  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, FloatDirectives)
{
  constexpr char assembly[] = ".float 0\n"
                              ".float 1, 2, 3.0\n"
                              ".float 1.25, 1.5e6, -2e-5\n"
                              ".double 0\n"
                              ".double 1, 2, 3.0\n"
                              ".double 1.0000001, 0.0000025, .000006e9\n";
  constexpr u8 expect[] = {
      0,    0,    0,    0,    0x3f, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40,
      0x00, 0x00, 0x3f, 0xa0, 0x00, 0x00, 0x49, 0xb7, 0x1b, 0x00, 0xb7, 0xa7, 0xc5, 0xac,
      0,    0,    0,    0,    0,    0,    0,    0,    0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x08, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x1a, 0xd7, 0xf2, 0x9b, 0x3e, 0xc4,
      0xf8, 0xb5, 0x88, 0xe3, 0x68, 0xf1, 0x40, 0xb7, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, ZeroDirectives)
{
  constexpr char assembly[] = ".zeros 0\n"
                              ".zeros 1\n"
                              ".zeros 5 + 5\n";
  constexpr u8 expect[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, StringDirectives)
{
  constexpr char assembly[] = ".ascii \"test string\"\n"
                              ".ascii \"string with \\n escapes \\r\"\n"
                              ".ascii \"string with octals \\123 \\0\"\n"
                              ".ascii \"string with hex \\x12\\x45\\x9912\"\n"
                              ".asciz \"null terminator\"\n";
  constexpr u8 expect[] = {
      't', 'e',  's', 't', ' ', 's',    't', 'r',  'i',    'n',    'g',    's', 't', 'r', 'i', 'n',
      'g', ' ',  'w', 'i', 't', 'h',    ' ', '\n', ' ',    'e',    's',    'c', 'a', 'p', 'e', 's',
      ' ', '\r', 's', 't', 'r', 'i',    'n', 'g',  ' ',    'w',    'i',    't', 'h', ' ', 'o', 'c',
      't', 'a',  'l', 's', ' ', '\123', ' ', '\0', 's',    't',    'r',    'i', 'n', 'g', ' ', 'w',
      'i', 't',  'h', ' ', 'h', 'e',    'x', ' ',  '\x12', '\x45', '\x12', 'n', 'u', 'l', 'l', ' ',
      't', 'e',  'r', 'm', 'i', 'n',    'a', 't',  'o',    'r',    '\0'};
  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));

  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, RelocateDirective)
{
  constexpr char assembly[] = ".zeros 5\n"
                              ".locate 100\n"
                              ".zeros 9\n"
                              ".locate 110\n"
                              ".zeros 10\n"
                              ".locate 120\n"
                              ".zeros 29\n"
                              ".locate 120 + 5*5+4 + 1\n"
                              ".zeros 1\n";
  constexpr u32 expect_addr[] = {0, 100, 110, 120, 150};
  constexpr size_t expect_size[] = {5, 9, 10, 29, 1};

  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), sizeof(expect_size) / sizeof(expect_size[0]));
  for (size_t i = 0; i < code_blocks.size(); i++)
  {
    EXPECT_EQ(code_blocks[i].instructions.size(), expect_size[i]) << " -> i=" << i;
    EXPECT_EQ(code_blocks[i].block_address, expect_addr[i]) << " -> i=" << i;
  }
}

TEST(Assembler, AlignmentDirectives)
{
  constexpr char assembly_align[] = ".zeros 1\n"
                                    ".align 0\n"
                                    ".zeros 1\n"
                                    ".align 1\n"
                                    ".zeros 1\n"
                                    ".align 2\n"
                                    ".zeros 1\n"
                                    ".align 4\n"
                                    ".zeros 1\n"
                                    ".align 4\n"
                                    ".zeros 1\n"
                                    ".align 10\n"
                                    ".byte 1\n"
                                    ".padalign 0\n"
                                    ".byte 1\n"
                                    ".padalign 1\n"
                                    ".byte 1\n"
                                    ".padalign 2\n"
                                    ".byte 1\n"
                                    ".padalign 4\n"
                                    ".byte 1\n"
                                    ".padalign 4\n"
                                    ".byte 1\n"
                                    ".padalign 10\n";
  constexpr u32 expect_addr[] = {0, 4, 16, 32, 1024};

  auto res = Assemble(assembly_align, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), sizeof(expect_addr) / sizeof(expect_addr[0]));
  for (size_t i = 0; i < code_blocks.size(); i++)
  {
    EXPECT_EQ(code_blocks[i].block_address, expect_addr[i]) << " -> i=" << i;
  }

  auto&& last_block = code_blocks.back().instructions;
  ASSERT_EQ(last_block.size(), 1024);
  for (size_t i = 0; i < 3; i++)
  {
    EXPECT_EQ(last_block[i], 1) << " -> i=" << i;
  }
  EXPECT_EQ(last_block[3], 0) << " -> i=4";
  EXPECT_EQ(last_block[4], 1) << " -> i=4";
  for (size_t i = 5; i < 16; i++)
  {
    EXPECT_EQ(last_block[i], 0) << " -> i=" << i;
  }
  EXPECT_EQ(last_block[16], 1) << " -> i=16";
  for (size_t i = 17; i < 32; i++)
  {
    EXPECT_EQ(last_block[i], 0) << " -> i=" << i;
  }
  EXPECT_EQ(last_block[32], 1) << " -> i=32";
  for (size_t i = 33; i < last_block.size(); i++)
  {
    EXPECT_EQ(last_block[i], 0) << " -> i=" << i;
  }
}

TEST(Assembler, SkipDirective)
{
  constexpr char assembly_align[] = ".byte 5\n"
                                    ".skip 0\n"
                                    ".byte 6\n"
                                    ".skip 1\n"
                                    ".byte 7\n"
                                    ".skip 10 * 10\n"
                                    ".byte 8\n";
  constexpr u32 expect_addr[] = {0, 3, 104};

  auto res = Assemble(assembly_align, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), sizeof(expect_addr) / sizeof(expect_addr[0]));

  EXPECT_EQ(code_blocks[0].block_address, expect_addr[0]) << " -> i=0";
  ASSERT_EQ(code_blocks[0].instructions.size(), 2);
  EXPECT_EQ(code_blocks[0].instructions[0], 5) << " -> i=0";
  EXPECT_EQ(code_blocks[0].instructions[1], 6) << " -> i=0";

  EXPECT_EQ(code_blocks[1].block_address, expect_addr[1]) << " -> i=1";
  ASSERT_EQ(code_blocks[1].instructions.size(), 1);
  EXPECT_EQ(code_blocks[1].instructions[0], 7) << " -> i=1";

  EXPECT_EQ(code_blocks[2].block_address, expect_addr[2]) << " -> i=2";
  ASSERT_EQ(code_blocks[2].instructions.size(), 1);
  EXPECT_EQ(code_blocks[2].instructions[0], 8) << " -> i=2";
}

TEST(Assembler, DefvarDirective)
{
  constexpr char assembly[] = ".defvar NewVar, 0\n"
                              ".defvar NewVar2, 123\n"
                              ".defvar __Name, 1*2+3+4\n"
                              ".defvar AB_cd00, 5*5+__Name\n"
                              ".2byte NewVar\n"
                              ".2byte NewVar2\n"
                              ".4byte __Name\n"
                              ".4byte AB_cd00\n"
                              ".4byte AB_cd00 + NewVar2\n";
  constexpr u8 expect[] = {
      0, 0, 0, 123, 0, 0, 0, 9, 0, 0, 0, 34, 0, 0, 0, 157,
  };

  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, VariousOperandExpressions)
{
  constexpr char assembly[] = ".locate 0x400\n"
                              "b .\n"
                              "b .\n"
                              ".locate 0x800\n"
                              "post_locate:\n"
                              "b `0x900`\n"
                              "b `0x800`\n"
                              "b `. + 0x10`\n"
                              "b post_locate\n"
                              "lis r0, post_locate_2@ha\n"
                              "ori r0, r0, post_locate_2@l\n"
                              "li r0, TestValue\n"
                              ".defvar TestValue, 1234\n"
                              "li r0, TestValue\n"
                              ".locate 0x80001234\n"
                              "post_locate_2:\n";
  constexpr u8 expect_0[] = {
      0x48, 0x00, 0x04, 0x00, 0x48, 0x00, 0x04, 0x04,
  };
  constexpr u8 expect_1[] = {
      0x48, 0x00, 0x01, 0x00, 0x4b, 0xff, 0xff, 0xfc, 0x48, 0x00, 0x00,
      0x10, 0x4b, 0xff, 0xff, 0xf4, 0x3c, 0x00, 0x80, 0x00, 0x60, 0x00,
      0x12, 0x34, 0x38, 0x00, 0x04, 0xd2, 0x38, 0x00, 0x04, 0xd2,
  };

  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 2);

  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect_0));
  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect_0[i]) << " -> i=" << i;
  }

  ASSERT_EQ(code_blocks[1].instructions.size(), sizeof(expect_1));
  for (size_t i = 0; i < code_blocks[1].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[1].instructions[i], expect_1[i]) << " -> i=" << i;
  }
}

TEST(Assembler, AbsRel)
{
  constexpr char assembly[] = ".locate 0x80001234\n"
                              "lbl0:\n"
                              ".defvar abs_loc, lbl0\n"
                              ".4byte lbl0\n"
                              ".2byte lbl0@ha\n"
                              ".2byte lbl0@l\n"
                              ".4byte .\n"
                              "b lbl0\n"
                              "b `abs_loc`\n";
  constexpr u8 expect[] = {
      0x80, 0x00, 0x12, 0x34, 0x80, 0x00, 0x12, 0x34, 0x80, 0x00,
      0x12, 0x3c, 0x4b, 0xff, 0xff, 0xf4, 0x4b, 0xff, 0xff, 0xf0,
  };

  auto res = Assemble(assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);

  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(expect));
  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], expect[i]) << " -> i=" << i;
  }
}

TEST(Assembler, BadTokens)
{
  constexpr char unterminated_str[] = ".ascii \"no terminator";
  constexpr char bad_hex_in_str[] = ".ascii \"\\xnot hex\"";
  constexpr char newline_in_str[] = ".ascii \"abc\nd\"";
  constexpr char bad_float_0[] = ".float";
  constexpr char bad_float_1[] = ".float 1.";
  constexpr char bad_float_2[] = ".float .";
  constexpr char bad_float_3[] = ".float -.5e";
  constexpr char bad_float_4[] = ".float -.6e+";

  EXPECT_TRUE(IsFailure(Assemble(unterminated_str, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_hex_in_str, 0)));
  EXPECT_TRUE(IsFailure(Assemble(newline_in_str, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_float_0, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_float_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_float_2, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_float_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bad_float_4, 0)));
}

TEST(Assembler, RangeTest)
{
  constexpr char gpr_range_0[] = "mr r3, -1";
  constexpr char gpr_range_1[] = "mr r3, 0";
  constexpr char gpr_range_2[] = "mr r3, 32";
  constexpr char gpr_range_3[] = "mr r3, 31";
  constexpr char crf_range_0[] = "cmpw -1, 0, 0";
  constexpr char crf_range_1[] = "cmpw 0, 0, 0";
  constexpr char crf_range_2[] = "cmpw 8, 0, 0";
  constexpr char crf_range_3[] = "cmpw 7, 0, 0";
  constexpr char bc_range_0[] = "beq 1 << 15";
  constexpr char bc_range_1[] = "beq (1 << 15) - 4";
  constexpr char bc_range_2[] = "beq -(1 << 15) - 4";
  constexpr char bc_range_3[] = "beq -(1 << 15)";
  constexpr char b_range_0[] = "b 1 << 25";
  constexpr char b_range_1[] = "b (1 << 25) - 4";
  constexpr char b_range_2[] = "b -(1 << 25) - 4";
  constexpr char b_range_3[] = "b -(1 << 25)";
  constexpr char crb_range_0[] = "cror -1, -1, -1";
  constexpr char crb_range_1[] = "cror 0, 0, 0";
  constexpr char crb_range_2[] = "cror 32, 32, 32";
  constexpr char crb_range_3[] = "cror 31, 31, 31";
  constexpr char off_range_0[] = "lwz r0, 1 << 15(r3)";
  constexpr char off_range_1[] = "lwz r0, (1 << 15) - 1(r3)";
  constexpr char off_range_2[] = "lwz r0, -(1 << 15) - 1(r3)";
  constexpr char off_range_3[] = "lwz r0, -(1 << 15)(r3)";
  constexpr char psoff_range_0[] = "psq_l f0, 1 << 11(r3), 0, 0";
  constexpr char psoff_range_1[] = "psq_l f0, (1 << 11) - 1(r3), 0, 0";
  constexpr char psoff_range_2[] = "psq_l f0, -(1 << 11) - 1(r3), 0, 0";
  constexpr char psoff_range_3[] = "psq_l f0, -(1 << 11)(r3), 0, 0";
  constexpr char simm_range_0[] = "addi r0, r1, 0x8000";
  constexpr char simm_range_1[] = "addi r0, r1, 0x7fff";
  constexpr char simm_range_2[] = "addi r0, r1, -0x8001";
  constexpr char simm_range_3[] = "addi r0, r1, -0x8000";
  constexpr char uimm_range_0[] = "andi. r0, r1, 0x10000";
  constexpr char uimm_range_1[] = "andi. r0, r1, 0xffff";
  constexpr char uimm_range_2[] = "andi. r0, r1, -1";
  constexpr char uimm_range_3[] = "andi. r0, r1, 0";

  EXPECT_TRUE(IsFailure(Assemble(gpr_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(gpr_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(gpr_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(gpr_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(crf_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(crf_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(crf_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(crf_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bc_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(bc_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(bc_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(bc_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(b_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(b_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(b_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(b_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(crb_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(crb_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(crb_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(crb_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(off_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(off_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(off_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(off_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(psoff_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(psoff_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(psoff_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(psoff_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(psoff_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(psoff_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(psoff_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(psoff_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(simm_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(simm_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(simm_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(simm_range_3, 0)));
  EXPECT_TRUE(IsFailure(Assemble(uimm_range_0, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(uimm_range_1, 0)));
  EXPECT_TRUE(IsFailure(Assemble(uimm_range_2, 0)));
  EXPECT_TRUE(!IsFailure(Assemble(uimm_range_3, 0)));
}

TEST(Assembly, MalformedExpressions)
{
  constexpr char missing_arg[] = "add 0, 1";
  constexpr char missing_paren_0[] = ".4byte (1 + 2), ((3 * 6) + 7";
  constexpr char missing_paren_1[] = ".4byte (1 + 2), `(3 * 6) + 7";
  constexpr char mismatched_paren[] = ".4byte (1 + 2), (`3 * 6) + 7`";
  constexpr char wrong_arg_format[] = "lwz r3, 100, r4";
  constexpr char no_operator[] = "b . .";
  constexpr char no_operand[] = "b 4 + +";

  auto res = Assemble(missing_arg, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Expected ',' but found '<EOF>'")
      << GetFailure(res).message;
  res = Assemble(missing_paren_0, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Expected ')' but found '<EOF>'")
      << GetFailure(res).message;
  res = Assemble(missing_paren_1, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Expected '`' but found '<EOF>'")
      << GetFailure(res).message;
  res = Assemble(mismatched_paren, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Expected '`' but found ')'")
      << GetFailure(res).message;
  res = Assemble(wrong_arg_format, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Expected '(' but found ','")
      << GetFailure(res).message;
  res = Assemble(no_operator, 0);
  EXPECT_TRUE(IsFailure(res) &&
              GetFailure(res).message == "Unexpected token '.' where line should have ended")
      << GetFailure(res).message;
  res = Assemble(no_operand, 0);
  EXPECT_TRUE(IsFailure(res) && GetFailure(res).message == "Unexpected token '+' in expression")
      << GetFailure(res).message;
}

// Modified listing of a subroutine, listing generated by IDA
// Expect bytes are based on disc contents
TEST(Assembly, RealAssembly)
{
  constexpr char real_assembly[] = ".locate 0x8046A690\n"
                                   ".defvar back_chain, -0x30\n"
                                   ".defvar var_28, -0x28\n"
                                   ".defvar pre_back_chain,  0\n"
                                   ".defvar sender_lr,  4\n"
                                   "stwu      r1, back_chain(r1)\n"
                                   "mfspr     r0, LR\n"
                                   "stw       r0, 0x30+sender_lr(r1)\n"
                                   "addi      r11, r1, 0x30+pre_back_chain\n"
                                   "bl        `0x802BCA84`\n"
                                   "li        r0, 0\n"
                                   "mr        r28, r7\n"
                                   "stw       r0, 0(r8)\n"
                                   "mr        r24, r3\n"
                                   "mr        r25, r4\n"
                                   "mr        r26, r5\n"
                                   "mr        r27, r6\n"
                                   "mr        r29, r8\n"
                                   "mr        r30, r9\n"
                                   "mr        r31, r10\n"
                                   "mr        r3, r28\n"
                                   "bl        `0x80468140`\n"
                                   "cmplwi    r3, 0x1A\n"
                                   "bge       loc_8046A6F0\n"
                                   "mulli     r0, r3, 0x14\n"
                                   "lis       r3, -0x7FA4\n"
                                   "addi      r3, r3, 0x870\n"
                                   "add       r3, r3, r0\n"
                                   "b         loc_8046A6F4\n"
                                   "loc_8046A6F0:\n"
                                   "li        r3, 0\n"
                                   "loc_8046A6F4:\n"
                                   "cmpwi     r3, 0\n"
                                   "beq       loc_8046A704\n"
                                   "lwz       r0, 0xC(r3)\n"
                                   "b         loc_8046A708\n"
                                   "loc_8046A704:\n"
                                   "li        r0, 0\n"
                                   "loc_8046A708:\n"
                                   "cmplw     r26, r0\n"
                                   "bge       loc_8046A7EC\n"
                                   "cmpwi     r26, 0\n"
                                   "bne       loc_8046A758\n"
                                   "mr        r12, r30\n"
                                   "mr        r3, r28\n"
                                   "mr        r4, r25\n"
                                   "addi      r5, r1, 0x30+var_28\n"
                                   "mtspr     CTR, r12\n"
                                   "bctrl\n"
                                   "cmpwi     r3, 0\n"
                                   "stw       r3, 0(r29)\n"
                                   "beq       loc_8046A744\n"
                                   "li        r3, 0\n"
                                   "b         loc_8046A7F0\n"
                                   "loc_8046A744:\n"
                                   "stw       r24, 0(r27)\n"
                                   "li        r0, 0\n"
                                   "li        r3, 0\n"
                                   "stw       r0, 0(r29)\n"
                                   "b         loc_8046A7F0\n"
                                   "loc_8046A758:\n"
                                   "cmplwi    r26, 1\n"
                                   "bne       loc_8046A7DC\n"
                                   "mr        r3, r28\n"
                                   "bl        `0x80468140`\n"
                                   "cmplwi    r3, 0x1A\n"
                                   "bge       loc_8046A784\n"
                                   "mulli     r0, r3, 0x14\n"
                                   "lis       r3, -0x7FA4\n"
                                   "addi      r3, r3, 0x870\n"
                                   "add       r3, r3, r0\n"
                                   "b         loc_8046A788\n"
                                   "loc_8046A784:\n"
                                   "li        r3, 0\n"
                                   "loc_8046A788:\n"
                                   "cmpwi     r3, 0\n"
                                   "beq       loc_8046A798\n"
                                   "lwz       r0, 8(r3)\n"
                                   "b         loc_8046A79C\n"
                                   "loc_8046A798:\n"
                                   "li        r0, 1\n"
                                   "loc_8046A79C:\n"
                                   "cmplwi    r0, 2\n"
                                   "bne       loc_8046A7DC\n"
                                   "mr        r12, r31\n"
                                   "mr        r3, r25\n"
                                   "mtspr     CTR, r12\n"
                                   "bctrl\n"
                                   "cmpwi     r3, 0\n"
                                   "stw       r3, 0(r29)\n"
                                   "beq       loc_8046A7C8\n"
                                   "li        r3, 0\n"
                                   "b         loc_8046A7F0\n"
                                   "loc_8046A7C8:\n"
                                   "stw       r24, 0(r27)\n"
                                   "li        r0, 0\n"
                                   "li        r3, 0\n"
                                   "stw       r0, 0(r29)\n"
                                   "b         loc_8046A7F0\n"
                                   "loc_8046A7DC:\n"
                                   "li        r0, -0x16\n"
                                   "li        r3, 0\n"
                                   "stw       r0, 0(r29)\n"
                                   "b         loc_8046A7F0\n"
                                   "loc_8046A7EC:\n"
                                   "li        r3, 1\n"
                                   "loc_8046A7F0:\n"
                                   "addi      r11, r1, 0x30+pre_back_chain\n"
                                   "bl        `0x802BCAD0`\n"
                                   "lwz       r0, 0x30+sender_lr(r1)\n"
                                   "mtspr     LR, r0\n"
                                   "addi      r1, r1, 0x30\n"
                                   "blr\n"
                                   "loc_8046A804:\n";

  constexpr u8 real_expect[] = {
      0x94, 0x21, 0xff, 0xd0, 0x7c, 0x08, 0x02, 0xa6, 0x90, 0x01, 0x00, 0x34, 0x39, 0x61, 0x00,
      0x30, 0x4b, 0xe5, 0x23, 0xe5, 0x38, 0x00, 0x00, 0x00, 0x7c, 0xfc, 0x3b, 0x78, 0x90, 0x08,
      0x00, 0x00, 0x7c, 0x78, 0x1b, 0x78, 0x7c, 0x99, 0x23, 0x78, 0x7c, 0xba, 0x2b, 0x78, 0x7c,
      0xdb, 0x33, 0x78, 0x7d, 0x1d, 0x43, 0x78, 0x7d, 0x3e, 0x4b, 0x78, 0x7d, 0x5f, 0x53, 0x78,
      0x7f, 0x83, 0xe3, 0x78, 0x4b, 0xff, 0xda, 0x71, 0x28, 0x03, 0x00, 0x1a, 0x40, 0x80, 0x00,
      0x18, 0x1c, 0x03, 0x00, 0x14, 0x3c, 0x60, 0x80, 0x5c, 0x38, 0x63, 0x08, 0x70, 0x7c, 0x63,
      0x02, 0x14, 0x48, 0x00, 0x00, 0x08, 0x38, 0x60, 0x00, 0x00, 0x2c, 0x03, 0x00, 0x00, 0x41,
      0x82, 0x00, 0x0c, 0x80, 0x03, 0x00, 0x0c, 0x48, 0x00, 0x00, 0x08, 0x38, 0x00, 0x00, 0x00,
      0x7c, 0x1a, 0x00, 0x40, 0x40, 0x80, 0x00, 0xe0, 0x2c, 0x1a, 0x00, 0x00, 0x40, 0x82, 0x00,
      0x44, 0x7f, 0xcc, 0xf3, 0x78, 0x7f, 0x83, 0xe3, 0x78, 0x7f, 0x24, 0xcb, 0x78, 0x38, 0xa1,
      0x00, 0x08, 0x7d, 0x89, 0x03, 0xa6, 0x4e, 0x80, 0x04, 0x21, 0x2c, 0x03, 0x00, 0x00, 0x90,
      0x7d, 0x00, 0x00, 0x41, 0x82, 0x00, 0x0c, 0x38, 0x60, 0x00, 0x00, 0x48, 0x00, 0x00, 0xb0,
      0x93, 0x1b, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x60, 0x00, 0x00, 0x90, 0x1d, 0x00,
      0x00, 0x48, 0x00, 0x00, 0x9c, 0x28, 0x1a, 0x00, 0x01, 0x40, 0x82, 0x00, 0x80, 0x7f, 0x83,
      0xe3, 0x78, 0x4b, 0xff, 0xd9, 0xdd, 0x28, 0x03, 0x00, 0x1a, 0x40, 0x80, 0x00, 0x18, 0x1c,
      0x03, 0x00, 0x14, 0x3c, 0x60, 0x80, 0x5c, 0x38, 0x63, 0x08, 0x70, 0x7c, 0x63, 0x02, 0x14,
      0x48, 0x00, 0x00, 0x08, 0x38, 0x60, 0x00, 0x00, 0x2c, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00,
      0x0c, 0x80, 0x03, 0x00, 0x08, 0x48, 0x00, 0x00, 0x08, 0x38, 0x00, 0x00, 0x01, 0x28, 0x00,
      0x00, 0x02, 0x40, 0x82, 0x00, 0x3c, 0x7f, 0xec, 0xfb, 0x78, 0x7f, 0x23, 0xcb, 0x78, 0x7d,
      0x89, 0x03, 0xa6, 0x4e, 0x80, 0x04, 0x21, 0x2c, 0x03, 0x00, 0x00, 0x90, 0x7d, 0x00, 0x00,
      0x41, 0x82, 0x00, 0x0c, 0x38, 0x60, 0x00, 0x00, 0x48, 0x00, 0x00, 0x2c, 0x93, 0x1b, 0x00,
      0x00, 0x38, 0x00, 0x00, 0x00, 0x38, 0x60, 0x00, 0x00, 0x90, 0x1d, 0x00, 0x00, 0x48, 0x00,
      0x00, 0x18, 0x38, 0x00, 0xff, 0xea, 0x38, 0x60, 0x00, 0x00, 0x90, 0x1d, 0x00, 0x00, 0x48,
      0x00, 0x00, 0x08, 0x38, 0x60, 0x00, 0x01, 0x39, 0x61, 0x00, 0x30, 0x4b, 0xe5, 0x22, 0xdd,
      0x80, 0x01, 0x00, 0x34, 0x7c, 0x08, 0x03, 0xa6, 0x38, 0x21, 0x00, 0x30, 0x4e, 0x80, 0x00,
      0x20,
  };

  auto res = Assemble(real_assembly, 0);
  ASSERT_TRUE(!IsFailure(res));
  auto&& code_blocks = GetT(res);
  ASSERT_EQ(code_blocks.size(), 1);
  ASSERT_EQ(code_blocks[0].instructions.size(), sizeof(real_expect));

  EXPECT_EQ(code_blocks[0].block_address, 0x8046a690);
  for (size_t i = 0; i < code_blocks[0].instructions.size(); i++)
  {
    EXPECT_EQ(code_blocks[0].instructions[i], real_expect[i]) << " -> i=" << i;
  }
}
