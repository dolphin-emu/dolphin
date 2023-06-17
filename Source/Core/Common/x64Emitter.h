// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// WARNING - THIS LIBRARY IS NOT THREAD SAFE!!!

#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CodeBlock.h"
#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"

namespace Gen
{
enum CCFlags
{
  CC_O = 0,
  CC_NO = 1,
  CC_B = 2,
  CC_C = 2,
  CC_NAE = 2,
  CC_NB = 3,
  CC_NC = 3,
  CC_AE = 3,
  CC_Z = 4,
  CC_E = 4,
  CC_NZ = 5,
  CC_NE = 5,
  CC_BE = 6,
  CC_NA = 6,
  CC_NBE = 7,
  CC_A = 7,
  CC_S = 8,
  CC_NS = 9,
  CC_P = 0xA,
  CC_PE = 0xA,
  CC_NP = 0xB,
  CC_PO = 0xB,
  CC_L = 0xC,
  CC_NGE = 0xC,
  CC_NL = 0xD,
  CC_GE = 0xD,
  CC_LE = 0xE,
  CC_NG = 0xE,
  CC_NLE = 0xF,
  CC_G = 0xF
};

enum
{
  NUMGPRs = 16,
  NUMXMMs = 16,
};

enum
{
  SCALE_NONE = 0,
  SCALE_1 = 1,
  SCALE_2 = 2,
  SCALE_4 = 4,
  SCALE_8 = 8,
  SCALE_ATREG = 16,
  // SCALE_NOBASE_1 is not supported and can be replaced with SCALE_ATREG
  SCALE_NOBASE_2 = 34,
  SCALE_NOBASE_4 = 36,
  SCALE_NOBASE_8 = 40,
  SCALE_RIP = 0xFF,
  SCALE_IMM8 = 0xF0,
  SCALE_IMM16 = 0xF1,
  SCALE_IMM32 = 0xF2,
  SCALE_IMM64 = 0xF3,
};

enum SSECompare
{
  CMP_EQ = 0,
  CMP_LT = 1,
  CMP_LE = 2,
  CMP_UNORD = 3,
  CMP_NEQ = 4,
  CMP_NLT = 5,
  CMP_NLE = 6,
  CMP_ORD = 7,
};

class XEmitter;
enum class FloatOp;
enum class NormalOp;

// Information about a generated MOV op
struct MovInfo final
{
  u8* address;
  bool nonAtomicSwapStore;
  // valid iff nonAtomicSwapStore is true
  X64Reg nonAtomicSwapStoreSrc;
};

// RIP addressing does not benefit from micro op fusion on Core arch
struct OpArg
{
  // For accessing offset and operandReg.
  // This also allows us to keep the op writing functions private.
  friend class XEmitter;

  // dummy op arg, used for storage
  constexpr OpArg() = default;
  constexpr OpArg(u64 offset_, int scale_, X64Reg rm_reg = RAX, X64Reg scaled_reg = RAX)
      : scale{static_cast<u8>(scale_)}, offsetOrBaseReg{static_cast<u16>(rm_reg)},
        indexReg{static_cast<u16>(scaled_reg)}, offset{offset_}
  {
  }
  constexpr bool operator==(const OpArg& b) const
  {
    return std::tie(scale, offsetOrBaseReg, indexReg, offset, operandReg) ==
           std::tie(b.scale, b.offsetOrBaseReg, b.indexReg, b.offset, b.operandReg);
  }
  constexpr bool operator!=(const OpArg& b) const { return !operator==(b); }
  u64 Imm64() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM64);
    return (u64)offset;
  }
  u32 Imm32() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM32);
    return (u32)offset;
  }
  u16 Imm16() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM16);
    return (u16)offset;
  }
  u8 Imm8() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM8);
    return (u8)offset;
  }

  s64 SImm64() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM64);
    return (s64)offset;
  }
  s32 SImm32() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM32);
    return (s32)offset;
  }
  s16 SImm16() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM16);
    return (s16)offset;
  }
  s8 SImm8() const
  {
    DEBUG_ASSERT(scale == SCALE_IMM8);
    return (s8)offset;
  }

  OpArg AsImm64() const
  {
    DEBUG_ASSERT(IsImm());
    return OpArg((u64)offset, SCALE_IMM64);
  }
  OpArg AsImm32() const
  {
    DEBUG_ASSERT(IsImm());
    return OpArg((u32)offset, SCALE_IMM32);
  }
  OpArg AsImm16() const
  {
    DEBUG_ASSERT(IsImm());
    return OpArg((u16)offset, SCALE_IMM16);
  }
  OpArg AsImm8() const
  {
    DEBUG_ASSERT(IsImm());
    return OpArg((u8)offset, SCALE_IMM8);
  }

  constexpr bool IsImm() const
  {
    return scale == SCALE_IMM8 || scale == SCALE_IMM16 || scale == SCALE_IMM32 ||
           scale == SCALE_IMM64;
  }
  constexpr bool IsSimpleReg() const { return scale == SCALE_NONE; }
  constexpr bool IsSimpleReg(X64Reg reg) const { return IsSimpleReg() && GetSimpleReg() == reg; }
  constexpr bool IsZero() const { return IsImm() && offset == 0; }
  constexpr int GetImmBits() const
  {
    switch (scale)
    {
    case SCALE_IMM8:
      return 8;
    case SCALE_IMM16:
      return 16;
    case SCALE_IMM32:
      return 32;
    case SCALE_IMM64:
      return 64;
    default:
      return -1;
    }
  }

  constexpr X64Reg GetSimpleReg() const
  {
    if (scale == SCALE_NONE)
      return static_cast<X64Reg>(offsetOrBaseReg);

    return INVALID_REG;
  }

  void AddMemOffset(int val)
  {
    DEBUG_ASSERT_MSG(DYNA_REC, scale == SCALE_RIP || (scale <= SCALE_ATREG && scale > SCALE_NONE),
                     "Tried to increment an OpArg which doesn't have an offset");
    offset += val;
  }

