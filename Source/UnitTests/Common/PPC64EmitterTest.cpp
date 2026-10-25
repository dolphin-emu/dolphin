// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/PPC64Emitter.h"

using namespace PPC64Gen;
using enum GPR;
using enum FPR;
using enum VSR;
using enum VR;
using enum CRF;
using enum CCFlags;

namespace
{
struct EncodingCase
{
  const char* assembly;
  void (*emit)(PPC64XEmitter& e);
  u32 expected;
};

// The expected encodings were produced by assembling each case with GNU as -mpower8.
const EncodingCase encoding_cases[] = {
    {"addi 3,4,-1", [](PPC64XEmitter& e) { e.ADDI(R3, R4, -1); }, 0x3864FFFF},
    {"addi 31,1,32767", [](PPC64XEmitter& e) { e.ADDI(R31, R1, 32767); }, 0x3BE17FFF},
    {"addis 5,6,-32768", [](PPC64XEmitter& e) { e.ADDIS(R5, R6, -32768); }, 0x3CA68000},
    {"addic 7,8,100", [](PPC64XEmitter& e) { e.ADDIC(R7, R8, 100); }, 0x30E80064},
    {"addic. 7,8,-100", [](PPC64XEmitter& e) { e.ADDIC(R7, R8, -100, true); }, 0x34E8FF9C},
    {"subfic 9,10,5", [](PPC64XEmitter& e) { e.SUBFIC(R9, R10, 5); }, 0x212A0005},
    {"mulli 11,12,-7", [](PPC64XEmitter& e) { e.MULLI(R11, R12, -7); }, 0x1D6CFFF9},
    {"add 3,4,5", [](PPC64XEmitter& e) { e.ADD(R3, R4, R5); }, 0x7C642A14},
    {"add. 31,30,29", [](PPC64XEmitter& e) { e.ADD(R31, R30, R29, true); }, 0x7FFEEA15},
    {"addc 3,4,5", [](PPC64XEmitter& e) { e.ADDC(R3, R4, R5); }, 0x7C642814},
    {"addc. 31,30,29", [](PPC64XEmitter& e) { e.ADDC(R31, R30, R29, true); }, 0x7FFEE815},
    {"adde 3,4,5", [](PPC64XEmitter& e) { e.ADDE(R3, R4, R5); }, 0x7C642914},
    {"adde. 31,30,29", [](PPC64XEmitter& e) { e.ADDE(R31, R30, R29, true); }, 0x7FFEE915},
    {"subf 3,4,5", [](PPC64XEmitter& e) { e.SUBF(R3, R4, R5); }, 0x7C642850},
    {"subf. 31,30,29", [](PPC64XEmitter& e) { e.SUBF(R31, R30, R29, true); }, 0x7FFEE851},
    {"subfc 3,4,5", [](PPC64XEmitter& e) { e.SUBFC(R3, R4, R5); }, 0x7C642810},
    {"subfc. 31,30,29", [](PPC64XEmitter& e) { e.SUBFC(R31, R30, R29, true); }, 0x7FFEE811},
    {"subfe 3,4,5", [](PPC64XEmitter& e) { e.SUBFE(R3, R4, R5); }, 0x7C642910},
    {"subfe. 31,30,29", [](PPC64XEmitter& e) { e.SUBFE(R31, R30, R29, true); }, 0x7FFEE911},
    {"mullw 3,4,5", [](PPC64XEmitter& e) { e.MULLW(R3, R4, R5); }, 0x7C6429D6},
    {"mullw. 31,30,29", [](PPC64XEmitter& e) { e.MULLW(R31, R30, R29, true); }, 0x7FFEE9D7},
    {"mulld 3,4,5", [](PPC64XEmitter& e) { e.MULLD(R3, R4, R5); }, 0x7C6429D2},
    {"mulld. 31,30,29", [](PPC64XEmitter& e) { e.MULLD(R31, R30, R29, true); }, 0x7FFEE9D3},
    {"mulhw 3,4,5", [](PPC64XEmitter& e) { e.MULHW(R3, R4, R5); }, 0x7C642896},
    {"mulhw. 31,30,29", [](PPC64XEmitter& e) { e.MULHW(R31, R30, R29, true); }, 0x7FFEE897},
    {"mulhwu 3,4,5", [](PPC64XEmitter& e) { e.MULHWU(R3, R4, R5); }, 0x7C642816},
    {"mulhwu. 31,30,29", [](PPC64XEmitter& e) { e.MULHWU(R31, R30, R29, true); }, 0x7FFEE817},
    {"mulhd 3,4,5", [](PPC64XEmitter& e) { e.MULHD(R3, R4, R5); }, 0x7C642892},
    {"mulhd. 31,30,29", [](PPC64XEmitter& e) { e.MULHD(R31, R30, R29, true); }, 0x7FFEE893},
    {"mulhdu 3,4,5", [](PPC64XEmitter& e) { e.MULHDU(R3, R4, R5); }, 0x7C642812},
    {"mulhdu. 31,30,29", [](PPC64XEmitter& e) { e.MULHDU(R31, R30, R29, true); }, 0x7FFEE813},
    {"divw 3,4,5", [](PPC64XEmitter& e) { e.DIVW(R3, R4, R5); }, 0x7C642BD6},
    {"divw. 31,30,29", [](PPC64XEmitter& e) { e.DIVW(R31, R30, R29, true); }, 0x7FFEEBD7},
    {"divwu 3,4,5", [](PPC64XEmitter& e) { e.DIVWU(R3, R4, R5); }, 0x7C642B96},
    {"divwu. 31,30,29", [](PPC64XEmitter& e) { e.DIVWU(R31, R30, R29, true); }, 0x7FFEEB97},
    {"divd 3,4,5", [](PPC64XEmitter& e) { e.DIVD(R3, R4, R5); }, 0x7C642BD2},
    {"divd. 31,30,29", [](PPC64XEmitter& e) { e.DIVD(R31, R30, R29, true); }, 0x7FFEEBD3},
    {"divdu 3,4,5", [](PPC64XEmitter& e) { e.DIVDU(R3, R4, R5); }, 0x7C642B92},
    {"divdu. 31,30,29", [](PPC64XEmitter& e) { e.DIVDU(R31, R30, R29, true); }, 0x7FFEEB93},
    {"addze 3,4", [](PPC64XEmitter& e) { e.ADDZE(R3, R4); }, 0x7C640194},
    {"addze. 31,30", [](PPC64XEmitter& e) { e.ADDZE(R31, R30, true); }, 0x7FFE0195},
    {"addme 3,4", [](PPC64XEmitter& e) { e.ADDME(R3, R4); }, 0x7C6401D4},
    {"addme. 31,30", [](PPC64XEmitter& e) { e.ADDME(R31, R30, true); }, 0x7FFE01D5},
    {"subfze 3,4", [](PPC64XEmitter& e) { e.SUBFZE(R3, R4); }, 0x7C640190},
    {"subfze. 31,30", [](PPC64XEmitter& e) { e.SUBFZE(R31, R30, true); }, 0x7FFE0191},
    {"subfme 3,4", [](PPC64XEmitter& e) { e.SUBFME(R3, R4); }, 0x7C6401D0},
    {"subfme. 31,30", [](PPC64XEmitter& e) { e.SUBFME(R31, R30, true); }, 0x7FFE01D1},
    {"neg 3,4", [](PPC64XEmitter& e) { e.NEG(R3, R4); }, 0x7C6400D0},
    {"neg. 31,30", [](PPC64XEmitter& e) { e.NEG(R31, R30, true); }, 0x7FFE00D1},
    {"cmpwi 3,4,-5", [](PPC64XEmitter& e) { e.CMPWI(CR3, R4, -5); }, 0x2D84FFFB},
    {"cmpdi 7,31,100", [](PPC64XEmitter& e) { e.CMPDI(CR7, R31, 100); }, 0x2FBF0064},
    {"cmplwi 0,3,65535", [](PPC64XEmitter& e) { e.CMPLWI(CR0, R3, 65535); }, 0x2803FFFF},
    {"cmpldi 2,3,1", [](PPC64XEmitter& e) { e.CMPLDI(CR2, R3, 1); }, 0x29230001},
    {"cmpw 1,3,4", [](PPC64XEmitter& e) { e.CMPW(CR1, R3, R4); }, 0x7C832000},
    {"cmpd 0,5,6", [](PPC64XEmitter& e) { e.CMPD(CR0, R5, R6); }, 0x7C253000},
    {"cmplw 7,8,9", [](PPC64XEmitter& e) { e.CMPLW(CR7, R8, R9); }, 0x7F884840},
    {"cmpld 6,10,11", [](PPC64XEmitter& e) { e.CMPLD(CR6, R10, R11); }, 0x7F2A5840},
    {"isel 3,4,5,10", [](PPC64XEmitter& e) { e.ISEL(R3, R4, R5, 10); }, 0x7C642A9E},
    {"isel 3,0,5,31", [](PPC64XEmitter& e) { e.ISEL(R3, R0, R5, 31); }, 0x7C602FDE},
    {"andi. 3,4,0xffff", [](PPC64XEmitter& e) { e.ANDI_(R3, R4, 0xFFFF); }, 0x7083FFFF},
    {"andis. 3,4,0x8000", [](PPC64XEmitter& e) { e.ANDIS_(R3, R4, 0x8000); }, 0x74838000},
    {"ori 3,4,0x8000", [](PPC64XEmitter& e) { e.ORI(R3, R4, 0x8000); }, 0x60838000},
    {"oris 5,6,1", [](PPC64XEmitter& e) { e.ORIS(R5, R6, 1); }, 0x64C50001},
    {"xori 7,8,0x1234", [](PPC64XEmitter& e) { e.XORI(R7, R8, 0x1234); }, 0x69071234},
    {"xoris 9,10,0xffff", [](PPC64XEmitter& e) { e.XORIS(R9, R10, 0xFFFF); }, 0x6D49FFFF},
    {"and 3,4,5", [](PPC64XEmitter& e) { e.AND(R3, R4, R5); }, 0x7C832838},
    {"and. 31,30,29", [](PPC64XEmitter& e) { e.AND(R31, R30, R29, true); }, 0x7FDFE839},
    {"andc 3,4,5", [](PPC64XEmitter& e) { e.ANDC(R3, R4, R5); }, 0x7C832878},
    {"andc. 31,30,29", [](PPC64XEmitter& e) { e.ANDC(R31, R30, R29, true); }, 0x7FDFE879},
    {"or 3,4,5", [](PPC64XEmitter& e) { e.OR(R3, R4, R5); }, 0x7C832B78},
    {"or. 31,30,29", [](PPC64XEmitter& e) { e.OR(R31, R30, R29, true); }, 0x7FDFEB79},
    {"orc 3,4,5", [](PPC64XEmitter& e) { e.ORC(R3, R4, R5); }, 0x7C832B38},
    {"orc. 31,30,29", [](PPC64XEmitter& e) { e.ORC(R31, R30, R29, true); }, 0x7FDFEB39},
    {"xor 3,4,5", [](PPC64XEmitter& e) { e.XOR(R3, R4, R5); }, 0x7C832A78},
    {"xor. 31,30,29", [](PPC64XEmitter& e) { e.XOR(R31, R30, R29, true); }, 0x7FDFEA79},
    {"nand 3,4,5", [](PPC64XEmitter& e) { e.NAND(R3, R4, R5); }, 0x7C832BB8},
    {"nand. 31,30,29", [](PPC64XEmitter& e) { e.NAND(R31, R30, R29, true); }, 0x7FDFEBB9},
    {"nor 3,4,5", [](PPC64XEmitter& e) { e.NOR(R3, R4, R5); }, 0x7C8328F8},
    {"nor. 31,30,29", [](PPC64XEmitter& e) { e.NOR(R31, R30, R29, true); }, 0x7FDFE8F9},
    {"eqv 3,4,5", [](PPC64XEmitter& e) { e.EQV(R3, R4, R5); }, 0x7C832A38},
    {"eqv. 31,30,29", [](PPC64XEmitter& e) { e.EQV(R31, R30, R29, true); }, 0x7FDFEA39},
    {"slw 3,4,5", [](PPC64XEmitter& e) { e.SLW(R3, R4, R5); }, 0x7C832830},
    {"slw. 31,30,29", [](PPC64XEmitter& e) { e.SLW(R31, R30, R29, true); }, 0x7FDFE831},
    {"srw 3,4,5", [](PPC64XEmitter& e) { e.SRW(R3, R4, R5); }, 0x7C832C30},
    {"srw. 31,30,29", [](PPC64XEmitter& e) { e.SRW(R31, R30, R29, true); }, 0x7FDFEC31},
    {"sraw 3,4,5", [](PPC64XEmitter& e) { e.SRAW(R3, R4, R5); }, 0x7C832E30},
    {"sraw. 31,30,29", [](PPC64XEmitter& e) { e.SRAW(R31, R30, R29, true); }, 0x7FDFEE31},
    {"sld 3,4,5", [](PPC64XEmitter& e) { e.SLD(R3, R4, R5); }, 0x7C832836},
    {"sld. 31,30,29", [](PPC64XEmitter& e) { e.SLD(R31, R30, R29, true); }, 0x7FDFE837},
    {"srd 3,4,5", [](PPC64XEmitter& e) { e.SRD(R3, R4, R5); }, 0x7C832C36},
    {"srd. 31,30,29", [](PPC64XEmitter& e) { e.SRD(R31, R30, R29, true); }, 0x7FDFEC37},
    {"srad 3,4,5", [](PPC64XEmitter& e) { e.SRAD(R3, R4, R5); }, 0x7C832E34},
    {"srad. 31,30,29", [](PPC64XEmitter& e) { e.SRAD(R31, R30, R29, true); }, 0x7FDFEE35},
    {"extsb 3,4", [](PPC64XEmitter& e) { e.EXTSB(R3, R4); }, 0x7C830774},
    {"extsb. 31,30", [](PPC64XEmitter& e) { e.EXTSB(R31, R30, true); }, 0x7FDF0775},
    {"extsh 3,4", [](PPC64XEmitter& e) { e.EXTSH(R3, R4); }, 0x7C830734},
    {"extsh. 31,30", [](PPC64XEmitter& e) { e.EXTSH(R31, R30, true); }, 0x7FDF0735},
    {"extsw 3,4", [](PPC64XEmitter& e) { e.EXTSW(R3, R4); }, 0x7C8307B4},
    {"extsw. 31,30", [](PPC64XEmitter& e) { e.EXTSW(R31, R30, true); }, 0x7FDF07B5},
    {"cntlzw 3,4", [](PPC64XEmitter& e) { e.CNTLZW(R3, R4); }, 0x7C830034},
    {"cntlzw. 31,30", [](PPC64XEmitter& e) { e.CNTLZW(R31, R30, true); }, 0x7FDF0035},
    {"cntlzd 3,4", [](PPC64XEmitter& e) { e.CNTLZD(R3, R4); }, 0x7C830074},
    {"cntlzd. 31,30", [](PPC64XEmitter& e) { e.CNTLZD(R31, R30, true); }, 0x7FDF0075},
    {"popcntw 3,4", [](PPC64XEmitter& e) { e.POPCNTW(R3, R4); }, 0x7C8302F4},
    {"popcntd 5,6", [](PPC64XEmitter& e) { e.POPCNTD(R5, R6); }, 0x7CC503F4},
    {"srawi 3,4,31", [](PPC64XEmitter& e) { e.SRAWI(R3, R4, 31); }, 0x7C83FE70},
    {"srawi. 3,4,0", [](PPC64XEmitter& e) { e.SRAWI(R3, R4, 0, true); }, 0x7C830671},
    {"sradi 3,4,63", [](PPC64XEmitter& e) { e.SRADI(R3, R4, 63); }, 0x7C83FE76},
    {"sradi 3,4,5", [](PPC64XEmitter& e) { e.SRADI(R3, R4, 5); }, 0x7C832E74},
    {"sradi. 3,4,32", [](PPC64XEmitter& e) { e.SRADI(R3, R4, 32, true); }, 0x7C830677},
    {"rlwinm 3,4,5,6,7", [](PPC64XEmitter& e) { e.RLWINM(R3, R4, 5, 6, 7); }, 0x5483298E},
    {"rlwinm. 3,4,31,0,31", [](PPC64XEmitter& e) { e.RLWINM(R3, R4, 31, 0, 31, true); }, 0x5483F83F},
    {"rlwnm 3,4,5,0,31", [](PPC64XEmitter& e) { e.RLWNM(R3, R4, R5, 0, 31); }, 0x5C83283E},
    {"rlwimi 3,4,8,16,23", [](PPC64XEmitter& e) { e.RLWIMI(R3, R4, 8, 16, 23); }, 0x5083442E},
    {"rldicl 3,4,10,40", [](PPC64XEmitter& e) { e.RLDICL(R3, R4, 10, 40); }, 0x78835220},
    {"rldicl 3,4,63,63", [](PPC64XEmitter& e) { e.RLDICL(R3, R4, 63, 63); }, 0x7883FFE2},
    {"rldicl. 3,4,32,0", [](PPC64XEmitter& e) { e.RLDICL(R3, R4, 32, 0, true); }, 0x78830003},
    {"rldicr 3,4,32,31", [](PPC64XEmitter& e) { e.RLDICR(R3, R4, 32, 31); }, 0x788307C6},
    {"rldicr 3,4,1,62", [](PPC64XEmitter& e) { e.RLDICR(R3, R4, 1, 62); }, 0x78830FA4},
    {"rldic 3,4,5,33", [](PPC64XEmitter& e) { e.RLDIC(R3, R4, 5, 33); }, 0x78832868},
    {"rldimi 3,4,32,0", [](PPC64XEmitter& e) { e.RLDIMI(R3, R4, 32, 0); }, 0x7883000E},
    {"rldimi 3,4,0,32", [](PPC64XEmitter& e) { e.RLDIMI(R3, R4, 0, 32); }, 0x7883002C},
    {"rldcl 3,4,5,48", [](PPC64XEmitter& e) { e.RLDCL(R3, R4, R5, 48); }, 0x78832C30},
    {"lbz 3,-1(4)", [](PPC64XEmitter& e) { e.LBZ(R3, R4, -1); }, 0x8864FFFF},
    {"lhz 3,100(4)", [](PPC64XEmitter& e) { e.LHZ(R3, R4, 100); }, 0xA0640064},
    {"lha 3,-32768(4)", [](PPC64XEmitter& e) { e.LHA(R3, R4, -32768); }, 0xA8648000},
    {"lwz 3,0(1)", [](PPC64XEmitter& e) { e.LWZ(R3, R1, 0); }, 0x80610000},
    {"lwa 3,8(4)", [](PPC64XEmitter& e) { e.LWA(R3, R4, 8); }, 0xE864000A},
    {"ld 3,-8(1)", [](PPC64XEmitter& e) { e.LD(R3, R1, -8); }, 0xE861FFF8},
    {"ld 31,32764(31)", [](PPC64XEmitter& e) { e.LD(R31, R31, 32764); }, 0xEBFF7FFC},
    {"lbzx 3,4,5", [](PPC64XEmitter& e) { e.LBZX(R3, R4, R5); }, 0x7C6428AE},
    {"lhzx 3,4,5", [](PPC64XEmitter& e) { e.LHZX(R3, R4, R5); }, 0x7C642A2E},
    {"lhax 3,4,5", [](PPC64XEmitter& e) { e.LHAX(R3, R4, R5); }, 0x7C642AAE},
    {"lwzx 3,4,5", [](PPC64XEmitter& e) { e.LWZX(R3, R4, R5); }, 0x7C64282E},
    {"lwax 3,4,5", [](PPC64XEmitter& e) { e.LWAX(R3, R4, R5); }, 0x7C642AAA},
    {"ldx 3,4,5", [](PPC64XEmitter& e) { e.LDX(R3, R4, R5); }, 0x7C64282A},
    {"lhbrx 3,4,5", [](PPC64XEmitter& e) { e.LHBRX(R3, R4, R5); }, 0x7C642E2C},
    {"lwbrx 3,4,5", [](PPC64XEmitter& e) { e.LWBRX(R3, R4, R5); }, 0x7C642C2C},
    {"ldbrx 3,4,5", [](PPC64XEmitter& e) { e.LDBRX(R3, R4, R5); }, 0x7C642C28},
    {"lwbrx 3,0,5", [](PPC64XEmitter& e) { e.LWBRX(R3, R0, R5); }, 0x7C602C2C},
    {"stb 3,1(4)", [](PPC64XEmitter& e) { e.STB(R3, R4, 1); }, 0x98640001},
    {"sth 3,-2(4)", [](PPC64XEmitter& e) { e.STH(R3, R4, -2); }, 0xB064FFFE},
    {"stw 3,4(4)", [](PPC64XEmitter& e) { e.STW(R3, R4, 4); }, 0x90640004},
    {"std 3,16(1)", [](PPC64XEmitter& e) { e.STD(R3, R1, 16); }, 0xF8610010},
    {"stdu 1,-32(1)", [](PPC64XEmitter& e) { e.STDU(R1, R1, -32); }, 0xF821FFE1},
    {"stbx 3,4,5", [](PPC64XEmitter& e) { e.STBX(R3, R4, R5); }, 0x7C6429AE},
    {"sthx 3,4,5", [](PPC64XEmitter& e) { e.STHX(R3, R4, R5); }, 0x7C642B2E},
    {"stwx 3,4,5", [](PPC64XEmitter& e) { e.STWX(R3, R4, R5); }, 0x7C64292E},
    {"stdx 3,4,5", [](PPC64XEmitter& e) { e.STDX(R3, R4, R5); }, 0x7C64292A},
    {"sthbrx 3,4,5", [](PPC64XEmitter& e) { e.STHBRX(R3, R4, R5); }, 0x7C642F2C},
    {"stwbrx 3,4,5", [](PPC64XEmitter& e) { e.STWBRX(R3, R4, R5); }, 0x7C642D2C},
    {"stdbrx 3,4,5", [](PPC64XEmitter& e) { e.STDBRX(R3, R4, R5); }, 0x7C642D28},
    {"crand 1,2,31", [](PPC64XEmitter& e) { e.CRAND(1, 2, 31); }, 0x4C22FA02},
    {"cror 1,2,31", [](PPC64XEmitter& e) { e.CROR(1, 2, 31); }, 0x4C22FB82},
    {"crxor 1,2,31", [](PPC64XEmitter& e) { e.CRXOR(1, 2, 31); }, 0x4C22F982},
    {"crnand 1,2,31", [](PPC64XEmitter& e) { e.CRNAND(1, 2, 31); }, 0x4C22F9C2},
    {"crnor 1,2,31", [](PPC64XEmitter& e) { e.CRNOR(1, 2, 31); }, 0x4C22F842},
    {"creqv 1,2,31", [](PPC64XEmitter& e) { e.CREQV(1, 2, 31); }, 0x4C22FA42},
    {"crandc 1,2,31", [](PPC64XEmitter& e) { e.CRANDC(1, 2, 31); }, 0x4C22F902},
    {"crorc 1,2,31", [](PPC64XEmitter& e) { e.CRORC(1, 2, 31); }, 0x4C22FB42},
    {"mcrf 7,0", [](PPC64XEmitter& e) { e.MCRF(CR7, CR0); }, 0x4F800000},
    {"mfcr 3", [](PPC64XEmitter& e) { e.MFCR(R3); }, 0x7C600026},
    {"mfocrf 3,128", [](PPC64XEmitter& e) { e.MFOCRF(R3, CR0); }, 0x7C780026},
    {"mfocrf 4,1", [](PPC64XEmitter& e) { e.MFOCRF(R4, CR7); }, 0x7C901026},
    {"mtcrf 255,3", [](PPC64XEmitter& e) { e.MTCRF(0xFF, R3); }, 0x7C6FF120},
    {"mtcrf 0x81,3", [](PPC64XEmitter& e) { e.MTCRF(0x81, R3); }, 0x7C681120},
    {"mtocrf 32,5", [](PPC64XEmitter& e) { e.MTOCRF(CR2, R5); }, 0x7CB20120},
    {"mflr 0", [](PPC64XEmitter& e) { e.MFLR(R0); }, 0x7C0802A6},
    {"mtlr 0", [](PPC64XEmitter& e) { e.MTLR(R0); }, 0x7C0803A6},
    {"mfctr 12", [](PPC64XEmitter& e) { e.MFCTR(R12); }, 0x7D8902A6},
    {"mtctr 12", [](PPC64XEmitter& e) { e.MTCTR(R12); }, 0x7D8903A6},
    {"mfxer 5", [](PPC64XEmitter& e) { e.MFXER(R5); }, 0x7CA102A6},
    {"mtxer 5", [](PPC64XEmitter& e) { e.MTXER(R5); }, 0x7CA103A6},
    {"mfspr 3,268", [](PPC64XEmitter& e) { e.MFSPR(R3, SPR::TB); }, 0x7C6C42A6},
    {"mtspr 256,4", [](PPC64XEmitter& e) { e.MTSPR(SPR::VRSAVE, R4); }, 0x7C8043A6},
    {"sync", [](PPC64XEmitter& e) { e.SYNC(); }, 0x7C0004AC},
    {"lwsync", [](PPC64XEmitter& e) { e.LWSYNC(); }, 0x7C2004AC},
    {"isync", [](PPC64XEmitter& e) { e.ISYNC(); }, 0x4C00012C},
    {"icbi 3,4", [](PPC64XEmitter& e) { e.ICBI(R3, R4); }, 0x7C0327AC},
    {"dcbst 0,5", [](PPC64XEmitter& e) { e.DCBST(R0, R5); }, 0x7C00286C},
    {"trap", [](PPC64XEmitter& e) { e.TRAP(); }, 0x7FE00008},
    {"nop", [](PPC64XEmitter& e) { e.NOP(); }, 0x60000000},
    {"b .+0x100", [](PPC64XEmitter& e) { e.B(e.GetCodePtr() + (0x100)); }, 0x48000100},
    {"b .-0x2000000", [](PPC64XEmitter& e) { e.B(e.GetCodePtr() + (-0x2000000)); }, 0x4A000000},
    {"bl .+0x1fffffc", [](PPC64XEmitter& e) { e.BL(e.GetCodePtr() + (0x1FFFFFC)); }, 0x49FFFFFD},
    {"blt 3,.+0x40", [](PPC64XEmitter& e) { e.BC(LT, CR3, e.GetCodePtr() + (0x40)); }, 0x418C0040},
    {"bne 0,.-0x8000", [](PPC64XEmitter& e) { e.BC(NE, CR0, e.GetCodePtr() + (-0x8000)); }, 0x40828000},
    {"bge 7,.+0x7ffc", [](PPC64XEmitter& e) { e.BC(GE, CR7, e.GetCodePtr() + (0x7FFC)); }, 0x409C7FFC},
    {"bso 1,.+8", [](PPC64XEmitter& e) { e.BC(SO, CR1, e.GetCodePtr() + (8)); }, 0x41870008},
    {"bns 2,.+12", [](PPC64XEmitter& e) { e.BC(NS, CR2, e.GetCodePtr() + (12)); }, 0x408B000C},
    {"ble 4,.+16", [](PPC64XEmitter& e) { e.BC(LE, CR4, e.GetCodePtr() + (16)); }, 0x40910010},
    {"bgt 5,.-4", [](PPC64XEmitter& e) { e.BC(GT, CR5, e.GetCodePtr() + (-4)); }, 0x4195FFFC},
    {"beq 6,.", [](PPC64XEmitter& e) { e.BC(EQ, CR6, e.GetCodePtr() + (0)); }, 0x419A0000},
    {"b .+0x1000", [](PPC64XEmitter& e) { const FixupBranch b = e.B(); e.SetJumpTarget(b, b.ptr + 0x1000); }, 0x48001000},
    {"bl .-0x100", [](PPC64XEmitter& e) { const FixupBranch b = e.BL(); e.SetJumpTarget(b, b.ptr - 0x100); }, 0x4BFFFF01},
    {"beq 1,.+0x20", [](PPC64XEmitter& e) { const FixupBranch b = e.BC(EQ, CR1); e.SetJumpTarget(b, b.ptr + 0x20); }, 0x41860020},
    {"bge 0,.-0x20", [](PPC64XEmitter& e) { const FixupBranch b = e.BC(GE); e.SetJumpTarget(b, b.ptr - 0x20); }, 0x4080FFE0},
    {"bdnz .+0x20", [](PPC64XEmitter& e) { const FixupBranch b = e.BDNZ(); e.SetJumpTarget(b, b.ptr + 0x20); }, 0x42000020},
    {"bltlr 2", [](PPC64XEmitter& e) { e.BCLR(LT, CR2); }, 0x4D880020},
    {"bnelr 0", [](PPC64XEmitter& e) { e.BCLR(NE, CR0); }, 0x4C820020},
    {"blr", [](PPC64XEmitter& e) { e.BLR(); }, 0x4E800020},
    {"bcl 20,31,.+4", [](PPC64XEmitter& e) { e.BCL_NEXT(); }, 0x429F0005},
    {"bctr", [](PPC64XEmitter& e) { e.BCTR(); }, 0x4E800420},
    {"bctrl", [](PPC64XEmitter& e) { e.BCTRL(); }, 0x4E800421},
    {"lfs 1,-4(3)", [](PPC64XEmitter& e) { e.LFS(F1, R3, -4); }, 0xC023FFFC},
    {"lfd 31,8(1)", [](PPC64XEmitter& e) { e.LFD(F31, R1, 8); }, 0xCBE10008},
    {"stfs 2,4(3)", [](PPC64XEmitter& e) { e.STFS(F2, R3, 4); }, 0xD0430004},
    {"stfd 0,-8(1)", [](PPC64XEmitter& e) { e.STFD(F0, R1, -8); }, 0xD801FFF8},
    {"lfsx 5,3,4", [](PPC64XEmitter& e) { e.LFSX(F5, R3, R4); }, 0x7CA3242E},
    {"lfdx 5,3,4", [](PPC64XEmitter& e) { e.LFDX(F5, R3, R4); }, 0x7CA324AE},
    {"stfsx 5,3,4", [](PPC64XEmitter& e) { e.STFSX(F5, R3, R4); }, 0x7CA3252E},
    {"stfdx 5,3,4", [](PPC64XEmitter& e) { e.STFDX(F5, R3, R4); }, 0x7CA325AE},
    {"stfiwx 5,3,4", [](PPC64XEmitter& e) { e.STFIWX(F5, R3, R4); }, 0x7CA327AE},
    {"fmr 1,2", [](PPC64XEmitter& e) { e.FMR(F1, F2); }, 0xFC201090},
    {"fmr. 31,30", [](PPC64XEmitter& e) { e.FMR(F31, F30, true); }, 0xFFE0F091},
    {"fneg 1,2", [](PPC64XEmitter& e) { e.FNEG(F1, F2); }, 0xFC201050},
    {"fneg. 31,30", [](PPC64XEmitter& e) { e.FNEG(F31, F30, true); }, 0xFFE0F051},
    {"fabs 1,2", [](PPC64XEmitter& e) { e.FABS(F1, F2); }, 0xFC201210},
    {"fabs. 31,30", [](PPC64XEmitter& e) { e.FABS(F31, F30, true); }, 0xFFE0F211},
    {"fnabs 1,2", [](PPC64XEmitter& e) { e.FNABS(F1, F2); }, 0xFC201110},
    {"fnabs. 31,30", [](PPC64XEmitter& e) { e.FNABS(F31, F30, true); }, 0xFFE0F111},
    {"fsqrt 1,2", [](PPC64XEmitter& e) { e.FSQRT(F1, F2); }, 0xFC20102C},
    {"fsqrt. 31,30", [](PPC64XEmitter& e) { e.FSQRT(F31, F30, true); }, 0xFFE0F02D},
    {"fsqrts 1,2", [](PPC64XEmitter& e) { e.FSQRTS(F1, F2); }, 0xEC20102C},
    {"fsqrts. 31,30", [](PPC64XEmitter& e) { e.FSQRTS(F31, F30, true); }, 0xEFE0F02D},
    {"fres 1,2", [](PPC64XEmitter& e) { e.FRES(F1, F2); }, 0xEC201030},
    {"fres. 31,30", [](PPC64XEmitter& e) { e.FRES(F31, F30, true); }, 0xEFE0F031},
    {"frsqrte 1,2", [](PPC64XEmitter& e) { e.FRSQRTE(F1, F2); }, 0xFC201034},
    {"frsqrte. 31,30", [](PPC64XEmitter& e) { e.FRSQRTE(F31, F30, true); }, 0xFFE0F035},
    {"frsp 1,2", [](PPC64XEmitter& e) { e.FRSP(F1, F2); }, 0xFC201018},
    {"frsp. 31,30", [](PPC64XEmitter& e) { e.FRSP(F31, F30, true); }, 0xFFE0F019},
    {"fctiw 1,2", [](PPC64XEmitter& e) { e.FCTIW(F1, F2); }, 0xFC20101C},
    {"fctiw. 31,30", [](PPC64XEmitter& e) { e.FCTIW(F31, F30, true); }, 0xFFE0F01D},
    {"fctiwz 1,2", [](PPC64XEmitter& e) { e.FCTIWZ(F1, F2); }, 0xFC20101E},
    {"fctiwz. 31,30", [](PPC64XEmitter& e) { e.FCTIWZ(F31, F30, true); }, 0xFFE0F01F},
    {"fctid 1,2", [](PPC64XEmitter& e) { e.FCTID(F1, F2); }, 0xFC20165C},
    {"fctid. 31,30", [](PPC64XEmitter& e) { e.FCTID(F31, F30, true); }, 0xFFE0F65D},
    {"fctidz 1,2", [](PPC64XEmitter& e) { e.FCTIDZ(F1, F2); }, 0xFC20165E},
    {"fctidz. 31,30", [](PPC64XEmitter& e) { e.FCTIDZ(F31, F30, true); }, 0xFFE0F65F},
    {"fcfid 1,2", [](PPC64XEmitter& e) { e.FCFID(F1, F2); }, 0xFC20169C},
    {"fcfid. 31,30", [](PPC64XEmitter& e) { e.FCFID(F31, F30, true); }, 0xFFE0F69D},
    {"fcpsgn 1,2,3", [](PPC64XEmitter& e) { e.FCPSGN(F1, F2, F3); }, 0xFC221810},
    {"fcpsgn. 31,30,29", [](PPC64XEmitter& e) { e.FCPSGN(F31, F30, F29, true); }, 0xFFFEE811},
    {"fadd 1,2,3", [](PPC64XEmitter& e) { e.FADD(F1, F2, F3); }, 0xFC22182A},
    {"fadd. 31,30,29", [](PPC64XEmitter& e) { e.FADD(F31, F30, F29, true); }, 0xFFFEE82B},
    {"fadds 1,2,3", [](PPC64XEmitter& e) { e.FADDS(F1, F2, F3); }, 0xEC22182A},
    {"fadds. 31,30,29", [](PPC64XEmitter& e) { e.FADDS(F31, F30, F29, true); }, 0xEFFEE82B},
    {"fsub 1,2,3", [](PPC64XEmitter& e) { e.FSUB(F1, F2, F3); }, 0xFC221828},
    {"fsub. 31,30,29", [](PPC64XEmitter& e) { e.FSUB(F31, F30, F29, true); }, 0xFFFEE829},
    {"fsubs 1,2,3", [](PPC64XEmitter& e) { e.FSUBS(F1, F2, F3); }, 0xEC221828},
    {"fsubs. 31,30,29", [](PPC64XEmitter& e) { e.FSUBS(F31, F30, F29, true); }, 0xEFFEE829},
    {"fmul 1,2,3", [](PPC64XEmitter& e) { e.FMUL(F1, F2, F3); }, 0xFC2200F2},
    {"fmul. 31,30,29", [](PPC64XEmitter& e) { e.FMUL(F31, F30, F29, true); }, 0xFFFE0773},
    {"fmuls 1,2,3", [](PPC64XEmitter& e) { e.FMULS(F1, F2, F3); }, 0xEC2200F2},
    {"fmuls. 31,30,29", [](PPC64XEmitter& e) { e.FMULS(F31, F30, F29, true); }, 0xEFFE0773},
    {"fdiv 1,2,3", [](PPC64XEmitter& e) { e.FDIV(F1, F2, F3); }, 0xFC221824},
    {"fdiv. 31,30,29", [](PPC64XEmitter& e) { e.FDIV(F31, F30, F29, true); }, 0xFFFEE825},
    {"fdivs 1,2,3", [](PPC64XEmitter& e) { e.FDIVS(F1, F2, F3); }, 0xEC221824},
    {"fdivs. 31,30,29", [](PPC64XEmitter& e) { e.FDIVS(F31, F30, F29, true); }, 0xEFFEE825},
    {"fmadd 1,2,3,4", [](PPC64XEmitter& e) { e.FMADD(F1, F2, F3, F4); }, 0xFC2220FA},
    {"fmadd. 31,30,29,28", [](PPC64XEmitter& e) { e.FMADD(F31, F30, F29, F28, true); }, 0xFFFEE77B},
    {"fmadds 1,2,3,4", [](PPC64XEmitter& e) { e.FMADDS(F1, F2, F3, F4); }, 0xEC2220FA},
    {"fmadds. 31,30,29,28", [](PPC64XEmitter& e) { e.FMADDS(F31, F30, F29, F28, true); }, 0xEFFEE77B},
    {"fmsub 1,2,3,4", [](PPC64XEmitter& e) { e.FMSUB(F1, F2, F3, F4); }, 0xFC2220F8},
    {"fmsub. 31,30,29,28", [](PPC64XEmitter& e) { e.FMSUB(F31, F30, F29, F28, true); }, 0xFFFEE779},
    {"fmsubs 1,2,3,4", [](PPC64XEmitter& e) { e.FMSUBS(F1, F2, F3, F4); }, 0xEC2220F8},
    {"fmsubs. 31,30,29,28", [](PPC64XEmitter& e) { e.FMSUBS(F31, F30, F29, F28, true); }, 0xEFFEE779},
    {"fnmadd 1,2,3,4", [](PPC64XEmitter& e) { e.FNMADD(F1, F2, F3, F4); }, 0xFC2220FE},
    {"fnmadd. 31,30,29,28", [](PPC64XEmitter& e) { e.FNMADD(F31, F30, F29, F28, true); }, 0xFFFEE77F},
    {"fnmadds 1,2,3,4", [](PPC64XEmitter& e) { e.FNMADDS(F1, F2, F3, F4); }, 0xEC2220FE},
    {"fnmadds. 31,30,29,28", [](PPC64XEmitter& e) { e.FNMADDS(F31, F30, F29, F28, true); }, 0xEFFEE77F},
    {"fnmsub 1,2,3,4", [](PPC64XEmitter& e) { e.FNMSUB(F1, F2, F3, F4); }, 0xFC2220FC},
    {"fnmsub. 31,30,29,28", [](PPC64XEmitter& e) { e.FNMSUB(F31, F30, F29, F28, true); }, 0xFFFEE77D},
    {"fnmsubs 1,2,3,4", [](PPC64XEmitter& e) { e.FNMSUBS(F1, F2, F3, F4); }, 0xEC2220FC},
    {"fnmsubs. 31,30,29,28", [](PPC64XEmitter& e) { e.FNMSUBS(F31, F30, F29, F28, true); }, 0xEFFEE77D},
    {"fsel 1,2,3,4", [](PPC64XEmitter& e) { e.FSEL(F1, F2, F3, F4); }, 0xFC2220EE},
    {"fsel. 31,30,29,28", [](PPC64XEmitter& e) { e.FSEL(F31, F30, F29, F28, true); }, 0xFFFEE76F},
    {"fcmpu 3,1,2", [](PPC64XEmitter& e) { e.FCMPU(CR3, F1, F2); }, 0xFD811000},
    {"fcmpo 7,1,2", [](PPC64XEmitter& e) { e.FCMPO(CR7, F1, F2); }, 0xFF811040},
    {"mffs 0", [](PPC64XEmitter& e) { e.MFFS(F0); }, 0xFC00048E},
    {"mtfsf 0xff,1", [](PPC64XEmitter& e) { e.MTFSF(0xFF, F1); }, 0xFDFE0D8E},
    {"mtfsf 1,0", [](PPC64XEmitter& e) { e.MTFSF(0x01, F0); }, 0xFC02058E},
    {"mtfsfi 7,3", [](PPC64XEmitter& e) { e.MTFSFI(7, 3); }, 0xFF80310C},
    {"mtfsb0 29", [](PPC64XEmitter& e) { e.MTFSB0(29); }, 0xFFA0008C},
    {"mtfsb1 31", [](PPC64XEmitter& e) { e.MTFSB1(31); }, 0xFFE0004C},
    {"lxsdx 1,3,4", [](PPC64XEmitter& e) { e.LXSDX(VS1, R3, R4); }, 0x7C232498},
    {"lxsdx 63,0,5", [](PPC64XEmitter& e) { e.LXSDX(VS63, R0, R5); }, 0x7FE02C99},
    {"lxsspx 1,3,4", [](PPC64XEmitter& e) { e.LXSSPX(VS1, R3, R4); }, 0x7C232418},
    {"lxsspx 63,0,5", [](PPC64XEmitter& e) { e.LXSSPX(VS63, R0, R5); }, 0x7FE02C19},
    {"lxsiwzx 1,3,4", [](PPC64XEmitter& e) { e.LXSIWZX(VS1, R3, R4); }, 0x7C232018},
    {"lxsiwzx 63,0,5", [](PPC64XEmitter& e) { e.LXSIWZX(VS63, R0, R5); }, 0x7FE02819},
    {"lxsiwax 1,3,4", [](PPC64XEmitter& e) { e.LXSIWAX(VS1, R3, R4); }, 0x7C232098},
    {"lxsiwax 63,0,5", [](PPC64XEmitter& e) { e.LXSIWAX(VS63, R0, R5); }, 0x7FE02899},
    {"lxvd2x 1,3,4", [](PPC64XEmitter& e) { e.LXVD2X(VS1, R3, R4); }, 0x7C232698},
    {"lxvd2x 63,0,5", [](PPC64XEmitter& e) { e.LXVD2X(VS63, R0, R5); }, 0x7FE02E99},
    {"lxvw4x 1,3,4", [](PPC64XEmitter& e) { e.LXVW4X(VS1, R3, R4); }, 0x7C232618},
    {"lxvw4x 63,0,5", [](PPC64XEmitter& e) { e.LXVW4X(VS63, R0, R5); }, 0x7FE02E19},
    {"lxvdsx 1,3,4", [](PPC64XEmitter& e) { e.LXVDSX(VS1, R3, R4); }, 0x7C232298},
    {"lxvdsx 63,0,5", [](PPC64XEmitter& e) { e.LXVDSX(VS63, R0, R5); }, 0x7FE02A99},
    {"stxsdx 1,3,4", [](PPC64XEmitter& e) { e.STXSDX(VS1, R3, R4); }, 0x7C232598},
    {"stxsdx 63,0,5", [](PPC64XEmitter& e) { e.STXSDX(VS63, R0, R5); }, 0x7FE02D99},
    {"stxsspx 1,3,4", [](PPC64XEmitter& e) { e.STXSSPX(VS1, R3, R4); }, 0x7C232518},
    {"stxsspx 63,0,5", [](PPC64XEmitter& e) { e.STXSSPX(VS63, R0, R5); }, 0x7FE02D19},
    {"stxsiwx 1,3,4", [](PPC64XEmitter& e) { e.STXSIWX(VS1, R3, R4); }, 0x7C232118},
    {"stxsiwx 63,0,5", [](PPC64XEmitter& e) { e.STXSIWX(VS63, R0, R5); }, 0x7FE02919},
    {"stxvd2x 1,3,4", [](PPC64XEmitter& e) { e.STXVD2X(VS1, R3, R4); }, 0x7C232798},
    {"stxvd2x 63,0,5", [](PPC64XEmitter& e) { e.STXVD2X(VS63, R0, R5); }, 0x7FE02F99},
    {"stxvw4x 1,3,4", [](PPC64XEmitter& e) { e.STXVW4X(VS1, R3, R4); }, 0x7C232718},
    {"stxvw4x 63,0,5", [](PPC64XEmitter& e) { e.STXVW4X(VS63, R0, R5); }, 0x7FE02F19},
    {"mtvsrd 33,5", [](PPC64XEmitter& e) { e.MTVSRD(VS33, R5); }, 0x7C250167},
    {"mtvsrd 1,5", [](PPC64XEmitter& e) { e.MTVSRD(VS1, R5); }, 0x7C250166},
    {"mtvsrwz 34,6", [](PPC64XEmitter& e) { e.MTVSRWZ(VS34, R6); }, 0x7C4601E7},
    {"mtvsrwa 3,7", [](PPC64XEmitter& e) { e.MTVSRWA(VS3, R7); }, 0x7C6701A6},
    {"mfvsrd 5,33", [](PPC64XEmitter& e) { e.MFVSRD(R5, VS33); }, 0x7C250067},
    {"mfvsrwz 6,2", [](PPC64XEmitter& e) { e.MFVSRWZ(R6, VS2); }, 0x7C4600E6},
    {"xxpermdi 34,35,36,2", [](PPC64XEmitter& e) { e.XXPERMDI(VS34, VS35, VS36, 2); }, 0xF0432257},
    {"xxpermdi 0,1,63,1", [](PPC64XEmitter& e) { e.XXPERMDI(VS0, VS1, VS63, 1); }, 0xF001F952},
    {"xxswapd 32,33", [](PPC64XEmitter& e) { e.XXSWAPD(VS32, VS33); }, 0xF0010A57},
    {"xxmrghd 1,2,3", [](PPC64XEmitter& e) { e.XXMRGHD(VS1, VS2, VS3); }, 0xF0221850},
    {"xxmrgld 1,2,3", [](PPC64XEmitter& e) { e.XXMRGLD(VS1, VS2, VS3); }, 0xF0221B50},
    {"xxsldwi 34,35,36,3", [](PPC64XEmitter& e) { e.XXSLDWI(VS34, VS35, VS36, 3); }, 0xF0432317},
    {"xxspltw 34,35,2", [](PPC64XEmitter& e) { e.XXSPLTW(VS34, VS35, 2); }, 0xF0421A93},
    {"xxspltw 1,63,0", [](PPC64XEmitter& e) { e.XXSPLTW(VS1, VS63, 0); }, 0xF020FA92},
    {"xxsel 34,35,36,37", [](PPC64XEmitter& e) { e.XXSEL(VS34, VS35, VS36, VS37); }, 0xF043217F},
    {"xxsel 1,2,3,4", [](PPC64XEmitter& e) { e.XXSEL(VS1, VS2, VS3, VS4); }, 0xF0221930},
    {"xxlor 1,2,2", [](PPC64XEmitter& e) { e.XXLMR(VS1, VS2); }, 0xF0221490},
    {"xxland 33,34,63", [](PPC64XEmitter& e) { e.XXLAND(VS33, VS34, VS63); }, 0xF022FC17},
    {"xxlandc 33,34,63", [](PPC64XEmitter& e) { e.XXLANDC(VS33, VS34, VS63); }, 0xF022FC57},
    {"xxlor 33,34,63", [](PPC64XEmitter& e) { e.XXLOR(VS33, VS34, VS63); }, 0xF022FC97},
    {"xxlorc 33,34,63", [](PPC64XEmitter& e) { e.XXLORC(VS33, VS34, VS63); }, 0xF022FD57},
    {"xxlxor 33,34,63", [](PPC64XEmitter& e) { e.XXLXOR(VS33, VS34, VS63); }, 0xF022FCD7},
    {"xxlnor 33,34,63", [](PPC64XEmitter& e) { e.XXLNOR(VS33, VS34, VS63); }, 0xF022FD17},
    {"xxlnand 33,34,63", [](PPC64XEmitter& e) { e.XXLNAND(VS33, VS34, VS63); }, 0xF022FD97},
    {"xxleqv 33,34,63", [](PPC64XEmitter& e) { e.XXLEQV(VS33, VS34, VS63); }, 0xF022FDD7},
    {"xsadddp 34,1,63", [](PPC64XEmitter& e) { e.XSADDDP(VS34, VS1, VS63); }, 0xF041F903},
    {"xsadddp 1,34,2", [](PPC64XEmitter& e) { e.XSADDDP(VS1, VS34, VS2); }, 0xF0221104},
    {"xssubdp 34,1,63", [](PPC64XEmitter& e) { e.XSSUBDP(VS34, VS1, VS63); }, 0xF041F943},
    {"xssubdp 1,34,2", [](PPC64XEmitter& e) { e.XSSUBDP(VS1, VS34, VS2); }, 0xF0221144},
    {"xsmuldp 34,1,63", [](PPC64XEmitter& e) { e.XSMULDP(VS34, VS1, VS63); }, 0xF041F983},
    {"xsmuldp 1,34,2", [](PPC64XEmitter& e) { e.XSMULDP(VS1, VS34, VS2); }, 0xF0221184},
    {"xsdivdp 34,1,63", [](PPC64XEmitter& e) { e.XSDIVDP(VS34, VS1, VS63); }, 0xF041F9C3},
    {"xsdivdp 1,34,2", [](PPC64XEmitter& e) { e.XSDIVDP(VS1, VS34, VS2); }, 0xF02211C4},
    {"xsaddsp 34,1,63", [](PPC64XEmitter& e) { e.XSADDSP(VS34, VS1, VS63); }, 0xF041F803},
    {"xsaddsp 1,34,2", [](PPC64XEmitter& e) { e.XSADDSP(VS1, VS34, VS2); }, 0xF0221004},
    {"xssubsp 34,1,63", [](PPC64XEmitter& e) { e.XSSUBSP(VS34, VS1, VS63); }, 0xF041F843},
    {"xssubsp 1,34,2", [](PPC64XEmitter& e) { e.XSSUBSP(VS1, VS34, VS2); }, 0xF0221044},
    {"xsmulsp 34,1,63", [](PPC64XEmitter& e) { e.XSMULSP(VS34, VS1, VS63); }, 0xF041F883},
    {"xsmulsp 1,34,2", [](PPC64XEmitter& e) { e.XSMULSP(VS1, VS34, VS2); }, 0xF0221084},
    {"xsdivsp 34,1,63", [](PPC64XEmitter& e) { e.XSDIVSP(VS34, VS1, VS63); }, 0xF041F8C3},
    {"xsdivsp 1,34,2", [](PPC64XEmitter& e) { e.XSDIVSP(VS1, VS34, VS2); }, 0xF02210C4},
    {"xsmaddadp 34,1,63", [](PPC64XEmitter& e) { e.XSMADDADP(VS34, VS1, VS63); }, 0xF041F90B},
    {"xsmaddadp 1,34,2", [](PPC64XEmitter& e) { e.XSMADDADP(VS1, VS34, VS2); }, 0xF022110C},
    {"xsmaddmdp 34,1,63", [](PPC64XEmitter& e) { e.XSMADDMDP(VS34, VS1, VS63); }, 0xF041F94B},
    {"xsmaddmdp 1,34,2", [](PPC64XEmitter& e) { e.XSMADDMDP(VS1, VS34, VS2); }, 0xF022114C},
    {"xsmsubadp 34,1,63", [](PPC64XEmitter& e) { e.XSMSUBADP(VS34, VS1, VS63); }, 0xF041F98B},
    {"xsmsubadp 1,34,2", [](PPC64XEmitter& e) { e.XSMSUBADP(VS1, VS34, VS2); }, 0xF022118C},
    {"xsmsubmdp 34,1,63", [](PPC64XEmitter& e) { e.XSMSUBMDP(VS34, VS1, VS63); }, 0xF041F9CB},
    {"xsmsubmdp 1,34,2", [](PPC64XEmitter& e) { e.XSMSUBMDP(VS1, VS34, VS2); }, 0xF02211CC},
    {"xsnmaddadp 34,1,63", [](PPC64XEmitter& e) { e.XSNMADDADP(VS34, VS1, VS63); }, 0xF041FD0B},
    {"xsnmaddadp 1,34,2", [](PPC64XEmitter& e) { e.XSNMADDADP(VS1, VS34, VS2); }, 0xF022150C},
    {"xsnmaddmdp 34,1,63", [](PPC64XEmitter& e) { e.XSNMADDMDP(VS34, VS1, VS63); }, 0xF041FD4B},
    {"xsnmaddmdp 1,34,2", [](PPC64XEmitter& e) { e.XSNMADDMDP(VS1, VS34, VS2); }, 0xF022154C},
    {"xsnmsubadp 34,1,63", [](PPC64XEmitter& e) { e.XSNMSUBADP(VS34, VS1, VS63); }, 0xF041FD8B},
    {"xsnmsubadp 1,34,2", [](PPC64XEmitter& e) { e.XSNMSUBADP(VS1, VS34, VS2); }, 0xF022158C},
    {"xsnmsubmdp 34,1,63", [](PPC64XEmitter& e) { e.XSNMSUBMDP(VS34, VS1, VS63); }, 0xF041FDCB},
    {"xsnmsubmdp 1,34,2", [](PPC64XEmitter& e) { e.XSNMSUBMDP(VS1, VS34, VS2); }, 0xF02215CC},
    {"xsmaddasp 34,1,63", [](PPC64XEmitter& e) { e.XSMADDASP(VS34, VS1, VS63); }, 0xF041F80B},
    {"xsmaddasp 1,34,2", [](PPC64XEmitter& e) { e.XSMADDASP(VS1, VS34, VS2); }, 0xF022100C},
    {"xsmaddmsp 34,1,63", [](PPC64XEmitter& e) { e.XSMADDMSP(VS34, VS1, VS63); }, 0xF041F84B},
    {"xsmaddmsp 1,34,2", [](PPC64XEmitter& e) { e.XSMADDMSP(VS1, VS34, VS2); }, 0xF022104C},
    {"xsmsubasp 34,1,63", [](PPC64XEmitter& e) { e.XSMSUBASP(VS34, VS1, VS63); }, 0xF041F88B},
    {"xsmsubasp 1,34,2", [](PPC64XEmitter& e) { e.XSMSUBASP(VS1, VS34, VS2); }, 0xF022108C},
    {"xsmsubmsp 34,1,63", [](PPC64XEmitter& e) { e.XSMSUBMSP(VS34, VS1, VS63); }, 0xF041F8CB},
    {"xsmsubmsp 1,34,2", [](PPC64XEmitter& e) { e.XSMSUBMSP(VS1, VS34, VS2); }, 0xF02210CC},
    {"xsnmaddasp 34,1,63", [](PPC64XEmitter& e) { e.XSNMADDASP(VS34, VS1, VS63); }, 0xF041FC0B},
    {"xsnmaddasp 1,34,2", [](PPC64XEmitter& e) { e.XSNMADDASP(VS1, VS34, VS2); }, 0xF022140C},
    {"xsnmaddmsp 34,1,63", [](PPC64XEmitter& e) { e.XSNMADDMSP(VS34, VS1, VS63); }, 0xF041FC4B},
    {"xsnmaddmsp 1,34,2", [](PPC64XEmitter& e) { e.XSNMADDMSP(VS1, VS34, VS2); }, 0xF022144C},
    {"xsnmsubasp 34,1,63", [](PPC64XEmitter& e) { e.XSNMSUBASP(VS34, VS1, VS63); }, 0xF041FC8B},
    {"xsnmsubasp 1,34,2", [](PPC64XEmitter& e) { e.XSNMSUBASP(VS1, VS34, VS2); }, 0xF022148C},
    {"xsnmsubmsp 34,1,63", [](PPC64XEmitter& e) { e.XSNMSUBMSP(VS34, VS1, VS63); }, 0xF041FCCB},
    {"xsnmsubmsp 1,34,2", [](PPC64XEmitter& e) { e.XSNMSUBMSP(VS1, VS34, VS2); }, 0xF02214CC},
    {"xsmaxdp 34,1,63", [](PPC64XEmitter& e) { e.XSMAXDP(VS34, VS1, VS63); }, 0xF041FD03},
    {"xsmaxdp 1,34,2", [](PPC64XEmitter& e) { e.XSMAXDP(VS1, VS34, VS2); }, 0xF0221504},
    {"xsmindp 34,1,63", [](PPC64XEmitter& e) { e.XSMINDP(VS34, VS1, VS63); }, 0xF041FD43},
    {"xsmindp 1,34,2", [](PPC64XEmitter& e) { e.XSMINDP(VS1, VS34, VS2); }, 0xF0221544},
    {"xscpsgndp 34,1,63", [](PPC64XEmitter& e) { e.XSCPSGNDP(VS34, VS1, VS63); }, 0xF041FD83},
    {"xscpsgndp 1,34,2", [](PPC64XEmitter& e) { e.XSCPSGNDP(VS1, VS34, VS2); }, 0xF0221584},
    {"xvadddp 34,1,63", [](PPC64XEmitter& e) { e.XVADDDP(VS34, VS1, VS63); }, 0xF041FB03},
    {"xvadddp 1,34,2", [](PPC64XEmitter& e) { e.XVADDDP(VS1, VS34, VS2); }, 0xF0221304},
    {"xvsubdp 34,1,63", [](PPC64XEmitter& e) { e.XVSUBDP(VS34, VS1, VS63); }, 0xF041FB43},
    {"xvsubdp 1,34,2", [](PPC64XEmitter& e) { e.XVSUBDP(VS1, VS34, VS2); }, 0xF0221344},
    {"xvmuldp 34,1,63", [](PPC64XEmitter& e) { e.XVMULDP(VS34, VS1, VS63); }, 0xF041FB83},
    {"xvmuldp 1,34,2", [](PPC64XEmitter& e) { e.XVMULDP(VS1, VS34, VS2); }, 0xF0221384},
    {"xvdivdp 34,1,63", [](PPC64XEmitter& e) { e.XVDIVDP(VS34, VS1, VS63); }, 0xF041FBC3},
    {"xvdivdp 1,34,2", [](PPC64XEmitter& e) { e.XVDIVDP(VS1, VS34, VS2); }, 0xF02213C4},
    {"xvmaddadp 34,1,63", [](PPC64XEmitter& e) { e.XVMADDADP(VS34, VS1, VS63); }, 0xF041FB0B},
    {"xvmaddadp 1,34,2", [](PPC64XEmitter& e) { e.XVMADDADP(VS1, VS34, VS2); }, 0xF022130C},
    {"xvmaddmdp 34,1,63", [](PPC64XEmitter& e) { e.XVMADDMDP(VS34, VS1, VS63); }, 0xF041FB4B},
    {"xvmaddmdp 1,34,2", [](PPC64XEmitter& e) { e.XVMADDMDP(VS1, VS34, VS2); }, 0xF022134C},
    {"xvmsubadp 34,1,63", [](PPC64XEmitter& e) { e.XVMSUBADP(VS34, VS1, VS63); }, 0xF041FB8B},
    {"xvmsubadp 1,34,2", [](PPC64XEmitter& e) { e.XVMSUBADP(VS1, VS34, VS2); }, 0xF022138C},
    {"xvmsubmdp 34,1,63", [](PPC64XEmitter& e) { e.XVMSUBMDP(VS34, VS1, VS63); }, 0xF041FBCB},
    {"xvmsubmdp 1,34,2", [](PPC64XEmitter& e) { e.XVMSUBMDP(VS1, VS34, VS2); }, 0xF02213CC},
    {"xvnmaddadp 34,1,63", [](PPC64XEmitter& e) { e.XVNMADDADP(VS34, VS1, VS63); }, 0xF041FF0B},
    {"xvnmaddadp 1,34,2", [](PPC64XEmitter& e) { e.XVNMADDADP(VS1, VS34, VS2); }, 0xF022170C},
    {"xvnmaddmdp 34,1,63", [](PPC64XEmitter& e) { e.XVNMADDMDP(VS34, VS1, VS63); }, 0xF041FF4B},
    {"xvnmaddmdp 1,34,2", [](PPC64XEmitter& e) { e.XVNMADDMDP(VS1, VS34, VS2); }, 0xF022174C},
    {"xvnmsubadp 34,1,63", [](PPC64XEmitter& e) { e.XVNMSUBADP(VS34, VS1, VS63); }, 0xF041FF8B},
    {"xvnmsubadp 1,34,2", [](PPC64XEmitter& e) { e.XVNMSUBADP(VS1, VS34, VS2); }, 0xF022178C},
    {"xvnmsubmdp 34,1,63", [](PPC64XEmitter& e) { e.XVNMSUBMDP(VS34, VS1, VS63); }, 0xF041FFCB},
    {"xvnmsubmdp 1,34,2", [](PPC64XEmitter& e) { e.XVNMSUBMDP(VS1, VS34, VS2); }, 0xF02217CC},
    {"xvmaxdp 34,1,63", [](PPC64XEmitter& e) { e.XVMAXDP(VS34, VS1, VS63); }, 0xF041FF03},
    {"xvmaxdp 1,34,2", [](PPC64XEmitter& e) { e.XVMAXDP(VS1, VS34, VS2); }, 0xF0221704},
    {"xvmindp 34,1,63", [](PPC64XEmitter& e) { e.XVMINDP(VS34, VS1, VS63); }, 0xF041FF43},
    {"xvmindp 1,34,2", [](PPC64XEmitter& e) { e.XVMINDP(VS1, VS34, VS2); }, 0xF0221744},
    {"xvcpsgndp 34,1,63", [](PPC64XEmitter& e) { e.XVCPSGNDP(VS34, VS1, VS63); }, 0xF041FF83},
    {"xvcpsgndp 1,34,2", [](PPC64XEmitter& e) { e.XVCPSGNDP(VS1, VS34, VS2); }, 0xF0221784},
    {"xvcmpeqdp 34,1,63", [](PPC64XEmitter& e) { e.XVCMPEQDP(VS34, VS1, VS63); }, 0xF041FB1B},
    {"xvcmpeqdp 1,34,2", [](PPC64XEmitter& e) { e.XVCMPEQDP(VS1, VS34, VS2); }, 0xF022131C},
    {"xvcmpgedp 34,1,63", [](PPC64XEmitter& e) { e.XVCMPGEDP(VS34, VS1, VS63); }, 0xF041FB9B},
    {"xvcmpgedp 1,34,2", [](PPC64XEmitter& e) { e.XVCMPGEDP(VS1, VS34, VS2); }, 0xF022139C},
    {"xvcmpgtdp 34,1,63", [](PPC64XEmitter& e) { e.XVCMPGTDP(VS34, VS1, VS63); }, 0xF041FB5B},
    {"xvcmpgtdp 1,34,2", [](PPC64XEmitter& e) { e.XVCMPGTDP(VS1, VS34, VS2); }, 0xF022135C},
    {"xscmpudp 3,1,33", [](PPC64XEmitter& e) { e.XSCMPUDP(CR3, VS1, VS33); }, 0xF181091A},
    {"xscmpodp 7,34,2", [](PPC64XEmitter& e) { e.XSCMPODP(CR7, VS34, VS2); }, 0xF382115C},
    {"xssqrtdp 34,1", [](PPC64XEmitter& e) { e.XSSQRTDP(VS34, VS1); }, 0xF040092D},
    {"xssqrtdp 1,63", [](PPC64XEmitter& e) { e.XSSQRTDP(VS1, VS63); }, 0xF020F92E},
    {"xsabsdp 34,1", [](PPC64XEmitter& e) { e.XSABSDP(VS34, VS1); }, 0xF0400D65},
    {"xsabsdp 1,63", [](PPC64XEmitter& e) { e.XSABSDP(VS1, VS63); }, 0xF020FD66},
    {"xsnabsdp 34,1", [](PPC64XEmitter& e) { e.XSNABSDP(VS34, VS1); }, 0xF0400DA5},
    {"xsnabsdp 1,63", [](PPC64XEmitter& e) { e.XSNABSDP(VS1, VS63); }, 0xF020FDA6},
    {"xsnegdp 34,1", [](PPC64XEmitter& e) { e.XSNEGDP(VS34, VS1); }, 0xF0400DE5},
    {"xsnegdp 1,63", [](PPC64XEmitter& e) { e.XSNEGDP(VS1, VS63); }, 0xF020FDE6},
    {"xsredp 34,1", [](PPC64XEmitter& e) { e.XSREDP(VS34, VS1); }, 0xF0400969},
    {"xsredp 1,63", [](PPC64XEmitter& e) { e.XSREDP(VS1, VS63); }, 0xF020F96A},
    {"xsrsqrtedp 34,1", [](PPC64XEmitter& e) { e.XSRSQRTEDP(VS34, VS1); }, 0xF0400929},
    {"xsrsqrtedp 1,63", [](PPC64XEmitter& e) { e.XSRSQRTEDP(VS1, VS63); }, 0xF020F92A},
    {"xsrsp 34,1", [](PPC64XEmitter& e) { e.XSRSP(VS34, VS1); }, 0xF0400C65},
    {"xsrsp 1,63", [](PPC64XEmitter& e) { e.XSRSP(VS1, VS63); }, 0xF020FC66},
    {"xscvdpsp 34,1", [](PPC64XEmitter& e) { e.XSCVDPSP(VS34, VS1); }, 0xF0400C25},
    {"xscvdpsp 1,63", [](PPC64XEmitter& e) { e.XSCVDPSP(VS1, VS63); }, 0xF020FC26},
    {"xscvdpspn 34,1", [](PPC64XEmitter& e) { e.XSCVDPSPN(VS34, VS1); }, 0xF0400C2D},
    {"xscvdpspn 1,63", [](PPC64XEmitter& e) { e.XSCVDPSPN(VS1, VS63); }, 0xF020FC2E},
    {"xscvspdp 34,1", [](PPC64XEmitter& e) { e.XSCVSPDP(VS34, VS1); }, 0xF0400D25},
    {"xscvspdp 1,63", [](PPC64XEmitter& e) { e.XSCVSPDP(VS1, VS63); }, 0xF020FD26},
    {"xscvspdpn 34,1", [](PPC64XEmitter& e) { e.XSCVSPDPN(VS34, VS1); }, 0xF0400D2D},
    {"xscvspdpn 1,63", [](PPC64XEmitter& e) { e.XSCVSPDPN(VS1, VS63); }, 0xF020FD2E},
    {"xscvdpsxws 34,1", [](PPC64XEmitter& e) { e.XSCVDPSXWS(VS34, VS1); }, 0xF0400961},
    {"xscvdpsxws 1,63", [](PPC64XEmitter& e) { e.XSCVDPSXWS(VS1, VS63); }, 0xF020F962},
    {"xscvdpuxws 34,1", [](PPC64XEmitter& e) { e.XSCVDPUXWS(VS34, VS1); }, 0xF0400921},
    {"xscvdpuxws 1,63", [](PPC64XEmitter& e) { e.XSCVDPUXWS(VS1, VS63); }, 0xF020F922},
    {"xscvdpsxds 34,1", [](PPC64XEmitter& e) { e.XSCVDPSXDS(VS34, VS1); }, 0xF0400D61},
    {"xscvdpsxds 1,63", [](PPC64XEmitter& e) { e.XSCVDPSXDS(VS1, VS63); }, 0xF020FD62},
    {"xscvsxddp 34,1", [](PPC64XEmitter& e) { e.XSCVSXDDP(VS34, VS1); }, 0xF0400DE1},
    {"xscvsxddp 1,63", [](PPC64XEmitter& e) { e.XSCVSXDDP(VS1, VS63); }, 0xF020FDE2},
    {"xscvuxddp 34,1", [](PPC64XEmitter& e) { e.XSCVUXDDP(VS34, VS1); }, 0xF0400DA1},
    {"xscvuxddp 1,63", [](PPC64XEmitter& e) { e.XSCVUXDDP(VS1, VS63); }, 0xF020FDA2},
    {"xsrdpi 34,1", [](PPC64XEmitter& e) { e.XSRDPI(VS34, VS1); }, 0xF0400925},
    {"xsrdpi 1,63", [](PPC64XEmitter& e) { e.XSRDPI(VS1, VS63); }, 0xF020F926},
    {"xsrdpiz 34,1", [](PPC64XEmitter& e) { e.XSRDPIZ(VS34, VS1); }, 0xF0400965},
    {"xsrdpiz 1,63", [](PPC64XEmitter& e) { e.XSRDPIZ(VS1, VS63); }, 0xF020F966},
    {"xvsqrtdp 34,1", [](PPC64XEmitter& e) { e.XVSQRTDP(VS34, VS1); }, 0xF0400B2D},
    {"xvsqrtdp 1,63", [](PPC64XEmitter& e) { e.XVSQRTDP(VS1, VS63); }, 0xF020FB2E},
    {"xvabsdp 34,1", [](PPC64XEmitter& e) { e.XVABSDP(VS34, VS1); }, 0xF0400F65},
    {"xvabsdp 1,63", [](PPC64XEmitter& e) { e.XVABSDP(VS1, VS63); }, 0xF020FF66},
    {"xvnabsdp 34,1", [](PPC64XEmitter& e) { e.XVNABSDP(VS34, VS1); }, 0xF0400FA5},
    {"xvnabsdp 1,63", [](PPC64XEmitter& e) { e.XVNABSDP(VS1, VS63); }, 0xF020FFA6},
    {"xvnegdp 34,1", [](PPC64XEmitter& e) { e.XVNEGDP(VS34, VS1); }, 0xF0400FE5},
    {"xvnegdp 1,63", [](PPC64XEmitter& e) { e.XVNEGDP(VS1, VS63); }, 0xF020FFE6},
    {"xvredp 34,1", [](PPC64XEmitter& e) { e.XVREDP(VS34, VS1); }, 0xF0400B69},
    {"xvredp 1,63", [](PPC64XEmitter& e) { e.XVREDP(VS1, VS63); }, 0xF020FB6A},
    {"xvrsqrtedp 34,1", [](PPC64XEmitter& e) { e.XVRSQRTEDP(VS34, VS1); }, 0xF0400B29},
    {"xvrsqrtedp 1,63", [](PPC64XEmitter& e) { e.XVRSQRTEDP(VS1, VS63); }, 0xF020FB2A},
    {"xvcvdpsp 34,1", [](PPC64XEmitter& e) { e.XVCVDPSP(VS34, VS1); }, 0xF0400E25},
    {"xvcvdpsp 1,63", [](PPC64XEmitter& e) { e.XVCVDPSP(VS1, VS63); }, 0xF020FE26},
    {"xvcvspdp 34,1", [](PPC64XEmitter& e) { e.XVCVSPDP(VS34, VS1); }, 0xF0400F25},
    {"xvcvspdp 1,63", [](PPC64XEmitter& e) { e.XVCVSPDP(VS1, VS63); }, 0xF020FF26},
    {"mfvscr 3", [](PPC64XEmitter& e) { e.MFVSCR(V3); }, 0x10600604},
    {"mtvscr 4", [](PPC64XEmitter& e) { e.MTVSCR(V4); }, 0x10002644},
    {"li 3,-1", [](PPC64XEmitter& e) { e.LI(R3, -1); }, 0x3860FFFF},
    {"lis 3,0x1234", [](PPC64XEmitter& e) { e.LIS(R3, 0x1234); }, 0x3C601234},
    {"mr 3,4", [](PPC64XEmitter& e) { e.MR(R3, R4); }, 0x7C832378},
    {"mr. 3,4", [](PPC64XEmitter& e) { e.MR(R3, R4, true); }, 0x7C832379},
    {"not 3,4", [](PPC64XEmitter& e) { e.NOT(R3, R4); }, 0x7C8320F8},
    {"sub 3,4,5", [](PPC64XEmitter& e) { e.SUB(R3, R4, R5); }, 0x7C652050},
    {"subc 3,4,5", [](PPC64XEmitter& e) { e.SUBC(R3, R4, R5); }, 0x7C652010},
    {"slwi 3,4,5", [](PPC64XEmitter& e) { e.SLWI(R3, R4, 5); }, 0x54832834},
    {"srwi 3,4,5", [](PPC64XEmitter& e) { e.SRWI(R3, R4, 5); }, 0x5483D97E},
    {"rotlwi 3,4,5", [](PPC64XEmitter& e) { e.ROTLWI(R3, R4, 5); }, 0x5483283E},
    {"clrlwi 3,4,16", [](PPC64XEmitter& e) { e.CLRLWI(R3, R4, 16); }, 0x5483043E},
    {"sldi 3,4,32", [](PPC64XEmitter& e) { e.SLDI(R3, R4, 32); }, 0x788307C6},
    {"srdi 3,4,48", [](PPC64XEmitter& e) { e.SRDI(R3, R4, 48); }, 0x78838420},
    {"rotldi 3,4,8", [](PPC64XEmitter& e) { e.ROTLDI(R3, R4, 8); }, 0x78834000},
    {"clrldi 3,4,32", [](PPC64XEmitter& e) { e.CLRLDI(R3, R4, 32); }, 0x78830020},
    {"clrrdi 3,4,12", [](PPC64XEmitter& e) { e.CLRRDI(R3, R4, 12); }, 0x788304E4},    // Power ISA 3.0 and 3.1, assembled with GNU as -mpower10.
    {"setb 3,1", [](PPC64XEmitter& e) { e.SETB(R3, CR1); }, 0x7C640100},
    {"addpcis 4,0", [](PPC64XEmitter& e) { e.ADDPCIS(R4, 0); }, 0x4C800004},
    {"addpcis 31,-1", [](PPC64XEmitter& e) { e.ADDPCIS(R31, -1); }, 0x4FFFFFC5},
    {"addpcis 3,0x1234", [](PPC64XEmitter& e) { e.ADDPCIS(R3, 0x1234); }, 0x4C7A1204},
    {"setb 31,7", [](PPC64XEmitter& e) { e.SETB(R31, CR7); }, 0x7FFC0100},
    {"cnttzw 3,4", [](PPC64XEmitter& e) { e.CNTTZW(R3, R4); }, 0x7C830434},
    {"cnttzw. 31,30", [](PPC64XEmitter& e) { e.CNTTZW(R31, R30, true); }, 0x7FDF0435},
    {"cnttzd 3,4", [](PPC64XEmitter& e) { e.CNTTZD(R3, R4); }, 0x7C830474},
    {"cnttzd. 31,30", [](PPC64XEmitter& e) { e.CNTTZD(R31, R30, true); }, 0x7FDF0475},
    {"xststdcdp 1,32,0x7f", [](PPC64XEmitter& e) { e.XSTSTDCDP(CR1, VS32, 0x7F); }, 0xF0FF05AA},
    {"xststdcdp 7,63,0x40", [](PPC64XEmitter& e) { e.XSTSTDCDP(CR7, VS63, 0x40); }, 0xF3C0FDAA},
    {"xststdcdp 0,5,0x30", [](PPC64XEmitter& e) { e.XSTSTDCDP(CR0, VS5, 0x30); }, 0xF0302DA8},
    {"xvtstdcsp 32,33,0x7f", [](PPC64XEmitter& e) { e.XVTSTDCSP(VS32, VS33, 0x7F); }, 0xF01F0EEF},
    {"xvtstdcsp 5,63,0x40", [](PPC64XEmitter& e) { e.XVTSTDCSP(VS5, VS63, 0x40); }, 0xF0A0FEEA},
    {"xsxexpdp 3,32", [](PPC64XEmitter& e) { e.XSXEXPDP(R3, VS32); }, 0xF060056E},
    {"xsxexpdp 31,5", [](PPC64XEmitter& e) { e.XSXEXPDP(R31, VS5); }, 0xF3E02D6C},
    {"xsxsigdp 3,32", [](PPC64XEmitter& e) { e.XSXSIGDP(R3, VS32); }, 0xF061056E},
    {"xxbrw 32,33", [](PPC64XEmitter& e) { e.XXBRW(VS32, VS33); }, 0xF00F0F6F},
    {"xxbrw 5,63", [](PPC64XEmitter& e) { e.XXBRW(VS5, VS63); }, 0xF0AFFF6E},
    {"xxbrd 32,33", [](PPC64XEmitter& e) { e.XXBRD(VS32, VS33); }, 0xF0170F6F},
    {"xxbrh 32,33", [](PPC64XEmitter& e) { e.XXBRH(VS32, VS33); }, 0xF0070F6F},
    {"xxbrq 32,33", [](PPC64XEmitter& e) { e.XXBRQ(VS32, VS33); }, 0xF01F0F6F},
    {"lxvwsx 32,3,4", [](PPC64XEmitter& e) { e.LXVWSX(VS32, R3, R4); }, 0x7C0322D9},
    {"lxvwsx 5,0,31", [](PPC64XEmitter& e) { e.LXVWSX(VS5, R0, R31); }, 0x7CA0FAD8},
    {"lxv 32,16(3)", [](PPC64XEmitter& e) { e.LXV(VS32, R3, 16); }, 0xF4030019},
    {"lxv 5,-32(31)", [](PPC64XEmitter& e) { e.LXV(VS5, R31, -32); }, 0xF4BFFFE1},
    {"stxv 32,16(3)", [](PPC64XEmitter& e) { e.STXV(VS32, R3, 16); }, 0xF403001D},
    {"stxv 5,-32(31)", [](PPC64XEmitter& e) { e.STXV(VS5, R31, -32); }, 0xF4BFFFE5},
    {"lxvx 32,3,4", [](PPC64XEmitter& e) { e.LXVX(VS32, R3, R4); }, 0x7C032219},
    {"stxvx 32,3,4", [](PPC64XEmitter& e) { e.STXVX(VS32, R3, R4); }, 0x7C032319},
    {"lxsd 20,8(3)", [](PPC64XEmitter& e) { e.LXSD(VS52, R3, 8); }, 0xE683000A},
    {"lxsd 0,-16(31)", [](PPC64XEmitter& e) { e.LXSD(VS32, R31, -16); }, 0xE41FFFF2},
    {"stxsd 20,8(3)", [](PPC64XEmitter& e) { e.STXSD(VS52, R3, 8); }, 0xF683000A},
    {"lxssp 20,4(3)", [](PPC64XEmitter& e) { e.LXSSP(VS52, R3, 4); }, 0xE6830007},
    {"stxssp 20,4(3)", [](PPC64XEmitter& e) { e.STXSSP(VS52, R3, 4); }, 0xF6830007},
    {"mtvsrws 32,3", [](PPC64XEmitter& e) { e.MTVSRWS(VS32, R3); }, 0x7C030327},
    {"mtvsrws 5,31", [](PPC64XEmitter& e) { e.MTVSRWS(VS5, R31); }, 0x7CBF0326},
    {"mtvsrdd 32,3,4", [](PPC64XEmitter& e) { e.MTVSRDD(VS32, R3, R4); }, 0x7C032367},
    {"mtvsrdd 5,31,0", [](PPC64XEmitter& e) { e.MTVSRDD(VS5, R31, R0); }, 0x7CBF0366},
    {"mfvsrld 3,32", [](PPC64XEmitter& e) { e.MFVSRLD(R3, VS32); }, 0x7C030267},
    {"mfvsrld 31,5", [](PPC64XEmitter& e) { e.MFVSRLD(R31, VS5); }, 0x7CBF0266},
    {"xxspltib 32,255", [](PPC64XEmitter& e) { e.XXSPLTIB(VS32, 255); }, 0xF007FAD1},
    {"xxspltib 5,0", [](PPC64XEmitter& e) { e.XXSPLTIB(VS5, 0); }, 0xF0A002D0},
    {"setbc 3,0", [](PPC64XEmitter& e) { e.SETBC(R3, 0); }, 0x7C600300},
    {"setbc 31,31", [](PPC64XEmitter& e) { e.SETBC(R31, 31); }, 0x7FFF0300},
    {"setbcr 3,2", [](PPC64XEmitter& e) { e.SETBCR(R3, 2); }, 0x7C620340},
    {"setnbc 3,1", [](PPC64XEmitter& e) { e.SETNBC(R3, 1); }, 0x7C610380},
    {"setnbcr 3,3", [](PPC64XEmitter& e) { e.SETNBCR(R3, 3); }, 0x7C6303C0},
    {"brw 3,4", [](PPC64XEmitter& e) { e.BRW(R3, R4); }, 0x7C830136},
    {"brd 3,4", [](PPC64XEmitter& e) { e.BRD(R3, R4); }, 0x7C830176},
    {"brh 31,30", [](PPC64XEmitter& e) { e.BRH(R31, R30); }, 0x7FDF01B6},
    {"vsld 0,1,2", [](PPC64XEmitter& e) { e.VSLD(VS32, VS33, VS34); }, 0x100115C4},
    {"vsld 31,30,29", [](PPC64XEmitter& e) { e.VSLD(VS63, VS62, VS61); }, 0x13FEEDC4},
    {"vcmpequd 0,1,2", [](PPC64XEmitter& e) { e.VCMPEQUD(VS32, VS33, VS34); }, 0x100110C7},
    {"xvcvsxddp 32,33", [](PPC64XEmitter& e) { e.XVCVSXDDP(VS32, VS33); }, 0xF0000FE3},
    {"xvcvsxddp 5,63", [](PPC64XEmitter& e) { e.XVCVSXDDP(VS5, VS63); }, 0xF0A0FFE2},
    {"xvcvdpsxws 32,33", [](PPC64XEmitter& e) { e.XVCVDPSXWS(VS32, VS33); }, 0xF0000B63},
    {"xvcvdpsxws 5,63", [](PPC64XEmitter& e) { e.XVCVDPSXWS(VS5, VS63); }, 0xF0A0FB62},
    {"vmaxsw 0,1,2", [](PPC64XEmitter& e) { e.VMAXSW(VS32, VS33, VS34); }, 0x10011182},
    {"vmaxsw 31,30,29", [](PPC64XEmitter& e) { e.VMAXSW(VS63, VS62, VS61); }, 0x13FEE982},
    {"vminsw 0,1,2", [](PPC64XEmitter& e) { e.VMINSW(VS32, VS33, VS34); }, 0x10011382},
    {"vminsw 31,30,29", [](PPC64XEmitter& e) { e.VMINSW(VS63, VS62, VS61); }, 0x13FEEB82},
    {"vcmpequd. 31,30,29", [](PPC64XEmitter& e) { e.VCMPEQUD(VS63, VS62, VS61, true); },
     0x13FEECC7},
};

// Emits functions and calls them, using the ELFv2 calling convention.
class TestCode : public PPC64CodeBlock
{
public:
  explicit TestCode(size_t size = 4096) { AllocCodeSpace(size); }

