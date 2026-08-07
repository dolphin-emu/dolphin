/////////////////////////////////////////////////////////////////////////
// $Id: disasm.h 12420 2014-07-18 11:14:25Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2005-2014 Stanislav Shwartsman
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

#ifndef _BX_DISASM_H_
#define _BX_DISASM_H_

#include "config.h"

#define BX_DECODE_MODRM(modrm_byte, mod, opcode, rm) { \
  mod    = (modrm_byte >> 6) & 0x03; \
  opcode = (modrm_byte >> 3) & 0x07; \
  rm     =  modrm_byte & 0x07;       \
}

#define BX_DECODE_SIB(sib_byte, scale, index, base) { \
  scale =  sib_byte >> 6;          \
  index = (sib_byte >> 3) & 0x07;  \
  base  =  sib_byte & 0x07;        \
}

/* Instruction set attributes (duplicated in cpu.h) */
#define IA_X87              (BX_CONST64(1) << 0)   /* FPU (X87) instruction */
#define IA_486              (BX_CONST64(1) << 1)   /* 486 new instruction */
#define IA_PENTIUM          (BX_CONST64(1) << 2)   /* Pentium new instruction */
#define IA_P6               (BX_CONST64(1) << 3)   /* P6 new instruction */
#define IA_MMX              (BX_CONST64(1) << 4)   /* MMX instruction */
#define IA_3DNOW            (BX_CONST64(1) << 5)   /* 3DNow! instruction (AMD) */
#define IA_SYSCALL_SYSRET   (BX_CONST64(1) << 6)   /* SYSCALL/SYSRET in legacy mode (AMD) */
#define IA_SYSENTER_SYSEXIT (BX_CONST64(1) << 7)   /* SYSENTER/SYSEXIT instruction */
#define IA_CLFLUSH          (BX_CONST64(1) << 8)   /* CLFLUSH instruction */
#define IA_SSE              (BX_CONST64(1) << 9)   /* SSE  instruction */
#define IA_SSE2             (BX_CONST64(1) << 10)  /* SSE2 instruction */
#define IA_SSE3             (BX_CONST64(1) << 11)  /* SSE3 instruction */
#define IA_SSSE3            (BX_CONST64(1) << 12)  /* SSSE3 instruction */
#define IA_SSE4_1           (BX_CONST64(1) << 13)  /* SSE4_1 instruction */
#define IA_SSE4_2           (BX_CONST64(1) << 14)  /* SSE4_2 instruction */
#define IA_POPCNT           (BX_CONST64(1) << 15)  /* POPCNT instruction */
#define IA_MONITOR_MWAIT    (BX_CONST64(1) << 16)  /* MONITOR/MWAIT instruction */
#define IA_VMX              (BX_CONST64(1) << 17)  /* VMX instruction */
#define IA_SMX              (BX_CONST64(1) << 18)  /* SMX instruction */
#define IA_LM_LAHF_SAHF     (BX_CONST64(1) << 19)  /* Long Mode LAHF/SAHF instruction */
#define IA_CMPXCHG16B       (BX_CONST64(1) << 20)  /* CMPXCHG16B instruction */
#define IA_RDTSCP           (BX_CONST64(1) << 21)  /* RDTSCP instruction */
#define IA_XSAVE            (BX_CONST64(1) << 22)  /* XSAVE/XRSTOR extensions instruction */
#define IA_XSAVEOPT         (BX_CONST64(1) << 23)  /* XSAVEOPT instruction */
#define IA_AES_PCLMULQDQ    (BX_CONST64(1) << 24)  /* AES+PCLMULQDQ instruction */
#define IA_MOVBE            (BX_CONST64(1) << 25)  /* MOVBE Intel Atom(R) instruction */
#define IA_FSGSBASE         (BX_CONST64(1) << 26)  /* FS/GS BASE access instruction */
#define IA_INVPCID          (BX_CONST64(1) << 27)  /* INVPCID instruction */
#define IA_AVX              (BX_CONST64(1) << 28)  /* AVX instruction */
#define IA_AVX2             (BX_CONST64(1) << 29)  /* AVX2 instruction */
#define IA_AVX_F16C         (BX_CONST64(1) << 30)  /* AVX F16 convert instruction */
#define IA_AVX_FMA          (BX_CONST64(1) << 31)  /* AVX FMA instruction */
#define IA_SSE4A            (BX_CONST64(1) << 32)  /* SSE4A instruction (AMD) */
#define IA_LZCNT            (BX_CONST64(1) << 33)  /* LZCNT instruction */
#define IA_BMI1             (BX_CONST64(1) << 34)  /* BMI1 instruction */
#define IA_BMI2             (BX_CONST64(1) << 35)  /* BMI2 instruction */
#define IA_FMA4             (BX_CONST64(1) << 36)  /* FMA4 instruction (AMD) */
#define IA_XOP              (BX_CONST64(1) << 37)  /* XOP instruction (AMD) */
#define IA_TBM              (BX_CONST64(1) << 38)  /* TBM instruction (AMD) */
#define IA_SVM              (BX_CONST64(1) << 39)  /* SVM instruction (AMD) */
#define IA_RDRAND           (BX_CONST64(1) << 40)  /* RDRAND instruction */
#define IA_ADX              (BX_CONST64(1) << 41)  /* ADCX/ADOX instruction */
#define IA_SMAP             (BX_CONST64(1) << 42)  /* SMAP support */
#define IA_RDSEED           (BX_CONST64(1) << 43)  /* RDSEED instruction */
#define IA_SHA              (BX_CONST64(1) << 44)  /* SHA instruction */
#define IA_AVX512           (BX_CONST64(1) << 45)  /* AVX-512 instruction */
#define IA_AVX512_CD        (BX_CONST64(1) << 46)  /* AVX-512 Conflict Detection instruction */
#define IA_AVX512_PF        (BX_CONST64(1) << 47)  /* AVX-512 Sparse Prefetch instruction */
#define IA_AVX512_ER        (BX_CONST64(1) << 48)  /* AVX-512 Exponential/Reciprocal instruction */
#define IA_AVX512_DQ        (BX_CONST64(1) << 49)  /* AVX-512DQ instruction */
#define IA_AVX512_BW        (BX_CONST64(1) << 50)  /* AVX-512 Byte/Word instruction */
#define IA_CLFLUSHOPT       (BX_CONST64(1) << 51)  /* CLFLUSHOPT instruction */
#define IA_XSAVEC           (BX_CONST64(1) << 52)  /* XSAVEC instruction */
#define IA_XSAVES           (BX_CONST64(1) << 53)  /* XSAVES instruction */

