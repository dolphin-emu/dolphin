// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/BitSet.h"
#include "Common/x64Reg.h"

// x64 ABI:s, and helpers to help follow them when JIT-ing code.
// All conventions return values in EAX (+ possibly EDX).

// Windows 64-bit
// * 4-reg "fastcall" variant, very new-skool stack handling
// * Callee moves stack pointer, to make room for shadow regs for the biggest function _it itself
// calls_
// * Parameters passed in RCX, RDX, ... further parameters are MOVed into the allocated stack space.
// Scratch:      RAX RCX RDX R8 R9 R10 R11
// Callee-save:  RBX RSI RDI RBP R12 R13 R14 R15
// Parameters:   RCX RDX R8 R9, further MOV-ed

// Linux 64-bit
// * 6-reg "fastcall" variant, old skool stack handling (parameters are pushed)
// Scratch:      RAX RCX RDX RSI RDI R8 R9 R10 R11
// Callee-save:  RBX RBP R12 R13 R14 R15
// Parameters:   RDI RSI RDX RCX R8 R9

namespace Gen
{

constexpr BitSet32 ABI_ALL_FPRS(0xffff0000);
constexpr BitSet32 ABI_ALL_GPRS(0x0000ffff);

#ifdef _WIN32  // 64-bit Windows - the really exotic calling convention

constexpr X64Reg ABI_PARAM1 = RCX;
constexpr X64Reg ABI_PARAM2 = RDX;
constexpr X64Reg ABI_PARAM3 = R8;
constexpr X64Reg ABI_PARAM4 = R9;

constexpr std::array<X64Reg, 4> ABI_PARAMS = {ABI_PARAM1, ABI_PARAM2, ABI_PARAM3, ABI_PARAM4};

// xmm0-xmm15 use the upper 16 bits in the functions that push/pop registers.
constexpr BitSet32 ABI_ALL_CALLER_SAVED{RAX,       RCX,       RDX,       R8,        R9,
                                        R10,       R11,       XMM0 + 16, XMM1 + 16, XMM2 + 16,
                                        XMM3 + 16, XMM4 + 16, XMM5 + 16};

#else   // 64-bit Unix / OS X

constexpr X64Reg ABI_PARAM1 = RDI;
constexpr X64Reg ABI_PARAM2 = RSI;
constexpr X64Reg ABI_PARAM3 = RDX;
constexpr X64Reg ABI_PARAM4 = RCX;
constexpr X64Reg ABI_PARAM5 = R8;
constexpr X64Reg ABI_PARAM6 = R9;

constexpr std::array<X64Reg, 6> ABI_PARAMS = {ABI_PARAM1, ABI_PARAM2, ABI_PARAM3,
                                              ABI_PARAM4, ABI_PARAM5, ABI_PARAM6};

// FIXME: avoid pushing all 16 XMM registers when possible? most functions we call probably
// don't actually clobber them.
constexpr BitSet32 ABI_ALL_CALLER_SAVED(BitSet32{RAX, RCX, RDX, RDI, RSI, R8, R9, R10, R11} |
                                        ABI_ALL_FPRS);
#endif  // _WIN32

constexpr BitSet32 ABI_ALL_CALLEE_SAVED(~ABI_ALL_CALLER_SAVED);

constexpr X64Reg ABI_RETURN = RAX;

};  // namespace Gen
