// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/* $VER: ppc_disasm.h V1.6 (09.12.2011)
 *
 * Disassembler module for the PowerPC microprocessor family
 * Copyright (c) 1998-2001,2009,2011 Frank Wille
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Modified for use with Dolphin

#pragma once

#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
class GekkoDisassembler final
{
public:
  static std::string Disassemble(u32 opcode, u32 current_instruction_address,
                                 bool big_endian = true);
  static const char* GetGPRName(u32 index);
  static const char* GetFPRName(u32 index);

private:
  GekkoDisassembler() = delete;

  static void ill(u32 in);
  static std::string imm(u32 in, int uimm, int type, bool hex);

  static std::string ra_rb(u32 in);
  static std::string rd_ra_rb(u32 in, int mask);
  static std::string fd_ra_rb(u32 in);

  static void trapi(u32 in, unsigned char dmode);
  static void cmpi(u32 in, int uimm);
  static void addi(u32 in, std::string_view ext);
  static size_t branch(u32 in, std::string_view bname, int aform, int bdisp);
  static void bc(u32 in);
  static void bli(u32 in);
  static void mcrf(u32 in, std::string_view suffix);
  static void crop(u32 in, std::string_view n1, std::string_view n2);
  static void nooper(u32 in, std::string_view name);
  static void rlw(u32 in, std::string_view name, int i);
  static void ori(u32 in, std::string_view name);
  static void rld(u32 in, std::string_view name, int i);
  static void cmp(u32 in);
  static void trap(u32 in, unsigned char dmode);
  static void dab(u32 in, std::string_view name, int mask, int smode, int chkoe, int chkrc);
  static void rrn(u32 in, std::string_view name, int smode, int chkoe, int chkrc);
  static void mtcr(u32 in);
  static void msr(u32 in, int smode);
  static void mspr(u32 in, int smode);
  static void mtb(u32 in);
  static void sradi(u32 in);
  static void ldst(u32 in, std::string_view name, char reg);
  static void fdabc(u32 in, std::string_view name, int mask);
  static void fmr(u32 in);
  static void fdab(u32 in, std::string_view name);
  static void fcmp(u32 in, char c);
  static void mtfsb(u32 in, int n);
  static void ps(u32 inst);
  static void ps_mem(u32 inst);

  static u32* DoDisassembly(bool big_endian);

  enum Flags
  {
    PPCF_ILLEGAL = (1 << 0),   // Illegal PowerPC instruction
    PPCF_UNSIGNED = (1 << 1),  // Unsigned immediate instruction
    PPCF_SUPER = (1 << 2),     // Supervisor level instruction
    PPCF_64 = (1 << 3),        // 64-bit only instruction
  };

  static u32* m_instr;            // Pointer to instruction to disassemble
  static u32* m_iaddr;            // Instruction.address., usually the same as instr
  static std::string m_opcode;    // Buffer for opcode, min. 10 chars.
  static std::string m_operands;  // Operand buffer, min. 24 chars.
};
}  // namespace Common