  template <typename F>
  void Emit(F f)
  {
    ResetCodePtr();
    m_start = GetWritableCodePtr();
    {
      const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
      f();
    }
    ASSERT_FALSE(HasWriteFailed());
    FlushIcacheSection(m_start, GetWritableCodePtr());
  }

  size_t InstructionCount() const { return (GetCodePtr() - m_start) / sizeof(u32); }

  template <typename R, typename... Args>
  R Call(Args... args) const
  {
    return std::bit_cast<R (*)(Args...)>(m_start)(args...);
  }

  u8* RegionStart() const { return region; }

private:
  u8* m_start = nullptr;
};

u64 TwoParameterFunction(u64 a, u64 b)
{
  return a * 10 + b + 3;
}

// Emits a function that returns the given constant.
void EmitReturnConstant(PPC64XEmitter& e, u64 value)
{
  e.MOVI2R(R3, value);
  e.BLR();
}
}  // namespace

TEST(PPC64Emitter, Encoding)
{
  for (const EncodingCase& c : encoding_cases)
  {
    std::array<u8, 16> buffer{};
    PPC64XEmitter e(buffer.data(), buffer.data() + buffer.size());
    c.emit(e);
    ASSERT_EQ(sizeof(u32), static_cast<size_t>(e.GetCodePtr() - buffer.data())) << c.assembly;

    u32 actual;
    std::memcpy(&actual, buffer.data(), sizeof(u32));
    EXPECT_EQ(c.expected, actual) << fmt::format("{}: expected {:08x}, emitted {:08x}", c.assembly,
                                                 c.expected, actual);
  }
}

