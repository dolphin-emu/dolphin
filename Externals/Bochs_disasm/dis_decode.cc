/////////////////////////////////////////////////////////////////////////
// $Id: dis_decode.cc 11873 2013-10-10 21:00:26Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2012 Stanislav Shwartsman
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "disasm.h"
#include "dis_tables.h"

#define OPCODE(entry) ((BxDisasmOpcodeInfo_t*) entry->OpcodeInfo)
#define OPCODE_TABLE(entry) ((BxDisasmOpcodeTable_t*) entry->OpcodeInfo)

static const unsigned char instruction_has_modrm[512] = {
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f          */
  /*       -------------------------------          */
  /* 00 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,
  /* 10 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,
  /* 20 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,
  /* 30 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,
  /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* 60 */ 0,0,1,1,0,0,0,0,0,1,0,1,0,0,0,0,
  /* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* 80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* A0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* B0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* C0 */ 1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,
  /* D0 */ 1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,
  /* E0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  /* F0 */ 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f           */
  /*       -------------------------------           */
           1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1, /* 0F 00 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 10 */
           1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1, /* 0F 20 */
           0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /* 0F 30 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 40 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 50 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 60 */
           1,1,1,1,1,1,1,0,1,1,0,0,1,1,1,1, /* 0F 70 */
           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0F 80 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 90 */
           0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1, /* 0F A0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F B0 */
           1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0, /* 0F C0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F D0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F E0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0  /* 0F F0 */
  /*       -------------------------------           */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f           */
};

