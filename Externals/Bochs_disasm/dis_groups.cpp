/////////////////////////////////////////////////////////////////////////
// $Id: dis_groups.cc,v 1.33 2006/08/13 09:40:07 sshwarts Exp $
/////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <assert.h>
#include "disasm.h"

/*
#if BX_DEBUGGER
#include "../bx_debug/debug.h"
#endif
*/

void disassembler::Apw(const x86_insn *insn)
{
  Bit16u imm16 = fetch_word();
  Bit16u cs_selector = fetch_word();
  dis_sprintf("%04x:%04x", (unsigned) cs_selector, (unsigned) imm16);
}

void disassembler::Apd(const x86_insn *insn)
{
  Bit32u imm32 = fetch_dword();
  Bit16u cs_selector = fetch_word();
  dis_sprintf("%04x:%08x", (unsigned) cs_selector, (unsigned) imm32);
}

// 8-bit general purpose registers
void disassembler::AL(const x86_insn *insn) { dis_sprintf("%s", general_8bit_regname[rAX_REG]); }
void disassembler::CL(const x86_insn *insn) { dis_sprintf("%s", general_8bit_regname[rCX_REG]); }

// 16-bit general purpose registers
void disassembler::AX(const x86_insn *insn) {
  dis_sprintf("%s", general_16bit_regname[rAX_REG]);
}

void disassembler::DX(const x86_insn *insn) {
  dis_sprintf("%s", general_16bit_regname[rDX_REG]);
}

// 32-bit general purpose registers
void disassembler::EAX(const x86_insn *insn)
{
  dis_sprintf("%s", general_32bit_regname[rAX_REG]);
}

// 64-bit general purpose registers
void disassembler::RAX(const x86_insn *insn)
{
  dis_sprintf("%s", general_64bit_regname[rAX_REG]);
}

// segment registers
void disassembler::CS(const x86_insn *insn) { dis_sprintf("%s", segment_name[CS_REG]); }
void disassembler::DS(const x86_insn *insn) { dis_sprintf("%s", segment_name[DS_REG]); }
void disassembler::ES(const x86_insn *insn) { dis_sprintf("%s", segment_name[ES_REG]); }
void disassembler::SS(const x86_insn *insn) { dis_sprintf("%s", segment_name[SS_REG]); }
void disassembler::FS(const x86_insn *insn) { dis_sprintf("%s", segment_name[FS_REG]); }
void disassembler::GS(const x86_insn *insn) { dis_sprintf("%s", segment_name[GS_REG]); }

void disassembler::Sw(const x86_insn *insn) { dis_sprintf("%s", segment_name[insn->nnn]); }

// test registers
void disassembler::Td(const x86_insn *insn)
{
  if (intel_mode)
    dis_sprintf  ("tr%d", insn->nnn);
  else
    dis_sprintf("%%tr%d", insn->nnn);
}

// control register
void disassembler::Cd(const x86_insn *insn) 
{ 
  if (intel_mode)
    dis_sprintf  ("cr%d", insn->nnn);
  else
    dis_sprintf("%%cr%d", insn->nnn);
}

void disassembler::Cq(const x86_insn *insn) { Cd(insn); }

// debug register
void disassembler::Dd(const x86_insn *insn) 
{
  if (intel_mode)
    dis_sprintf  ("db%d", insn->nnn);
  else
    dis_sprintf("%%db%d", insn->nnn);
}

void disassembler::Dq(const x86_insn *insn) { Dd(insn); }

// 8-bit general purpose register
void disassembler::R8(const x86_insn *insn)
{ 
  unsigned reg = (insn->b1 & 7) | insn->rex_b;
 
  if (reg < 4 || insn->extend8b)
    dis_sprintf("%s", general_8bit_regname_rex[reg]);
  else
    dis_sprintf("%s", general_8bit_regname[reg]);
}

// 16-bit general purpose register
void disassembler::RX(const x86_insn *insn)
{ 
  dis_sprintf("%s", general_16bit_regname[(insn->b1 & 7) | insn->rex_b]);
}