TEST(PPC64Emitter, MOVI2R)
{
  TestCode code;
  std::vector<u64> values = {
      0,
      1,
      0xFFFFFFFFFFFFFFFF,
      0x7FFF,
      0x8000,
      0xFFFF,
      0x10000,
      0x12345678,
      0x7FFFFFFF,
      0x80000000,
      0xFFFFFFFF,
      0xFFFF0000,
      0xFFFFFFFF80000000,
      0xFFFFFFFFFFFF8000,
      0x100000000,
      0x123456789ABCDEF0,
      0x8000000000000000,
      0x7FFFFFFFFFFFFFFF,
      0xFFFF0000FFFF0000,
      0x0000FFFF0000FFFF,
      0xFFFFFFFF00000000,
      0x0000FFFF00000000,
      0x00000001FFFFFFFF,
      0x00007FFF80000000,
  };
  std::mt19937_64 rng(0x5EED);
  for (int i = 0; i < 10000; i++)
  {
    // Mostly random values, with random 16-bit halves cleared or set to exercise every path.
    u64 value = rng();
    for (int half = 0; half < 4; half++)
    {
      const u32 choice = rng() % 4;
      if (choice == 0)
        value &= ~(u64(0xFFFF) << (half * 16));
      else if (choice == 1)
        value |= u64(0xFFFF) << (half * 16);
    }
    values.push_back(value);
  }

  for (const u64 value : values)
  {
    code.Emit([&] { EmitReturnConstant(code, value); });
    EXPECT_EQ(value, code.Call<u64>()) << fmt::format("{:016x}", value);
    // MOVI2R itself takes at most 5 instructions, plus the BLR.
    EXPECT_LE(code.InstructionCount(), 6u) << fmt::format("{:016x}", value);

    const s64 signed_value = static_cast<s64>(value);
    if (signed_value == static_cast<s16>(signed_value))
    {
      EXPECT_EQ(2u, code.InstructionCount()) << fmt::format("{:016x}", value);
    }
    else if (signed_value == static_cast<s32>(signed_value))
    {
      EXPECT_LE(code.InstructionCount(), 3u) << fmt::format("{:016x}", value);
    }
  }
}