/* general purpose bit register */
enum {
	rAX_REG,
	rCX_REG,
	rDX_REG,
	rBX_REG,
	rSP_REG,
	rBP_REG,
	rSI_REG,
	rDI_REG
};

/* segment register */
enum {
	ES_REG,
	CS_REG,
	SS_REG,
	DS_REG,
	FS_REG,
	GS_REG,
        INVALID_SEG1,
        INVALID_SEG2
};

class disassembler;
struct x86_insn;

typedef void (disassembler::*BxDisasmPtr_t)(const x86_insn *insn);
typedef void (disassembler::*BxDisasmResolveModrmPtr_t)(const x86_insn *insn, unsigned attr);

struct BxDisasmOpcodeInfo_t
{
    const char *IntelOpcode;
    const char *AttOpcode;
    BxDisasmPtr_t Operand1;
    BxDisasmPtr_t Operand2;
    BxDisasmPtr_t Operand3;
    BxDisasmPtr_t Operand4;
    Bit64u Feature;
};

struct BxDisasmOpcodeTable_t
{
    Bit32u Attr;
    const void *OpcodeInfo;
};

// segment override not used
#define NO_SEG_OVERRIDE 0xFF

// datasize attributes
#define   X_SIZE      0x00 /* no size */
#define   B_SIZE      0x01 /* byte */
#define   W_SIZE      0x02 /* word */
#define   D_SIZE      0x03 /* double word */
#define   Q_SIZE      0x04 /* quad word */
#define   Z_SIZE      0x05 /* double word in 32-bit mode, quad word in 64-bit mode */
#define   T_SIZE      0x06 /* 10-byte x87 floating point */
#define XMM_SIZE      0x07 /* double quad word (XMM) */
#define YMM_SIZE      0x08 /* quadruple quad word (YMM) */

#define VSIB_Index    0x80

