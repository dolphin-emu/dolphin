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

// will be used in future
#define IA_286        0x00000001        /* 286+ instruction */
#define IA_386        0x00000002        /* 386+ instruction */
#define IA_486        0x00000004        /* 486+ instruction */
#define IA_PENTIUM    0x00000008        /* Pentium+ instruction */
#define IA_P6         0x00000010        /* P6 new instruction */
#define IA_SYSTEM     0x00000020        /* system instruction (require CPL=0) */
#define IA_LEGACY     0x00000040        /* legacy instruction */
#define IA_X87        0x00000080        /* FPU (X87) instruction */
#define IA_MMX        0x00000100        /* MMX instruction */
#define IA_3DNOW      0x00000200        /* 3DNow! instruction */
#define IA_PREFETCH   0x00000400        /* Prefetch instruction */
#define IA_SSE        0x00000800        /* SSE  instruction */
#define IA_SSE2       0x00001000        /* SSE2 instruction */
#define IA_SSE3       0x00002000        /* SSE3 instruction */
#define IA_SSE4       0x00004000        /* SSE4 instruction */
#define IA_X86_64     0x00008000        /* x86-64 instruction */

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
};

struct BxDisasmOpcodeTable_t
{
    Bit32u Attr;
    const void *OpcodeInfo;
};

// segment override not used
#define NO_SEG_OVERRIDE 0xFF

// datasize attributes
#define X_SIZE      0x0000
#define B_SIZE      0x0100
#define W_SIZE      0x0200
#define D_SIZE      0x0300
#define Q_SIZE      0x0400
#define Z_SIZE      0x0500
#define V_SIZE      0x0600
#define O_SIZE      0x0700
#define T_SIZE      0x0800
#define P_SIZE      0x0900

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
  unsigned b1, prefixes;
  unsigned ilen;

  Bit8u modrm, mod, nnn, rm;
  Bit8u sib, scale, index, base;
  union {
     Bit16u displ16;
     Bit32u displ32;
  } displacement;
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
  prefixes = 0;
  ilen = 0;
  b1 = 0;

  modrm = mod = nnn = rm = 0;
  sib = scale = index = base = 0;
  displacement.displ32 = 0;
}

class disassembler {
public:
  disassembler() { set_syntax_intel(); }

  unsigned disasm(bx_bool is_32, bx_bool is_64, bx_address base, bx_address ip, const Bit8u *instr, char *disbuf);

  unsigned disasm16(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(0, 0, base, ip, instr, disbuf); }

  unsigned disasm32(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(1, 0, base, ip, instr, disbuf); }

  unsigned disasm64(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return disasm(1, 1, base, ip, instr, disbuf); }

  x86_insn decode(bx_bool is_32, bx_bool is_64, bx_address base, bx_address ip, const Bit8u *instr, char *disbuf);

  x86_insn decode16(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(0, 0, base, ip, instr, disbuf); }

  x86_insn decode32(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(1, 0, base, ip, instr, disbuf); }

  x86_insn decode64(bx_address base, bx_address ip, const Bit8u *instr, char *disbuf)
    { return decode(1, 1, base, ip, instr, disbuf); }

  void set_syntax_intel();
  void set_syntax_att  ();

  void toggle_syntax_mode();

private:
  bx_bool intel_mode;

  const char **general_16bit_regname;
  const char **general_8bit_regname;
  const char **general_32bit_regname;
  const char **general_8bit_regname_rex;
  const char **general_64bit_regname;

  const char **segment_name;
  const char **index16;

  const char *sreg_mod01or10_rm32[8];
  const char *sreg_mod00_base32[8];
  const char *sreg_mod01or10_base32[8];
  const char *sreg_mod00_rm16[8];
  const char *sreg_mod01or10_rm16[8];

private:

  bx_address db_eip, db_base;

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

  void print_datasize (unsigned mode);