TEST(PPC64Emitter, PrefixedEncoding)
{
  // Assembled with GNU as -mpower10: the prefix, then the suffix.
  struct PrefixedCase
  {
    const char* assembly;
    void (*emit)(PPC64XEmitter& e);
    u32 prefix;
    u32 suffix;
  };
  const PrefixedCase cases[] = {
      {"pli 3,0x1ffffffff", [](PPC64XEmitter& e) { e.PLI(R3, 0x1FFFFFFFF); }, 0x0601FFFF,
       0x3860FFFF},
      {"pli 31,-0x200000000", [](PPC64XEmitter& e) { e.PLI(R31, -0x200000000); }, 0x06020000,
       0x3BE00000},
      {"pli 5,0x80008000", [](PPC64XEmitter& e) { e.PLI(R5, 0x80008000); }, 0x06008000, 0x38A08000},
      {"xxspltiw 32,0x3f800000", [](PPC64XEmitter& e) { e.XXSPLTIW(VS32, 0x3F800000); }, 0x05003F80,
       0x80070000},
      {"xxspltiw 5,0xffffffff", [](PPC64XEmitter& e) { e.XXSPLTIW(VS5, 0xFFFFFFFF); }, 0x0500FFFF,
       0x80A6FFFF},
      {"xxspltidp 32,0x3f800000", [](PPC64XEmitter& e) { e.XXSPLTIDP(VS32, 0x3F800000); },
       0x05003F80, 0x80050000},
      {"xxspltidp 5,0x7fc00000", [](PPC64XEmitter& e) { e.XXSPLTIDP(VS5, 0x7FC00000); }, 0x05007FC0,
       0x80A40000},
  };
  // At every word of a 64-byte block: a prefixed instruction at its last word would cross into
  // the next one, so a nop comes first.
  alignas(64) std::array<u8, 192> buffer{};
  for (const PrefixedCase& c : cases)
  {
    for (u32 offset = 0; offset < 64; offset += 4)
    {
      PPC64XEmitter e(buffer.data() + 64 + offset, buffer.data() + buffer.size());
      c.emit(e);
      const bool aligned = offset == 60;
      ASSERT_EQ(aligned ? 12u : 8u,
                static_cast<size_t>(e.GetCodePtr() - buffer.data() - 64 - offset))
          << c.assembly;
      std::array<u32, 3> words;
      std::memcpy(words.data(), buffer.data() + 64 + offset, sizeof(words));
      if (aligned)
      {
        EXPECT_EQ(0x60000000u, words[0]) << c.assembly;
      }
      EXPECT_EQ(c.prefix, words[aligned]) << c.assembly << " at " << offset;
      EXPECT_EQ(c.suffix, words[aligned + 1]) << c.assembly << " at " << offset;
    }
  }

  // PC-relative: pla 3,0 and pld 3,0(0),1 address themselves.
  {
    alignas(64) std::array<u8, 64> code{};
    PPC64XEmitter e(code.data(), code.data() + code.size());
    e.PLA(R3, code.data());
    e.PLD(R3, code.data() + 8);
    std::array<u32, 4> words;
    std::memcpy(words.data(), code.data(), sizeof(words));
    EXPECT_EQ(0x06100000u, words[0]);
    EXPECT_EQ(0x38600000u, words[1]);
    EXPECT_EQ(0x04100000u, words[2]);
    EXPECT_EQ(0xE4600000u, words[3]);
  }
}