// 32-bit general purpose register
void disassembler::ERX(const x86_insn *insn)
{ 
  dis_sprintf("%s", general_32bit_regname[(insn->b1 & 7) | insn->rex_b]);
}

// 64-bit general purpose register
void disassembler::RRX(const x86_insn *insn)
{ 
  dis_sprintf("%s", general_64bit_regname[(insn->b1 & 7) | insn->rex_b]);
}

// general purpose register or memory operand
void disassembler::Eb(const x86_insn *insn) 
{
  if (insn->mod == 3) {
    if (insn->rm < 4 || insn->extend8b)
      dis_sprintf("%s", general_8bit_regname_rex[insn->rm]);
    else
      dis_sprintf("%s", general_8bit_regname[insn->rm]);
  }
  else
    (this->*resolve_modrm)(insn, B_SIZE);
}

void disassembler::Ew(const x86_insn *insn) 
{
  if (insn->mod == 3)
    dis_sprintf("%s", general_16bit_regname[insn->rm]);
  else
    (this->*resolve_modrm)(insn, W_SIZE);
}

void disassembler::Ed(const x86_insn *insn) 
{
  if (insn->mod == 3)
    dis_sprintf("%s", general_32bit_regname[insn->rm]);
  else
    (this->*resolve_modrm)(insn, D_SIZE);
}

void disassembler::Eq(const x86_insn *insn) 
{
  if (insn->mod == 3)
    dis_sprintf("%s", general_64bit_regname[insn->rm]);
  else
    (this->*resolve_modrm)(insn, Q_SIZE);
}

// general purpose register
void disassembler::Gb(const x86_insn *insn) 
{
  if (insn->nnn < 4 || insn->extend8b)
    dis_sprintf("%s", general_8bit_regname_rex[insn->nnn]);
  else
    dis_sprintf("%s", general_8bit_regname[insn->nnn]);
}

void disassembler::Gw(const x86_insn *insn) 
{
  dis_sprintf("%s", general_16bit_regname[insn->nnn]);
}

void disassembler::Gd(const x86_insn *insn) 
{
  dis_sprintf("%s", general_32bit_regname[insn->nnn]);
}

void disassembler::Gq(const x86_insn *insn) 
{
  dis_sprintf("%s", general_64bit_regname[insn->nnn]);
}

// immediate
void disassembler::I1(const x86_insn *insn) 
{ 
  if (! intel_mode) dis_putc('$');
  dis_putc ('1');
}

void disassembler::Ib(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  dis_sprintf("0x%02x", (unsigned) fetch_byte());
}

void disassembler::Iw(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  dis_sprintf("0x%04x", (unsigned) fetch_word());
}

void disassembler::IwIb(const x86_insn *insn) 
{
  Bit16u iw = fetch_word();
  Bit8u  ib = fetch_byte();

  if (intel_mode) {
     dis_sprintf("0x%04x, 0x%02x", iw, ib);
  }
  else {
     dis_sprintf("$0x%02x, $0x%04x", ib, iw);
  }
}

void disassembler::Id(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  dis_sprintf("0x%08x", (unsigned) fetch_dword());
}

void disassembler::Iq(const x86_insn *insn) 
{
  Bit64u value = fetch_qword();

  if (! intel_mode) dis_putc('$');
  dis_sprintf("0x%08x%08x", 
      (unsigned)(value>>32), (unsigned)(value & 0xffffffff));
}

// sign extended immediate
void disassembler::sIbw(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  Bit16u imm16 = (Bit8s) fetch_byte();
  dis_sprintf("0x%04x", (unsigned) imm16);
}

// sign extended immediate
void disassembler::sIbd(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  Bit32u imm32 = (Bit8s) fetch_byte();
  dis_sprintf ("0x%08x", (unsigned) imm32);
}

// sign extended immediate
void disassembler::sIbq(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  Bit64u imm64 = (Bit8s) fetch_byte();
  dis_sprintf ("0x%08x%08x",
      (unsigned)(imm64>>32), (unsigned)(imm64 & 0xffffffff));
}