  void print_memory_access16(int datasize,
          const char *seg, const char *index, Bit16u disp);
  void print_memory_access  (int datasize,
          const char *seg, const char *base, const char *index, int scale, Bit32u disp);

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
 *      If it is a memory address, the address is computed from a segment 
 *      register and any of the following values: a base register, an
 *      index register, a scaling factor, a displacement.
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
 * U  - The R/M field of the ModR/M byte selects a 128-bit XMM register.
 * T  - The reg field of the ModR/M byte selects a test register.
 * V  - The reg field of the ModR/M byte selects a 128-bit XMM register.
 * W  - A ModR/M byte follows the opcode and specifies the operand. The 
 *      operand is either a 128-bit XMM register or a memory address. If 
 *      it is a memory address, the address is computed from a segment 
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
 * pd - 128-bit packed double-precision floating-point data.
 * pi - Quadword MMX technology register (packed integer)
 * ps - 128-bit packed single-precision floating-point data.
 * q  - Quadword, regardless of operand-size attribute.
 * s  - 6-byte or 10-byte pseudo-descriptor.
 * si - Doubleword integer register (scalar integer)
 * ss - Scalar element of a 128-bit packed single-precision floating data.
 * sd - Scalar element of a 128-bit packed double-precision floating data.
 * v  - Word, doubleword or quadword, depending on operand-size attribute.
 * w  - Word, regardless of operand-size attr.
 */

  // far call/jmp
  void Apw(const x86_insn *insn);
  void Apd(const x86_insn *insn);

  // 8-bit general purpose registers
  void AL(const x86_insn *insn);
  void CL(const x86_insn *insn);

  // 16-bit general purpose registers
  void AX(const x86_insn *insn);
  void DX(const x86_insn *insn);

  // 32-bit general purpose registers
  void EAX(const x86_insn *insn);

  // 64-bit general purpose registers
  void RAX(const x86_insn *insn);

  // segment registers
  void CS(const x86_insn *insn);
  void DS(const x86_insn *insn);
  void ES(const x86_insn *insn);
  void SS(const x86_insn *insn);
  void FS(const x86_insn *insn);
  void GS(const x86_insn *insn);

  // segment registers
  void Sw(const x86_insn *insn);

  // test registers
  void Td(const x86_insn *insn);

  // control register
  void Cd(const x86_insn *insn);
  void Cq(const x86_insn *insn);

  // debug register
  void Dd(const x86_insn *insn);
  void Dq(const x86_insn *insn);

  //  8-bit general purpose register
  void R8(const x86_insn *insn);

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

  // general purpose register
  void Gb(const x86_insn *insn);
  void Gw(const x86_insn *insn);
  void Gd(const x86_insn *insn);
  void Gq(const x86_insn *insn);

  // immediate
  void I1(const x86_insn *insn);
  void Ib(const x86_insn *insn);
  void Iw(const x86_insn *insn);
  void Id(const x86_insn *insn);
  void Iq(const x86_insn *insn);

  // two immediates Iw/Ib
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

  // mmx register
  void Pq(const x86_insn *insn);

  // mmx register or memory operand
  void Qd(const x86_insn *insn);
  void Qq(const x86_insn *insn);
  void Vq(const x86_insn *insn);
  void Nq(const x86_insn *insn);

  // xmm register
  void Udq(const x86_insn *insn);
  void Vdq(const x86_insn *insn);
  void Vss(const x86_insn *insn);
  void Vsd(const x86_insn *insn);
  void Vps(const x86_insn *insn);
  void Vpd(const x86_insn *insn);

  // xmm register or memory operand
  void Wq(const x86_insn *insn);
  void Wdq(const x86_insn *insn);
  void Wss(const x86_insn *insn);
  void Wsd(const x86_insn *insn);
  void Wps(const x86_insn *insn);
  void Wpd(const x86_insn *insn);

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

  // jump offset
  void Jb(const x86_insn *insn);
  void Jw(const x86_insn *insn);
  void Jd(const x86_insn *insn);
};

#endif
