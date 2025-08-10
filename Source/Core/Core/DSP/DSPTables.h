// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Additional copyrights go to Duddie (c) 2005 (duddie@walla.com)

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "Core/DSP/DSPCommon.h"
#include "Core/DSP/DSPCore.h"

namespace DSP
{
// The non-ADDR ones that end with _D are the opposite one - if the bit specify
// ACC0, then ACC_D will be ACC1.

// The values of these are very important.
// For the reg ones, the value >> 8 is the base register.
// & 0x80  means it's a "D".

enum partype_t
{
  P_NONE = 0x0000,
  P_VAL = 0x0001,
  P_IMM = 0x0002,
  P_MEM = 0x0003,
  P_STR = 0x0004,
  P_ADDR_I = 0x0005,
  P_ADDR_D = 0x0006,
  P_REG = 0x8000,
  P_REG04 = P_REG | DSP_REG_IX0 << 8,
  P_REG08 = P_REG | DSP_REG_WR0 << 8,
  P_REG18 = P_REG | DSP_REG_AXL0 << 8,
  P_REGM18 = P_REG | DSP_REG_AXL0 << 8 | 0x10,  // used in multiply instructions
  P_REG19 = P_REG | DSP_REG_AXL1 << 8,
  P_REGM19 = P_REG | DSP_REG_AXL1 << 8 | 0x10,  // used in multiply instructions
  P_REG1A = P_REG | DSP_REG_AXH0 << 8 | 0x80,
  // P_ACC       = P_REG | 0x1c10, // used for global accum (gcdsptool's value)
  P_ACCL = P_REG | DSP_REG_ACL0 << 8,          // used for low part of accum
  P_REG1C = P_REG | DSP_REG_ACL0 << 8 | 0x10,  // gcdsptool calls this P_ACCLM
  P_ACCM = P_REG | DSP_REG_ACM0 << 8,          // used for mid part of accum
  // The following are not in gcdsptool
  P_ACCM_D = P_REG | DSP_REG_ACM0 << 8 | 0x80,
  P_ACC = P_REG | DSP_REG_ACC0_FULL << 8,  // used for full accum.
  P_ACCH = P_REG | DSP_REG_ACH0 << 8,      // used for high part of accum
  P_ACC_D = P_REG | DSP_REG_ACC0_FULL << 8 | 0x80,
  P_AX = P_REG | DSP_REG_AX0_FULL << 8,
  P_REGS_MASK = 0x03f80,  // gcdsptool's value = 0x01f80
  P_REF = P_REG | 0x4000,
  P_PRG = P_REF | P_REG,
};

struct param2_t
{
  partype_t type;
  u8 size;
  u8 loc;
  s8 lshift;
  u16 mask;
};

struct DSPOPCTemplate
{
  const char* name;
  u16 opcode;
  u16 opcode_mask;

  u8 size;
  u8 param_count;
  param2_t params[8];
  bool extended;
  bool branch;
  bool uncond_branch;
  bool reads_pc;
  bool updates_sr;
};

// Opcodes
extern const DSPOPCTemplate cw;

// Predefined labels
struct pdlabel_t
{
  u16 addr;
  const char* name;
  const char* description;
};

extern const std::array<pdlabel_t, 36> regnames;
extern const std::array<pdlabel_t, 96> pdlabels;

std::string pdname(u16 val);
std::string pdregname(int val);
std::string pdregnamelong(int val);

void InitInstructionTable();

// Used by the assembler and disassembler for info retrieval.
const DSPOPCTemplate* FindOpInfoByOpcode(UDSPInstruction opcode);
const DSPOPCTemplate* FindOpInfoByName(std::string_view name);

const DSPOPCTemplate* FindExtOpInfoByOpcode(UDSPInstruction opcode);
const DSPOPCTemplate* FindExtOpInfoByName(std::string_view name);

// Used by the interpreter and JIT for instruction emulation
const DSPOPCTemplate* GetOpTemplate(UDSPInstruction inst);
const DSPOPCTemplate* GetExtOpTemplate(UDSPInstruction inst);

template <typename T, size_t N>
auto FindByOpcode(UDSPInstruction opcode, const std::array<T, N>& data)
{
  return std::ranges::find_if(
      data, [opcode](const auto& info) { return (opcode & info.opcode_mask) == info.opcode; });
}
}  // namespace DSP