// branch hint attribute
#define BRANCH_HINT 0x1000

struct x86_insn
{
public:
  x86_insn(bx_bool is32, bx_bool is64);

  bx_bool is_seg_override() const {
     return (seg_override != NO_SEG_OVERRIDE);
 }

public:
  bx_bool is_32, is_64;
  bx_bool as_32, as_64;
  bx_bool os_32, os_64;

  Bit8u extend8b;
  Bit8u rex_r, rex_x, rex_b;
  Bit8u seg_override;
  unsigned b1;
  unsigned ilen;

#define BX_AVX_VL128 0
#define BX_AVX_VL256 1
  Bit8u vex_vvv, vex_l, vex_w;
  int is_vex; // 0 - no VEX used, 1 - VEX is used, -1 - invalid VEX
  int is_evex; // 0 - no EVEX used, 1 - EVEX is used, -1 - invalid EVEX
  int is_xop; // 0 - no XOP used, 1 - XOP is used, -1 - invalid XOP
  Bit8u modrm, mod, nnn, rm;
  Bit8u sib, scale, index, base;
  union {
     Bit16u displ16;
     Bit32u displ32;
  } displacement;

  bx_bool evex_b;
  bx_bool evex_z;
  unsigned evex_ll_rc; 
};

BX_CPP_INLINE x86_insn::x86_insn(bx_bool is32, bx_bool is64)
{
  is_32 = is32;
  is_64 = is64;

  if (is_64) {
    os_64 = 0;
    as_64 = 1;
    os_32 = 1;
    as_32 = 1;
  }
  else {
    os_64 = 0;
    as_64 = 0;
    os_32 = is_32;
    as_32 = is_32;
  }

  extend8b = 0;
  rex_r = rex_b = rex_x = 0;
  seg_override = NO_SEG_OVERRIDE;
  ilen = 0;
  b1 = 0;

  is_vex = 0;
  is_evex = 0;
  is_xop = 0;
  vex_vvv = 0;
  vex_l = BX_AVX_VL128;
  vex_w = 0;
  modrm = mod = nnn = rm = 0;
  sib = scale = index = base = 0;
  displacement.displ32 = 0;

  evex_b = 0;
  evex_ll_rc = 0;
  evex_z = 0;
}

class disassembler {
public:
  disassembler(): offset_mode_hex(0), print_mem_datasize(1) { set_syntax_intel(); }

  unsigned disasm(bx_bool is_32, bx_bool is_64, bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf);

  unsigned disasm16(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(0, 0, cs_base, ip, instr, disbuf); }

  unsigned disasm32(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(1, 0, cs_base, ip, instr, disbuf); }

  unsigned disasm64(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(1, 1, cs_base, ip, instr, disbuf); }

  x86_insn decode(bx_bool is_32, bx_bool is_64, bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf);

  x86_insn decode16(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(0, 0, cs_base, ip, instr, disbuf); }

  x86_insn decode32(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(1, 0, cs_base, ip, instr, disbuf); }

  x86_insn decode64(bx_address cs_base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(1, 1, cs_base, ip, instr, disbuf); }

  void set_syntax_intel();
  void set_syntax_att();

  void set_offset_mode_hex(bx_bool mode) { offset_mode_hex = mode; }
  void set_mem_datasize_print(bx_bool mode) { print_mem_datasize = mode; }

  void toggle_syntax_mode();

private:
  bx_bool intel_mode, offset_mode_hex, print_mem_datasize;

  const char **general_16bit_regname;
  const char **general_8bit_regname;
  const char **general_32bit_regname;
  const char **general_8bit_regname_rex;
  const char **general_64bit_regname;

  const char **segment_name;
  const char **index16;
  const char **vector_reg_name;

  const char *sreg_mod00_base32[16];
  const char *sreg_mod01or10_base32[16];
  const char *sreg_mod00_rm16[8];
  const char *sreg_mod01or10_rm16[8];

private:

  bx_address db_eip, db_cs_base;

  const Bit8u *instruction;        // for fetching of next byte of instruction

  char *disbufptr;

  BxDisasmResolveModrmPtr_t resolve_modrm;

  BX_CPP_INLINE Bit8u  fetch_byte() {
    db_eip++;
    return(*instruction++);
  };