TEST(PPC64Emitter, ISA3_0)
{
  if (!cpu_info.bISA_3_0)
  {
    GTEST_SKIP() << "The host doesn't implement Power ISA 3.0";
  }

  TestCode code;
  // setb: -1, 0 or 1 from a comparison.
  code.Emit([&] {
    code.CMPD(CR1, R3, R4);
    code.SETB(R3, CR1);
    code.BLR();
  });
  EXPECT_EQ(-1, code.Call<s64>(s64(1), s64(2)));
  EXPECT_EQ(0, code.Call<s64>(s64(2), s64(2)));
  EXPECT_EQ(1, code.Call<s64>(s64(3), s64(2)));

  code.Emit([&] {
    code.CNTTZW(R3, R3);
    code.BLR();
  });
  EXPECT_EQ(4u, code.Call<u64>(u64(0x10)));
  EXPECT_EQ(32u, code.Call<u64>(u64(0x100000000)));

  // Byte reversal and the moves of both doublewords.
  code.Emit([&] {
    code.MTVSRDD(VS32, R3, R4);
    code.XXBRW(VS32, VS32);
    code.MFVSRLD(R3, VS32);
    code.BLR();
  });
  EXPECT_EQ(0x7856341221436587u, code.Call<u64>(u64(0x1111111111111111), u64(0x1234567887654321)));
  code.Emit([&] {
    code.MTVSRDD(VS32, R3, R4);
    code.XXBRD(VS32, VS32);
    code.MFVSRD(R3, VS32);
    code.BLR();
  });
  EXPECT_EQ(0x2143658778563412u, code.Call<u64>(u64(0x1234567887654321), u64(0)));
  code.Emit([&] {
    code.MTVSRWS(VS33, R3);
    code.MFVSRLD(R3, VS33);
    code.BLR();
  });
  EXPECT_EQ(0xAABBCCDDAABBCCDDu, code.Call<u64>(u64(0xAABBCCDD)));
  code.Emit([&] {
    code.XXSPLTIB(VS34, 0x5A);
    code.MFVSRD(R3, VS34);
    code.BLR();
  });
  EXPECT_EQ(0x5A5A5A5A5A5A5A5Au, code.Call<u64>());

  // Loads and stores in the host's element order, and the scalar ones.
  alignas(16) const std::array<u64, 2> memory = {0x1111111111111111, 0x2222222222222222};
  alignas(16) std::array<u64, 2> stored{};
  code.Emit([&] {
    code.LXV(VS35, R3, 0);
    code.STXV(VS35, R4, 0);
    code.MFVSRLD(R3, VS35);
    code.BLR();
  });
  // In the host's (little-endian) order, doubleword 1 is the one at the lower address.
  EXPECT_EQ(memory[0], code.Call<u64>(memory.data(), stored.data()));
  EXPECT_EQ(memory, stored);
  stored = {};
  code.Emit([&] {
    code.LXVX(VS35, R0, R3);
    code.STXVX(VS35, R0, R4);
    code.BLR();
  });
  code.Call<void>(memory.data(), stored.data());
  EXPECT_EQ(memory, stored);
  code.Emit([&] {
    code.LXSD(VS36, R3, 8);
    code.MFVSRD(R3, VS36);
    code.BLR();
  });
  EXPECT_EQ(memory[1], code.Call<u64>(memory.data()));
  const float one = 1.0f;
  code.Emit([&] {
    code.LXSSP(VS37, R3, 0);
    code.MFVSRD(R3, VS37);
    code.BLR();
  });
  EXPECT_EQ(std::bit_cast<u64>(1.0), code.Call<u64>(&one));
  const u32 word = 0x01020304;
  code.Emit([&] {
    code.LXVWSX(VS38, R0, R3);
    code.MFVSRLD(R3, VS38);
    code.BLR();
  });
  EXPECT_EQ(0x0102030401020304u, code.Call<u64>(&word));

  // Test data class: EQ is set if the class matches.
  code.Emit([&] {
    code.XSTSTDCDP(CR1, ToVSR(F1), 0x40);
    code.SETB(R3, CR1);
    code.BLR();
  });
  EXPECT_EQ(0, code.Call<s64>(1.0));
  code.Emit([&] {
    code.XSTSTDCDP(CR1, ToVSR(F1), 0x40);
    code.MFOCRF(R3, CR1);
    code.RLWINM(R3, R3, 4 + 3, 31, 31);  // EQ of CR1
    code.BLR();
  });
  EXPECT_EQ(1u, code.Call<u64>(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_EQ(0u, code.Call<u64>(1.0));
  code.Emit([&] {
    code.XSXEXPDP(R3, ToVSR(F1));
    code.BLR();
  });
  EXPECT_EQ(1023u + 3, code.Call<u64>(8.0));
  code.Emit([&] {
    code.XSXSIGDP(R3, ToVSR(F1));
    code.BLR();
  });
  EXPECT_EQ(0x0018000000000000u, code.Call<u64>(1.5));
  code.Emit([&] {
    code.MTVSRWS(VS39, R3);
    code.XVTSTDCSP(VS39, VS39, 0x30);  // infinities
    code.MFVSRD(R3, VS39);
    code.BLR();
  });
  EXPECT_EQ(~u64(0), code.Call<u64>(u64(0x7F800000)));
  EXPECT_EQ(0u, code.Call<u64>(u64(0x3F800000)));
}

TEST(PPC64Emitter, ISA3_1)
{
  if (!cpu_info.bISA_3_1)
  {
    GTEST_SKIP() << "The host doesn't implement Power ISA 3.1";
  }

  TestCode code;
  code.Emit([&] {
    code.CMPD(CR1, R3, R4);
    code.SETBC(R3, CRBit(CR1, CCFlags::LT));
    code.BLR();
  });
  EXPECT_EQ(1u, code.Call<u64>(s64(1), s64(2)));
  EXPECT_EQ(0u, code.Call<u64>(s64(2), s64(2)));
  code.Emit([&] {
    code.CMPD(CR1, R3, R4);
    code.SETNBC(R3, CRBit(CR1, CCFlags::EQ));
    code.BLR();
  });
  EXPECT_EQ(-1, code.Call<s64>(s64(2), s64(2)));
  EXPECT_EQ(0, code.Call<s64>(s64(1), s64(2)));
  code.Emit([&] {
    code.CMPD(CR1, R3, R4);
    code.SETBCR(R3, CRBit(CR1, CCFlags::EQ));
    code.BLR();
  });
  EXPECT_EQ(0u, code.Call<u64>(s64(2), s64(2)));
  EXPECT_EQ(1u, code.Call<u64>(s64(1), s64(2)));

  code.Emit([&] {
    code.BRW(R3, R3);
    code.BLR();
  });
  EXPECT_EQ(0x4433221188776655u, code.Call<u64>(u64(0x1122334455667788)));
  code.Emit([&] {
    code.BRD(R3, R3);
    code.BLR();
  });
  EXPECT_EQ(0x8877665544332211u, code.Call<u64>(u64(0x1122334455667788)));
  code.Emit([&] {
    code.BRH(R3, R3);
    code.BLR();
  });
  EXPECT_EQ(0x2211443366558877u, code.Call<u64>(u64(0x1122334455667788)));

  code.Emit([&] {
    code.XXSPLTIW(VS40, 0xDEADBEEF);
    code.MFVSRLD(R3, VS40);
    code.BLR();
  });
  EXPECT_EQ(0xDEADBEEFDEADBEEFu, code.Call<u64>());
  code.Emit([&] {
    code.XXSPLTIDP(VS41, std::bit_cast<u32>(-2.5f));
    code.MFVSRD(R3, VS41);
    code.BLR();
  });
  EXPECT_EQ(std::bit_cast<u64>(-2.5), code.Call<u64>());

  // pla and pld at each alignment: a constant after the code, loaded from its address.
  for (u32 nops = 0; nops < 16; nops++)
  {
    code.Emit([&] {
      for (u32 i = 0; i < nops; i++)
        code.NOP();
      const u8* constant = code.GetCodePtr() + 64;
      code.PLD(R3, constant);
      code.PLA(R4, constant);
      code.LD(R4, R4, 0);
      code.XOR(R3, R3, R4);
      code.BLR();
      while (code.GetCodePtr() < constant)
        code.NOP();
      const u64 value = 0x0123456789ABCDEF;
      code.ReserveCodeSpace(sizeof(value));
      std::memcpy(code.GetWritableCodePtr() - sizeof(value), &value, sizeof(value));
    });
    EXPECT_EQ(0u, code.Call<u64>()) << nops;
  }
}

TEST(PPC64Emitter, MOVI2RPrefixed)
{
  // With prefixed instructions, any value of 34 bits takes one instruction, and others no more
  // than before. Executing them needs a Power10.
  TestCode code;
  code.SetPrefixedInstructionsEnabled(true);
  std::mt19937_64 rng(0x5EED);
  std::vector<u64> values = {
      0x80000000, 0xFFFFFFFF, 0x80008000,         0x1FFFFFFFF, 0xFFFFFFFE00000000,
      0x12345678, 0x7FFF0001, 0x123456789ABCDEF0, 0xFFFF,      0x10000};
  for (int i = 0; i < 1000; i++)
    values.push_back(static_cast<u64>(static_cast<s64>(rng()) >> (rng() % 64)));
  for (const u64 value : values)
  {
    code.Emit([&] { EmitReturnConstant(code, value); });
    const s64 signed_value = static_cast<s64>(value);
    // Words, with any nop that aligns a prefixed instruction.
    const size_t words = code.InstructionCount();
    if (signed_value >= -(s64{1} << 33) && signed_value < (s64{1} << 33))
    {
      EXPECT_LE(words, 4u) << fmt::format("{:016x}", value);
    }
    EXPECT_LE(words, 6u) << fmt::format("{:016x}", value);
    if (cpu_info.bISA_3_1)
    {
      EXPECT_EQ(value, code.Call<u64>()) << fmt::format("{:016x}", value);
    }
  }
}

TEST(PPC64Emitter, ConditionalBranches)
{
  TestCode code;
  const s64 inputs[][2] = {{1, 2}, {2, 1}, {5, 5}, {-1, 1}, {1, -1}};
  for (const CRF field : {CR1, CR7})
  {
    for (const CCFlags cond : {LT, GT, EQ, SO, GE, LE, NE, NS})
    {
      // Returns 1 if the condition holds after comparing the arguments, otherwise 0.
      code.Emit([&] {
        code.CMPD(field, R3, R4);
        const FixupBranch taken = code.BC(cond, field);
        code.LI(R3, 0);
        code.BLR();
        code.SetJumpTarget(taken);
        code.LI(R3, 1);
        code.BLR();
      });

      for (const auto& [a, b] : inputs)
      {
        bool expected = false;
        switch (cond)
        {
        case LT:
          expected = a < b;
          break;
        case GT:
          expected = a > b;
          break;
        case EQ:
          expected = a == b;
          break;
        case SO:
          // Copied from XER[SO], which is clear.
          expected = false;
          break;
        case GE:
          expected = a >= b;
          break;
        case LE:
          expected = a <= b;
          break;
        case NE:
          expected = a != b;
          break;
        case NS:
          expected = true;
          break;
        }
        EXPECT_EQ(expected, code.Call<u64>(a, b) != 0) << fmt::format(
            "cr{} cond {} on {}, {}", DecodeReg(field), static_cast<int>(cond), a, b);
      }
    }
  }
}

TEST(PPC64Emitter, BDNZLoop)
{
  TestCode code;
  code.Emit([&] {
    code.MTCTR(R3);
    code.LI(R4, 0);
    const u8* loop = code.GetCodePtr();
    code.ADDI(R4, R4, 3);
    const FixupBranch back = code.BDNZ();
    code.SetJumpTarget(back, loop);
    code.MR(R3, R4);
    code.BLR();
  });
  EXPECT_EQ(30u, code.Call<u64>(u64(10)));
  EXPECT_EQ(3u, code.Call<u64>(u64(1)));
}

TEST(PPC64Emitter, BranchRange)
{
  // Branches reach -32 MiB to +32 MiB - 4.
  constexpr s64 max_forward = 0x1FFFFFC;
  constexpr s64 max_backward = -0x2000000;
  TestCode code(0x2000000 + 0x10000);
  u8* const region = code.RegionStart();
  u8* const end = region + 0x2000000 + 0x10000;

  // At the start: a branch as far forward as possible, to a function returning 42.
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    code.SetCodePtr(region, end);
    ASSERT_TRUE(code.IsInBranchRange(region + max_forward));
    ASSERT_FALSE(code.IsInBranchRange(region + max_forward + 4));
    const FixupBranch forward = code.B();
    code.SetJumpTarget(forward, region + max_forward);
    code.SetCodePtr(region + max_forward, end);
    EmitReturnConstant(code, 42);
  }
  code.FlushIcacheSection(region, region + 4);
  code.FlushIcacheSection(region + max_forward, code.GetWritableCodePtr());
  EXPECT_EQ(42u, std::bit_cast<u64 (*)()>(region)());

  // At the end: a branch as far backward as possible, to a function returning 43 at the start.
  u8* const entry = region + 0x2000000;
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    code.SetCodePtr(entry, end);
    ASSERT_TRUE(code.IsInBranchRange(entry + max_backward));
    ASSERT_FALSE(code.IsInBranchRange(entry + max_backward - 4));
    code.B(entry + max_backward);
    code.SetCodePtr(region, end);
    EmitReturnConstant(code, 43);
  }
  code.FlushIcacheSection(entry, entry + 4);
  code.FlushIcacheSection(region, code.GetWritableCodePtr());
  EXPECT_EQ(43u, std::bit_cast<u64 (*)()>(entry)());

  // Conditional branches reach -32 KiB to +32 KiB - 4.
  code.SetCodePtr(region, end);
  EXPECT_TRUE(code.IsInConditionalBranchRange(region + 0x7FFC));
  EXPECT_FALSE(code.IsInConditionalBranchRange(region + 0x8000));
  code.SetCodePtr(region + 0x8000, end);
  EXPECT_TRUE(code.IsInConditionalBranchRange(region));
  EXPECT_FALSE(code.IsInConditionalBranchRange(region - 4));
}

