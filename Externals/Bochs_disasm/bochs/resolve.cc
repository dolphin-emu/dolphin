/////////////////////////////////////////////////////////////////////////
// $Id: resolve.cc 11863 2013-10-07 19:23:19Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2013 Stanislav Shwartsman
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
#include <assert.h>
#include "disasm.h"

void disassembler::decode_modrm(x86_insn *insn)
{
  insn->modrm = fetch_byte();
  BX_DECODE_MODRM(insn->modrm, insn->mod, insn->nnn, insn->rm);
  // MOVs with CRx and DRx always use register ops and ignore the mod field.
  if ((insn->b1 & ~3) == 0x120) insn->mod = 3;
  insn->nnn |= insn->rex_r;
  insn->rm  |= insn->rex_b;

  if (insn->mod == 3) {
    return; /* mod, reg, reg */
  }

  if (insn->as_64)
  {
      if ((insn->rm & 7) != 4) { /* rm != 100b, no s-i-b byte */
        // one byte modrm
        switch (insn->mod) {
          case 0:
            resolve_modrm = &disassembler::resolve64_mod0;
            if ((insn->rm & 7) == 5) /* no reg, 32-bit displacement */
              insn->displacement.displ32 = fetch_dword();
            break;
          case 1:
            /* reg, 8-bit displacement, sign extend */
            resolve_modrm = &disassembler::resolve64_mod1or2;
            insn->displacement.displ32 = (Bit8s) fetch_byte();
            break;
          case 2:
            /* reg, 32-bit displacement */
            resolve_modrm = &disassembler::resolve64_mod1or2;
            insn->displacement.displ32 = fetch_dword();
            break;
        } /* switch (mod) */
      } /* if (rm != 4) */
      else { /* rm == 4, s-i-b byte follows */
        insn->sib = fetch_byte();
        BX_DECODE_SIB(insn->sib, insn->scale, insn->index, insn->base);
        insn->base  |= insn->rex_b;
        insn->index |= insn->rex_x;

        switch (insn->mod) {
          case 0:
            resolve_modrm = &disassembler::resolve64_mod0_rm4;
            if ((insn->base & 7) == 5)
              insn->displacement.displ32 = fetch_dword();
            break;
          case 1:
            resolve_modrm = &disassembler::resolve64_mod1or2_rm4;
            insn->displacement.displ32 = (Bit8s) fetch_byte();
            break;
          case 2:
            resolve_modrm = &disassembler::resolve64_mod1or2_rm4;
            insn->displacement.displ32 = fetch_dword();
            break;
        }
      } /* s-i-b byte follows */
  }
  else
  {
    if (insn->as_32)
    {
      if ((insn->rm & 7) != 4) { /* rm != 100b, no s-i-b byte */
        // one byte modrm
        switch (insn->mod) {
          case 0:
            resolve_modrm = &disassembler::resolve32_mod0;
            if ((insn->rm & 7) == 5) /* no reg, 32-bit displacement */
              insn->displacement.displ32 = fetch_dword();
            break;
          case 1:
            /* reg, 8-bit displacement, sign extend */
            resolve_modrm = &disassembler::resolve32_mod1or2;
            insn->displacement.displ32 = (Bit8s) fetch_byte();
            break;
          case 2:
            /* reg, 32-bit displacement */
            resolve_modrm = &disassembler::resolve32_mod1or2;
            insn->displacement.displ32 = fetch_dword();
            break;
        } /* switch (mod) */
      } /* if (rm != 4) */
      else { /* rm == 4, s-i-b byte follows */
        insn->sib = fetch_byte();
        BX_DECODE_SIB(insn->sib, insn->scale, insn->index, insn->base);
        insn->base  |= insn->rex_b;
        insn->index |= insn->rex_x;

        switch (insn->mod) {
          case 0:
            resolve_modrm = &disassembler::resolve32_mod0_rm4;
            if ((insn->base & 7) == 5)
              insn->displacement.displ32 = fetch_dword();
            break;
          case 1:
            resolve_modrm = &disassembler::resolve32_mod1or2_rm4;
            insn->displacement.displ32 = (Bit8s) fetch_byte();
            break;
          case 2:
            resolve_modrm = &disassembler::resolve32_mod1or2_rm4;
            insn->displacement.displ32 = fetch_dword();
            break;
        }
      } /* s-i-b byte follows */
    }
    else {
      assert(insn->rex_b == 0);
      assert(insn->rex_x == 0);
      assert(insn->rex_r == 0);
      /* 16 bit addressing modes. */
      switch (insn->mod) {
        case 0:
          resolve_modrm = &disassembler::resolve16_mod0;
          if(insn->rm == 6)
            insn->displacement.displ16 = fetch_word();
          break;
        case 1:
          /* reg, 8-bit displacement, sign extend */
          resolve_modrm = &disassembler::resolve16_mod1or2;
          insn->displacement.displ16 = (Bit8s) fetch_byte();
          break;
        case 2:
          resolve_modrm = &disassembler::resolve16_mod1or2;
          insn->displacement.displ16 = fetch_word();
          break;
      } /* switch (mod) ... */
    }
  }
}

