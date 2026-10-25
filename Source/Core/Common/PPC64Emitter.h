// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Emitter for 64-bit little-endian PowerPC hosts (Power ISA 2.07, i.e. POWER8, and later), using
// the ELFv2 ABI.

#pragma once

#include <cstring>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/CodeBlock.h"
#include "Common/CommonTypes.h"

namespace PPC64Gen
{
// General purpose registers. In the ELFv2 ABI, R1 is the stack pointer, R2 the TOC pointer and
// R13 the thread pointer, so none of them may be used for anything else. R12 holds the address of
// a called function, for its global entry point.
enum class GPR : u8
{
  R0,
  R1,
  R2,
  R3,
  R4,
  R5,
  R6,
  R7,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  R16,
  R17,
  R18,
  R19,
  R20,
  R21,
  R22,
  R23,
  R24,
  R25,
  R26,
  R27,
  R28,
  R29,
  R30,
  R31,

  SP = R1,
  TOC = R2,
};

// Floating-point registers. These are VSX registers VS0-VS31.
enum class FPR : u8
{
  F0,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  F16,
  F17,
  F18,
  F19,
  F20,
  F21,
  F22,
  F23,
  F24,
  F25,
  F26,
  F27,
  F28,
  F29,
  F30,
  F31,
};

// Vector (VMX) registers. These are VSX registers VS32-VS63.
enum class VR : u8
{
  V0,
  V1,
  V2,
  V3,
  V4,
  V5,
  V6,
  V7,
  V8,
  V9,
  V10,
  V11,
  V12,
  V13,
  V14,
  V15,
  V16,
  V17,
  V18,
  V19,
  V20,
  V21,
  V22,
  V23,
  V24,
  V25,
  V26,
  V27,
  V28,
  V29,
  V30,
  V31,
};

// VSX registers: VS0-VS31 overlay the floating-point registers (in doubleword 0), and VS32-VS63
// are the vector registers.
enum class VSR : u8
{
  VS0,
  VS1,
  VS2,
  VS3,
  VS4,
  VS5,
  VS6,
  VS7,
  VS8,
  VS9,
  VS10,
  VS11,
  VS12,
  VS13,
  VS14,
  VS15,
  VS16,
  VS17,
  VS18,
  VS19,
  VS20,
  VS21,
  VS22,
  VS23,
  VS24,
  VS25,
  VS26,
  VS27,
  VS28,
  VS29,
  VS30,
  VS31,
  VS32,
  VS33,
  VS34,
  VS35,
  VS36,
  VS37,
  VS38,
  VS39,
  VS40,
  VS41,
  VS42,
  VS43,
  VS44,
  VS45,
  VS46,
  VS47,
  VS48,
  VS49,
  VS50,
  VS51,
  VS52,
  VS53,
  VS54,
  VS55,
  VS56,
  VS57,
  VS58,
  VS59,
  VS60,
  VS61,
  VS62,
  VS63,
};

// Condition register fields.
enum class CRF : u8
{
  CR0,
  CR1,
  CR2,
  CR3,
  CR4,
  CR5,
  CR6,
  CR7,
};

constexpr u32 DecodeReg(GPR reg)
{
  return static_cast<u32>(reg);
}
constexpr u32 DecodeReg(FPR reg)
{
  return static_cast<u32>(reg);
}
constexpr u32 DecodeReg(VR reg)
{
  return static_cast<u32>(reg);
}
constexpr u32 DecodeReg(VSR reg)
{
  return static_cast<u32>(reg);
}
constexpr u32 DecodeReg(CRF reg)
{
  return static_cast<u32>(reg);
}

constexpr VSR ToVSR(FPR reg)
{
  return static_cast<VSR>(DecodeReg(reg));
}
constexpr VSR ToVSR(VR reg)
{
  return static_cast<VSR>(32 + DecodeReg(reg));
}

// Conditions on a condition register field. The first four test a bit for being set, the others
// for being clear.
enum class CCFlags : u8
{
  LT,  // Less than (negative)
  GT,  // Greater than (positive)
  EQ,  // Equal (zero)
  SO,  // Summary overflow (unordered, after a floating-point comparison)
  GE,  // Not less than
  LE,  // Not greater than
  NE,  // Not equal
  NS,  // Not summary overflow (ordered)
};

constexpr CCFlags Invert(CCFlags cond)
{
  return static_cast<CCFlags>(static_cast<u8>(cond) ^ 4);
}

// The bit of the condition register that a condition on a field tests, as the BI field of a
// branch or condition register logical instruction.
constexpr u32 CRBit(CRF field, CCFlags cond)
{
  return DecodeReg(field) * 4 + (static_cast<u32>(cond) & 3);
}

enum class SPR : u32
{
  XER = 1,
  LR = 8,
  CTR = 9,
  VRSAVE = 256,
  TB = 268,
};

struct FixupBranch
{
  enum class Type : u8
  {
    // I-form: +/- 32 MiB
    B,
    BL,
    // B-form: +/- 32 KiB
    BC,
  };