TEST(PPC64Emitter, ByteReversedLoadsAndStores)
{
  TestCode code;
  alignas(8) const std::array<u8, 8> memory = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

  code.Emit([&] {
    code.LHBRX(R3, R0, R3);
    code.BLR();
  });
  EXPECT_EQ(0x0123u, code.Call<u64>(memory.data()));

  code.Emit([&] {
    code.LWBRX(R3, R0, R3);
    code.BLR();
  });
  EXPECT_EQ(0x01234567u, code.Call<u64>(memory.data()));

  code.Emit([&] {
    code.LDBRX(R3, R0, R3);
    code.BLR();
  });
  EXPECT_EQ(0x0123456789ABCDEFu, code.Call<u64>(memory.data()));

  alignas(8) std::array<u8, 8> stored{};
  code.Emit([&] {
    code.STWBRX(R4, R0, R3);
    code.BLR();
  });
  code.Call<void>(stored.data(), u64(0x01234567));
  EXPECT_EQ((std::array<u8, 8>{0x01, 0x23, 0x45, 0x67, 0, 0, 0, 0}), stored);

  code.Emit([&] {
    code.STDBRX(R4, R0, R3);
    code.BLR();
  });
  code.Call<void>(stored.data(), u64(0x0123456789ABCDEF));
  EXPECT_EQ(memory, stored);
}