  BX_CPP_INLINE Bit8u  peek_byte() {
    return(*instruction);
  };

  BX_CPP_INLINE Bit16u fetch_word() {
    Bit8u b0 = * (Bit8u *) instruction++;
    Bit8u b1 = * (Bit8u *) instruction++;
    Bit16u ret16 = (b1<<8) | b0;
    db_eip += 2;
    return(ret16);
  };

  BX_CPP_INLINE Bit32u fetch_dword() {
    Bit8u b0 = * (Bit8u *) instruction++;
    Bit8u b1 = * (Bit8u *) instruction++;
    Bit8u b2 = * (Bit8u *) instruction++;
    Bit8u b3 = * (Bit8u *) instruction++;
    Bit32u ret32 = (b3<<24) | (b2<<16) | (b1<<8) | b0;
    db_eip += 4;
    return(ret32);
  };

  BX_CPP_INLINE Bit64u fetch_qword() {
    Bit64u d0 = fetch_dword();
    Bit64u d1 = fetch_dword();
    Bit64u ret64 = (d1<<32) | d0;
    return(ret64);
  };

  void dis_putc(char symbol);
  void dis_sprintf(const char *fmt, ...);
  void decode_modrm(x86_insn *insn);
  unsigned decode_vex(x86_insn *insn);
  unsigned decode_evex(x86_insn *insn);
  unsigned decode_xop(x86_insn *insn);

  void resolve16_mod0   (const x86_insn *insn, unsigned mode);
  void resolve16_mod1or2(const x86_insn *insn, unsigned mode);

  void resolve32_mod0   (const x86_insn *insn, unsigned mode);
  void resolve32_mod1or2(const x86_insn *insn, unsigned mode);

  void resolve32_mod0_rm4   (const x86_insn *insn, unsigned mode);
  void resolve32_mod1or2_rm4(const x86_insn *insn, unsigned mode);

  void resolve64_mod0   (const x86_insn *insn, unsigned mode);
  void resolve64_mod1or2(const x86_insn *insn, unsigned mode);

  void resolve64_mod0_rm4   (const x86_insn *insn, unsigned mode);
  void resolve64_mod1or2_rm4(const x86_insn *insn, unsigned mode);

  void initialize_modrm_segregs();

  void print_datasize(unsigned mode);

  void print_memory_access16(int datasize,
          const char *seg, const char *index, Bit16u disp);
  void print_memory_access32(int datasize,
          const char *seg, const char *base, const char *index, int scale, Bit32s disp);
  void print_memory_access64(int datasize,
          const char *seg, const char *base, const char *index, int scale, Bit32s disp);