  u8* ptr;
  Type type;
  // Used with BC
  u8 bo;
  u8 bi;
};

// The minimum ELFv2 stack frame: back chain, CR save word, LR save doubleword and TOC save
// doubleword. A frame needs no parameter save area unless it calls a function with more than
// eight parameters, or with a variable number of them.
constexpr s32 STACK_FRAME_MIN_SIZE = 32;
constexpr s32 STACK_FRAME_LR_SAVE = 16;
constexpr s32 STACK_FRAME_TOC_SAVE = 24;
// Leaf functions may use this many bytes below the stack pointer without allocating a frame.
constexpr s32 STACK_RED_ZONE_SIZE = 288;

class PPC64XEmitter
{
private:
  // Pointer to memory where code will be emitted to.
  u8* m_code = nullptr;

  // Pointer past the end of the memory region we're allowed to emit to.
  // Writes that would reach this memory are refused and will set the m_write_failed flag instead.
  u8* m_code_end = nullptr;

  u8* m_last_cache_flush_end = nullptr;

  // Set to true when a write request happens that would write past m_code_end.
  // Must be cleared with SetCodePtr() afterwards.
  bool m_write_failed = false;

  void EncodeIForm(u32 op, s64 offset, bool aa, bool lk);
  void EncodeBForm(u32 op, u32 bo, u32 bi, s64 offset, bool aa, bool lk);
  void EncodeDForm(u32 op, u32 rt, u32 ra, u32 d);
  void EncodeDSForm(u32 op, u32 rt, u32 ra, s32 ds, u32 xo);
  void EncodeXForm(u32 op, u32 rt, u32 ra, u32 rb, u32 xo, bool rc = false);
  void EncodeXOForm(u32 rt, u32 ra, u32 rb, u32 xo, bool rc);
  void EncodeXLForm(u32 bt, u32 ba, u32 bb, u32 xo, bool lk = false);
  void EncodeMForm(u32 op, u32 rs, u32 ra, u32 rb_sh, u32 mb, u32 me, bool rc);
  void EncodeMDForm(u32 rs, u32 ra, u32 sh, u32 mb, u32 xo, bool rc);
  void EncodeAForm(u32 op, u32 frt, u32 fra, u32 frb, u32 frc, u32 xo, bool rc);
  void EncodeXX1Form(u32 t, GPR ra, GPR rb, u32 xo);
  void EncodeXX2Form(u32 t, u32 b, u32 xo);
  void EncodeXX3Form(u32 t, u32 a, u32 b, u32 xo);
  void EncodeSPR(u32 rs, SPR spr, u32 xo);

  [[nodiscard]] FixupBranch WriteFixupBranch(FixupBranch::Type type, u8 bo = 0, u8 bi = 0);

  // Whether MOVI2R may use Power ISA 3.1's prefixed instructions.
  bool m_prefixed_instructions = false;

protected:
  void Write32(u32 value);
  // Writes a prefixed (Power ISA 3.1) instruction, which mustn't cross a 64-byte boundary.
  void AlignForPrefixed();
  void WritePrefixed(u32 prefix, u32 suffix);
  // Loads a constant that would take MOVI2R more than two instructions in fewer, if the code
  // that's being emitted has a way to.
  virtual bool LoadConstantFromPool(GPR rd, u64 imm) { return false; }

public:
  PPC64XEmitter() = default;
  PPC64XEmitter(u8* code, u8* code_end)
      : m_code(code), m_code_end(code_end), m_last_cache_flush_end(code)
  {
  }

  virtual ~PPC64XEmitter() = default;

  void SetCodePtr(u8* ptr, u8* end, bool write_failed = false);

  void SetCodePtrUnsafe(u8* ptr, u8* end, bool write_failed = false);
  const u8* GetCodePtr() const { return m_code; }
  u8* GetWritableCodePtr() { return m_code; }
  const u8* GetCodeEnd() const { return m_code_end; }
  u8* GetWritableCodeEnd() { return m_code_end; }
  void ReserveCodeSpace(u32 bytes);
  u8* AlignCode16();
  u8* AlignCodePage();
  void FlushIcache();
  void FlushIcacheSection(u8* start, u8* end);

  // Lets MOVI2R use Power ISA 3.1's prefixed instructions, which the host must support.
  void SetPrefixedInstructionsEnabled(bool enabled) { m_prefixed_instructions = enabled; }
  bool ArePrefixedInstructionsEnabled() const { return m_prefixed_instructions; }

  // Should be checked after a block of code has been generated to see if the code has been
  // successfully written to memory. Do not call the generated code when this returns true!
  bool HasWriteFailed() const { return m_write_failed; }

  // Whether an unconditional (I-form) or conditional (B-form) branch from the current code
  // pointer can reach the target.
  bool IsInBranchRange(const void* target) const;
  bool IsInConditionalBranchRange(const void* target) const;

  // FixupBranch branching
  void SetJumpTarget(const FixupBranch& branch);
  void SetJumpTarget(const FixupBranch& branch, const u8* target);
  [[nodiscard]] FixupBranch B();
  [[nodiscard]] FixupBranch BL();
  [[nodiscard]] FixupBranch BC(CCFlags cond, CRF field = CRF::CR0);
  [[nodiscard]] FixupBranch BDNZ();

  // Branches to a known target.
  void B(const void* target);
  void BL(const void* target);
  void BC(CCFlags cond, CRF field, const void* target);
  void BCLR(CCFlags cond, CRF field);
  void BLR();
  // bcl 20,31,$+4: LR = the address of the next instruction. Processors don't treat this form as a
  // call, which would upset their prediction of returns.
  void BCL_NEXT();
  void BCTR();
  void BCTRL();

