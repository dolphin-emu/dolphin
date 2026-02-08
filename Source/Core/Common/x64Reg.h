// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Gen
{
enum X64Reg
{
  EAX = 0,
  EBX = 3,
  ECX = 1,
  EDX = 2,
  ESI = 6,
  EDI = 7,
  EBP = 5,
  ESP = 4,

  RAX = 0,
  RBX = 3,
  RCX = 1,
  RDX = 2,
  RSI = 6,
  RDI = 7,
  RBP = 5,
  RSP = 4,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,

  AL = 0,
  BL = 3,
  CL = 1,
  DL = 2,
  SIL = 6,
  DIL = 7,
  BPL = 5,
  SPL = 4,
  AH = 0x104,
  BH = 0x107,
  CH = 0x105,
  DH = 0x106,

  AX = 0,
  BX = 3,
  CX = 1,
  DX = 2,
  SI = 6,
  DI = 7,
  BP = 5,
  SP = 4,

  XMM0 = 0,
  XMM1,
  XMM2,
  XMM3,
  XMM4,
  XMM5,
  XMM6,
  XMM7,
  XMM8,
  XMM9,
  XMM10,
  XMM11,
  XMM12,
  XMM13,
  XMM14,
  XMM15,

  YMM0 = 0,
  YMM1,
  YMM2,
  YMM3,
  YMM4,
  YMM5,
  YMM6,
  YMM7,
  YMM8,
  YMM9,
  YMM10,
  YMM11,
  YMM12,
  YMM13,
  YMM14,
  YMM15,

  INVALID_REG = 0xFFFFFFFF
};

}  // namespace Gen