// sign extended immediate
void disassembler::sIdq(const x86_insn *insn) 
{
  if (! intel_mode) dis_putc('$');
  Bit64u imm64 = (Bit32s) fetch_dword();
  dis_sprintf ("0x%08x%08x",
      (unsigned)(imm64>>32), (unsigned)(imm64 & 0xffffffff));
}

// floating point
void disassembler::ST0(const x86_insn *insn)
{ 
  if (intel_mode)
    dis_sprintf  ("st(0)");
  else
    dis_sprintf("%%st(0)");
}

void disassembler::STi(const x86_insn *insn) 
{ 
  if (intel_mode)
    dis_sprintf  ("st(%d)", insn->rm);
  else
    dis_sprintf("%%st(%d)", insn->rm);
}

// 16-bit general purpose register
void disassembler::Rw(const x86_insn *insn)
{
  dis_sprintf("%s", general_16bit_regname[insn->rm]);
}

// 32-bit general purpose register
void disassembler::Rd(const x86_insn *insn)
{
  dis_sprintf("%s", general_32bit_regname[insn->rm]);
}

// 64-bit general purpose register
void disassembler::Rq(const x86_insn *insn)
{
  dis_sprintf("%s", general_64bit_regname[insn->rm]);
}

// mmx register
void disassembler::Pq(const x86_insn *insn)
{
  if (intel_mode)
    dis_sprintf  ("mm%d", insn->nnn);
  else
    dis_sprintf("%%mm%d", insn->nnn);
}

void disassembler::Nq(const x86_insn *insn)
{
  if (intel_mode)
    dis_sprintf  ("mm%d", insn->rm);
  else
    dis_sprintf("%%mm%d", insn->rm);
}

void disassembler::Qd(const x86_insn *insn)
{
  if (insn->mod == 3)
  {
    if (intel_mode)
      dis_sprintf  ("mm%d", insn->rm);
    else
      dis_sprintf("%%mm%d", insn->rm);
  }
  else
    (this->*resolve_modrm)(insn, D_SIZE);
}

void disassembler::Qq(const x86_insn *insn)
{
  if (insn->mod == 3)
  {
    if (intel_mode)
      dis_sprintf  ("mm%d", insn->rm);
    else
      dis_sprintf("%%mm%d", insn->rm);
  }
  else
    (this->*resolve_modrm)(insn, Q_SIZE);
}

// xmm register
void disassembler::Udq(const x86_insn *insn)
{
  if (intel_mode)
    dis_sprintf  ("xmm%d", insn->rm);
  else
    dis_sprintf("%%xmm%d", insn->rm);
}

void disassembler::Vq(const x86_insn *insn)
{
  if (intel_mode)
    dis_sprintf  ("xmm%d", insn->nnn);
  else
    dis_sprintf("%%xmm%d", insn->nnn);
}

void disassembler::Vdq(const x86_insn *insn) { Vq(insn); }
void disassembler::Vss(const x86_insn *insn) { Vq(insn); }
void disassembler::Vsd(const x86_insn *insn) { Vq(insn); }
void disassembler::Vps(const x86_insn *insn) { Vq(insn); }
void disassembler::Vpd(const x86_insn *insn) { Vq(insn); }

void disassembler::Wq(const x86_insn *insn)
{
  if (insn->mod == 3)
  {
    if (intel_mode)
      dis_sprintf  ("xmm%d", insn->rm);
    else
      dis_sprintf("%%xmm%d", insn->rm);
  }
  else
    (this->*resolve_modrm)(insn, Q_SIZE);
}

void disassembler::Wdq(const x86_insn *insn)
{
  if (insn->mod == 3)
  {
    if (intel_mode)
      dis_sprintf  ("xmm%d", insn->rm);
    else
      dis_sprintf("%%xmm%d", insn->rm);
  }
  else
    (this->*resolve_modrm)(insn, O_SIZE);
}

void disassembler::Wsd(const x86_insn *insn) { Wq(insn); }