  // Integer arithmetic
  void ADDI(GPR rt, GPR ra, s16 imm);
  void ADDIS(GPR rt, GPR ra, s16 imm);
  void ADDIC(GPR rt, GPR ra, s16 imm, bool rc = false);
  void SUBFIC(GPR rt, GPR ra, s16 imm);
  void MULLI(GPR rt, GPR ra, s16 imm);
  void ADD(GPR rt, GPR ra, GPR rb, bool rc = false);
  void ADDC(GPR rt, GPR ra, GPR rb, bool rc = false);
  void ADDE(GPR rt, GPR ra, GPR rb, bool rc = false);
  void ADDZE(GPR rt, GPR ra, bool rc = false);
  void ADDME(GPR rt, GPR ra, bool rc = false);
  // rt = rb - ra
  void SUBF(GPR rt, GPR ra, GPR rb, bool rc = false);
  void SUBFC(GPR rt, GPR ra, GPR rb, bool rc = false);
  void SUBFE(GPR rt, GPR ra, GPR rb, bool rc = false);
  void SUBFZE(GPR rt, GPR ra, bool rc = false);
  void SUBFME(GPR rt, GPR ra, bool rc = false);
  void NEG(GPR rt, GPR ra, bool rc = false);
  void MULLW(GPR rt, GPR ra, GPR rb, bool rc = false);
  void MULLD(GPR rt, GPR ra, GPR rb, bool rc = false);
  void MULHW(GPR rt, GPR ra, GPR rb, bool rc = false);
  void MULHWU(GPR rt, GPR ra, GPR rb, bool rc = false);
  void MULHD(GPR rt, GPR ra, GPR rb, bool rc = false);
  void MULHDU(GPR rt, GPR ra, GPR rb, bool rc = false);
  void DIVW(GPR rt, GPR ra, GPR rb, bool rc = false);
  void DIVWU(GPR rt, GPR ra, GPR rb, bool rc = false);
  void DIVD(GPR rt, GPR ra, GPR rb, bool rc = false);
  void DIVDU(GPR rt, GPR ra, GPR rb, bool rc = false);

  // Integer comparison. W compares the low 32 bits, D all 64.
  void CMPWI(CRF bf, GPR ra, s16 imm);
  void CMPDI(CRF bf, GPR ra, s16 imm);
  void CMPLWI(CRF bf, GPR ra, u16 imm);
  void CMPLDI(CRF bf, GPR ra, u16 imm);
  void CMPW(CRF bf, GPR ra, GPR rb);
  void CMPD(CRF bf, GPR ra, GPR rb);
  void CMPLW(CRF bf, GPR ra, GPR rb);
  void CMPLD(CRF bf, GPR ra, GPR rb);
  // rt = (CR bit bc set) ? ra : rb. ra = R0 means 0.
  void ISEL(GPR rt, GPR ra, GPR rb, u32 bc);

  // Logical
  void ANDI_(GPR ra, GPR rs, u16 imm);
  void ANDIS_(GPR ra, GPR rs, u16 imm);
  void ORI(GPR ra, GPR rs, u16 imm);
  void ORIS(GPR ra, GPR rs, u16 imm);
  void XORI(GPR ra, GPR rs, u16 imm);
  void XORIS(GPR ra, GPR rs, u16 imm);
  void AND(GPR ra, GPR rs, GPR rb, bool rc = false);
  void ANDC(GPR ra, GPR rs, GPR rb, bool rc = false);
  void OR(GPR ra, GPR rs, GPR rb, bool rc = false);
  void ORC(GPR ra, GPR rs, GPR rb, bool rc = false);
  void XOR(GPR ra, GPR rs, GPR rb, bool rc = false);
  void NAND(GPR ra, GPR rs, GPR rb, bool rc = false);
  void NOR(GPR ra, GPR rs, GPR rb, bool rc = false);
  void EQV(GPR ra, GPR rs, GPR rb, bool rc = false);
  void EXTSB(GPR ra, GPR rs, bool rc = false);
  void EXTSH(GPR ra, GPR rs, bool rc = false);
  void EXTSW(GPR ra, GPR rs, bool rc = false);
  void CNTLZW(GPR ra, GPR rs, bool rc = false);
  void CNTLZD(GPR ra, GPR rs, bool rc = false);
  void POPCNTW(GPR ra, GPR rs);
  void POPCNTD(GPR ra, GPR rs);
  // Power ISA 3.0 (POWER9): count trailing zeros.
  void CNTTZW(GPR ra, GPR rs, bool rc = false);
  void CNTTZD(GPR ra, GPR rs, bool rc = false);
  // Power ISA 3.1 (Power10): byte-reverse each halfword, word or doubleword.
  void BRH(GPR ra, GPR rs);
  void BRW(GPR ra, GPR rs);
  void BRD(GPR ra, GPR rs);