void disassembler::resolve16_mod0(const x86_insn *insn, unsigned datasize)
{
  const char *seg;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod00_rm16[insn->rm];

  if(insn->rm == 6)
    print_memory_access16(datasize, seg, NULL, insn->displacement.displ16);
  else
    print_memory_access16(datasize, seg, index16[insn->rm], 0);
}

void disassembler::resolve16_mod1or2(const x86_insn *insn, unsigned datasize)
{
  const char *seg;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod01or10_rm16[insn->rm];

  print_memory_access16(datasize, seg, index16[insn->rm], insn->displacement.displ16);
}

void disassembler::resolve32_mod0(const x86_insn *insn, unsigned datasize)
{
  const char *seg, *eip_regname = NULL;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = segment_name[DS_REG];

  if (insn->is_64) {
    if (intel_mode) eip_regname = "eip";
    else eip_regname = "%eip";
  }

  if ((insn->rm & 7) == 5) /* no reg, 32-bit displacement */
    print_memory_access32(datasize, seg, eip_regname, NULL, 0, insn->displacement.displ32);
  else
    print_memory_access32(datasize, seg, general_32bit_regname[insn->rm], NULL, 0, 0);
}

void disassembler::resolve32_mod1or2(const x86_insn *insn, unsigned datasize)
{
  const char *seg;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod01or10_base32[insn->rm];

  print_memory_access32(datasize, seg,
      general_32bit_regname[insn->rm], NULL, 0, insn->displacement.displ32);
}

void disassembler::resolve32_mod0_rm4(const x86_insn *insn, unsigned datasize)
{
  char vsib_index[8];
  const char *seg, *base = NULL, *index = NULL;
  Bit32u disp32 = 0;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod00_base32[insn->base];

  if ((insn->base & 7) != 5)
    base = general_32bit_regname[insn->base];
  else
    disp32 = insn->displacement.displ32;

  if (datasize & VSIB_Index) {
    sprintf(vsib_index, "%s%d", vector_reg_name[insn->vex_l], insn->index);
    index = vsib_index;
  }
  else {
    if (insn->index != 4)
      index = general_32bit_regname[insn->index];
  }

  print_memory_access32(datasize, seg, base, index, insn->scale, disp32);
}

void disassembler::resolve32_mod1or2_rm4(const x86_insn *insn, unsigned datasize)
{
  char vsib_index[8];
  const char *seg, *index = NULL;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod01or10_base32[insn->base];

  if (datasize & VSIB_Index) {
    sprintf(vsib_index, "%s%d", vector_reg_name[insn->vex_l], insn->index);
    index = vsib_index;
  }
  else {
    if (insn->index != 4)
      index = general_32bit_regname[insn->index];
  }

  print_memory_access32(datasize, seg,
      general_32bit_regname[insn->base], index, insn->scale, insn->displacement.displ32);
}

void disassembler::resolve64_mod0(const x86_insn *insn, unsigned datasize)
{
  const char *seg, *rip_regname;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = segment_name[DS_REG];

  if (intel_mode) rip_regname = "rip";
  else rip_regname = "%rip";

  if ((insn->rm & 7) == 5) /* no reg, 32-bit displacement */
    print_memory_access64(datasize, seg, rip_regname, NULL, 0, (Bit32s) insn->displacement.displ32);
  else
    print_memory_access64(datasize, seg, general_64bit_regname[insn->rm], NULL, 0, 0);
}

void disassembler::resolve64_mod1or2(const x86_insn *insn, unsigned datasize)
{
  const char *seg;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod01or10_base32[insn->rm];

  print_memory_access64(datasize, seg,
      general_64bit_regname[insn->rm], NULL, 0, (Bit32s) insn->displacement.displ32);
}

