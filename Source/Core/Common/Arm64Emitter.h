// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <functional>
#include <optional>
#include <utility>

#include "Common/ArmCommon.h"
#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/BitUtils.h"
#include "Common/CodeBlock.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

namespace Arm64Gen
{
// X30 serves a dual purpose as a link register
// Encoded as <u3:type><u5:reg>
// Types:
// 000 - 32bit GPR
// 001 - 64bit GPR
// 010 - VFP single precision
// 100 - VFP double precision
// 110 - VFP quad precision
enum class ARM64Reg
{
  // 32bit registers
  W0 = 0,
  W1,
  W2,
  W3,
  W4,
  W5,
  W6,
  W7,
  W8,
  W9,
  W10,
  W11,
  W12,
  W13,
  W14,
  W15,
  W16,
  W17,
  W18,
  W19,
  W20,
  W21,
  W22,
  W23,
  W24,
  W25,
  W26,
  W27,
  W28,
  W29,
  W30,

  WSP,  // 32bit stack pointer

  // 64bit registers
  X0 = 0x20,
  X1,
  X2,
  X3,
  X4,
  X5,
  X6,
  X7,
  X8,
  X9,
  X10,
  X11,
  X12,
  X13,
  X14,
  X15,
  X16,
  X17,
  X18,
  X19,
  X20,
  X21,
  X22,
  X23,
  X24,
  X25,
  X26,
  X27,
  X28,
  X29,
  X30,

  SP,  // 64bit stack pointer

  // VFP single precision registers
  S0 = 0x40,
  S1,
  S2,
  S3,
  S4,
  S5,
  S6,
  S7,
  S8,
  S9,
  S10,
  S11,
  S12,
  S13,
  S14,
  S15,
  S16,
  S17,
  S18,
  S19,
  S20,
  S21,
  S22,
  S23,
  S24,
  S25,
  S26,
  S27,
  S28,
  S29,
  S30,
  S31,

  // VFP Double Precision registers
  D0 = 0x80,
  D1,
  D2,
  D3,
  D4,
  D5,
  D6,
  D7,
  D8,
  D9,
  D10,
  D11,
  D12,
  D13,
  D14,
  D15,
  D16,
  D17,
  D18,
  D19,
  D20,
  D21,
  D22,
  D23,
  D24,
  D25,
  D26,
  D27,
  D28,
  D29,
  D30,
  D31,

  // ASIMD Quad-Word registers
  Q0 = 0xC0,
  Q1,
  Q2,
  Q3,
  Q4,
  Q5,
  Q6,
  Q7,
  Q8,
  Q9,
  Q10,
  Q11,
  Q12,
  Q13,
  Q14,
  Q15,
  Q16,
  Q17,
  Q18,
  Q19,
  Q20,
  Q21,
  Q22,
  Q23,
  Q24,
  Q25,
  Q26,
  Q27,
  Q28,
  Q29,
  Q30,
  Q31,

  // For PRFM(prefetch memory) encoding
  // This is encoded in the Rt register
  // Data preload
  PLDL1KEEP = 0,
  PLDL1STRM,
  PLDL2KEEP,
  PLDL2STRM,
  PLDL3KEEP,
  PLDL3STRM,
  // Instruction preload
  PLIL1KEEP = 8,
  PLIL1STRM,
  PLIL2KEEP,
  PLIL2STRM,
  PLIL3KEEP,
  PLIL3STRM,
  // Prepare for store
  PLTL1KEEP = 16,
  PLTL1STRM,
  PLTL2KEEP,
  PLTL2STRM,
  PLTL3KEEP,
  PLTL3STRM,

  WZR = WSP,
  ZR = SP,

  INVALID_REG = -1,
};

constexpr int operator&(const ARM64Reg& reg, const int mask)
{
  return static_cast<int>(reg) & mask;
}
constexpr int operator|(const ARM64Reg& reg, const int mask)
{
  return static_cast<int>(reg) | mask;
}
constexpr ARM64Reg operator+(const ARM64Reg& reg, const int addend)
{
  return static_cast<ARM64Reg>(static_cast<int>(reg) + addend);
}
constexpr bool Is64Bit(ARM64Reg reg)
{
  return (reg & 0x20) != 0;
}
constexpr bool IsSingle(ARM64Reg reg)
{
  return (reg & 0xC0) == 0x40;
}
constexpr bool IsDouble(ARM64Reg reg)
{
  return (reg & 0xC0) == 0x80;
}
constexpr bool IsScalar(ARM64Reg reg)
{
  return IsSingle(reg) || IsDouble(reg);
}
constexpr bool IsQuad(ARM64Reg reg)
{
  return (reg & 0xC0) == 0xC0;
}
constexpr bool IsVector(ARM64Reg reg)
{
  return (reg & 0xC0) != 0;
}
constexpr bool IsGPR(ARM64Reg reg)
{
  return static_cast<int>(reg) < 0x40;
}

constexpr int DecodeReg(ARM64Reg reg)
{
  return reg & 0x1F;
}
constexpr ARM64Reg EncodeRegTo32(ARM64Reg reg)
{
  return static_cast<ARM64Reg>(DecodeReg(reg));
}
constexpr ARM64Reg EncodeRegTo64(ARM64Reg reg)
{
  return static_cast<ARM64Reg>(reg | 0x20);
}
constexpr ARM64Reg EncodeRegToSingle(ARM64Reg reg)
{
  return static_cast<ARM64Reg>(ARM64Reg::S0 | DecodeReg(reg));
}
constexpr ARM64Reg EncodeRegToDouble(ARM64Reg reg)
{
  return static_cast<ARM64Reg>((reg & ~0xC0) | 0x80);
}
constexpr ARM64Reg EncodeRegToQuad(ARM64Reg reg)
{
  return static_cast<ARM64Reg>(reg | 0xC0);
}

enum class ShiftType
{
  // Logical Shift Left
  LSL = 0,
  // Logical Shift Right
  LSR = 1,
  // Arithmetic Shift Right
  ASR = 2,
  // Rotate Right
  ROR = 3,
};

enum class IndexType
{
  Unsigned,
  Post,
  Pre,
  Signed,  // used in LDP/STP
};

enum class ShiftAmount
{
  Shift0,
  Shift16,
  Shift32,
  Shift48,
};

enum class RoundingMode
{
  A,  // round to nearest, ties to away
  M,  // round towards -inf
  N,  // round to nearest, ties to even
  P,  // round towards +inf
  Z,  // round towards zero
};

struct FixupBranch
{
  enum class Type : u32
  {
    CBZ,
    CBNZ,
    BConditional,
    TBZ,
    TBNZ,
    B,
    BL,
  };