  // Shifts and rotates
  void SLW(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRW(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRAW(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRAWI(GPR ra, GPR rs, u32 sh, bool rc = false);
  void SLD(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRD(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRAD(GPR ra, GPR rs, GPR rb, bool rc = false);
  void SRADI(GPR ra, GPR rs, u32 sh, bool rc = false);
  void RLWINM(GPR ra, GPR rs, u32 sh, u32 mb, u32 me, bool rc = false);
  void RLWNM(GPR ra, GPR rs, GPR rb, u32 mb, u32 me, bool rc = false);
  void RLWIMI(GPR ra, GPR rs, u32 sh, u32 mb, u32 me, bool rc = false);
  void RLDICL(GPR ra, GPR rs, u32 sh, u32 mb, bool rc = false);
  void RLDICR(GPR ra, GPR rs, u32 sh, u32 me, bool rc = false);
  void RLDIC(GPR ra, GPR rs, u32 sh, u32 mb, bool rc = false);
  void RLDIMI(GPR ra, GPR rs, u32 sh, u32 mb, bool rc = false);
  void RLDCL(GPR ra, GPR rs, GPR rb, u32 mb, bool rc = false);

  // Loads. The D-form offset of LD, LWA and STD must be a multiple of 4.
  void LBZ(GPR rt, GPR ra, s16 offset);
  void LHZ(GPR rt, GPR ra, s16 offset);
  void LHA(GPR rt, GPR ra, s16 offset);
  void LWZ(GPR rt, GPR ra, s16 offset);
  void LWA(GPR rt, GPR ra, s16 offset);
  void LD(GPR rt, GPR ra, s16 offset);
  void LBZX(GPR rt, GPR ra, GPR rb);
  void LHZX(GPR rt, GPR ra, GPR rb);
  void LHAX(GPR rt, GPR ra, GPR rb);
  void LWZX(GPR rt, GPR ra, GPR rb);
  void LWAX(GPR rt, GPR ra, GPR rb);
  void LDX(GPR rt, GPR ra, GPR rb);
  // Byte-reversed loads, i.e. big-endian loads on a little-endian host.
  void LHBRX(GPR rt, GPR ra, GPR rb);
  void LWBRX(GPR rt, GPR ra, GPR rb);
  void LDBRX(GPR rt, GPR ra, GPR rb);

  // Stores
  void STB(GPR rs, GPR ra, s16 offset);
  void STH(GPR rs, GPR ra, s16 offset);
  void STW(GPR rs, GPR ra, s16 offset);
  void STD(GPR rs, GPR ra, s16 offset);
  void STDU(GPR rs, GPR ra, s16 offset);
  void STBX(GPR rs, GPR ra, GPR rb);
  void STHX(GPR rs, GPR ra, GPR rb);
  void STWX(GPR rs, GPR ra, GPR rb);
  void STDX(GPR rs, GPR ra, GPR rb);
  void STHBRX(GPR rs, GPR ra, GPR rb);
  void STWBRX(GPR rs, GPR ra, GPR rb);
  void STDBRX(GPR rs, GPR ra, GPR rb);

  // Condition register
  void CRAND(u32 bt, u32 ba, u32 bb);
  void CROR(u32 bt, u32 ba, u32 bb);
  void CRXOR(u32 bt, u32 ba, u32 bb);
  void CRNAND(u32 bt, u32 ba, u32 bb);
  void CRNOR(u32 bt, u32 ba, u32 bb);
  void CREQV(u32 bt, u32 ba, u32 bb);
  void CRANDC(u32 bt, u32 ba, u32 bb);
  void CRORC(u32 bt, u32 ba, u32 bb);
  void MCRF(CRF bf, CRF bfa);
  // Power ISA 3.0 (POWER9): rt = -1 if field bfa has LT set, else 1 if GT, else 0.
  void SETB(GPR rt, CRF bfa);
  // Power ISA 3.0 (POWER9): rt = the address of the next instruction + (d << 16).
  void ADDPCIS(GPR rt, s16 d);
  // Power ISA 3.1 (Power10): rt = CR bit bi (SETBC), its complement (SETBCR), or their negations
  // (SETNBC: -1 if set, SETNBCR: -1 if clear).
  void SETBC(GPR rt, u32 bi);
  void SETBCR(GPR rt, u32 bi);
  void SETNBC(GPR rt, u32 bi);
  void SETNBCR(GPR rt, u32 bi);
  void MFCR(GPR rt);
  void MFOCRF(GPR rt, CRF field);
  // Sets the fields of the condition register selected by the bits of mask (bit 7 = CR0).
  void MTCRF(u32 mask, GPR rs);
  void MTOCRF(CRF field, GPR rs);

  // Special purpose registers
  void MFSPR(GPR rt, SPR spr);
  void MTSPR(SPR spr, GPR rs);
  void MFLR(GPR rt) { MFSPR(rt, SPR::LR); }
  void MTLR(GPR rs) { MTSPR(SPR::LR, rs); }
  void MFCTR(GPR rt) { MFSPR(rt, SPR::CTR); }
  void MTCTR(GPR rs) { MTSPR(SPR::CTR, rs); }
  void MFXER(GPR rt) { MFSPR(rt, SPR::XER); }
  void MTXER(GPR rs) { MTSPR(SPR::XER, rs); }

  // Storage control and synchronisation
  void SYNC();
  void LWSYNC();
  void ISYNC();
  void ICBI(GPR ra, GPR rb);
  void DCBST(GPR ra, GPR rb);
  // Traps unconditionally.
  void TRAP();
  void NOP();

  // Floating-point loads and stores
  void LFS(FPR frt, GPR ra, s16 offset);
  void LFD(FPR frt, GPR ra, s16 offset);
  void LFSX(FPR frt, GPR ra, GPR rb);
  void LFDX(FPR frt, GPR ra, GPR rb);
  void STFS(FPR frs, GPR ra, s16 offset);
  void STFD(FPR frs, GPR ra, s16 offset);
  void STFSX(FPR frs, GPR ra, GPR rb);
  void STFDX(FPR frs, GPR ra, GPR rb);
  void STFIWX(FPR frs, GPR ra, GPR rb);

  // Floating-point arithmetic. The S forms round the result to single precision.
  void FMR(FPR frt, FPR frb, bool rc = false);
  void FNEG(FPR frt, FPR frb, bool rc = false);
  void FABS(FPR frt, FPR frb, bool rc = false);
  void FNABS(FPR frt, FPR frb, bool rc = false);
  void FCPSGN(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FADD(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FADDS(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FSUB(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FSUBS(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FMUL(FPR frt, FPR fra, FPR frc, bool rc = false);
  void FMULS(FPR frt, FPR fra, FPR frc, bool rc = false);
  void FDIV(FPR frt, FPR fra, FPR frb, bool rc = false);
  void FDIVS(FPR frt, FPR fra, FPR frb, bool rc = false);
  // frt = fra * frc + frb, and so on.
  void FMADD(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FMADDS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FMSUB(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FMSUBS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FNMADD(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FNMADDS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FNMSUB(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FNMSUBS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FSQRT(FPR frt, FPR frb, bool rc = false);
  void FSQRTS(FPR frt, FPR frb, bool rc = false);
  void FRES(FPR frt, FPR frb, bool rc = false);
  void FRSQRTE(FPR frt, FPR frb, bool rc = false);
  // frt = (fra >= 0) ? frc : frb
  void FSEL(FPR frt, FPR fra, FPR frc, FPR frb, bool rc = false);
  void FRSP(FPR frt, FPR frb, bool rc = false);
  void FCTIW(FPR frt, FPR frb, bool rc = false);
  void FCTIWZ(FPR frt, FPR frb, bool rc = false);
  void FCTID(FPR frt, FPR frb, bool rc = false);
  void FCTIDZ(FPR frt, FPR frb, bool rc = false);
  void FCFID(FPR frt, FPR frb, bool rc = false);
  void FCMPU(CRF bf, FPR fra, FPR frb);
  void FCMPO(CRF bf, FPR fra, FPR frb);

  // Floating-point status and control register
  void MFFS(FPR frt);
  // Sets the fields of the FPSCR selected by the bits of mask (bit 7 = field 0) from frb.
  void MTFSF(u32 mask, FPR frb);
  void MTFSFI(u32 field, u32 imm);
  // bt numbers the bits of the low word of the FPSCR, as in the ISA (e.g. NI is 29).
  void MTFSB0(u32 bt);
  void MTFSB1(u32 bt);

  // VSX loads and stores. LXVD2X and STXVD2X transfer the doublewords in big-endian element
  // order: on a little-endian host, the doublewords of the register are swapped relative to
  // memory. LXSSPX converts a single to double precision, and STXSSPX back.
  void LXSDX(VSR xt, GPR ra, GPR rb);
  void LXSSPX(VSR xt, GPR ra, GPR rb);
  void LXSIWZX(VSR xt, GPR ra, GPR rb);
  void LXSIWAX(VSR xt, GPR ra, GPR rb);
  void LXVD2X(VSR xt, GPR ra, GPR rb);
  void LXVW4X(VSR xt, GPR ra, GPR rb);
  void LXVDSX(VSR xt, GPR ra, GPR rb);
  void STXSDX(VSR xs, GPR ra, GPR rb);
  void STXSSPX(VSR xs, GPR ra, GPR rb);
  void STXSIWX(VSR xs, GPR ra, GPR rb);
  void STXVD2X(VSR xs, GPR ra, GPR rb);
  void STXVW4X(VSR xs, GPR ra, GPR rb);
  // Power ISA 3.0 (POWER9) loads and stores. LXV, STXV, LXVX and STXVX transfer the register in
  // the host's element order, unlike LXVD2X and STXVD2X. LXVWSX splats a word. LXSD, LXSSP and
  // their stores use doubleword 0 of a VR (VS32-VS63), and LXSSP converts a single to double
  // precision.
  void LXV(VSR xt, GPR ra, s16 dq);
  void STXV(VSR xs, GPR ra, s16 dq);
  void LXVX(VSR xt, GPR ra, GPR rb);
  void STXVX(VSR xs, GPR ra, GPR rb);
  void LXVWSX(VSR xt, GPR ra, GPR rb);
  void LXSD(VSR vrt, GPR ra, s16 ds);
  void STXSD(VSR vrs, GPR ra, s16 ds);
  void LXSSP(VSR vrt, GPR ra, s16 ds);
  void STXSSP(VSR vrs, GPR ra, s16 ds);

  // Moves between GPRs and doubleword 0 (or word 1) of VSX registers.
  void MTVSRD(VSR xt, GPR ra);
  void MTVSRWZ(VSR xt, GPR ra);
  void MTVSRWA(VSR xt, GPR ra);
  void MFVSRD(GPR ra, VSR xs);
  void MFVSRWZ(GPR ra, VSR xs);
  // Power ISA 3.0 (POWER9): splats a word into all four, sets both doublewords (ra into
  // doubleword 0), and moves doubleword 1 to a GPR.
  void MTVSRWS(VSR xt, GPR ra);
  void MTVSRDD(VSR xt, GPR ra, GPR rb);
  void MFVSRLD(GPR ra, VSR xs);

  // VSX permutes and logical operations. dm selects doubleword (dm >> 1) of xa and doubleword
  // (dm & 1) of xb, numbering doublewords in big-endian order.
  void XXPERMDI(VSR xt, VSR xa, VSR xb, u32 dm);
  void XXSWAPD(VSR xt, VSR xa) { XXPERMDI(xt, xa, xa, 2); }
  void XXMRGHD(VSR xt, VSR xa, VSR xb) { XXPERMDI(xt, xa, xb, 0); }
  void XXMRGLD(VSR xt, VSR xa, VSR xb) { XXPERMDI(xt, xa, xb, 3); }
  void XXSLDWI(VSR xt, VSR xa, VSR xb, u32 shw);
  void XXSPLTW(VSR xt, VSR xb, u32 uim);
  // xt = (xc & xb) | (~xc & xa)
  void XXSEL(VSR xt, VSR xa, VSR xb, VSR xc);
  void XXLAND(VSR xt, VSR xa, VSR xb);
  void XXLANDC(VSR xt, VSR xa, VSR xb);
  void XXLOR(VSR xt, VSR xa, VSR xb);
  void XXLORC(VSR xt, VSR xa, VSR xb);
  void XXLXOR(VSR xt, VSR xa, VSR xb);
  void XXLNOR(VSR xt, VSR xa, VSR xb);
  void XXLNAND(VSR xt, VSR xa, VSR xb);
  void XXLEQV(VSR xt, VSR xa, VSR xb);
  void XXLMR(VSR xt, VSR xa) { XXLOR(xt, xa, xa); }
  // Power ISA 3.0 (POWER9): byte-reverse each halfword, word, doubleword, or the quadword, and
  // splat a byte.
  void XXBRH(VSR xt, VSR xb);
  void XXBRW(VSR xt, VSR xb);
  void XXBRD(VSR xt, VSR xb);
  void XXBRQ(VSR xt, VSR xb);
  void XXSPLTIB(VSR xt, u8 imm);
  // Power ISA 3.1 (Power10, prefixed): splat a word, or a single-precision value converted to
  // double precision, into the register.
  void XXSPLTIW(VSR xt, u32 imm);
  void XXSPLTIDP(VSR xt, u32 single_bits);

  // VSX scalar floating-point. The SP forms round the result to single precision, and leave it
  // in double-precision format.
  void XSADDDP(VSR xt, VSR xa, VSR xb);
  void XSSUBDP(VSR xt, VSR xa, VSR xb);
  void XSMULDP(VSR xt, VSR xa, VSR xb);
  void XSDIVDP(VSR xt, VSR xa, VSR xb);
  void XSADDSP(VSR xt, VSR xa, VSR xb);
  void XSSUBSP(VSR xt, VSR xa, VSR xb);
  void XSMULSP(VSR xt, VSR xa, VSR xb);
  void XSDIVSP(VSR xt, VSR xa, VSR xb);
  // The A forms compute xt = xa * xb + xt (xt is the addend), the M forms xt = xa * xt + xb
  // (xt is the multiplicand).
  void XSMADDADP(VSR xt, VSR xa, VSR xb);
  void XSMADDMDP(VSR xt, VSR xa, VSR xb);
  void XSMSUBADP(VSR xt, VSR xa, VSR xb);
  void XSMSUBMDP(VSR xt, VSR xa, VSR xb);
  void XSNMADDADP(VSR xt, VSR xa, VSR xb);
  void XSNMADDMDP(VSR xt, VSR xa, VSR xb);
  void XSNMSUBADP(VSR xt, VSR xa, VSR xb);
  void XSNMSUBMDP(VSR xt, VSR xa, VSR xb);
  void XSMADDASP(VSR xt, VSR xa, VSR xb);
  void XSMADDMSP(VSR xt, VSR xa, VSR xb);
  void XSMSUBASP(VSR xt, VSR xa, VSR xb);
  void XSMSUBMSP(VSR xt, VSR xa, VSR xb);
  void XSNMADDASP(VSR xt, VSR xa, VSR xb);
  void XSNMADDMSP(VSR xt, VSR xa, VSR xb);
  void XSNMSUBASP(VSR xt, VSR xa, VSR xb);
  void XSNMSUBMSP(VSR xt, VSR xa, VSR xb);
  void XSMAXDP(VSR xt, VSR xa, VSR xb);
  void XSMINDP(VSR xt, VSR xa, VSR xb);
  void XSCPSGNDP(VSR xt, VSR xa, VSR xb);
  void XSCMPUDP(CRF bf, VSR xa, VSR xb);
  void XSCMPODP(CRF bf, VSR xa, VSR xb);
  void XSSQRTDP(VSR xt, VSR xb);
  void XSABSDP(VSR xt, VSR xb);
  void XSNABSDP(VSR xt, VSR xb);
  void XSNEGDP(VSR xt, VSR xb);
  void XSREDP(VSR xt, VSR xb);
  void XSRSQRTEDP(VSR xt, VSR xb);
  // Rounds to single precision, leaving the result in double-precision format.
  void XSRSP(VSR xt, VSR xb);
  // Converts to and from single-precision format in word 0. The N forms don't signal, and don't
  // round.
  void XSCVDPSP(VSR xt, VSR xb);
  void XSCVDPSPN(VSR xt, VSR xb);
  void XSCVSPDP(VSR xt, VSR xb);
  void XSCVSPDPN(VSR xt, VSR xb);
  void XSCVDPSXWS(VSR xt, VSR xb);
  void XSCVDPUXWS(VSR xt, VSR xb);
  void XSCVDPSXDS(VSR xt, VSR xb);
  void XSCVSXDDP(VSR xt, VSR xb);
  void XSCVUXDDP(VSR xt, VSR xb);
  void XSRDPI(VSR xt, VSR xb);
  void XSRDPIZ(VSR xt, VSR xb);
  // Power ISA 3.0 (POWER9). Test data class: field bf gets LT = the sign, EQ = whether xb is of a
  // class that dcmx selects (0x40 NaN, 0x20 +infinity, 0x10 -infinity, 0x08 +zero, 0x04 -zero,
  // 0x02 +denormal, 0x01 -denormal). XVTSTDCSP sets each word of xt to all ones if it is. XSXEXPDP
  // and XSXSIGDP extract the biased exponent and the significand (with the implicit bit).
  void XSTSTDCDP(CRF bf, VSR xb, u32 dcmx);
  void XVTSTDCSP(VSR xt, VSR xb, u32 dcmx);
  void XSXEXPDP(GPR rt, VSR xb);
  void XSXSIGDP(GPR rt, VSR xb);

  // VSX vector floating-point, on both doublewords
  void XVADDDP(VSR xt, VSR xa, VSR xb);
  void XVSUBDP(VSR xt, VSR xa, VSR xb);
  void XVMULDP(VSR xt, VSR xa, VSR xb);
  void XVDIVDP(VSR xt, VSR xa, VSR xb);
  void XVMADDADP(VSR xt, VSR xa, VSR xb);
  void XVMADDMDP(VSR xt, VSR xa, VSR xb);
  void XVMSUBADP(VSR xt, VSR xa, VSR xb);
  void XVMSUBMDP(VSR xt, VSR xa, VSR xb);
  void XVNMADDADP(VSR xt, VSR xa, VSR xb);
  void XVNMADDMDP(VSR xt, VSR xa, VSR xb);
  void XVNMSUBADP(VSR xt, VSR xa, VSR xb);
  void XVNMSUBMDP(VSR xt, VSR xa, VSR xb);
  void XVMAXDP(VSR xt, VSR xa, VSR xb);
  void XVMINDP(VSR xt, VSR xa, VSR xb);
  void XVCPSGNDP(VSR xt, VSR xa, VSR xb);
  void XVCMPEQDP(VSR xt, VSR xa, VSR xb);
  void XVCMPGEDP(VSR xt, VSR xa, VSR xb);
  void XVCMPGTDP(VSR xt, VSR xa, VSR xb);
  void XVSQRTDP(VSR xt, VSR xb);
  void XVABSDP(VSR xt, VSR xb);
  void XVNABSDP(VSR xt, VSR xb);
  void XVNEGDP(VSR xt, VSR xb);
  void XVREDP(VSR xt, VSR xb);
  void XVRSQRTEDP(VSR xt, VSR xb);
  void XVCVDPSP(VSR xt, VSR xb);
  void XVCVSPDP(VSR xt, VSR xb);
  // Converts both signed doublewords to double precision, and both doubles to signed words 0 and
  // 2, saturating, NaNs to 0x80000000.
  void XVCVSXDDP(VSR xt, VSR xb);
  void XVCVDPSXWS(VSR xt, VSR xb);

  // VMX doubleword shift left (by the low 6 bits of each doubleword of vrb) and compare equal, on
  // VRs given as VS32-VS63. With rc, CR6 gets LT if all doublewords are equal, EQ if none are.
  void VSLD(VSR vrt, VSR vra, VSR vrb);
  void VCMPEQUD(VSR vrt, VSR vra, VSR vrb, bool rc = false);
  // VMX signed word maximum and minimum.
  void VMAXSW(VSR vrt, VSR vra, VSR vrb);
  void VMINSW(VSR vrt, VSR vra, VSR vrb);

  // Vector status and control register
  void MFVSCR(VR vrt);
  void MTVSCR(VR vrb);

  // Simplified mnemonics
  void LI(GPR rt, s16 imm) { ADDI(rt, GPR::R0, imm); }
  void LIS(GPR rt, s16 imm) { ADDIS(rt, GPR::R0, imm); }
  void MR(GPR ra, GPR rs, bool rc = false) { OR(ra, rs, rs, rc); }
  void NOT(GPR ra, GPR rs, bool rc = false) { NOR(ra, rs, rs, rc); }
  // rt = ra - rb
  void SUB(GPR rt, GPR ra, GPR rb, bool rc = false) { SUBF(rt, rb, ra, rc); }
  void SUBC(GPR rt, GPR ra, GPR rb, bool rc = false) { SUBFC(rt, rb, ra, rc); }
  void SLWI(GPR ra, GPR rs, u32 n) { RLWINM(ra, rs, n, 0, 31 - n); }
  void SRWI(GPR ra, GPR rs, u32 n) { RLWINM(ra, rs, (32 - n) & 31, n, 31); }
  void ROTLWI(GPR ra, GPR rs, u32 n) { RLWINM(ra, rs, n, 0, 31); }
  void CLRLWI(GPR ra, GPR rs, u32 n) { RLWINM(ra, rs, 0, n, 31); }
  void SLDI(GPR ra, GPR rs, u32 n) { RLDICR(ra, rs, n, 63 - n); }
  void SRDI(GPR ra, GPR rs, u32 n) { RLDICL(ra, rs, (64 - n) & 63, n); }
  void ROTLDI(GPR ra, GPR rs, u32 n) { RLDICL(ra, rs, n, 0); }
  void CLRLDI(GPR ra, GPR rs, u32 n) { RLDICL(ra, rs, 0, n); }
  void CLRRDI(GPR ra, GPR rs, u32 n) { RLDICR(ra, rs, 0, 63 - n); }

  // Loads any 64-bit constant, in up to five instructions, or with prefixed instructions in one
  // from the constant pool, or else up to four after a nop that aligns the first.
  void MOVI2R(GPR rd, u64 imm);
  // Power ISA 3.1 (Power10, prefixed): rd = a signed 34-bit value, rd = an address within 8 GiB
  // of the code, and a doubleword load from such an address.
  void PLI(GPR rd, s64 imm);
  void PLA(GPR rd, const void* target);
  void PLD(GPR rd, const void* target);
  // Whether a value fits in 34 bits, signed, as the immediates and offsets of prefixed
  // instructions do.
  static bool IsInPrefixedRange(s64 value)
  {
    return value >= -(s64{1} << 33) && value < (s64{1} << 33);
  }
  // Whether PC-relative prefixed instructions here can reach the target.
  bool IsInPrefixedPCRange(const void* target) const;
  template <class P>
  void MOVP2R(GPR rd, P* ptr)
  {
    MOVI2R(rd, reinterpret_cast<uintptr_t>(ptr));
  }

  // Calls a function through CTR, with R12 holding its address as its global entry point
  // requires. Clobbers R12, CTR and LR, and the callee may change R2 (the TOC pointer).
  void QuickCallFunction(const void* func);
  template <typename T>
  void QuickCallFunction(T func)
  {
    QuickCallFunction(reinterpret_cast<const void*>(func));
  }

  // Calls a function with up to eight integer or pointer arguments (references are passed as
  // pointers), each either a constant or a GPR. A GPR argument must not be one of the argument
  // registers R3-R10, unless it is already in the register for its position.
  template <typename FuncRet, typename... FuncArgs, typename... Args>
  void ABI_CallFunction(FuncRet (*func)(FuncArgs...), Args... args)
  {
    static_assert(sizeof...(FuncArgs) == sizeof...(Args), "Wrong number of arguments");
    static_assert(sizeof...(Args) <= 8, "Only arguments in registers are supported");

    u32 position = 0;
    (ABI_SetArgument(static_cast<GPR>(DecodeReg(GPR::R3) + position++), args), ...);
    QuickCallFunction(func);
  }

private:
  void ABI_SetArgument(GPR reg, GPR value)
  {
    ASSERT_MSG(DYNA_REC,
               value == reg || DecodeReg(value) < DecodeReg(GPR::R3) ||
                   DecodeReg(value) > DecodeReg(GPR::R10),
               "Argument register r{} would be overwritten", DecodeReg(value));
    if (value != reg)
      MR(reg, value);
  }

  template <typename T>
  void ABI_SetArgument(GPR reg, T value)
  {
    if constexpr (std::is_pointer_v<T>)
    {
      MOVP2R(reg, value);
    }
    else
    {
      static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "Unsupported argument type");
      // Integers are sign- or zero-extended to 64 bits, as the ABI requires.
      using U = std::conditional_t<std::is_enum_v<T>, std::underlying_type<T>,
                                   std::type_identity<T>>::type;
      if constexpr (std::is_signed_v<U>)
        MOVI2R(reg, static_cast<u64>(static_cast<s64>(static_cast<U>(value))));
      else
        MOVI2R(reg, static_cast<u64>(static_cast<U>(value)));
    }
  }
};

class PPC64CodeBlock : public Common::CodeBlock<PPC64XEmitter>
{
private:
  void PoisonMemory() override
  {
    // If our memory isn't a multiple of u32 then this won't write the last remaining bytes with
    // anything
    // Less than optimal, but there would be nothing we could do but throw a runtime warning anyway.
    // 0x7FE00008 = trap
    constexpr u32 trap = 0x7FE00008;

    for (size_t i = 0; i < region_size; i += sizeof(u32))
    {
      std::memcpy(region + i, &trap, sizeof(u32));
    }
  }
};
}  // namespace PPC64Gen