  void print_disassembly_intel(const x86_insn *insn, const BxDisasmOpcodeInfo_t *entry);
  void print_disassembly_att  (const x86_insn *insn, const BxDisasmOpcodeInfo_t *entry);

public:

/*
 * Codes for Addressing Method:
 * ---------------------------
 * A  - Direct address. The instruction has no ModR/M byte; the address
 *      of the operand is encoded in the instruction; and no base register,
 *      index register, or scaling factor can be applied.
 * C  - The reg field of the ModR/M byte selects a control register.
 * D  - The reg field of the ModR/M byte selects a debug register.
 * E  - A ModR/M byte follows the opcode and specifies the operand. The
 *      operand is either a general-purpose register or a memory address.
 *      In case of the register operand, the R/M field of the ModR/M byte
 *      selects a general register.
 * F  - Flags Register.
 * G  - The reg field of the ModR/M byte selects a general register.
 * I  - Immediate data. The operand value is encoded in subsequent bytes of
 *      the instruction.
 * J  - The instruction contains a relative offset to be added to the
 *      instruction pointer register.
 * M  - The ModR/M byte may refer only to memory.
 * N  - The R/M field of the ModR/M byte selects a packed-quadword  MMX
        technology register.
 * O  - The instruction has no ModR/M byte; the offset of the operand is
 *      coded as a word or double word (depending on address size attribute)
 *      in the instruction. No base register, index register, or scaling
 *      factor can be applied.
 * P  - The reg field of the ModR/M byte selects a packed quadword MMX
 *      technology register.
 * Q  - A ModR/M byte follows the opcode and specifies the operand. The
 *      operand is either an MMX technology register or a memory address.
 *      If it is a memory address, the address is computed from a segment
 *      register and any of the following values: a base register, an
 *      index register, a scaling factor, and a displacement.
 * R  - The mod field of the ModR/M byte may refer only to a general register.
 * S  - The reg field of the ModR/M byte selects a segment register.
 * T  - The reg field of the ModR/M byte selects a test register.
 * U  - The R/M field of the ModR/M byte selects a 128-bit XMM/256-bit YMM register.
 * V  - The reg field of the ModR/M byte selects a 128-bit XMM/256-bit YMM register.
 * W  - A ModR/M byte follows the opcode and specifies the operand. The
 *      operand is either a 128-bit XMM/256-bit YMM register or a memory address.
 *      If it is a memory address, the address is computed from a segment
 *      register and any of the following values: a base register, an
 *      index register, a scaling factor, and a displacement.
 * X  - Memory addressed by the DS:rSI register pair.
 * Y  - Memory addressed by the ES:rDI register pair.
 */

/*
 * Codes for Operand Type:
 * ----------------------
 * a  - Two one-word operands in memory or two double-word operands in
 *      memory, depending on operand-size attribute (used only by the BOUND
 *      instruction).
 * b  - Byte, regardless of operand-size attribute.
 * d  - Doubleword, regardless of operand-size attribute.
 * dq - Double-quadword, regardless of operand-size attribute.
 * p  - 32-bit or 48-bit pointer, depending on operand-size attribute.
 * pd - 128-bit/256-bit packed double-precision floating-point data.
 * pi - Quadword MMX technology register (packed integer)
 * ps - 128-bit/256-bit packed single-precision floating-point data.
 * q  - Quadword, regardless of operand-size attribute.
 * s  - 6-byte or 10-byte pseudo-descriptor.
 * si - Doubleword integer register (scalar integer)
 * ss - Scalar element of a packed single-precision floating data.
 * sd - Scalar element of a packed double-precision floating data.
 * v  - Word, doubleword or quadword, depending on operand-size attribute.
 * w  - Word, regardless of operand-size attr.
 * y  - Doubleword or quadword (in 64-bit mode) depending on 32/64 bit
 *      operand size.
 */

  // far call/jmp
  void Apw(const x86_insn *insn);
  void Apd(const x86_insn *insn);

  // 8-bit general purpose registers
  void AL_Reg(const x86_insn *insn);
  void CL_Reg(const x86_insn *insn);

  // 16-bit general purpose registers
  void AX_Reg(const x86_insn *insn);
  void DX_Reg(const x86_insn *insn);

  // 32-bit general purpose registers
  void EAX_Reg(const x86_insn *insn);

  // 64-bit general purpose registers
  void RAX_Reg(const x86_insn *insn);
  void RCX_Reg(const x86_insn *insn);

  // segment registers
  void CS(const x86_insn *insn);
  void DS(const x86_insn *insn);
  void ES(const x86_insn *insn);
  void SS(const x86_insn *insn);
  void FS(const x86_insn *insn);
  void GS(const x86_insn *insn);

  // segment registers
  void Sw(const x86_insn *insn);

  // control register
  void Cd(const x86_insn *insn);
  void Cq(const x86_insn *insn);

  // debug register
  void Dd(const x86_insn *insn);
  void Dq(const x86_insn *insn);

  //  8-bit general purpose register
  void Reg8(const x86_insn *insn);

  // 16-bit general purpose register
  void RX(const x86_insn *insn);

  // 32-bit general purpose register
  void ERX(const x86_insn *insn);

  // 64-bit general purpose register
  void RRX(const x86_insn *insn);

  // general purpose register or memory operand
  void Eb(const x86_insn *insn);
  void Ew(const x86_insn *insn);
  void Ed(const x86_insn *insn);
  void Eq(const x86_insn *insn);
  void Ey(const x86_insn *insn);
  void Ebd(const x86_insn *insn);
  void Ewd(const x86_insn *insn);
  void Edq(const x86_insn *insn);

  // general purpose register
  void Gb(const x86_insn *insn);
  void Gw(const x86_insn *insn);
  void Gd(const x86_insn *insn);
  void Gq(const x86_insn *insn);
  void Gy(const x86_insn *insn);