private:
  void WriteREX(XEmitter* emit, int opBits, int bits, int customOp = -1) const;
  void WriteVEX(XEmitter* emit, X64Reg regOp1, X64Reg regOp2, int L, int pp, int mmmmm,
                int W = 0) const;
  void WriteRest(XEmitter* emit, int extraBytes = 0, X64Reg operandReg = INVALID_REG,
                 bool warn_64bit_offset = true) const;
  void WriteSingleByteOp(XEmitter* emit, u8 op, X64Reg operandReg, int bits);
  void WriteNormalOp(XEmitter* emit, bool toRM, NormalOp op, const OpArg& operand, int bits) const;

  u8 scale = 0;
  u16 offsetOrBaseReg = 0;
  u16 indexReg = 0;
  u64 offset = 0;  // Also used to store immediates.
  u16 operandReg = 0;
};

template <typename T>
inline OpArg M(const T* ptr)
{
  return OpArg((u64)(const void*)ptr, (int)SCALE_RIP);
}
constexpr OpArg R(X64Reg value)
{
  return OpArg(0, SCALE_NONE, value);
}
constexpr OpArg MatR(X64Reg value)
{
  return OpArg(0, SCALE_ATREG, value);
}

constexpr OpArg MDisp(X64Reg value, int offset)
{
  return OpArg(static_cast<u32>(offset), SCALE_ATREG, value);
}

constexpr OpArg MComplex(X64Reg base, X64Reg scaled, int scale, int offset)
{
  return OpArg(offset, scale, base, scaled);
}

constexpr OpArg MScaled(X64Reg scaled, int scale, int offset)
{
  if (scale == SCALE_1)
    return OpArg(offset, SCALE_ATREG, scaled);

  return OpArg(offset, scale | 0x20, RAX, scaled);
}

constexpr OpArg MRegSum(X64Reg base, X64Reg offset)
{
  return MComplex(base, offset, 1, 0);
}

constexpr OpArg Imm8(u8 imm)
{
  return OpArg(imm, SCALE_IMM8);
}
constexpr OpArg Imm16(u16 imm)
{
  return OpArg(imm, SCALE_IMM16);
}  // rarely used
constexpr OpArg Imm32(u32 imm)
{
  return OpArg(imm, SCALE_IMM32);
}
constexpr OpArg Imm64(u64 imm)
{
  return OpArg(imm, SCALE_IMM64);
}
inline OpArg ImmPtr(const void* imm)
{
  return Imm64(reinterpret_cast<u64>(imm));
}

inline u32 PtrOffset(const void* ptr, const void* base = nullptr)
{
  s64 distance = (s64)ptr - (s64)base;
  if (distance >= 0x80000000LL || distance < -0x80000000LL)
  {
    ASSERT_MSG(DYNA_REC, 0, "pointer offset out of range");
    return 0;
  }

  return (u32)distance;
}

struct FixupBranch
{
  enum class Type
  {
    Branch8Bit,
    Branch32Bit
  };

  u8* ptr;
  Type type;
};

class XEmitter
{
  friend struct OpArg;  // for Write8 etc
private:
  // Pointer to memory where code will be emitted to.
  u8* code = nullptr;

  // Pointer past the end of the memory region we're allowed to emit to.
  // Writes that would reach this memory are refused and will set the m_write_failed flag instead.
  u8* m_code_end = nullptr;

  bool flags_locked = false;

  // Set to true when a write request happens that would write past m_code_end.
  // Must be cleared with SetCodePtr() afterwards.
  bool m_write_failed = false;

  void CheckFlags();

  void Rex(int w, int r, int x, int b);
  void WriteModRM(int mod, int reg, int rm);
  void WriteSIB(int scale, int index, int base);
  void WriteSimple1Byte(int bits, u8 byte, X64Reg reg);
  void WriteSimple2Byte(int bits, u8 byte1, u8 byte2, X64Reg reg);
  void WriteMulDivType(int bits, OpArg src, int ext);
  void WriteBitSearchType(int bits, X64Reg dest, OpArg src, u8 byte2, bool rep = false);
  void WriteShift(int bits, OpArg dest, const OpArg& shift, int ext);
  void WriteBitTest(int bits, const OpArg& dest, const OpArg& index, int ext);
  void WriteMXCSR(OpArg arg, int ext);
  void WriteSSEOp(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int extrabytes = 0);
  void WriteSSSE3Op(u8 opPrefix, u16 op, X64Reg regOp, const OpArg& arg, int extrabytes = 0);
  void WriteSSE41Op(u8 opPrefix, u16 op, X64Reg regOp, const OpArg& arg, int extrabytes = 0);
  void WriteVEXOp(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg, int W = 0,
                  int extrabytes = 0);
  void WriteVEXOp4(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                   X64Reg regOp3, int W = 0);
  void WriteAVXOp(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg, int W = 0,
                  int extrabytes = 0);
  void WriteAVXOp4(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                   X64Reg regOp3, int W = 0);
  void WriteFMA3Op(u8 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg, int W = 0);
  void WriteFMA4Op(u8 op, X64Reg dest, X64Reg regOp1, X64Reg regOp2, const OpArg& arg, int W = 0);
  void WriteBMIOp(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                  int extrabytes = 0);
  void WriteBMI1Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                   int extrabytes = 0);
  void WriteBMI2Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                   int extrabytes = 0);
  void WriteMOVBE(int bits, u8 op, X64Reg regOp, const OpArg& arg);
  void WriteNormalOp(int bits, NormalOp op, const OpArg& a1, const OpArg& a2);

  void ABI_CalculateFrameSize(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size,
                              size_t* shadowp, size_t* subtractionp, size_t* xmm_offsetp);