void disassembler::Wss(const x86_insn *insn)
{ 
  if (insn->mod == 3)
  {
    if (intel_mode)
      dis_sprintf  ("xmm%d", insn->rm);
    else
      dis_sprintf("%%xmm%d", insn->rm);
  }
  else
    (this->*resolve_modrm)(insn, D_SIZE);
}

void disassembler::Wpd(const x86_insn *insn) { Wdq(insn); }
void disassembler::Wps(const x86_insn *insn) { Wdq(insn); }

// direct memory access
void disassembler::OP_O(const x86_insn *insn, unsigned size)
{
  const char *seg;

  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = segment_name[DS_REG];

  print_datasize(size);

  if (insn->as_64) {
    Bit64u imm64 = fetch_qword();
    dis_sprintf("%s:0x%08x%08x", seg,
        (unsigned)(imm64>>32), (unsigned)(imm64 & 0xffffffff));
  }
  else if (insn->as_32) {
    Bit32u imm32 = fetch_dword();
    dis_sprintf("%s:0x%x", seg, (unsigned) imm32);
  }
  else {
    Bit16u imm16 = fetch_word();
    dis_sprintf("%s:0x%x", seg, (unsigned) imm16);
  }
}

void disassembler::Ob(const x86_insn *insn) { OP_O(insn, B_SIZE); }
void disassembler::Ow(const x86_insn *insn) { OP_O(insn, W_SIZE); }
void disassembler::Od(const x86_insn *insn) { OP_O(insn, D_SIZE); }
void disassembler::Oq(const x86_insn *insn) { OP_O(insn, Q_SIZE); }

// memory operand
void disassembler::OP_M(const x86_insn *insn, unsigned size)
{
  if(insn->mod == 3)
    dis_sprintf("(bad)");
  else
    (this->*resolve_modrm)(insn, size);
}

void disassembler::Ma(const x86_insn *insn) { OP_M(insn, X_SIZE); }
void disassembler::Mp(const x86_insn *insn) { OP_M(insn, X_SIZE); }
void disassembler::Ms(const x86_insn *insn) { OP_M(insn, X_SIZE); }
void disassembler::Mx(const x86_insn *insn) { OP_M(insn, X_SIZE); }

void disassembler::Mb(const x86_insn *insn) { OP_M(insn, B_SIZE); }
void disassembler::Mw(const x86_insn *insn) { OP_M(insn, W_SIZE); }
void disassembler::Md(const x86_insn *insn) { OP_M(insn, D_SIZE); }
void disassembler::Mq(const x86_insn *insn) { OP_M(insn, Q_SIZE); }
void disassembler::Mt(const x86_insn *insn) { OP_M(insn, T_SIZE); }

void disassembler::Mdq(const x86_insn *insn) { OP_M(insn, O_SIZE); }
void disassembler::Mps(const x86_insn *insn) { OP_M(insn, O_SIZE); }
void disassembler::Mpd(const x86_insn *insn) { OP_M(insn, O_SIZE); }

// string instructions
void disassembler::OP_X(const x86_insn *insn, unsigned size)
{
  const char *rsi, *seg;

  if (insn->as_64) {
    rsi = general_64bit_regname[rSI_REG];
  }
  else {
    if (insn->as_32)
      rsi = general_32bit_regname[rSI_REG];
    else
      rsi = general_16bit_regname[rSI_REG];
  }
  
  if (insn->is_seg_override())
    seg = segment_name[insn->seg_override];
  else
    seg = segment_name[DS_REG];

  print_datasize(size);

  if (intel_mode)
    dis_sprintf("%s:[%s]", seg, rsi);
  else
    dis_sprintf("%s:(%s)", seg, rsi);
}

void disassembler::Xb(const x86_insn *insn) { OP_X(insn, B_SIZE); }
void disassembler::Xw(const x86_insn *insn) { OP_X(insn, W_SIZE); }
void disassembler::Xd(const x86_insn *insn) { OP_X(insn, D_SIZE); }
void disassembler::Xq(const x86_insn *insn) { OP_X(insn, Q_SIZE); }