  // vex encoded general purpose register
  void By(const x86_insn *insn);

  // immediate
  void I1(const x86_insn *insn);
  void Ib(const x86_insn *insn);
  void Iw(const x86_insn *insn);
  void Id(const x86_insn *insn);
  void Iq(const x86_insn *insn);

  // double immediate
  void IbIb(const x86_insn *insn);
  void IwIb(const x86_insn *insn);

  // sign extended immediate
  void sIbw(const x86_insn *insn);
  void sIbd(const x86_insn *insn);
  void sIbq(const x86_insn *insn);
  void sIdq(const x86_insn *insn);

  // floating point
  void ST0(const x86_insn *insn);
  void STi(const x86_insn *insn);

  // general purpose register
  void Rw(const x86_insn *insn);
  void Rd(const x86_insn *insn);
  void Rq(const x86_insn *insn);
  void Ry(const x86_insn *insn);

  // mmx register
  void Pq(const x86_insn *insn);

  // mmx register or memory operand
  void Qd(const x86_insn *insn);
  void Qq(const x86_insn *insn);
  void Vq(const x86_insn *insn);
  void Nq(const x86_insn *insn);

  // xmm/ymm register
  void Ups(const x86_insn *insn);
  void Upd(const x86_insn *insn);
  void Udq(const x86_insn *insn);
  void Uq(const x86_insn *insn);

  void Vdq(const x86_insn *insn);
  void Vss(const x86_insn *insn);
  void Vsd(const x86_insn *insn);
  void Vps(const x86_insn *insn);
  void Vpd(const x86_insn *insn);
  // xmm/ymm register through imm byte
  void VIb(const x86_insn *insn);

  // xmm/ymm register or memory operand
  void Wb(const x86_insn *insn);
  void Ww(const x86_insn *insn);
  void Wd(const x86_insn *insn);
  void Wq(const x86_insn *insn);

  void Wdq(const x86_insn *insn);
  void Wss(const x86_insn *insn);
  void Wsd(const x86_insn *insn);
  void Wps(const x86_insn *insn);
  void Wpd(const x86_insn *insn);

  // vex encoded xmm/ymm register
  void Hdq(const x86_insn *insn);
  void Hps(const x86_insn *insn);
  void Hpd(const x86_insn *insn);
  void Hss(const x86_insn *insn);
  void Hsd(const x86_insn *insn);

  // direct memory access
  void OP_O(const x86_insn *insn, unsigned size);
  void Ob(const x86_insn *insn);
  void Ow(const x86_insn *insn);
  void Od(const x86_insn *insn);
  void Oq(const x86_insn *insn);

  // memory operand
  void OP_M(const x86_insn *insn, unsigned size);
  void Ma(const x86_insn *insn);
  void Mp(const x86_insn *insn);
  void Ms(const x86_insn *insn);
  void Mx(const x86_insn *insn);
  void Mb(const x86_insn *insn);
  void Mw(const x86_insn *insn);
  void Md(const x86_insn *insn);
  void Mq(const x86_insn *insn);
  void Mt(const x86_insn *insn);
  void Mdq(const x86_insn *insn);
  void Mps(const x86_insn *insn);
  void Mpd(const x86_insn *insn);
  void Mss(const x86_insn *insn);
  void Msd(const x86_insn *insn);

  // gather VSib
  void VSib(const x86_insn *insn);

  // string instructions
  void OP_X(const x86_insn *insn, unsigned size);
  void Xb(const x86_insn *insn);
  void Xw(const x86_insn *insn);
  void Xd(const x86_insn *insn);
  void Xq(const x86_insn *insn);

  // string instructions
  void OP_Y(const x86_insn *insn, unsigned size);
  void Yb(const x86_insn *insn);
  void Yw(const x86_insn *insn);
  void Yd(const x86_insn *insn);
  void Yq(const x86_insn *insn);

  // maskmovdq/maskmovdqu
  void OP_sY(const x86_insn *insn, unsigned size);
  void sYq(const x86_insn *insn);
  void sYdq(const x86_insn *insn);

  // jump offset
  void Jb(const x86_insn *insn);
  void Jw(const x86_insn *insn);
  void Jd(const x86_insn *insn);
};

#endif