  u8* ptr;
  Type type;
  // Used with B.cond
  CCFlags cond;
  // Used with TBZ/TBNZ
  u8 bit;
  // Used with Test/Compare and Branch
  ARM64Reg reg;
};

enum class PStateField
{
  SPSel = 0,
  DAIFSet,
  DAIFClr,
  NZCV,  // The only system registers accessible from EL0 (user space)
  PMCR_EL0,
  PMCCNTR_EL0,
  FPCR = 0x340,
  FPSR = 0x341,
};

enum class SystemHint
{
  NOP,
  YIELD,
  WFE,
  WFI,
  SEV,
  SEVL,
};

enum class BarrierType
{
  OSHLD = 1,
  OSHST = 2,
  OSH = 3,
  NSHLD = 5,
  NSHST = 6,
  NSH = 7,
  ISHLD = 9,
  ISHST = 10,
  ISH = 11,
  LD = 13,
  ST = 14,
  SY = 15,
};

class ArithOption
{
private:
  enum class WidthSpecifier
  {
    Default,
    Width32Bit,
    Width64Bit,
  };

  enum class ExtendSpecifier
  {
    UXTB = 0x0,
    UXTH = 0x1,
    UXTW = 0x2, /* Also LSL on 32bit width */
    UXTX = 0x3, /* Also LSL on 64bit width */
    SXTB = 0x4,
    SXTH = 0x5,
    SXTW = 0x6,
    SXTX = 0x7,
  };

  enum class TypeSpecifier
  {
    ExtendedReg,
    Immediate,
    ShiftedReg,
  };

  ARM64Reg m_destReg;
  WidthSpecifier m_width;
  ExtendSpecifier m_extend;
  TypeSpecifier m_type;
  ShiftType m_shifttype;
  u32 m_shift;

public:
  ArithOption(ARM64Reg Rd, bool index = false)
  {
    // Indexed registers are a certain feature of AARch64
    // On Loadstore instructions that use a register offset
    // We can have the register as an index
    // If we are indexing then the offset register will
    // be shifted to the left so we are indexing at intervals
    // of the size of what we are loading
    // 8-bit: Index does nothing
    // 16-bit: Index LSL 1
    // 32-bit: Index LSL 2
    // 64-bit: Index LSL 3
    if (index)
      m_shift = 4;
    else
      m_shift = 0;

    m_destReg = Rd;
    m_type = TypeSpecifier::ExtendedReg;
    if (Is64Bit(Rd))
    {
      m_width = WidthSpecifier::Width64Bit;
      m_extend = ExtendSpecifier::UXTX;
    }
    else
    {
      m_width = WidthSpecifier::Width32Bit;
      m_extend = ExtendSpecifier::UXTW;
    }
    m_shifttype = ShiftType::LSL;
  }
  ArithOption(ARM64Reg Rd, ShiftType shift_type, u32 shift)
  {
    m_destReg = Rd;
    m_shift = shift;
    m_shifttype = shift_type;
    m_type = TypeSpecifier::ShiftedReg;
    if (Is64Bit(Rd))
    {
      m_width = WidthSpecifier::Width64Bit;
      if (shift == 64)
        m_shift = 0;
    }
    else
    {
      m_width = WidthSpecifier::Width32Bit;
      if (shift == 32)
        m_shift = 0;
    }
  }
  ARM64Reg GetReg() const { return m_destReg; }
  u32 GetData() const
  {
    switch (m_type)
    {
    case TypeSpecifier::ExtendedReg:
      return (static_cast<u32>(m_extend) << 13) | (m_shift << 10);
    case TypeSpecifier::ShiftedReg:
      return (static_cast<u32>(m_shifttype) << 22) | (m_shift << 10);
    default:
      DEBUG_ASSERT_MSG(DYNA_REC, false, "Invalid type in GetData");
      break;
    }
    return 0;
  }

  bool IsExtended() const { return m_type == TypeSpecifier::ExtendedReg; }
};

struct LogicalImm
{
  constexpr LogicalImm() {}