protected:
  void Write8(u8 value);
  void Write16(u16 value);
  void Write32(u32 value);
  void Write64(u64 value);

public:
  XEmitter() = default;
  explicit XEmitter(u8* code_ptr, u8* code_end) : code(code_ptr), m_code_end(code_end) {}
  virtual ~XEmitter() = default;
  void SetCodePtr(u8* ptr, u8* end, bool write_failed = false);
  void ReserveCodeSpace(int bytes);
  u8* AlignCodeTo(size_t alignment);
  u8* AlignCode4();
  u8* AlignCode16();
  u8* AlignCodePage();
  const u8* GetCodePtr() const;
  u8* GetWritableCodePtr();
  const u8* GetCodeEnd() const;
  u8* GetWritableCodeEnd();

  void LockFlags() { flags_locked = true; }
  void UnlockFlags() { flags_locked = false; }

  // Should be checked after a block of code has been generated to see if the code has been
  // successfully written to memory. Do not call the generated code when this returns true!
  bool HasWriteFailed() const { return m_write_failed; }

  // Looking for one of these? It's BANNED!! Some instructions are slow on modern CPU
  // INC, DEC, LOOP, LOOPNE, LOOPE, ENTER, LEAVE, XCHG, XLAT, REP MOVSB/MOVSD, REP SCASD + other
  // string instr.,
  // INC and DEC are slow on Intel Core, but not on AMD. They create a
  // false flag dependency because they only update a subset of the flags.
  // XCHG is SLOW and should be avoided.

  // Debug breakpoint
  void INT3();

  // Do nothing
  void NOP(size_t count = 1);

  // Save energy in wait-loops on P4 only. Probably not too useful.
  void PAUSE();

  // Flag control
  void STC();
  void CLC();
  void CMC();

  // These two can not be executed in 64-bit mode on early Intel 64-bit CPU:s, only on Core2 and
  // AMD!
  void LAHF();  // 3 cycle vector path
  void SAHF();  // direct path fast

  // Stack control
  void PUSH(X64Reg reg);
  void POP(X64Reg reg);
  void PUSH(int bits, const OpArg& reg);
  void POP(int bits, const OpArg& reg);
  void PUSHF();
  void POPF();

  // Flow control
  void RET();
  void RET_FAST();
  void UD2();
  FixupBranch J(bool force5bytes = false);

  void JMP(const u8* addr, bool force5Bytes = false);
  void JMPptr(const OpArg& arg);
  void JMPself();  // infinite loop!