TEST(PPC64Emitter, CallFunction)
{
  TestCode code;
  // A function with a minimal stack frame, which saves the TOC pointer because the callee may
  // change it, and passes its arguments on to the callee.
  code.Emit([&] {
    code.MFLR(R0);
    code.STD(R0, R1, STACK_FRAME_LR_SAVE);
    code.STDU(R1, R1, -STACK_FRAME_MIN_SIZE);
    code.STD(R2, R1, STACK_FRAME_TOC_SAVE);
    code.QuickCallFunction(&TwoParameterFunction);
    code.LD(R2, R1, STACK_FRAME_TOC_SAVE);
    code.ADDI(R1, R1, STACK_FRAME_MIN_SIZE);
    code.LD(R0, R1, STACK_FRAME_LR_SAVE);
    code.MTLR(R0);
    code.BLR();
  });
  EXPECT_EQ(TwoParameterFunction(7, 9), code.Call<u64>(u64(7), u64(9)));
  EXPECT_EQ(TwoParameterFunction(100, 1), code.Call<u64>(u64(100), u64(1)));
}

TEST(PPC64Emitter, RewriteCode)
{
  // Code that is executed, rewritten and executed again must see the new instructions.
  TestCode code;
  for (u64 value = 1; value <= 100; value++)
  {
    code.Emit([&] { EmitReturnConstant(code, value); });
    EXPECT_EQ(value, code.Call<u64>());
  }
}