unsigned disassembler::disasm(bx_bool is_32, bx_bool is_64, bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
{
  x86_insn insn = decode(is_32, is_64, cs_base, ip, instr, disbuf);
  return insn.ilen;
}

x86_insn disassembler::decode(bx_bool is_32, bx_bool is_64, bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
{
  if (is_64) is_32 = 1;
  x86_insn insn(is_32, is_64);
  const Bit8u *instruction_begin = instruction = instr;
  resolve_modrm = NULL;

  db_eip = ip;
  db_cs_base = cs_base; // cs linear base (cs_base for PM & cs<<4 for RM & VM)

  disbufptr = disbuf; // start sprintf()'ing into beginning of buffer

#define SSE_PREFIX_NONE 0
#define SSE_PREFIX_66   1
#define SSE_PREFIX_F3   2
#define SSE_PREFIX_F2   3      /* only one SSE prefix could be used */
  unsigned sse_prefix = SSE_PREFIX_NONE, sse_opcode = 0;
  unsigned rex_prefix = 0, prefixes = 0;

  for(;;)
  {
    insn.b1 = fetch_byte();
    prefixes++;

    switch(insn.b1) {
      case 0x40:     // rex
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
      case 0x48:
      case 0x49:
      case 0x4A:
      case 0x4B:
      case 0x4C:
      case 0x4D:
      case 0x4E:
      case 0x4F:
        if (! is_64) break;
        rex_prefix = insn.b1;
        continue;

      case 0x26:     // ES:
      case 0x2e:     // CS:
      case 0x36:     // SS:
      case 0x3e:     // DS:
        if (! is_64) insn.seg_override = (insn.b1 >> 3) & 3;
        rex_prefix = 0;
        continue;

      case 0x64:     // FS:
      case 0x65:     // GS:
        insn.seg_override = insn.b1 & 0xf;
        rex_prefix = 0;
        continue;

      case 0x66:     // operand size override
        if (!insn.os_64) insn.os_32 = !is_32;
        if (!sse_prefix) sse_prefix = SSE_PREFIX_66;
        rex_prefix = 0;
        continue;

      case 0x67:     // address size override
        if (!is_64) insn.as_32 = !is_32;
        insn.as_64 = 0;
        rex_prefix = 0;
        continue;

      case 0xf0:     // lock
        rex_prefix = 0;
        continue;

      case 0xf2:     // repne
      case 0xf3:     // rep
        sse_prefix = (insn.b1 & 0xf) ^ 1;
        rex_prefix = 0;
        continue;

      // no more prefixes
      default:
        break;
    }

    break;
  }

  if (insn.b1 == 0x0f)
  {
    insn.b1 = 0x100 | fetch_byte();
  }

  if (rex_prefix) {
    insn.extend8b = 1;
    if (rex_prefix & 0x8) {
      insn.os_64 = 1;
      insn.os_32 = 1;
    }
    if (rex_prefix & 0x4) insn.rex_r = 8;
    if (rex_prefix & 0x2) insn.rex_x = 8;
    if (rex_prefix & 0x1) insn.rex_b = 8;
  }

  const BxDisasmOpcodeTable_t *opcode_table, *entry;

  if (is_64) {
    if (insn.os_64)
      opcode_table = BxDisasmOpcodes64q;
    else if (insn.os_32)
      opcode_table = BxDisasmOpcodes64d;
    else
      opcode_table = BxDisasmOpcodes64w;
  } else {
    if (insn.os_32)
      opcode_table = BxDisasmOpcodes32;
    else
      opcode_table = BxDisasmOpcodes16;
  }

  entry = opcode_table + insn.b1;

  if ((insn.b1 & ~1) == 0xc4 && (is_64 || (peek_byte() & 0xc0) == 0xc0))
  {
    if (sse_prefix)
      dis_sprintf("(bad vex+rex prefix) ");
    if (rex_prefix)
      dis_sprintf("(bad vex+sse prefix) ");

    // decode 0xC4 or 0xC5 VEX prefix
    sse_prefix = decode_vex(&insn);
    if (insn.b1 < 256 || insn.b1 >= 1024)
      entry = &BxDisasmGroupSSE_ERR[0];
    else
      entry = BxDisasmOpcodesAVX + (insn.b1 - 256);
  }
/*
  if (insn.b1== 0x62 && (is_64 || (peek_byte() & 0xc0) == 0xc0))
  {
    if (sse_prefix)
      dis_sprintf("(bad evex+rex prefix) ");
    if (rex_prefix)
      dis_sprintf("(bad evex+sse prefix) ");

    // decode 0x62 EVEX prefix
    sse_prefix = decode_evex(&insn);
    if (insn.b1 < 256 || insn.b1 >= 1024)
      entry = &BxDisasmGroupSSE_ERR[0];
//  else
//    entry = BxDisasmOpcodesEVEX + (insn.b1 - 256);
  }
*/
  else if (insn.b1 == 0x8f && (is_64 || (peek_byte() & 0xc0) == 0xc0) && (peek_byte() & 0x8) == 0x8)
  {
    if (sse_prefix)
      dis_sprintf("(bad xop+rex prefix) ");
    if (rex_prefix)
      dis_sprintf("(bad xop+sse prefix) ");
    
    // decode 0x8F XOP prefix
    sse_prefix = decode_xop(&insn);
    if (insn.b1 >= 768 || sse_prefix != 0)
      entry = &BxDisasmGroupSSE_ERR[0];
    else
      entry = BxDisasmOpcodesXOP + insn.b1;
  }

  if (insn.b1 >= 512 || instruction_has_modrm[insn.b1] || insn.is_xop > 0)
  {
    // take 3rd byte for 3-byte opcode
    if (entry->Attr == _GRP3BOP) {
      entry = &(OPCODE_TABLE(entry)[fetch_byte()]);
    }

    decode_modrm(&insn);
  }

  int attr = entry->Attr;
  while(attr)
  {
    switch(attr) {
       case _GROUPN:
         entry = &(OPCODE_TABLE(entry)[insn.nnn & 7]);
         break;

       case _GRPSSE66:
         /* SSE opcode group with only prefix 0x66 allowed */
         sse_opcode = 1;
         if (sse_prefix != SSE_PREFIX_66)
             entry = &(BxDisasmGroupSSE_ERR[sse_prefix]);
         attr = 0;
         continue;

       case _GRPSSEF2:
         /* SSE opcode group with only prefix 0xF2 allowed */
         sse_opcode = 1;
         if (sse_prefix != SSE_PREFIX_F2)
             entry = &(BxDisasmGroupSSE_ERR[sse_prefix]);
         attr = 0;
         continue;

       case _GRPSSEF3:
         /* SSE opcode group with only prefix 0xF3 allowed */
         sse_opcode = 1;
         if (sse_prefix != SSE_PREFIX_F3)
             entry = &(BxDisasmGroupSSE_ERR[sse_prefix]);
         attr = 0;
         continue;

       case _GRPSSENONE:
         /* SSE opcode group with no prefix only allowed */
         sse_opcode = 1;
         if (sse_prefix != SSE_PREFIX_NONE)
             entry = &(BxDisasmGroupSSE_ERR[sse_prefix]);
         attr = 0;
         continue;

       case _GRPSSE:
         sse_opcode = 1;
         /* For SSE opcodes, look into another 4 entries table
            with the opcode prefixes (NONE, 0x66, 0xF2, 0xF3) */
         entry = &(OPCODE_TABLE(entry)[sse_prefix]);
         break;

       case _GRPSSE2:
         sse_opcode = 1;
         /* For SSE opcodes, look into another 2 entries table
            with the opcode prefixes (NONE, 0x66)
            SSE prefixes 0xF2 and 0xF3 are not allowed */
         if (sse_prefix > SSE_PREFIX_66)
             entry = &(BxDisasmGroupSSE_ERR[sse_prefix]);
         else
             entry = &(OPCODE_TABLE(entry)[sse_prefix]);
         break;

       case _SPLIT11B:
         entry = &(OPCODE_TABLE(entry)[insn.mod != 3]); /* REG/MEM */
         break;

       case _GRPRM:
         entry = &(OPCODE_TABLE(entry)[insn.rm & 7]);
         break;

       case _GRPFP:
         if(insn.mod != 3)
         {
             entry = &(OPCODE_TABLE(entry)[insn.nnn & 7]);
         } else {
             int index = (insn.b1-0xD8)*64 + (insn.modrm & 0x3f);
             entry = &(BxDisasmOpcodeInfoFP[index]);
         }
         break;

       case _GRP3DNOW:
         entry = &(BxDisasm3DNowGroup[fetch_byte()]);
         break;

       case _GRP64B:
         entry = &(OPCODE_TABLE(entry)[insn.os_64 ? 2 : insn.os_32]);
         if (sse_prefix == SSE_PREFIX_66)
             sse_prefix = 0;
         break;

       case _GRPVEXW:
         entry = &(OPCODE_TABLE(entry)[insn.vex_w]);
         break;

       default:
         printf("Internal disassembler error - unknown attribute !\n");
         return x86_insn(is_32, is_64);
    }

    /* get additional attributes from group table */
    attr = entry->Attr;
  }

#define BRANCH_NOT_TAKEN 0x2E
#define BRANCH_TAKEN     0x3E

  unsigned branch_hint = 0;

  // print prefixes
  for(unsigned i=0;i<prefixes;i++)
  {
    Bit8u prefix_byte = *(instr+i);

    if (prefix_byte == 0xF0) dis_sprintf("lock ");

    if (! insn.is_xop && ! insn.is_vex) {
      if (insn.b1 == 0x90 && !insn.rex_b && prefix_byte == 0xF3)
        continue;

      if (prefix_byte == 0xF3 || prefix_byte == 0xF2) {
        if (! sse_opcode) {
          const BxDisasmOpcodeTable_t *prefix = &(opcode_table[prefix_byte]);
          dis_sprintf("%s ", OPCODE(prefix)->IntelOpcode);
        }
      }

      // branch hint for jcc instructions
      if ((insn.b1 >= 0x070 && insn.b1 <= 0x07F) ||
          (insn.b1 >= 0x180 && insn.b1 <= 0x18F))
      {
        if (prefix_byte == BRANCH_NOT_TAKEN || prefix_byte == BRANCH_TAKEN)
          branch_hint = prefix_byte;
      }
    }
  }

  const BxDisasmOpcodeInfo_t *opcode = OPCODE(entry);

  if (! insn.is_xop && ! insn.is_vex) {
    // patch jecx opcode
    if (insn.b1 == 0xE3 && insn.as_32 && !insn.as_64)
      opcode = &Ia_jecxz_Jb;

    // fix nop opcode
    if (insn.b1 == 0x90) {
      if (sse_prefix == SSE_PREFIX_F3)
        opcode = &Ia_pause;
      else if (!insn.rex_b)
        opcode = &Ia_nop;
    }
  }

  // print instruction disassembly
  if (intel_mode)
    print_disassembly_intel(&insn, opcode);
  else
    print_disassembly_att  (&insn, opcode);

  if (branch_hint == BRANCH_NOT_TAKEN)
  {
    dis_sprintf(", not taken");
  }
  else if (branch_hint == BRANCH_TAKEN)
  {
    dis_sprintf(", taken");
  }

  if (insn.is_vex < 0)
    dis_sprintf(" (bad vex)");
  else if (insn.is_evex < 0)
    dis_sprintf(" (bad evex)");
  else if (insn.is_xop < 0)
    dis_sprintf(" (bad xop)");

  insn.ilen = (unsigned)(instruction - instruction_begin);

  return insn;
}

unsigned disassembler::decode_vex(x86_insn *insn)
{
  insn->is_vex = 1;

  unsigned b2 = fetch_byte(), vex_opcode_extension = 1;

  insn->rex_r = (b2 & 0x80) ? 0 : 0x8;

  if (insn->b1 == 0xc4) {
    // decode 3-byte VEX prefix
    insn->rex_x = (b2 & 0x40) ? 0 : 0x8;
    if (insn->is_64)
      insn->rex_b = (b2 & 0x20) ? 0 : 0x8;

    vex_opcode_extension = b2 & 0x1f;
    if (! vex_opcode_extension || vex_opcode_extension > 3)
      insn->is_vex = -1;

    b2 = fetch_byte(); // fetch VEX3 byte
    if (b2 & 0x80) {
      insn->os_64 = 1;
      insn->os_32 = 1;
      insn->vex_w = 1;
    }
  }

  insn->vex_vvv = 15 - ((b2 >> 3) & 0xf);
  if (! insn->is_64) insn->vex_vvv &= 7;
  insn->vex_l = (b2 >> 2) & 0x1;
  insn->b1 = fetch_byte() + 256 * vex_opcode_extension;
  return b2 & 0x3;
}

unsigned disassembler::decode_evex(x86_insn *insn)
{
  insn->is_evex = 1;

  Bit32u evex = fetch_dword();

  // check for reserved EVEX bits
  if ((evex & 0x0c) != 0 || (evex & 0x400) == 0) {
    insn->is_evex = -1;
  }

  unsigned evex_opcext = evex & 0x3;
  if (evex_opcext == 0) {
    insn->is_evex = -1;
  }
    
  if (insn->is_64) {
    insn->rex_r = ((evex >> 4) & 0x8) ^ 0x8;
    insn->rex_r |= (evex & 0x10) ^ 0x10;
    insn->rex_x = ((evex >> 3) & 0x8) ^ 0x8;
    insn->rex_b = ((evex >> 2) & 0x8) ^ 0x8;
    insn->rex_b |= (insn->rex_x << 1);
  }

  unsigned sse_prefix = (evex >> 8) & 0x3;

  insn->vex_vvv = 15 - ((evex >> 11) & 0xf);
  unsigned evex_v = ((evex >> 15) & 0x10) ^ 0x10;
  insn->vex_vvv |= evex_v;
  if (! insn->is_64) insn->vex_vvv &= 7;

  insn->vex_w = (evex >> 15) & 0x1;
  if (insn->vex_w) {
    insn->os_64 = 1;
    insn->os_32 = 1;
  }

  insn->evex_b = (evex >> 20) & 0x1;
  insn->evex_ll_rc = (evex >> 21) & 0x3;
  insn->evex_z = (evex >> 23) & 0x1;
  
  insn->b1 = (evex >> 24);
  insn->b1 += 256 * (evex_opcext-1);

  return sse_prefix;
}

unsigned disassembler::decode_xop(x86_insn *insn)
{
  insn->is_xop = 1;

  unsigned b2 = fetch_byte();

  insn->rex_r = (b2 & 0x80) ? 0 : 0x8;
  insn->rex_x = (b2 & 0x40) ? 0 : 0x8;
  if (insn->is_64)
    insn->rex_b = (b2 & 0x20) ? 0 : 0x8;

  unsigned xop_opcode_extension = (b2 & 0x1f) - 8;
  if (xop_opcode_extension >= 3)
    insn->is_xop = -1;

  b2 = fetch_byte(); // fetch VEX3 byte
  if (b2 & 0x80) {
    insn->os_64 = 1;
    insn->os_32 = 1;
    insn->vex_w = 1;
  }

  insn->vex_vvv = 15 - ((b2 >> 3) & 0xf);
  if (! insn->is_64) insn->vex_vvv &= 7;
  insn->vex_l = (b2 >> 2) & 0x1;
  insn->b1 = fetch_byte() + 256 * xop_opcode_extension;

  return b2 & 0x3;
}

void disassembler::dis_sprintf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsprintf(disbufptr, fmt, ap);
  va_end(ap);

  disbufptr += strlen(disbufptr);
}

void disassembler::dis_putc(char symbol)
{
  *disbufptr++ = symbol;
  *disbufptr = 0;
}