#ifdef CALL
#undef CALL
#endif
  void CALL(const void* fnptr);
  FixupBranch CALL();
  void CALLptr(OpArg arg);

  FixupBranch J_CC(CCFlags conditionCode, bool force5bytes = false);
  void J_CC(CCFlags conditionCode, const u8* addr);

  void SetJumpTarget(const FixupBranch& branch);

  void SETcc(CCFlags flag, OpArg dest);
  // Note: CMOV brings small if any benefit on current CPUs.
  void CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag);

  // Fences
  void LFENCE();
  void MFENCE();
  void SFENCE();

  // Bit scan
  void BSF(int bits, X64Reg dest, const OpArg& src);  // Bottom bit to top bit
  void BSR(int bits, X64Reg dest, const OpArg& src);  // Top bit to bottom bit

  // Cache control
  enum PrefetchLevel
  {
    PF_NTA,  // Non-temporal (data used once and only once)
    PF_T0,   // All cache levels
    PF_T1,   // Levels 2+ (aliased to T0 on AMD)
    PF_T2,   // Levels 3+ (aliased to T0 on AMD)
  };
  void PREFETCH(PrefetchLevel level, OpArg arg);
  void MOVNTI(int bits, const OpArg& dest, X64Reg src);
  void MOVNTDQ(const OpArg& arg, X64Reg regOp);
  void MOVNTPS(const OpArg& arg, X64Reg regOp);
  void MOVNTPD(const OpArg& arg, X64Reg regOp);

  // Multiplication / division
  void MUL(int bits, const OpArg& src);   // UNSIGNED
  void IMUL(int bits, const OpArg& src);  // SIGNED
  void IMUL(int bits, X64Reg regOp, const OpArg& src);
  void IMUL(int bits, X64Reg regOp, const OpArg& src, const OpArg& imm);
  void DIV(int bits, const OpArg& src);
  void IDIV(int bits, const OpArg& src);

  // Shift
  void ROL(int bits, const OpArg& dest, const OpArg& shift);
  void ROR(int bits, const OpArg& dest, const OpArg& shift);
  void RCL(int bits, const OpArg& dest, const OpArg& shift);
  void RCR(int bits, const OpArg& dest, const OpArg& shift);
  void SHL(int bits, const OpArg& dest, const OpArg& shift);
  void SHR(int bits, const OpArg& dest, const OpArg& shift);
  void SAR(int bits, const OpArg& dest, const OpArg& shift);

  // Bit Test
  void BT(int bits, const OpArg& dest, const OpArg& index);
  void BTS(int bits, const OpArg& dest, const OpArg& index);
  void BTR(int bits, const OpArg& dest, const OpArg& index);
  void BTC(int bits, const OpArg& dest, const OpArg& index);

  // Double-Precision Shift
  void SHRD(int bits, const OpArg& dest, const OpArg& src, const OpArg& shift);
  void SHLD(int bits, const OpArg& dest, const OpArg& src, const OpArg& shift);

  // Extend EAX into EDX in various ways
  void CWD(int bits = 16);
  inline void CDQ() { CWD(32); }
  inline void CQO() { CWD(64); }
  void CBW(int bits = 8);
  inline void CWDE() { CBW(16); }
  inline void CDQE() { CBW(32); }
  // Load effective address
  void LEA(int bits, X64Reg dest, OpArg src);

  // Integer arithmetic
  void NEG(int bits, const OpArg& src);
  void ADD(int bits, const OpArg& a1, const OpArg& a2);
  void ADC(int bits, const OpArg& a1, const OpArg& a2);
  void SUB(int bits, const OpArg& a1, const OpArg& a2);
  void SBB(int bits, const OpArg& a1, const OpArg& a2);
  void AND(int bits, const OpArg& a1, const OpArg& a2);
  void CMP(int bits, const OpArg& a1, const OpArg& a2);

  // Bit operations
  void NOT(int bits, const OpArg& src);
  void OR(int bits, const OpArg& a1, const OpArg& a2);
  void XOR(int bits, const OpArg& a1, const OpArg& a2);
  void MOV(int bits, const OpArg& a1, const OpArg& a2);
  void TEST(int bits, const OpArg& a1, const OpArg& a2);

  void CMP_or_TEST(int bits, const OpArg& a1, const OpArg& a2);
  void MOV_sum(int bits, X64Reg dest, const OpArg& a1, const OpArg& a2);

  // Are these useful at all? Consider removing.
  void XCHG(int bits, const OpArg& a1, const OpArg& a2);
  void XCHG_AHAL();

  // Byte swapping (32 and 64-bit only).
  void BSWAP(int bits, X64Reg reg);

  // Sign/zero extension
  void MOVSX(int dbits, int sbits, X64Reg dest,
             OpArg src);  // automatically uses MOVSXD if necessary
  void MOVZX(int dbits, int sbits, X64Reg dest, OpArg src);

  // Available only on Atom or >= Haswell so far. Test with cpu_info.bMOVBE.
  void MOVBE(int bits, X64Reg dest, const OpArg& src);
  void MOVBE(int bits, const OpArg& dest, X64Reg src);
  void LoadAndSwap(int size, X64Reg dst, const OpArg& src, bool sign_extend = false,
                   MovInfo* info = nullptr);
  void SwapAndStore(int size, const OpArg& dst, X64Reg src, MovInfo* info = nullptr);

  // Available only on AMD >= Phenom or Intel >= Haswell
  void LZCNT(int bits, X64Reg dest, const OpArg& src);
  // Note: this one is actually part of BMI1
  void TZCNT(int bits, X64Reg dest, const OpArg& src);

  // WARNING - These two take 11-13 cycles and are VectorPath! (AMD64)
  void STMXCSR(const OpArg& memloc);
  void LDMXCSR(const OpArg& memloc);

  // Prefixes
  void LOCK();
  void REP();
  void REPNE();
  void FSOverride();
  void GSOverride();

  // SSE/SSE2: Floating point arithmetic
  void ADDSS(X64Reg regOp, const OpArg& arg);
  void ADDSD(X64Reg regOp, const OpArg& arg);
  void SUBSS(X64Reg regOp, const OpArg& arg);
  void SUBSD(X64Reg regOp, const OpArg& arg);
  void MULSS(X64Reg regOp, const OpArg& arg);
  void MULSD(X64Reg regOp, const OpArg& arg);
  void DIVSS(X64Reg regOp, const OpArg& arg);
  void DIVSD(X64Reg regOp, const OpArg& arg);
  void MINSS(X64Reg regOp, const OpArg& arg);
  void MINSD(X64Reg regOp, const OpArg& arg);
  void MAXSS(X64Reg regOp, const OpArg& arg);
  void MAXSD(X64Reg regOp, const OpArg& arg);
  void SQRTSS(X64Reg regOp, const OpArg& arg);
  void SQRTSD(X64Reg regOp, const OpArg& arg);
  void RCPSS(X64Reg regOp, const OpArg& arg);
  void RSQRTSS(X64Reg regOp, const OpArg& arg);

  // SSE/SSE2: Floating point bitwise (yes)
  void CMPSS(X64Reg regOp, const OpArg& arg, u8 compare);
  void CMPSD(X64Reg regOp, const OpArg& arg, u8 compare);

  // SSE/SSE2: Floating point packed arithmetic (x4 for float, x2 for double)
  void ADDPS(X64Reg regOp, const OpArg& arg);
  void ADDPD(X64Reg regOp, const OpArg& arg);
  void SUBPS(X64Reg regOp, const OpArg& arg);
  void SUBPD(X64Reg regOp, const OpArg& arg);
  void CMPPS(X64Reg regOp, const OpArg& arg, u8 compare);
  void CMPPD(X64Reg regOp, const OpArg& arg, u8 compare);
  void MULPS(X64Reg regOp, const OpArg& arg);
  void MULPD(X64Reg regOp, const OpArg& arg);
  void DIVPS(X64Reg regOp, const OpArg& arg);
  void DIVPD(X64Reg regOp, const OpArg& arg);
  void MINPS(X64Reg regOp, const OpArg& arg);
  void MINPD(X64Reg regOp, const OpArg& arg);
  void MAXPS(X64Reg regOp, const OpArg& arg);
  void MAXPD(X64Reg regOp, const OpArg& arg);
  void SQRTPS(X64Reg regOp, const OpArg& arg);
  void SQRTPD(X64Reg regOp, const OpArg& arg);
  void RCPPS(X64Reg regOp, const OpArg& arg);
  void RSQRTPS(X64Reg regOp, const OpArg& arg);

  // SSE/SSE2: Floating point packed bitwise (x4 for float, x2 for double)
  void ANDPS(X64Reg regOp, const OpArg& arg);
  void ANDPD(X64Reg regOp, const OpArg& arg);
  void ANDNPS(X64Reg regOp, const OpArg& arg);
  void ANDNPD(X64Reg regOp, const OpArg& arg);
  void ORPS(X64Reg regOp, const OpArg& arg);
  void ORPD(X64Reg regOp, const OpArg& arg);
  void XORPS(X64Reg regOp, const OpArg& arg);
  void XORPD(X64Reg regOp, const OpArg& arg);

  // SSE/SSE2: Shuffle components. These are tricky - see Intel documentation.
  void SHUFPS(X64Reg regOp, const OpArg& arg, u8 shuffle);
  void SHUFPD(X64Reg regOp, const OpArg& arg, u8 shuffle);

  // SSE3
  void MOVSLDUP(X64Reg regOp, const OpArg& arg);
  void MOVSHDUP(X64Reg regOp, const OpArg& arg);
  void MOVDDUP(X64Reg regOp, const OpArg& arg);

  // SSE/SSE2: Useful alternative to shuffle in some cases.
  void UNPCKLPS(X64Reg dest, const OpArg& src);
  void UNPCKHPS(X64Reg dest, const OpArg& src);
  void UNPCKLPD(X64Reg dest, const OpArg& src);
  void UNPCKHPD(X64Reg dest, const OpArg& src);

  // SSE/SSE2: Compares.
  void COMISS(X64Reg regOp, const OpArg& arg);
  void COMISD(X64Reg regOp, const OpArg& arg);
  void UCOMISS(X64Reg regOp, const OpArg& arg);
  void UCOMISD(X64Reg regOp, const OpArg& arg);

  // SSE/SSE2: Moves. Use the right data type for your data, in most cases.
  void MOVAPS(X64Reg regOp, const OpArg& arg);
  void MOVAPD(X64Reg regOp, const OpArg& arg);
  void MOVAPS(const OpArg& arg, X64Reg regOp);
  void MOVAPD(const OpArg& arg, X64Reg regOp);

  void MOVUPS(X64Reg regOp, const OpArg& arg);
  void MOVUPD(X64Reg regOp, const OpArg& arg);
  void MOVUPS(const OpArg& arg, X64Reg regOp);
  void MOVUPD(const OpArg& arg, X64Reg regOp);

  void MOVDQA(X64Reg regOp, const OpArg& arg);
  void MOVDQA(const OpArg& arg, X64Reg regOp);
  void MOVDQU(X64Reg regOp, const OpArg& arg);
  void MOVDQU(const OpArg& arg, X64Reg regOp);

  void MOVSS(X64Reg regOp, const OpArg& arg);
  void MOVSD(X64Reg regOp, const OpArg& arg);
  void MOVSS(const OpArg& arg, X64Reg regOp);
  void MOVSD(const OpArg& arg, X64Reg regOp);

  void MOVLPS(X64Reg regOp, const OpArg& arg);
  void MOVLPD(X64Reg regOp, const OpArg& arg);
  void MOVLPS(const OpArg& arg, X64Reg regOp);
  void MOVLPD(const OpArg& arg, X64Reg regOp);

  void MOVHPS(X64Reg regOp, const OpArg& arg);
  void MOVHPD(X64Reg regOp, const OpArg& arg);
  void MOVHPS(const OpArg& arg, X64Reg regOp);
  void MOVHPD(const OpArg& arg, X64Reg regOp);

  void MOVHLPS(X64Reg regOp1, X64Reg regOp2);
  void MOVLHPS(X64Reg regOp1, X64Reg regOp2);

  // Be careful when using these overloads for reg <--> xmm moves.
  // The one you cast to OpArg with R(reg) is the x86 reg, the other
  // one is the xmm reg.
  // ie: "MOVD_xmm(eax, R(xmm1))" generates incorrect code (movd xmm0, rcx)
  //     use "MOVD_xmm(R(eax), xmm1)" instead.
  void MOVD_xmm(X64Reg dest, const OpArg& arg);
  void MOVQ_xmm(X64Reg dest, OpArg arg);
  void MOVD_xmm(const OpArg& arg, X64Reg src);
  void MOVQ_xmm(OpArg arg, X64Reg src);

  // SSE/SSE2: Generates a mask from the high bits of the components of the packed register in
  // question.
  void MOVMSKPS(X64Reg dest, const OpArg& arg);
  void MOVMSKPD(X64Reg dest, const OpArg& arg);

  // SSE2: Selective byte store, mask in src register. EDI/RDI specifies store address. This is a
  // weird one.
  void MASKMOVDQU(X64Reg dest, X64Reg src);
  void LDDQU(X64Reg dest, const OpArg& src);

  // SSE/SSE2: Data type conversions.
  void CVTPS2PD(X64Reg dest, const OpArg& src);
  void CVTPD2PS(X64Reg dest, const OpArg& src);
  void CVTSS2SD(X64Reg dest, const OpArg& src);
  void CVTSI2SS(X64Reg dest, const OpArg& src);
  void CVTSD2SS(X64Reg dest, const OpArg& src);
  void CVTSI2SD(X64Reg dest, const OpArg& src);
  void CVTDQ2PD(X64Reg regOp, const OpArg& arg);
  void CVTPD2DQ(X64Reg regOp, const OpArg& arg);
  void CVTDQ2PS(X64Reg regOp, const OpArg& arg);
  void CVTPS2DQ(X64Reg regOp, const OpArg& arg);

  void CVTTPS2DQ(X64Reg regOp, const OpArg& arg);
  void CVTTPD2DQ(X64Reg regOp, const OpArg& arg);

  // Destinations are X64 regs (rax, rbx, ...) for these instructions.
  void CVTSS2SI(X64Reg xregdest, const OpArg& src);
  void CVTSD2SI(X64Reg xregdest, const OpArg& src);
  void CVTTSS2SI(X64Reg xregdest, const OpArg& arg);
  void CVTTSD2SI(X64Reg xregdest, const OpArg& arg);

  // SSE2: Packed integer instructions
  void PACKSSDW(X64Reg dest, const OpArg& arg);
  void PACKSSWB(X64Reg dest, const OpArg& arg);
  void PACKUSDW(X64Reg dest, const OpArg& arg);
  void PACKUSWB(X64Reg dest, const OpArg& arg);

  void PUNPCKLBW(X64Reg dest, const OpArg& arg);
  void PUNPCKLWD(X64Reg dest, const OpArg& arg);
  void PUNPCKLDQ(X64Reg dest, const OpArg& arg);
  void PUNPCKLQDQ(X64Reg dest, const OpArg& arg);

  void PTEST(X64Reg dest, const OpArg& arg);
  void PAND(X64Reg dest, const OpArg& arg);
  void PANDN(X64Reg dest, const OpArg& arg);
  void PXOR(X64Reg dest, const OpArg& arg);
  void POR(X64Reg dest, const OpArg& arg);

  void PADDB(X64Reg dest, const OpArg& arg);
  void PADDW(X64Reg dest, const OpArg& arg);
  void PADDD(X64Reg dest, const OpArg& arg);
  void PADDQ(X64Reg dest, const OpArg& arg);

  void PADDSB(X64Reg dest, const OpArg& arg);
  void PADDSW(X64Reg dest, const OpArg& arg);
  void PADDUSB(X64Reg dest, const OpArg& arg);
  void PADDUSW(X64Reg dest, const OpArg& arg);

  void PSUBB(X64Reg dest, const OpArg& arg);
  void PSUBW(X64Reg dest, const OpArg& arg);
  void PSUBD(X64Reg dest, const OpArg& arg);
  void PSUBQ(X64Reg dest, const OpArg& arg);

  void PSUBSB(X64Reg dest, const OpArg& arg);
  void PSUBSW(X64Reg dest, const OpArg& arg);
  void PSUBUSB(X64Reg dest, const OpArg& arg);
  void PSUBUSW(X64Reg dest, const OpArg& arg);

  void PAVGB(X64Reg dest, const OpArg& arg);
  void PAVGW(X64Reg dest, const OpArg& arg);

  void PCMPEQB(X64Reg dest, const OpArg& arg);
  void PCMPEQW(X64Reg dest, const OpArg& arg);
  void PCMPEQD(X64Reg dest, const OpArg& arg);

  void PCMPGTB(X64Reg dest, const OpArg& arg);
  void PCMPGTW(X64Reg dest, const OpArg& arg);
  void PCMPGTD(X64Reg dest, const OpArg& arg);

  void PEXTRW(X64Reg dest, const OpArg& arg, u8 subreg);
  void PINSRW(X64Reg dest, const OpArg& arg, u8 subreg);
  void PINSRD(X64Reg dest, const OpArg& arg, u8 subreg);

  void PMADDWD(X64Reg dest, const OpArg& arg);
  void PSADBW(X64Reg dest, const OpArg& arg);

  void PMAXSW(X64Reg dest, const OpArg& arg);
  void PMAXUB(X64Reg dest, const OpArg& arg);
  void PMINSW(X64Reg dest, const OpArg& arg);
  void PMINUB(X64Reg dest, const OpArg& arg);

  void PMOVMSKB(X64Reg dest, const OpArg& arg);
  void PSHUFD(X64Reg dest, const OpArg& arg, u8 shuffle);
  void PSHUFB(X64Reg dest, const OpArg& arg);

  void PSHUFLW(X64Reg dest, const OpArg& arg, u8 shuffle);
  void PSHUFHW(X64Reg dest, const OpArg& arg, u8 shuffle);

  void PSRLW(X64Reg reg, int shift);
  void PSRLD(X64Reg reg, int shift);
  void PSRLQ(X64Reg reg, int shift);
  void PSRLQ(X64Reg reg, const OpArg& arg);
  void PSRLDQ(X64Reg reg, int shift);

  void PSLLW(X64Reg reg, int shift);
  void PSLLD(X64Reg reg, int shift);
  void PSLLQ(X64Reg reg, int shift);
  void PSLLDQ(X64Reg reg, int shift);

  void PSRAW(X64Reg reg, int shift);
  void PSRAD(X64Reg reg, int shift);

  // SSE4: data type conversions
  void PMOVSXBW(X64Reg dest, const OpArg& arg);
  void PMOVSXBD(X64Reg dest, const OpArg& arg);
  void PMOVSXBQ(X64Reg dest, const OpArg& arg);
  void PMOVSXWD(X64Reg dest, const OpArg& arg);
  void PMOVSXWQ(X64Reg dest, const OpArg& arg);
  void PMOVSXDQ(X64Reg dest, const OpArg& arg);
  void PMOVZXBW(X64Reg dest, const OpArg& arg);
  void PMOVZXBD(X64Reg dest, const OpArg& arg);
  void PMOVZXBQ(X64Reg dest, const OpArg& arg);
  void PMOVZXWD(X64Reg dest, const OpArg& arg);
  void PMOVZXWQ(X64Reg dest, const OpArg& arg);
  void PMOVZXDQ(X64Reg dest, const OpArg& arg);

  // SSE4: blend instructions
  void PBLENDVB(X64Reg dest, const OpArg& arg);
  void BLENDVPS(X64Reg dest, const OpArg& arg);
  void BLENDVPD(X64Reg dest, const OpArg& arg);
  void BLENDPS(X64Reg dest, const OpArg& arg, u8 blend);
  void BLENDPD(X64Reg dest, const OpArg& arg, u8 blend);

  // AVX
  void VADDSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VSUBSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VMULSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VDIVSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VADDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VSUBPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VMULPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VDIVPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VADDSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VSUBSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VMULSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VDIVSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VADDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VSUBPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VMULPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VDIVPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VSQRTSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VCMPPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 compare);
  void VSHUFPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 shuffle);
  void VSHUFPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 shuffle);
  void VUNPCKLPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VUNPCKLPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VUNPCKHPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VBLENDVPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, X64Reg mask);
  void VBLENDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 blend);
  void VBLENDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 blend);

  void VANDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VANDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VANDNPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VANDNPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VORPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VORPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VXORPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VXORPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);

  void VPAND(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VPANDN(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VPOR(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VPXOR(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);

  // FMA3
  void VFMADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADD231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUB231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMADD231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFNMSUB231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMADDSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void VFMSUBADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg);

#define FMA4(name)                                                                                 \
  void name(X64Reg dest, X64Reg regOp1, X64Reg regOp2, const OpArg& arg);                          \
  void name(X64Reg dest, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);

  FMA4(VFMADDSUBPS)
  FMA4(VFMADDSUBPD)
  FMA4(VFMSUBADDPS)
  FMA4(VFMSUBADDPD)
  FMA4(VFMADDPS)
  FMA4(VFMADDPD)
  FMA4(VFMADDSS)
  FMA4(VFMADDSD)
  FMA4(VFMSUBPS)
  FMA4(VFMSUBPD)
  FMA4(VFMSUBSS)
  FMA4(VFMSUBSD)
  FMA4(VFNMADDPS)
  FMA4(VFNMADDPD)
  FMA4(VFNMADDSS)
  FMA4(VFNMADDSD)
  FMA4(VFNMSUBPS)
  FMA4(VFNMSUBPD)
  FMA4(VFNMSUBSS)
  FMA4(VFNMSUBSD)
#undef FMA4

  // VEX GPR instructions
  void SARX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);
  void SHLX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);
  void SHRX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);
  void RORX(int bits, X64Reg regOp, const OpArg& arg, u8 rotate);
  void PEXT(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void PDEP(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void MULX(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg);
  void BZHI(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);
  void BLSR(int bits, X64Reg regOp, const OpArg& arg);
  void BLSMSK(int bits, X64Reg regOp, const OpArg& arg);
  void BLSI(int bits, X64Reg regOp, const OpArg& arg);
  void BEXTR(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2);
  void ANDN(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg);

  void RDTSC();

  // Utility functions
  // The difference between this and CALL is that this aligns the stack
  // where appropriate.
  template <typename FunctionPointer>
  void ABI_CallFunction(FunctionPointer func)
  {
    static_assert(std::is_pointer<FunctionPointer>() &&
                      std::is_function<std::remove_pointer_t<FunctionPointer>>(),
                  "Supplied type must be a function pointer.");

    const void* ptr = reinterpret_cast<const void*>(func);
    const u64 address = reinterpret_cast<u64>(ptr);
    const u64 distance = address - (reinterpret_cast<u64>(code) + 5);

    if (distance >= 0x0000000080000000ULL && distance < 0xFFFFFFFF80000000ULL)
    {
      // Far call
      MOV(64, R(RAX), Imm64(address));
      CALLptr(R(RAX));
    }
    else
    {
      CALL(ptr);
    }
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionC16(FunctionPointer func, u16 param1)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCC16(FunctionPointer func, u32 param1, u16 param2)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionC(FunctionPointer func, u32 param1)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCC(FunctionPointer func, u32 param1, u32 param2)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCP(FunctionPointer func, u32 param1, const void* param2)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(64, R(ABI_PARAM2), Imm64(reinterpret_cast<u64>(param2)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCCC(FunctionPointer func, u32 param1, u32 param2, u32 param3)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    MOV(32, R(ABI_PARAM3), Imm32(param3));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCCP(FunctionPointer func, u32 param1, u32 param2, const void* param3)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    MOV(64, R(ABI_PARAM3), Imm64(reinterpret_cast<u64>(param3)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionCCCP(FunctionPointer func, u32 param1, u32 param2, u32 param3,
                            const void* param4)
  {
    MOV(32, R(ABI_PARAM1), Imm32(param1));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    MOV(32, R(ABI_PARAM3), Imm32(param3));
    MOV(64, R(ABI_PARAM4), Imm64(reinterpret_cast<u64>(param4)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionP(FunctionPointer func, const void* param1)
  {
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(param1)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionPC(FunctionPointer func, const void* param1, u32 param2)
  {
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(param1)));
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionPPC(FunctionPointer func, const void* param1, const void* param2, u32 param3)
  {
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(param1)));
    MOV(64, R(ABI_PARAM2), Imm64(reinterpret_cast<u64>(param2)));
    MOV(32, R(ABI_PARAM3), Imm32(param3));
    ABI_CallFunction(func);
  }

  // Pass a register as a parameter.
  template <typename FunctionPointer>
  void ABI_CallFunctionR(FunctionPointer func, X64Reg reg1)
  {
    if (reg1 != ABI_PARAM1)
      MOV(32, R(ABI_PARAM1), R(reg1));
    ABI_CallFunction(func);
  }

  // Pass a pointer and register as a parameter.
  template <typename FunctionPointer>
  void ABI_CallFunctionPR(FunctionPointer func, const void* ptr, X64Reg reg1)
  {
    if (reg1 != ABI_PARAM2)
      MOV(64, R(ABI_PARAM2), R(reg1));
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(ptr)));
    ABI_CallFunction(func);
  }

  // Pass two registers as parameters.
  template <typename FunctionPointer>
  void ABI_CallFunctionRR(FunctionPointer func, X64Reg reg1, X64Reg reg2)
  {
    MOVTwo(64, ABI_PARAM1, reg1, 0, ABI_PARAM2, reg2);
    ABI_CallFunction(func);
  }

  // Pass a pointer and two registers as parameters.
  template <typename FunctionPointer>
  void ABI_CallFunctionPRR(FunctionPointer func, const void* ptr, X64Reg reg1, X64Reg reg2)
  {
    MOVTwo(64, ABI_PARAM2, reg1, 0, ABI_PARAM3, reg2);
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(ptr)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionAC(int bits, FunctionPointer func, const Gen::OpArg& arg1, u32 param2)
  {
    if (!arg1.IsSimpleReg(ABI_PARAM1))
      MOV(bits, R(ABI_PARAM1), arg1);
    MOV(32, R(ABI_PARAM2), Imm32(param2));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionPAC(int bits, FunctionPointer func, const void* ptr1, const Gen::OpArg& arg2,
                           u32 param3)
  {
    if (!arg2.IsSimpleReg(ABI_PARAM2))
      MOV(bits, R(ABI_PARAM2), arg2);
    MOV(32, R(ABI_PARAM3), Imm32(param3));
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(ptr1)));
    ABI_CallFunction(func);
  }

  template <typename FunctionPointer>
  void ABI_CallFunctionA(int bits, FunctionPointer func, const Gen::OpArg& arg1)
  {
    if (!arg1.IsSimpleReg(ABI_PARAM1))
      MOV(bits, R(ABI_PARAM1), arg1);
    ABI_CallFunction(func);
  }

  // Helper method for ABI functions related to calling functions. May be used by itself as well.
  void MOVTwo(int bits, X64Reg dst1, X64Reg src1, s32 offset, X64Reg dst2, X64Reg src2);

  // Saves/restores the registers and adjusts the stack to be aligned as
  // required by the ABI, where the previous alignment was as specified.
  // Push returns the size of the shadow space, i.e. the offset of the frame.
  size_t ABI_PushRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                         size_t needed_frame_size = 0);
  void ABI_PopRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                      size_t needed_frame_size = 0);

  // Utility to generate a call to a std::function object.
  //
  // Unfortunately, calling operator() directly is undefined behavior in C++
  // (this method might be a thunk in the case of multi-inheritance) so we
  // have to go through a trampoline function.
  template <typename T, typename... Args>
  static T CallLambdaTrampoline(const std::function<T(Args...)>* f, Args... args)
  {
    return (*f)(args...);
  }

  template <typename T, typename... Args>
  void ABI_CallLambdaPC(const std::function<T(Args...)>* f, void* p1, u32 p2)
  {
    auto trampoline = &XEmitter::CallLambdaTrampoline<T, Args...>;
    ABI_CallFunctionPPC(trampoline, reinterpret_cast<const void*>(f), p1, p2);
  }
};  // class XEmitter

class X64CodeBlock : public Common::CodeBlock<XEmitter>
{
private:
  void PoisonMemory() override
  {
    // x86/64: 0xCC = breakpoint
    memset(region, 0xCC, region_size);
  }
};

}  // namespace Gen
