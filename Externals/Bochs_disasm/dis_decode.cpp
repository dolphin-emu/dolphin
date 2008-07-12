/////////////////////////////////////////////////////////////////////////
// $Id: dis_decode.cc,v 1.32 2006/05/12 17:04:19 sshwarts Exp $
/////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "disasm.h"
#include "dis_tables.h"

#define OPCODE(entry) ((BxDisasmOpcodeInfo_t*) entry->OpcodeInfo)
#define OPCODE_TABLE(entry) ((BxDisasmOpcodeTable_t*) entry->OpcodeInfo)

#ifndef NULL
#define NULL 0
#endif 


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
           1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1, /* 0F 10 */
           1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1, /* 0F 20 */
           0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /* 0F 30 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 40 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 50 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 60 */
           1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1, /* 0F 70 */
           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0F 80 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F 90 */
           0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1, /* 0F A0 */
           1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1, /* 0F B0 */
           1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0, /* 0F C0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F D0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0F E0 */
           1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0  /* 0F F0 */
  /*       -------------------------------           */
  /*       0 1 2 3 4 5 6 7 8 9 a b c d e f           */
};

unsigned disassembler::disasm(bx_bool is_32, bx_bool is_64, bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
{
  x86_insn insn = decode(is_32, is_64, base, ip, instr, disbuf);
  return insn.ilen;
}

x86_insn disassembler::decode(bx_bool is_32, bx_bool is_64, bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
{
  x86_insn insn(is_32, is_64);
  const Bit8u *instruction_begin = instruction = instr;
  resolve_modrm = NULL;
  unsigned b3 = 0;

  db_eip = ip;
  db_base = base; // cs linear base (base for PM & cs<<4 for RM & VM)

  disbufptr = disbuf; // start sprintf()'ing into beginning of buffer

#define SSE_PREFIX_NONE 0
#define SSE_PREFIX_66   1
#define SSE_PREFIX_F2   2
#define SSE_PREFIX_F3   3      /* only one SSE prefix could be used */
  unsigned sse_prefix = SSE_PREFIX_NONE;

  for(;;)
  {
    insn.b1 = fetch_byte();
    insn.prefixes++;

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
        insn.extend8b = 1;
        if (insn.b1 & 0x8) {
          insn.os_64 = 1;
          insn.os_32 = 1;
        }
        if (insn.b1 & 0x4) insn.rex_r = 8;
        if (insn.b1 & 0x2) insn.rex_x = 8;
        if (insn.b1 & 0x1) insn.rex_b = 8;
        continue;

      case 0x26:     // ES:
        if (! is_64) insn.seg_override = ES_REG;
        continue;

      case 0x2e:     // CS:
        if (! is_64) insn.seg_override = CS_REG;
        continue;

      case 0x36:     // SS:
        if (! is_64) insn.seg_override = SS_REG;
        continue;

      case 0x3e:     // DS:
        if (! is_64) insn.seg_override = DS_REG;
        continue;

      case 0x64:     // FS:
        insn.seg_override = FS_REG;
        continue;

      case 0x65:     // GS:
        insn.seg_override = GS_REG;
        continue;

      case 0x66:     // operand size override
        if (!insn.os_64) insn.os_32 = !is_32;
        if (!sse_prefix) sse_prefix = SSE_PREFIX_66;
        continue;

      case 0x67:     // address size override
        if (!is_64) insn.as_32 = !is_32;
        insn.as_64 = 0;
        continue;

      case 0xf0:     // lock
        continue;

      case 0xf2:     // repne
        if (!sse_prefix) sse_prefix = SSE_PREFIX_F2;
        continue;

      case 0xf3:     // rep
        if (!sse_prefix) sse_prefix = SSE_PREFIX_F3;
        continue;

      // no more prefixes
      default:
        break;
    }

    insn.prefixes--;
    break;
  }

  if (insn.b1 == 0x0f)
  {
    insn.b1 = 0x100 | fetch_byte();
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

  // will require 3rd byte for 3-byte opcode
  if (entry->Attr & _GRP3BTAB) b3 = fetch_byte();

  if (instruction_has_modrm[insn.b1])
  {
    decode_modrm(&insn);
  }

  int attr = entry->Attr;
  while(attr) 
  {
    switch(attr) {
       case _GROUPN:
         entry = &(OPCODE_TABLE(entry)[insn.nnn]);
         break;

       case _GRPSSE:
         if(sse_prefix) insn.prefixes--;
         /* For SSE opcodes, look into another 4 entries table 
            with the opcode prefixes (NONE, 0x66, 0xF2, 0xF3) */
         entry = &(OPCODE_TABLE(entry)[sse_prefix]);
         break;

       case _SPLIT11B:
         entry = &(OPCODE_TABLE(entry)[insn.mod != 3]); /* REG/MEM */
         break;

       case _GRPRM:
         entry = &(OPCODE_TABLE(entry)[insn.rm]);
         break;

       case _GRPFP:
         if(insn.mod != 3)
         {
             entry = &(OPCODE_TABLE(entry)[insn.nnn]);
         } else {
             int index = (insn.b1-0xD8)*64 + (insn.modrm & 0x3f);
             entry = &(BxDisasmOpcodeInfoFP[index]);
         }
         break;

       case _GRP3DNOW:
         entry = &(BxDisasm3DNowGroup[peek_byte()]);
         break;

       case _GRP3BTAB:
         entry = &(OPCODE_TABLE(entry)[b3 >> 4]);
         break;

       case _GRP3BOP:
         entry = &(OPCODE_TABLE(entry)[b3 & 15]);
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
  for(unsigned i=0;i<insn.prefixes;i++)
  {
    Bit8u prefix_byte = *(instr+i);

    if (prefix_byte == 0xF3 || prefix_byte == 0xF2 || prefix_byte == 0xF0) 
    {
      const BxDisasmOpcodeTable_t *prefix = &(opcode_table[prefix_byte]);
      dis_sprintf("%s ", OPCODE(prefix)->IntelOpcode);
    }

    // branch hint for jcc instructions
    if ((insn.b1 >= 0x070 && insn.b1 <= 0x07F) ||
        (insn.b1 >= 0x180 && insn.b1 <= 0x18F))
    {
      if (prefix_byte == BRANCH_NOT_TAKEN || prefix_byte == BRANCH_TAKEN) 
        branch_hint = prefix_byte;
    }
  }

  const BxDisasmOpcodeInfo_t *opcode = OPCODE(entry);

  // patch jecx opcode
  if (insn.b1 == 0xE3 && insn.as_32 && !insn.as_64)
    opcode = &Ia_jecxz_Jb;

  // fix nop opcode
  if (insn.b1 == 0x90 && !insn.rex_b) {
    opcode = &Ia_nop;
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
 
  insn.ilen = (unsigned)(instruction - instruction_begin);

  return insn;
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