void disassembler::resolve64_mod0_rm4(const x86_insn *insn, unsigned datasize)
{
  char vsib_index[8];
  const char *seg, *base = NULL, *index = NULL;
  Bit32s disp32 = 0;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod00_base32[insn->base];

  if ((insn->base & 7) != 5)
    base = general_64bit_regname[insn->base];
  else
    disp32 = (Bit32s) insn->displacement.displ32;

  if (datasize & VSIB_Index) {
    sprintf(vsib_index, "%s%d", vector_reg_name[insn->vex_l], insn->index);
    index = vsib_index;
  }
  else {
    if (insn->index != 4)
      index = general_64bit_regname[insn->index];
  }

  print_memory_access64(datasize, seg, base, index, insn->scale, disp32);
}

void disassembler::resolve64_mod1or2_rm4(const x86_insn *insn, unsigned datasize)
{
  char vsib_index[8];
  const char *seg, *index = NULL;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = sreg_mod01or10_base32[insn->base];

  if (datasize & VSIB_Index) {
    sprintf(vsib_index, "%s%d", vector_reg_name[insn->vex_l], insn->index);
    index = vsib_index;
  }
  else {
    if (insn->index != 4)
      index = general_64bit_regname[insn->index];
  }

  print_memory_access64(datasize, seg,
      general_64bit_regname[insn->base], index, insn->scale, (Bit32s) insn->displacement.displ32);
}

void disassembler::print_datasize(unsigned size)
{
  if (!intel_mode || !print_mem_datasize) return;

  switch(size & 0xf)
  {
    case B_SIZE:
      dis_sprintf("byte ptr ");
      break;
    case W_SIZE:
      dis_sprintf("word ptr ");
      break;
    case D_SIZE:
      dis_sprintf("dword ptr ");
      break;
    case Q_SIZE:
      dis_sprintf("qword ptr ");
      break;
    case T_SIZE:
      dis_sprintf("tbyte ptr ");
      break;
    case XMM_SIZE:
      dis_sprintf("dqword ptr ");
      break;
    case YMM_SIZE:
      dis_sprintf("qqword ptr ");
      break;
    case X_SIZE:
      break;
  };
}

void disassembler::print_memory_access16(int datasize,
                const char *seg, const char *index, Bit16u disp)
{
  print_datasize(datasize);

  dis_sprintf("%s:", seg);

  if (intel_mode)
  {
    if (index == NULL)
    {
      dis_sprintf("0x%04x", (unsigned) disp);
    }
    else
    {
      if (disp != 0) {
        if (offset_mode_hex)
          dis_sprintf("[%s+0x%04x]", index, (unsigned) disp);
        else
          dis_sprintf("[%s%+d]", index, (int) (Bit16s) disp);
      }
      else
        dis_sprintf("[%s]", index);
    }
  }
  else
  {
    if (index == NULL)
    {
      dis_sprintf("0x%04x", (unsigned) disp);
    }
    else
    {
      if (disp != 0) {
        if (offset_mode_hex)
          dis_sprintf("0x%04x(%s,1)", (unsigned) disp, index);
        else
          dis_sprintf("%d(%s,1)", (int) (Bit16s) disp, index);
      }
      else
        dis_sprintf("(%s,1)", index);
    }
  }
}

void disassembler::print_memory_access32(int datasize,
        const char *seg, const char *base, const char *index, int scale, Bit32s disp)
{
  print_datasize(datasize);

  dis_sprintf("%s:", seg);

  scale = 1 << scale;

  if (intel_mode)
  {
    if (base == NULL)
    {
      if (index == NULL)
      {
        dis_sprintf("0x%08x", (unsigned) disp);
      }
      else
      {
        if (scale != 1)
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s*%d+0x%08x]", index, scale, (unsigned) disp);
            else
              dis_sprintf("[%s*%d%+d]", index, scale, (int) disp);
          }
          else
            dis_sprintf("[%s*%d]", index, scale);
        }
        else
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+0x%08x]", index, (unsigned) disp);
            else
              dis_sprintf("[%s%+d]", index, (int) disp);
          }
          else {
            dis_sprintf("[%s]", index);
          }
        }
      }
    }
    else
    {
      if (index == NULL)
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("[%s+0x%08x]", base, (unsigned) disp);
          else
            dis_sprintf("[%s%+d]", base, (int) disp);
        }
        else {
          dis_sprintf("[%s]", base);
        }
      }
      else
      {
        if (scale != 1)
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+%s*%d+0x%08x]", base, index, scale, (unsigned) disp);
            else
              dis_sprintf("[%s+%s*%d%+d]", base, index, scale, (int) disp);
          }
          else {
            dis_sprintf("[%s+%s*%d]", base, index, scale);
          }
        }
        else
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+%s+0x%08x]", base, index, (unsigned) disp);
            else
              dis_sprintf("[%s+%s%+d]", base, index, (int) disp);
          }
          else
            dis_sprintf("[%s+%s]", base, index);
        }
      }
    }
  }
  else
  {
    if (base == NULL)
    {
      if (index == NULL)
      {
        dis_sprintf("0x%08x", (unsigned) disp);
      }
      else
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x(,%s,%d)", (unsigned) disp, index, scale);
          else
            dis_sprintf("%d(,%s,%d)", (int) disp, index, scale);
        }
        else
          dis_sprintf("(,%s,%d)", index, scale);
      }
    }
    else
    {
      if (index == NULL)
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x(%s)", (unsigned) disp, base);
          else
            dis_sprintf("%d(%s)", (int) disp, base);
        }
        else
          dis_sprintf("(%s)", base);
      }
      else
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x(%s,%s,%d)", (unsigned) disp, base, index, scale);
          else
            dis_sprintf("%d(%s,%s,%d)", (int) disp, base, index, scale);
        }
        else
          dis_sprintf("(%s,%s,%d)", base, index, scale);
      }
    }
  }
}

