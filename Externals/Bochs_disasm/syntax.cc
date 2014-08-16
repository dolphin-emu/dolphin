/////////////////////////////////////////////////////////////////////////
// $Id: syntax.cc 11968 2013-11-29 20:49:20Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2011 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "disasm.h"

//////////////////
// Intel STYLE
//////////////////

static const char *intel_general_16bit_regname[16] = {
    "ax",  "cx",  "dx",   "bx",   "sp",   "bp",   "si",   "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
};

static const char *intel_general_32bit_regname[16] = {
    "eax", "ecx", "edx",  "ebx",  "esp",  "ebp",  "esi",  "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
};

static const char *intel_general_64bit_regname[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char *intel_general_8bit_regname_rex[16] = {
    "al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

static const char *intel_general_8bit_regname[8] = {
    "al",  "cl",  "dl",  "bl",  "ah",  "ch",  "dh",  "bh"
};

static const char *intel_segment_name[8] = {
    "es",  "cs",  "ss",  "ds",  "fs",  "gs",  "??",  "??"
};

static const char *intel_index16[8] = {
    "bx+si",
    "bx+di",
    "bp+si",
    "bp+di",
    "si",
    "di",
    "bp",
    "bx"
};

static const char *intel_vector_reg_name[4] = {
    "xmm", "ymm", "???", "zmm"
};

//////////////////
// AT&T STYLE
//////////////////

static const char *att_general_16bit_regname[16] = {
    "%ax",  "%cx",  "%dx",   "%bx",   "%sp",   "%bp",   "%si",   "%di",
    "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w"
};

static const char *att_general_32bit_regname[16] = {
    "%eax", "%ecx", "%edx",  "%ebx",  "%esp",  "%ebp",  "%esi",  "%edi",
    "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d"
};

static const char *att_general_64bit_regname[16] = {
    "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
    "%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
};

static const char *att_general_8bit_regname_rex[16] = {
    "%al",  "%cl",  "%dl",   "%bl",   "%spl",  "%bpl",  "%sil",  "%dil",
    "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b"
};

static const char *att_general_8bit_regname[8] = {
    "%al",  "%cl",  "%dl",  "%bl",  "%ah",  "%ch",  "%dh",  "%bh"
};

static const char *att_segment_name[8] = {
    "%es",  "%cs",  "%ss",  "%ds",  "%fs",  "%gs",  "%??",  "%??"
};

static const char *att_index16[8] = {
    "%bx,%si",
    "%bx,%di",
    "%bp,%si",
    "%bp,%di",
    "%si",
    "%di",
    "%bp",
    "%bx"
};

static const char *att_vector_reg_name[4] = {
    "%xmm", "%ymm", "%???", "%zmm"
};

#define NULL_SEGMENT_REGISTER 7

void disassembler::initialize_modrm_segregs()
{
  sreg_mod00_rm16[0] = segment_name[DS_REG];
  sreg_mod00_rm16[1] = segment_name[DS_REG];
  sreg_mod00_rm16[2] = segment_name[SS_REG];
  sreg_mod00_rm16[3] = segment_name[SS_REG];
  sreg_mod00_rm16[4] = segment_name[DS_REG];
  sreg_mod00_rm16[5] = segment_name[DS_REG];
  sreg_mod00_rm16[6] = segment_name[DS_REG];
  sreg_mod00_rm16[7] = segment_name[DS_REG];

  sreg_mod01or10_rm16[0] = segment_name[DS_REG];
  sreg_mod01or10_rm16[1] = segment_name[DS_REG];
  sreg_mod01or10_rm16[2] = segment_name[SS_REG];
  sreg_mod01or10_rm16[3] = segment_name[SS_REG];
  sreg_mod01or10_rm16[4] = segment_name[DS_REG];
  sreg_mod01or10_rm16[5] = segment_name[DS_REG];
  sreg_mod01or10_rm16[6] = segment_name[SS_REG];
  sreg_mod01or10_rm16[7] = segment_name[DS_REG];

  sreg_mod00_base32[0]  = segment_name[DS_REG];
  sreg_mod00_base32[1]  = segment_name[DS_REG];
  sreg_mod00_base32[2]  = segment_name[DS_REG];
  sreg_mod00_base32[3]  = segment_name[DS_REG];
  sreg_mod00_base32[4]  = segment_name[SS_REG];
  sreg_mod00_base32[5]  = segment_name[DS_REG];
  sreg_mod00_base32[6]  = segment_name[DS_REG];
  sreg_mod00_base32[7]  = segment_name[DS_REG];
  sreg_mod00_base32[8]  = segment_name[DS_REG];
  sreg_mod00_base32[9]  = segment_name[DS_REG];
  sreg_mod00_base32[10] = segment_name[DS_REG];
  sreg_mod00_base32[11] = segment_name[DS_REG];
  sreg_mod00_base32[12] = segment_name[DS_REG];
  sreg_mod00_base32[13] = segment_name[DS_REG];
  sreg_mod00_base32[14] = segment_name[DS_REG];
  sreg_mod00_base32[15] = segment_name[DS_REG];

  sreg_mod01or10_base32[0]  = segment_name[DS_REG];
  sreg_mod01or10_base32[1]  = segment_name[DS_REG];
  sreg_mod01or10_base32[2]  = segment_name[DS_REG];
  sreg_mod01or10_base32[3]  = segment_name[DS_REG];
  sreg_mod01or10_base32[4]  = segment_name[SS_REG];
  sreg_mod01or10_base32[5]  = segment_name[SS_REG];
  sreg_mod01or10_base32[6]  = segment_name[DS_REG];
  sreg_mod01or10_base32[7]  = segment_name[DS_REG];
  sreg_mod01or10_base32[8]  = segment_name[DS_REG];
  sreg_mod01or10_base32[9]  = segment_name[DS_REG];
  sreg_mod01or10_base32[10] = segment_name[DS_REG];
  sreg_mod01or10_base32[11] = segment_name[DS_REG];
  sreg_mod01or10_base32[12] = segment_name[DS_REG];
  sreg_mod01or10_base32[13] = segment_name[DS_REG];
  sreg_mod01or10_base32[14] = segment_name[DS_REG];
  sreg_mod01or10_base32[15] = segment_name[DS_REG];
}

//////////////////
// Intel STYLE
//////////////////

void disassembler::set_syntax_intel()
{
  intel_mode = 1;

  general_16bit_regname = intel_general_16bit_regname;
  general_8bit_regname = intel_general_8bit_regname;
  general_32bit_regname = intel_general_32bit_regname;
  general_8bit_regname_rex = intel_general_8bit_regname_rex;
  general_64bit_regname = intel_general_64bit_regname;

  segment_name = intel_segment_name;
  index16 = intel_index16;
  vector_reg_name = intel_vector_reg_name;

  initialize_modrm_segregs();
}

void disassembler::print_disassembly_intel(const x86_insn *insn, const BxDisasmOpcodeInfo_t *entry)
{
  // print opcode
  dis_sprintf("%s ", entry->IntelOpcode);

  if (entry->Operand1) {
    (this->*entry->Operand1)(insn);
  }
  if (entry->Operand2) {
    dis_sprintf(", ");
    (this->*entry->Operand2)(insn);
  }
  if (entry->Operand3) {
    dis_sprintf(", ");
    (this->*entry->Operand3)(insn);
  }
  if (entry->Operand4) {
    dis_sprintf(", ");
    (this->*entry->Operand4)(insn);
  }
}

//////////////////
// AT&T STYLE
//////////////////

void disassembler::set_syntax_att()
{
  intel_mode = 0;

  general_16bit_regname = att_general_16bit_regname;
  general_8bit_regname = att_general_8bit_regname;
  general_32bit_regname = att_general_32bit_regname;
  general_8bit_regname_rex = att_general_8bit_regname_rex;
  general_64bit_regname = att_general_64bit_regname;

  segment_name = att_segment_name;
  index16 = att_index16;
  vector_reg_name = att_vector_reg_name;

  initialize_modrm_segregs();
}

void disassembler::toggle_syntax_mode()
{
  if (intel_mode) set_syntax_att();
  else set_syntax_intel();
}

void disassembler::print_disassembly_att(const x86_insn *insn, const BxDisasmOpcodeInfo_t *entry)
{
  // print opcode
  dis_sprintf("%s ", entry->AttOpcode);

  if (entry->Operand4) {
    (this->*entry->Operand4)(insn);
    dis_sprintf(", ");
  }
  if (entry->Operand3) {
    (this->*entry->Operand3)(insn);
    dis_sprintf(", ");
  }
  if (entry->Operand2) {
    (this->*entry->Operand2)(insn);
    dis_sprintf(", ");
  }
  if (entry->Operand1) {
    (this->*entry->Operand1)(insn);
  }
}