  constexpr LogicalImm(u8 r_, u8 s_, bool n_) : r(r_), s(s_), n(n_), valid(true) {}

  constexpr LogicalImm(u64 value, u32 width)
  {
    bool negate = false;

    // Logical immediates are encoded using parameters n, imm_s and imm_r using
    // the following table:
    //
    //    N   imms    immr    size        S             R
    //    1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
    //    0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
    //    0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
    //    0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
    //    0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
    //    0  11110s  xxxxxr     2    UInt(s)       UInt(r)
    // (s bits must not be all set)
    //
    // A pattern is constructed of size bits, where the least significant S+1 bits
    // are set. The pattern is rotated right by R, and repeated across a 32 or
    // 64-bit value, depending on destination register width.
    //
    // Put another way: the basic format of a logical immediate is a single
    // contiguous stretch of 1 bits, repeated across the whole word at intervals
    // given by a power of 2. To identify them quickly, we first locate the
    // lowest stretch of 1 bits, then the next 1 bit above that; that combination
    // is different for every logical immediate, so it gives us all the
    // information we need to identify the only logical immediate that our input
    // could be, and then we simply check if that's the value we actually have.
    //
    // (The rotation parameter does give the possibility of the stretch of 1 bits
    // going 'round the end' of the word. To deal with that, we observe that in
    // any situation where that happens the bitwise NOT of the value is also a
    // valid logical immediate. So we simply invert the input whenever its low bit
    // is set, and then we know that the rotated case can't arise.)

    if (value & 1)
    {
      // If the low bit is 1, negate the value, and set a flag to remember that we
      // did (so that we can adjust the return values appropriately).
      negate = true;
      value = ~value;
    }

    constexpr int kWRegSizeInBits = 32;

    if (width == kWRegSizeInBits)
    {
      // To handle 32-bit logical immediates, the very easiest thing is to repeat
      // the input value twice to make a 64-bit word. The correct encoding of that
      // as a logical immediate will also be the correct encoding of the 32-bit
      // value.

      // The most-significant 32 bits may not be zero (ie. negate is true) so
      // shift the value left before duplicating it.
      value <<= kWRegSizeInBits;
      value |= value >> kWRegSizeInBits;
    }

    // The basic analysis idea: imagine our input word looks like this.
    //
    //    0011111000111110001111100011111000111110001111100011111000111110
    //                                                          c  b    a
    //                                                          |<--d-->|
    //
    // We find the lowest set bit (as an actual power-of-2 value, not its index)
    // and call it a. Then we add a to our original number, which wipes out the
    // bottommost stretch of set bits and replaces it with a 1 carried into the
    // next zero bit. Then we look for the new lowest set bit, which is in
    // position b, and subtract it, so now our number is just like the original
    // but with the lowest stretch of set bits completely gone. Now we find the
    // lowest set bit again, which is position c in the diagram above. Then we'll
    // measure the distance d between bit positions a and c (using CLZ), and that
    // tells us that the only valid logical immediate that could possibly be equal
    // to this number is the one in which a stretch of bits running from a to just
    // below b is replicated every d bits.
    u64 a = Common::LargestPowerOf2Divisor(value);
    u64 value_plus_a = value + a;
    u64 b = Common::LargestPowerOf2Divisor(value_plus_a);
    u64 value_plus_a_minus_b = value_plus_a - b;
    u64 c = Common::LargestPowerOf2Divisor(value_plus_a_minus_b);

    int d = 0, clz_a = 0, out_n = 0;
    u64 mask = 0;

    if (c != 0)
    {
      // The general case, in which there is more than one stretch of set bits.
      // Compute the repeat distance d, and set up a bitmask covering the basic
      // unit of repetition (i.e. a word with the bottom d bits set). Also, in all
      // of these cases the N bit of the output will be zero.
      clz_a = Common::CountLeadingZeros(a);
      int clz_c = Common::CountLeadingZeros(c);
      d = clz_a - clz_c;
      mask = ((UINT64_C(1) << d) - 1);
      out_n = 0;
    }
    else
    {
      // Handle degenerate cases.
      //
      // If any of those 'find lowest set bit' operations didn't find a set bit at
      // all, then the word will have been zero thereafter, so in particular the
      // last lowest_set_bit operation will have returned zero. So we can test for
      // all the special case conditions in one go by seeing if c is zero.
      if (a == 0)
      {
        // The input was zero (or all 1 bits, which will come to here too after we
        // inverted it at the start of the function), which is invalid.
        return;
      }
      else
      {
        // Otherwise, if c was zero but a was not, then there's just one stretch
        // of set bits in our word, meaning that we have the trivial case of
        // d == 64 and only one 'repetition'. Set up all the same variables as in
        // the general case above, and set the N bit in the output.
        clz_a = Common::CountLeadingZeros(a);
        d = 64;
        mask = ~UINT64_C(0);
        out_n = 1;
      }
    }

    // If the repeat period d is not a power of two, it can't be encoded.
    if (!MathUtil::IsPow2<u64>(d))
      return;

    // If the bit stretch (b - a) does not fit within the mask derived from the
    // repeat period, then fail.
    if (((b - a) & ~mask) != 0)
      return;

    // The only possible option is b - a repeated every d bits. Now we're going to
    // actually construct the valid logical immediate derived from that
    // specification, and see if it equals our original input.
    //
    // To repeat a value every d bits, we multiply it by a number of the form
    // (1 + 2^d + 2^(2d) + ...), i.e. 0x0001000100010001 or similar. These can
    // be derived using a table lookup on CLZ(d).
    constexpr std::array<u64, 6> multipliers = {{
        0x0000000000000001UL,
        0x0000000100000001UL,
        0x0001000100010001UL,
        0x0101010101010101UL,
        0x1111111111111111UL,
        0x5555555555555555UL,
    }};

    const int multiplier_idx = Common::CountLeadingZeros((u64)d) - 57;

    // Ensure that the index to the multipliers array is within bounds.
    DEBUG_ASSERT((multiplier_idx >= 0) &&
                 (static_cast<size_t>(multiplier_idx) < multipliers.size()));

    const u64 multiplier = multipliers[multiplier_idx];
    const u64 candidate = (b - a) * multiplier;

    // The candidate pattern doesn't match our input value, so fail.
    if (value != candidate)
      return;

    // We have a match! This is a valid logical immediate, so now we have to
    // construct the bits and pieces of the instruction encoding that generates
    // it.
    n = out_n;

    // Count the set bits in our basic stretch. The special case of clz(0) == -1
    // makes the answer come out right for stretches that reach the very top of
    // the word (e.g. numbers like 0xffffc00000000000).
    const int clz_b = (b == 0) ? -1 : Common::CountLeadingZeros(b);
    s = clz_a - clz_b;

    // Decide how many bits to rotate right by, to put the low bit of that basic
    // stretch in position a.
    if (negate)
    {
      // If we inverted the input right at the start of this function, here's
      // where we compensate: the number of set bits becomes the number of clear
      // bits, and the rotation count is based on position b rather than position
      // a (since b is the location of the 'lowest' 1 bit after inversion).
      s = d - s;
      r = (clz_b + 1) & (d - 1);
    }
    else
    {
      r = (clz_a + 1) & (d - 1);
    }

    // Now we're done, except for having to encode the S output in such a way that
    // it gives both the number of set bits and the length of the repeated
    // segment. The s field is encoded like this:
    //
    //     imms    size        S
    //    ssssss    64    UInt(ssssss)
    //    0sssss    32    UInt(sssss)
    //    10ssss    16    UInt(ssss)
    //    110sss     8    UInt(sss)
    //    1110ss     4    UInt(ss)
    //    11110s     2    UInt(s)
    //
    // So we 'or' (-d << 1) with our computed s to form imms.
    s = ((-d << 1) | (s - 1)) & 0x3f;

    valid = true;
  }