TEST(PPC64Emitter, FloatingPoint)
{
  TestCode code;
  code.Emit([&] {
    code.XSADDDP(ToVSR(F1), ToVSR(F1), ToVSR(F2));
    code.BLR();
  });
  EXPECT_EQ(3.75, code.Call<double>(1.5, 2.25));

  // frt = fra * frc + frb
  code.Emit([&] {
    code.FMADD(F1, F1, F2, F3);
    code.BLR();
  });
  EXPECT_EQ(2.0 * 3.0 + 4.0, code.Call<double>(2.0, 3.0, 4.0));

  code.Emit([&] {
    code.MFVSRD(R3, ToVSR(F1));
    code.BLR();
  });
  EXPECT_EQ(std::bit_cast<u64>(-1.25), code.Call<u64>(-1.25));
}

TEST(PPC64Emitter, VSXDoublewordOrder)
{
  // LXVD2X loads the doubleword at the lower address into doubleword 0 (the one that also holds
  // the floating-point register, and that MFVSRD reads), even on a little-endian host.
  TestCode code;
  alignas(16) const std::array<u64, 2> memory = {0x1111111111111111, 0x2222222222222222};

  code.Emit([&] {
    code.LXVD2X(VS0, R0, R3);
    code.MFVSRD(R3, VS0);
    code.BLR();
  });
  EXPECT_EQ(memory[0], code.Call<u64>(memory.data()));

  code.Emit([&] {
    code.LXVD2X(VS0, R0, R3);
    code.XXSWAPD(VS0, VS0);
    code.MFVSRD(R3, VS0);
    code.BLR();
  });
  EXPECT_EQ(memory[1], code.Call<u64>(memory.data()));

  alignas(16) std::array<u64, 2> stored{};
  code.Emit([&] {
    code.LXVD2X(VS32, R0, R3);
    code.STXVD2X(VS32, R0, R4);
    code.BLR();
  });
  code.Call<void>(memory.data(), stored.data());
  EXPECT_EQ(memory, stored);
}

TEST(PPC64Emitter, ConditionRegister)
{
  // CR2-CR4 are non-volatile in the ELFv2 ABI, so these functions restore the condition register
  // before returning.
  TestCode code;
  code.Emit([&] {
    code.MFCR(R5);
    code.MTCRF(0xFF, R3);
    code.MFCR(R3);
    code.MTCRF(0xFF, R5);
    code.BLR();
  });
  EXPECT_EQ(0x12345678u, code.Call<u64>(u64(0x12345678)));

  // Only CR2 (bits 8-11) is written.
  code.Emit([&] {
    code.MFCR(R5);
    code.MTCRF(0xFF, R3);
    code.MTOCRF(CR2, R4);
    code.MFCR(R3);
    code.MTCRF(0xFF, R5);
    code.BLR();
  });
  EXPECT_EQ(0x00F00000u, code.Call<u64>(u64(0), u64(0xFFFFFFFF)));
}

TEST(PPC64Emitter, WriteFailure)
{
  std::array<u8, 8> buffer{};
  PPC64XEmitter e(buffer.data(), buffer.data() + buffer.size());
  e.NOP();
  const FixupBranch fits = e.B();
  EXPECT_FALSE(e.HasWriteFailed());
  const FixupBranch overflows = e.B();
  EXPECT_TRUE(e.HasWriteFailed());
  EXPECT_EQ(nullptr, overflows.ptr);
  // Neither may write outside the buffer.
  e.SetJumpTarget(overflows, buffer.data());
  e.SetJumpTarget(fits, buffer.data());
  EXPECT_EQ(buffer.data() + buffer.size(), e.GetCodePtr());
}

namespace
{
u64 EightParameterFunction(u64 a, u64 b, u64 c, u64 d, u64 e, u64 f, u64 g, u64 h)
{
  return a / 20 + b / 8 + c / 10 + d / 2 + e / 5 - f + g + h / 3;
}

s64 SignedParameterFunction(s32 a, s64 b)
{
  return a * 3 + b;
}

u64 PointerParameterFunction(const u64* a, u32 b)
{
  return *a + b;
}

// Emits a function that calls another with ABI_CallFunction, inside a minimal stack frame.
template <typename F>
void EmitCall(TestCode& code, F call)
{
  code.Emit([&] {
    code.MFLR(R0);
    code.STD(R0, R1, STACK_FRAME_LR_SAVE);
    code.STDU(R1, R1, -STACK_FRAME_MIN_SIZE);
    code.STD(R2, R1, STACK_FRAME_TOC_SAVE);
    call();
    code.LD(R2, R1, STACK_FRAME_TOC_SAVE);
    code.ADDI(R1, R1, STACK_FRAME_MIN_SIZE);
    code.LD(R0, R1, STACK_FRAME_LR_SAVE);
    code.MTLR(R0);
    code.BLR();
  });
}
}  // namespace

TEST(PPC64Emitter, ABI_CallFunction_Constants)
{
  TestCode code;
  EmitCall(code, [&] { code.ABI_CallFunction(&TwoParameterFunction, 7, 9); });
  EXPECT_EQ(TwoParameterFunction(7, 9), code.Call<u64>());

  EmitCall(code,
           [&] { code.ABI_CallFunction(&EightParameterFunction, 100, 16, 30, 4, 50, 6, 7, 900); });
  EXPECT_EQ(EightParameterFunction(100, 16, 30, 4, 50, 6, 7, 900), code.Call<u64>());
}

TEST(PPC64Emitter, ABI_CallFunction_Registers)
{
  TestCode code;
  // An argument already in place, and one in a non-argument register. R14 is non-volatile, so
  // this function saves it below the stack pointer, and restores it for the caller.
  code.Emit([&] {
    code.MFLR(R0);
    code.STD(R0, R1, STACK_FRAME_LR_SAVE);
    code.STD(R14, R1, -8);
    code.STDU(R1, R1, -STACK_FRAME_MIN_SIZE - 16);
    code.STD(R2, R1, STACK_FRAME_TOC_SAVE);
    code.LI(R14, 9);
    code.ABI_CallFunction(&TwoParameterFunction, R3, R14);
    code.LD(R2, R1, STACK_FRAME_TOC_SAVE);
    code.ADDI(R1, R1, STACK_FRAME_MIN_SIZE + 16);
    code.LD(R14, R1, -8);
    code.LD(R0, R1, STACK_FRAME_LR_SAVE);
    code.MTLR(R0);
    code.BLR();
  });
  EXPECT_EQ(TwoParameterFunction(7, 9), code.Call<u64>(u64(7)));

  // A mix of a register and a constant.
  EmitCall(code, [&] { code.ABI_CallFunction(&TwoParameterFunction, 20, R4); });
  EXPECT_EQ(TwoParameterFunction(20, 5), code.Call<u64>(u64(0), u64(5)));
}

TEST(PPC64Emitter, ABI_CallFunction_Types)
{
  TestCode code;
  // Signed arguments are sign-extended.
  EmitCall(code, [&] { code.ABI_CallFunction(&SignedParameterFunction, s32(-5), s64(-100000)); });
  EXPECT_EQ(SignedParameterFunction(-5, -100000), code.Call<s64>());

  static const u64 value = 0x123456789;
  EmitCall(code,
           [&] { code.ABI_CallFunction(&PointerParameterFunction, &value, u32(0xFFFFFFFF)); });
  EXPECT_EQ(PointerParameterFunction(&value, 0xFFFFFFFF), code.Call<u64>());
}