void disassembler::OP_Y(const x86_insn *insn, unsigned size)
{
  const char *rdi;

  if (insn->as_64) {
    rdi = general_64bit_regname[rDI_REG];
  }
  else {
    if (insn->as_32)
      rdi = general_32bit_regname[rDI_REG];
    else
      rdi = general_16bit_regname[rDI_REG];
  }
  
  print_datasize(size);

  if (intel_mode)
    dis_sprintf("%s:[%s]", segment_name[ES_REG], rdi);
  else
    dis_sprintf("%s:(%s)", segment_name[ES_REG], rdi);
}

void disassembler::Yb(const x86_insn *insn) { OP_Y(insn, B_SIZE); }
void disassembler::Yw(const x86_insn *insn) { OP_Y(insn, W_SIZE); }
void disassembler::Yd(const x86_insn *insn) { OP_Y(insn, D_SIZE); }
void disassembler::Yq(const x86_insn *insn) { OP_Y(insn, Q_SIZE); }

#define BX_JUMP_TARGET_NOT_REQ ((bx_address)(-1))

// jump offset
void disassembler::Jb(const x86_insn *insn)
{
  Bit8s imm8 = (Bit8s) fetch_byte();

  if (insn->is_64) {
    Bit64u imm64 = (Bit64s) imm8;
    dis_sprintf(".+0x%08x%08x", 
        (unsigned)(imm64>>32), (unsigned)(imm64 & 0xffffffff));

    if (db_base != BX_JUMP_TARGET_NOT_REQ) {
      Bit64u target = db_eip + (Bit64s) imm64; target += db_base;
      dis_sprintf(" (0x%08x%08x)", 
        (unsigned)(target>>32), (unsigned)(target & 0xffffffff));
    }

    return;
  }

  if (insn->os_32) {
    Bit32u imm32 = (Bit32s) imm8;
    dis_sprintf(".+0x%08x", (unsigned) imm32);

    if (db_base != BX_JUMP_TARGET_NOT_REQ) {
      Bit32u target = db_eip + (Bit32s) imm32; target += db_base;
      dis_sprintf(" (0x%08x)", target);
    }
  }
  else {
    Bit16u imm16 = (Bit16s) imm8;
    dis_sprintf(".+0x%04x", (unsigned) imm16);

    if (db_base != BX_JUMP_TARGET_NOT_REQ) {
      Bit16u target = (db_eip + (Bit16s) imm16) & 0xffff;
      dis_sprintf(" (0x%08x)", target + db_base);
    }
  }
}

void disassembler::Jw(const x86_insn *insn)
{
  // Jw supported in 16-bit mode only
  assert(! insn->is_64);
  assert(! insn->is_32);

  Bit16u imm16 = (Bit16s) fetch_word();
  dis_sprintf(".+0x%04x", (unsigned) imm16);

  if (db_base != BX_JUMP_TARGET_NOT_REQ) {
    Bit16u target = (db_eip + (Bit16s) imm16) & 0xffff;
    dis_sprintf(" (0x%08x)", target + db_base);
  }
}

void disassembler::Jd(const x86_insn *insn)
{
  Bit32s imm32 = (Bit32s) fetch_dword();

  if (insn->is_64) {
    Bit64u imm64 = (Bit64s) imm32;
    dis_sprintf(".+0x%08x%08x", 
        (unsigned)(imm64>>32), (unsigned)(imm64 & 0xffffffff));

    if (db_base != BX_JUMP_TARGET_NOT_REQ) {
      Bit64u target = db_eip + (Bit64s) imm64; target += db_base;
      dis_sprintf(" (0x%08x%08x)", 
        (unsigned)(target>>32), (unsigned)(target & 0xffffffff));
    }

    return;
  }

  dis_sprintf(".+0x%08x", (unsigned) imm32);

  if (db_base != BX_JUMP_TARGET_NOT_REQ) {
    Bit32u target = db_eip + (Bit32s) imm32; target += db_base;
    dis_sprintf(" (0x%08x)", target);
  }
}