  constexpr operator bool() const { return valid; }

  u8 r = 0;
  u8 s = 0;
  bool n = false;
  bool valid = false;
};

class ARM64XEmitter
{
  friend class ARM64FloatEmitter;

private:
  // Pointer to memory where code will be emitted to.
  u8* m_code = nullptr;

  // Pointer past the end of the memory region we're allowed to emit to.
  // Writes that would reach this memory are refused and will set the m_write_failed flag instead.
  u8* m_code_end = nullptr;

  u8* m_lastCacheFlushEnd = nullptr;

  // Set to true when a write request happens that would write past m_code_end.
  // Must be cleared with SetCodePtr() afterwards.
  bool m_write_failed = false;

  void AddImmediate(ARM64Reg Rd, ARM64Reg Rn, u64 imm, bool shift, bool negative, bool flags);
  void EncodeCompareBranchInst(u32 op, ARM64Reg Rt, const void* ptr);
  void EncodeTestBranchInst(u32 op, ARM64Reg Rt, u8 bits, const void* ptr);
  void EncodeUnconditionalBranchInst(u32 op, const void* ptr);
  void EncodeUnconditionalBranchInst(u32 opc, u32 op2, u32 op3, u32 op4, ARM64Reg Rn);
  void EncodeExceptionInst(u32 instenc, u32 imm);
  void EncodeSystemInst(u32 op0, u32 op1, u32 CRn, u32 CRm, u32 op2, ARM64Reg Rt);
  void EncodeArithmeticInst(u32 instenc, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm,
                            ArithOption Option);
  void EncodeArithmeticCarryInst(u32 op, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EncodeCondCompareImmInst(u32 op, ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);
  void EncodeCondCompareRegInst(u32 op, ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);
  void EncodeCondSelectInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
  void EncodeData1SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn);
  void EncodeData2SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EncodeData3SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void EncodeLogicalInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void EncodeLoadRegisterInst(u32 bitop, ARM64Reg Rt, u32 imm);
  void EncodeLoadStoreExcInst(u32 instenc, ARM64Reg Rs, ARM64Reg Rt2, ARM64Reg Rn, ARM64Reg Rt);
  void EncodeLoadStorePairedInst(u32 op, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);
  void EncodeLoadStoreIndexedInst(u32 op, u32 op2, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void EncodeLoadStoreIndexedInst(u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm, u8 size);
  void EncodeMOVWideInst(u32 op, ARM64Reg Rd, u32 imm, ShiftAmount pos);
  void EncodeBitfieldMOVInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
  void EncodeLoadStoreRegisterOffset(u32 size, u32 opc, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void EncodeAddSubImmInst(u32 op, bool flags, u32 shift, u32 imm, ARM64Reg Rn, ARM64Reg Rd);
  void EncodeLogicalImmInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm);
  void EncodeLoadStorePair(u32 op, u32 load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn,
                           s32 imm);
  void EncodeAddressInst(u32 op, ARM64Reg Rd, s32 imm);
  void EncodeLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

  FixupBranch WriteFixupBranch();

  template <typename T>
  void MOVI2RImpl(ARM64Reg Rd, T imm);

protected:
  void Write32(u32 value);

public:
  ARM64XEmitter() = default;
  ARM64XEmitter(u8* code, u8* code_end)
      : m_code(code), m_code_end(code_end), m_lastCacheFlushEnd(code)
  {
  }

  virtual ~ARM64XEmitter() {}

  void SetCodePtr(u8* ptr, u8* end, bool write_failed = false);

  void SetCodePtrUnsafe(u8* ptr, u8* end, bool write_failed = false);
  const u8* GetCodePtr() const;
  u8* GetWritableCodePtr();
  const u8* GetCodeEnd() const;
  u8* GetWritableCodeEnd();
  void ReserveCodeSpace(u32 bytes);
  u8* AlignCode16();
  u8* AlignCodePage();
  void FlushIcache();
  void FlushIcacheSection(u8* start, u8* end);

  // Should be checked after a block of code has been generated to see if the code has been
  // successfully written to memory. Do not call the generated code when this returns true!
  bool HasWriteFailed() const { return m_write_failed; }

  // FixupBranch branching
  void SetJumpTarget(FixupBranch const& branch);
  FixupBranch CBZ(ARM64Reg Rt);
  FixupBranch CBNZ(ARM64Reg Rt);
  FixupBranch B(CCFlags cond);
  FixupBranch TBZ(ARM64Reg Rt, u8 bit);
  FixupBranch TBNZ(ARM64Reg Rt, u8 bit);
  FixupBranch B();
  FixupBranch BL();

  // Compare and Branch
  void CBZ(ARM64Reg Rt, const void* ptr);
  void CBNZ(ARM64Reg Rt, const void* ptr);

  // Conditional Branch
  void B(CCFlags cond, const void* ptr);

  // Test and Branch
  void TBZ(ARM64Reg Rt, u8 bits, const void* ptr);
  void TBNZ(ARM64Reg Rt, u8 bits, const void* ptr);

  // Unconditional Branch
  void B(const void* ptr);
  void BL(const void* ptr);

  // Unconditional Branch (register)
  void BR(ARM64Reg Rn);
  void BLR(ARM64Reg Rn);
  void RET(ARM64Reg Rn = ARM64Reg::X30);
  void ERET();
  void DRPS();

  // Exception generation
  void SVC(u32 imm);
  void HVC(u32 imm);
  void SMC(u32 imm);
  void BRK(u32 imm);
  void HLT(u32 imm);
  void DCPS1(u32 imm);
  void DCPS2(u32 imm);
  void DCPS3(u32 imm);

  // System
  void _MSR(PStateField field, u8 imm);
  void _MSR(PStateField field, ARM64Reg Rt);
  void MRS(ARM64Reg Rt, PStateField field);
  void CNTVCT(ARM64Reg Rt);

  void HINT(SystemHint op);
  void NOP() { HINT(SystemHint::NOP); }
  void SEV() { HINT(SystemHint::SEV); }
  void SEVL() { HINT(SystemHint::SEVL); }
  void WFE() { HINT(SystemHint::WFE); }
  void WFI() { HINT(SystemHint::WFI); }
  void YIELD() { HINT(SystemHint::YIELD); }

  void CLREX();
  void DSB(BarrierType type);
  void DMB(BarrierType type);
  void ISB(BarrierType type);

  // Add/Subtract (Extended/Shifted register)
  void ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
  void ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
  void SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
  void SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
  void CMN(ARM64Reg Rn, ARM64Reg Rm);
  void CMN(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
  void CMP(ARM64Reg Rn, ARM64Reg Rm);
  void CMP(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);

  // Add/Subtract (with carry)
  void ADC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ADCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SBC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SBCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Conditional Compare (immediate)
  void CCMN(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);
  void CCMP(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);

  // Conditional Compare (register)
  void CCMN(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);
  void CCMP(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);

  // Conditional Select
  void CSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
  void CSINC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
  void CSINV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
  void CSNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);

  // Aliases
  void CSET(ARM64Reg Rd, CCFlags cond)
  {
    ARM64Reg zr = Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR;
    CSINC(Rd, zr, zr, (CCFlags)((u32)cond ^ 1));
  }
  void CSETM(ARM64Reg Rd, CCFlags cond)
  {
    ARM64Reg zr = Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR;
    CSINV(Rd, zr, zr, (CCFlags)((u32)cond ^ 1));
  }
  void NEG(ARM64Reg Rd, ARM64Reg Rs) { SUB(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rs); }
  void NEG(ARM64Reg Rd, ARM64Reg Rs, ArithOption Option)
  {
    SUB(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rs, Option);
  }
  void NEGS(ARM64Reg Rd, ARM64Reg Rs) { SUBS(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rs); }
  void NEGS(ARM64Reg Rd, ARM64Reg Rs, ArithOption Option)
  {
    SUBS(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rs, Option);
  }
  // Data-Processing 1 source
  void RBIT(ARM64Reg Rd, ARM64Reg Rn);
  void REV16(ARM64Reg Rd, ARM64Reg Rn);
  void REV32(ARM64Reg Rd, ARM64Reg Rn);
  void REV64(ARM64Reg Rd, ARM64Reg Rn);
  void CLZ(ARM64Reg Rd, ARM64Reg Rn);
  void CLS(ARM64Reg Rd, ARM64Reg Rn);

  // Data-Processing 2 source
  void UDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void LSLV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void LSRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ASRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void RORV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32B(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32H(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32W(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32CB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32CH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32CW(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32X(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void CRC32CX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Data-Processing 3 source
  void MADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void MSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void SMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void SMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void SMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void SMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void UMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void UMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void UMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void UMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void MUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void MNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Logical (shifted register)
  void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
  void TST(ARM64Reg Rn, ARM64Reg Rm) { ANDS(Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, Rm); }
  void TST(ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
  {
    ANDS(Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, Rm, Shift);
  }

  // Wrap the above for saner syntax
  void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    AND(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    BIC(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    ORR(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    ORN(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    EOR(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    EON(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    ANDS(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  void BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
  {
    BICS(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
  }
  // Convenience wrappers around ORR. These match the official convenience syntax.
  void MOV(ARM64Reg Rd, ARM64Reg Rm, ArithOption Shift);
  void MOV(ARM64Reg Rd, ARM64Reg Rm);
  void MVN(ARM64Reg Rd, ARM64Reg Rm);

  // Convenience wrappers around UBFM/EXTR.
  void LSR(ARM64Reg Rd, ARM64Reg Rm, int shift);
  void LSL(ARM64Reg Rd, ARM64Reg Rm, int shift);
  void ASR(ARM64Reg Rd, ARM64Reg Rm, int shift);
  void ROR(ARM64Reg Rd, ARM64Reg Rm, int shift);

  // Logical (immediate)
  void AND(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm);
  void ANDS(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm);
  void EOR(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm);
  void ORR(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm);
  void TST(ARM64Reg Rn, LogicalImm imm);
  // Add/subtract (immediate)
  void ADD(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
  void ADDS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
  void SUB(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
  void SUBS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
  void CMP(ARM64Reg Rn, u32 imm, bool shift = false);
  void CMN(ARM64Reg Rn, u32 imm, bool shift = false);

  // Data Processing (Immediate)
  void MOVZ(ARM64Reg Rd, u32 imm, ShiftAmount pos = ShiftAmount::Shift0);
  void MOVN(ARM64Reg Rd, u32 imm, ShiftAmount pos = ShiftAmount::Shift0);
  void MOVK(ARM64Reg Rd, u32 imm, ShiftAmount pos = ShiftAmount::Shift0);

  // Bitfield move
  void BFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
  void SBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
  void UBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
  void BFI(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width);
  void BFXIL(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width);
  void UBFIZ(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width);

  // Extract register (ROR with two inputs, if same then faster on A67)
  void EXTR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u32 shift);

  // Aliases
  void SXTB(ARM64Reg Rd, ARM64Reg Rn);
  void SXTH(ARM64Reg Rd, ARM64Reg Rn);
  void SXTW(ARM64Reg Rd, ARM64Reg Rn);
  void UXTB(ARM64Reg Rd, ARM64Reg Rn);
  void UXTH(ARM64Reg Rd, ARM64Reg Rn);

  void UBFX(ARM64Reg Rd, ARM64Reg Rn, int lsb, int width) { UBFM(Rd, Rn, lsb, lsb + width - 1); }
  // Load Register (Literal)
  void LDR(ARM64Reg Rt, u32 imm);
  void LDRSW(ARM64Reg Rt, u32 imm);
  void PRFM(ARM64Reg Rt, u32 imm);

  // Load/Store Exclusive
  void STXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void STLXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void LDXRB(ARM64Reg Rt, ARM64Reg Rn);
  void LDAXRB(ARM64Reg Rt, ARM64Reg Rn);
  void STLRB(ARM64Reg Rt, ARM64Reg Rn);
  void LDARB(ARM64Reg Rt, ARM64Reg Rn);
  void STXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void STLXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void LDXRH(ARM64Reg Rt, ARM64Reg Rn);
  void LDAXRH(ARM64Reg Rt, ARM64Reg Rn);
  void STLRH(ARM64Reg Rt, ARM64Reg Rn);
  void LDARH(ARM64Reg Rt, ARM64Reg Rn);
  void STXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void STLXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
  void STXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
  void STLXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
  void LDXR(ARM64Reg Rt, ARM64Reg Rn);
  void LDAXR(ARM64Reg Rt, ARM64Reg Rn);
  void LDXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
  void LDAXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
  void STLR(ARM64Reg Rt, ARM64Reg Rn);
  void LDAR(ARM64Reg Rt, ARM64Reg Rn);

  // Load/Store no-allocate pair (offset)
  void STNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);
  void LDNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);

  // Load/Store register (immediate indexed)
  void STRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDRSB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDRSH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDRSW(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

  // Load/Store register (register offset)
  void STRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDRSB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void STRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDRSH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void STR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDRSW(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void PRFM(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);

  // Load/Store register (unscaled offset)
  void STURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDURSB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDURSH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void LDURSW(ARM64Reg Rt, ARM64Reg Rn, s32 imm);

  // Load/Store pair
  void LDP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
  void LDPSW(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
  void STP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);

  // Address of label/page PC-relative
  void ADR(ARM64Reg Rd, s32 imm);
  void ADRP(ARM64Reg Rd, s64 imm);

  // Wrapper around ADR/ADRP/MOVZ/MOVN/MOVK
  void MOVI2R(ARM64Reg Rd, u64 imm);
  bool MOVI2R2(ARM64Reg Rd, u64 imm1, u64 imm2);
  template <class P>
  void MOVP2R(ARM64Reg Rd, P* ptr)
  {
    ASSERT_MSG(DYNA_REC, Is64Bit(Rd), "Can't store pointers in 32-bit registers");
    MOVI2R(Rd, (uintptr_t)ptr);
  }

  // Wrapper around AND x, y, imm etc.
  // If you are sure the imm will work, preferably construct a LogicalImm directly instead,
  // since that is constexpr and thus can be done at compile-time for constant values.
  void ANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void ANDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void TSTI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch)
  {
    ANDSI2R(Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, imm, scratch);
  }
  void ORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void EORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);

  void ADDI2R_internal(ARM64Reg Rd, ARM64Reg Rn, u64 imm, bool negative, bool flags,
                       ARM64Reg scratch);
  void ADDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = ARM64Reg::INVALID_REG);
  void ADDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = ARM64Reg::INVALID_REG);
  void SUBI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = ARM64Reg::INVALID_REG);
  void SUBSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = ARM64Reg::INVALID_REG);
  void CMPI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch = ARM64Reg::INVALID_REG);

  bool TryADDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm);
  bool TrySUBI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm);
  bool TryCMPI2R(ARM64Reg Rn, u64 imm);

  bool TryANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm);
  bool TryORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm);
  bool TryEORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm);

  // ABI related
  static constexpr BitSet32 CALLER_SAVED_GPRS = BitSet32(0x4007FFFF);
  static constexpr BitSet32 CALLER_SAVED_FPRS = BitSet32(0xFFFF00FF);
  void ABI_PushRegisters(BitSet32 registers);
  void ABI_PopRegisters(BitSet32 registers, BitSet32 ignore_mask = BitSet32(0));

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

  // This function expects you to have set up the state.
  // Overwrites X0 and X8
  template <typename T, typename... Args>
  ARM64Reg ABI_SetupLambda(const std::function<T(Args...)>* f)
  {
    auto trampoline = &ARM64XEmitter::CallLambdaTrampoline<T, Args...>;
    MOVP2R(ARM64Reg::X8, trampoline);
    MOVP2R(ARM64Reg::X0, const_cast<void*>((const void*)f));
    return ARM64Reg::X8;
  }

  // Plain function call
  void QuickCallFunction(ARM64Reg scratchreg, const void* func);
  template <typename T>
  void QuickCallFunction(ARM64Reg scratchreg, T func)
  {
    QuickCallFunction(scratchreg, (const void*)func);
  }
};

class ARM64FloatEmitter
{
public:
  ARM64FloatEmitter(ARM64XEmitter* emit) : m_emit(emit) {}
  void LDR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

  // Loadstore unscaled
  void LDUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void STUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

  // Loadstore single structure
  void LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn);
  void LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm);
  void LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn);
  void LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn);
  void LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
  void LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
  void ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn);
  void ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm);

  // Loadstore multiple structure
  void LD1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn);
  void LD1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm = ARM64Reg::SP);
  void ST1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn);
  void ST1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm = ARM64Reg::SP);

  // Loadstore paired
  void LDP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
  void STP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);

  // Loadstore register offset
  void STR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void LDR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);

  // Scalar - 1 Source
  void FABS(ARM64Reg Rd, ARM64Reg Rn);
  void FNEG(ARM64Reg Rd, ARM64Reg Rn);
  void FSQRT(ARM64Reg Rd, ARM64Reg Rn);
  void FRINTI(ARM64Reg Rd, ARM64Reg Rn);
  void FMOV(ARM64Reg Rd, ARM64Reg Rn, bool top = false);  // Also generalized move between GPR/FP
  void FRECPE(ARM64Reg Rd, ARM64Reg Rn);
  void FRSQRTE(ARM64Reg Rd, ARM64Reg Rn);

  // Scalar - 2 Source
  void ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMAX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMIN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMAXNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMINNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FNMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Scalar - 3 Source. Note - the accumulator is last on ARM!
  void FMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void FMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void FNMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
  void FNMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);

  // Scalar floating point immediate
  void FMOV(ARM64Reg Rd, uint8_t imm8);

  // Vector
  void ADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void BIF(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void BIT(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void BSL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);
  void FABS(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMAX(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMLA(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMLS(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMIN(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FCVTL(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCVTL2(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCVTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void FCVTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void FCVTZS(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCVTZU(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FDIV(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FNEG(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FRECPE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FRSQRTE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FSUB(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void NOT(ARM64Reg Rd, ARM64Reg Rn);
  void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void MOV(ARM64Reg Rd, ARM64Reg Rn) { ORR(Rd, Rn, Rn); }
  void REV16(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void REV32(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void REV64(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale);
  void UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale);
  void SQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void SQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void UQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void UQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void XTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
  void XTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);

  // Move
  void DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void INS(u8 size, ARM64Reg Rd, u8 index, ARM64Reg Rn);
  void INS(u8 size, ARM64Reg Rd, u8 index1, ARM64Reg Rn, u8 index2);
  void UMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);
  void SMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);

  // One source
  void FCVT(u8 size_to, u8 size_from, ARM64Reg Rd, ARM64Reg Rn);

  // Scalar convert float to int, in a lot of variants.
  // Note that the scalar version of this operation has two encodings, one that goes to an integer
  // register
  // and one that outputs to a scalar fp register.
  void FCVTS(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round);
  void FCVTU(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round);

  // Scalar convert int to float. No rounding mode specifier necessary.
  void SCVTF(ARM64Reg Rd, ARM64Reg Rn);
  void UCVTF(ARM64Reg Rd, ARM64Reg Rn);

  // Scalar fixed point to float. scale is the number of fractional bits.
  void SCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale);
  void UCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale);

  // Float comparison
  void FCMP(ARM64Reg Rn, ARM64Reg Rm);
  void FCMP(ARM64Reg Rn);
  void FCMPE(ARM64Reg Rn, ARM64Reg Rm);
  void FCMPE(ARM64Reg Rn);
  void FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCMLE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FCMLT(u8 size, ARM64Reg Rd, ARM64Reg Rn);
  void FACGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void FACGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Conditional select
  void FCSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);

  // Permute
  void UZP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void TRN1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ZIP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void UZP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void TRN2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void ZIP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

  // Shift by immediate
  void SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SSHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void USHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SHRN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
  void SXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
  void UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
  void UXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);

  // vector x indexed element
  void FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index);
  void FMLA(u8 esize, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index);

  // Modified Immediate
  void MOVI(u8 size, ARM64Reg Rd, u64 imm, u8 shift = 0);
  void ORR(u8 size, ARM64Reg Rd, u8 imm, u8 shift = 0);
  void BIC(u8 size, ARM64Reg Rd, u8 imm, u8 shift = 0);

  void MOVI2F(ARM64Reg Rd, float value, ARM64Reg scratch = ARM64Reg::INVALID_REG,
              bool negate = false);
  void MOVI2FDUP(ARM64Reg Rd, float value, ARM64Reg scratch = ARM64Reg::INVALID_REG);

  // ABI related
  void ABI_PushRegisters(BitSet32 registers, ARM64Reg tmp = ARM64Reg::INVALID_REG);
  void ABI_PopRegisters(BitSet32 registers, ARM64Reg tmp = ARM64Reg::INVALID_REG);

private:
  ARM64XEmitter* m_emit;
  inline void Write32(u32 value) { m_emit->Write32(value); }
  // Emitting functions
  void EmitLoadStoreImmediate(u8 size, u32 opc, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void EmitScalar2Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn,
                         ARM64Reg Rm);
  void EmitScalarThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EmitThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EmitCopy(bool Q, u32 op, u32 imm5, u32 imm4, ARM64Reg Rd, ARM64Reg Rn);
  void EmitScalar2RegMisc(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void Emit2RegMisc(bool Q, bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt,
                                    ARM64Reg Rn);
  void EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt,
                                    ARM64Reg Rn, ARM64Reg Rm);
  void Emit1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitConversion(bool sf, bool S, u32 type, u32 rmode, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitConversion2(bool sf, bool S, bool direction, u32 type, u32 rmode, u32 opcode, int scale,
                       ARM64Reg Rd, ARM64Reg Rn);
  void EmitCompare(bool M, bool S, u32 op, u32 opcode2, ARM64Reg Rn, ARM64Reg Rm);
  void EmitCondSelect(bool M, bool S, CCFlags cond, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EmitPermute(u32 size, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EmitScalarImm(bool M, bool S, u32 type, u32 imm5, ARM64Reg Rd, u32 imm8);
  void EmitShiftImm(bool Q, bool U, u32 immh, u32 immb, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitScalarShiftImm(bool U, u32 immh, u32 immb, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitLoadStoreMultipleStructure(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn);
  void EmitLoadStoreMultipleStructurePost(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn,
                                          ARM64Reg Rm);
  void EmitScalar1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitVectorxElement(bool U, u32 size, bool L, u32 opcode, bool H, ARM64Reg Rd, ARM64Reg Rn,
                          ARM64Reg Rm);
  void EmitLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
  void EmitConvertScalarToInt(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round, bool sign);
  void EmitScalar3Source(bool isDouble, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra,
                         int opcode);
  void EncodeLoadStorePair(u32 size, bool load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2,
                           ARM64Reg Rn, s32 imm);
  void EncodeLoadStoreRegisterOffset(u32 size, bool load, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
  void EncodeModImm(bool Q, u8 op, u8 cmode, u8 o2, ARM64Reg Rd, u8 abcdefgh);

  void ORR_BIC(u8 size, ARM64Reg Rd, u8 imm, u8 shift, u8 op);

  void SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
  void USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
  void SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
  void SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper);
  void UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper);
};

class ARM64CodeBlock : public Common::CodeBlock<ARM64XEmitter>
{
private:
  void PoisonMemory() override
  {
    // If our memory isn't a multiple of u32 then this won't write the last remaining bytes with
    // anything
    // Less than optimal, but there would be nothing we could do but throw a runtime warning anyway.
    // AArch64: 0xD4200000 = BRK 0
    constexpr u32 brk_0 = 0xD4200000;

    for (size_t i = 0; i < region_size; i += sizeof(u32))
    {
      std::memcpy(region + i, &brk_0, sizeof(u32));
    }
  }
};
}  // namespace Arm64Gen