void disassembler::print_memory_access64(int datasize,
        const char *seg, const char *base, const char *index, int scale, Bit32s disp)
{
  Bit64u disp64 = (Bit64s) disp;

  print_datasize(datasize);

  dis_sprintf("%s:", seg);

  scale = 1 << scale;

  if (intel_mode)
  {
    if (base == NULL)
    {
      if (index == NULL)
      {
        dis_sprintf("0x%08x%08x", GET32H(disp64), GET32L(disp64));
      }
      else
      {
        if (scale != 1)
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s*%d+0x%08x%08x]", index, scale, GET32H(disp64), GET32L(disp64));
            else
              dis_sprintf("[%s*%d%+d]", index, scale, (int) disp);
          }
          else
            dis_sprintf("[%s*%d]", index, scale);
        }
        else
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+0x%08x%08x]", index, GET32H(disp64), GET32L(disp64));
            else
              dis_sprintf("[%s%+d]", index, (int) disp);
          }
          else {
            dis_sprintf("[%s]", index);
          }
        }
      }
    }
    else
    {
      if (index == NULL)
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("[%s+0x%08x%08x]", base, GET32H(disp64), GET32L(disp64));
          else
            dis_sprintf("[%s%+d]", base, (int) disp);
        }
        else {
          dis_sprintf("[%s]", base);
        }
      }
      else
      {
        if (scale != 1)
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+%s*%d+0x%08x%08x]", base, index, scale, GET32H(disp64), GET32L(disp64));
            else
              dis_sprintf("[%s+%s*%d%+d]", base, index, scale, (int) disp);
          }
          else {
            dis_sprintf("[%s+%s*%d]", base, index, scale);
          }
        }
        else
        {
          if (disp != 0) {
            if (offset_mode_hex)
              dis_sprintf("[%s+%s+0x%08x%08x]", base, index, GET32H(disp64), GET32L(disp64));
            else
              dis_sprintf("[%s+%s%+d]", base, index, (int) disp);
          }
          else
            dis_sprintf("[%s+%s]", base, index);
        }
      }
    }
  }
  else
  {
    if (base == NULL)
    {
      if (index == NULL)
      {
        dis_sprintf("0x%08x%08x", GET32H(disp64), GET32L(disp64));
      }
      else
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x%08x(,%s,%d)", GET32H(disp64), GET32L(disp64), index, scale);
          else
            dis_sprintf("%d(,%s,%d)", (int) disp, index, scale);
        }
        else
          dis_sprintf("(,%s,%d)", index, scale);
      }
    }
    else
    {
      if (index == NULL)
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x%08x(%s)", GET32H(disp64), GET32L(disp64), base);
          else
            dis_sprintf("%d(%s)", (int) disp, base);
        }
        else
          dis_sprintf("(%s)", base);
      }
      else
      {
        if (disp != 0) {
          if (offset_mode_hex)
            dis_sprintf("0x%08x%08x(%s,%s,%d)", GET32H(disp64), GET32L(disp64), base, index, scale);
          else
            dis_sprintf("%d(%s,%s,%d)", (int) disp, base, index, scale);
        }
        else
          dis_sprintf("(%s,%s,%d)", base, index, scale);
      }
    }
  }
}
