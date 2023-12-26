// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <bit>
#include <cstring>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "Common/ArmCommon.h"
#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/BitUtils.h"
#include "Common/CodeBlock.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/SmallVector.h"

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

enum class GPRSize
{
  B32,
  B64,
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
  ArithOption(ARM64Reg Rd, ExtendSpecifier extend_type, u32 shift = 0)
  {
    m_destReg = Rd;
    m_width = Is64Bit(Rd) ? WidthSpecifier::Width64Bit : WidthSpecifier::Width32Bit;
    m_extend = extend_type;
    m_type = TypeSpecifier::ExtendedReg;
    m_shifttype = ShiftType::LSL;
    m_shift = shift;
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
      m_extend = ExtendSpecifier::UXTX;
    }
    else
    {
      m_width = WidthSpecifier::Width32Bit;
      if (shift == 32)
        m_shift = 0;
      m_extend = ExtendSpecifier::UXTW;
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

  constexpr LogicalImm(u64 value, GPRSize size)
  {
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

    if (size == GPRSize::B32)
    {
      // To handle 32-bit logical immediates, the very easiest thing is to repeat
      // the input value twice to make a 64-bit word. The correct encoding of that
      // as a logical immediate will also be the correct encoding of the 32-bit
      // value.

      value = (value << 32) | (value & 0xFFFFFFFF);
    }

    if (value == 0 || (~value) == 0)
    {
      valid = false;
      return;
    }

    // Normalize value, rotating it such that the LSB is 1:
    // If LSB is already one, we mask away the trailing sequence of ones and
    // pick the next sequence of ones. This ensures we get a complete element
    // that has not been cut-in-half due to rotation across the word boundary.

    const int rotation = std::countr_zero(value & (value + 1));
    const u64 normalized = std::rotr(value, rotation);

    const int element_size = std::countr_zero(normalized & (normalized + 1));
    const int ones = std::countr_one(normalized);

    // Check the value is repeating; also ensures element size is a power of two.

    if (std::rotr(value, element_size) != value)
    {
      valid = false;
      return;
    }

    // Now we're done. We just have to encode the S output in such a way that
    // it gives both the number of set bits and the length of the repeated
    // segment.

    r = static_cast<u8>((element_size - rotation) & (element_size - 1));
    s = static_cast<u8>((((~element_size + 1) << 1) | (ones - 1)) & 0x3f);
    n = Common::ExtractBit<6>(element_size);

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
  struct RegisterMove
  {
    ARM64Reg dst;
    ARM64Reg src;
  };

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

  [[nodiscard]] FixupBranch WriteFixupBranch();

  // This function solves the "parallel moves" problem common in compilers.
  // The arguments are mutated!
  void ParallelMoves(RegisterMove* begin, RegisterMove* end, std::array<u8, 32>* source_gpr_usages);

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
  [[nodiscard]] FixupBranch CBZ(ARM64Reg Rt);
  [[nodiscard]] FixupBranch CBNZ(ARM64Reg Rt);
  [[nodiscard]] FixupBranch B(CCFlags cond);
  [[nodiscard]] FixupBranch TBZ(ARM64Reg Rt, u8 bit);
  [[nodiscard]] FixupBranch TBNZ(ARM64Reg Rt, u8 bit);
  [[nodiscard]] FixupBranch B();
  [[nodiscard]] FixupBranch BL();

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
    MOVI2R(Rd, reinterpret_cast<uintptr_t>(ptr));
  }
  template <class P>
  // Given an address, stores the page address into a register and returns the page-relative offset
  s32 MOVPage2R(ARM64Reg Rd, P* ptr)
  {
    ASSERT_MSG(DYNA_REC, Is64Bit(Rd), "Can't store pointers in 32-bit registers");
    MOVI2R(Rd, reinterpret_cast<uintptr_t>(ptr) & ~0xFFFULL);
    return static_cast<s32>(reinterpret_cast<uintptr_t>(ptr) & 0xFFFULL);
  }

  // Wrappers around bitwise operations with an immediate. If you're sure an imm can be encoded
  // without a scratch register, preferably construct a LogicalImm directly instead,
  // since that is constexpr and thus can be done at compile time for constant values.
  void ANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void ANDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void TSTI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch)
  {
    ANDSI2R(Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, imm, scratch);
  }
  void ORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);
  void EORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch);

  // Wrappers around arithmetic operations with an immediate.
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

  // Plain function call
  void QuickCallFunction(ARM64Reg scratchreg, const void* func);
  template <typename T>
  void QuickCallFunction(ARM64Reg scratchreg, T func)
  {
    QuickCallFunction(scratchreg, (const void*)func);
  }

  template <typename FuncRet, typename... FuncArgs, typename... Args>
  void ABI_CallFunction(FuncRet (*func)(FuncArgs...), Args... args)
  {
    static_assert(sizeof...(FuncArgs) == sizeof...(Args), "Wrong number of arguments");
    static_assert(sizeof...(FuncArgs) <= 8, "Passing arguments on the stack is not supported");

    if constexpr (!std::is_void_v<FuncRet>)
      static_assert(sizeof(FuncRet) <= 16, "Large return types are not supported");

    std::array<u8, 32> source_gpr_uses{};

    auto check_argument = [&](auto& arg) {
      using Arg = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<Arg, ARM64Reg>)
      {
        ASSERT(IsGPR(arg));
        source_gpr_uses[DecodeReg(arg)]++;
      }
      else
      {
        // To be more correct, we should be checking FuncArgs here rather than Args, but that's a
        // lot more effort to implement. Let's just do these best-effort checks for now.
        static_assert(!std::is_floating_point_v<Arg>, "Floating-point arguments are not supported");
        static_assert(sizeof(Arg) <= 8, "Arguments bigger than a register are not supported");
      }
    };

    (check_argument(args), ...);

    {
      Common::SmallVector<RegisterMove, sizeof...(Args)> pending_moves;

      size_t i = 0;

      auto handle_register_argument = [&](auto& arg) {
        using Arg = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<Arg, ARM64Reg>)
        {
          const ARM64Reg dst_reg =
              (Is64Bit(arg) ? EncodeRegTo64 : EncodeRegTo32)(static_cast<ARM64Reg>(i));

          if (dst_reg == arg)
          {
            // The value is already in the right register.
            source_gpr_uses[DecodeReg(arg)]--;
          }
          else if (source_gpr_uses[i] == 0)
          {
            // The destination register isn't used as the source of another move.
            // We can go ahead and do the move right away.
            MOV(dst_reg, arg);
            source_gpr_uses[DecodeReg(arg)]--;
          }
          else
          {
            // The destination register is used as the source of a move we haven't gotten to yet.
            // Let's record that we need to deal with this move later.
            pending_moves.emplace_back(dst_reg, arg);
          }
        }

        ++i;
      };

      (handle_register_argument(args), ...);

      if (!pending_moves.empty())
      {
        ParallelMoves(pending_moves.data(), pending_moves.data() + pending_moves.size(),
                      &source_gpr_uses);
      }
    }

    {
      size_t i = 0;

      auto handle_immediate_argument = [&](auto& arg) {
        using Arg = std::decay_t<decltype(arg)>;

        if constexpr (!std::is_same_v<Arg, ARM64Reg>)
        {
          const ARM64Reg dst_reg =
              (sizeof(arg) == 8 ? EncodeRegTo64 : EncodeRegTo32)(static_cast<ARM64Reg>(i));
          if constexpr (std::is_pointer_v<Arg>)
            MOVP2R(dst_reg, arg);
          else
            MOVI2R(dst_reg, arg);
        }

        ++i;
      };

      (handle_immediate_argument(args), ...);
    }

    QuickCallFunction(ARM64Reg::X8, func);
  }

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

  template <typename FuncRet, typename... FuncArgs, typename... Args>
  void ABI_CallLambdaFunction(const std::function<FuncRet(FuncArgs...)>* f, Args... args)
  {
    auto trampoline = &ARM64XEmitter::CallLambdaTrampoline<FuncRet, FuncArgs...>;
    ABI_CallFunction(trampoline, f, args...);
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

  // Scalar - pairwise
  void FADDP(ARM64Reg Rd, ARM64Reg Rn);
  void FMAXP(ARM64Reg Rd, ARM64Reg Rn);
  void FMINP(ARM64Reg Rd, ARM64Reg Rn);
  void FMAXNMP(ARM64Reg Rd, ARM64Reg Rn);
  void FMINNMP(ARM64Reg Rd, ARM64Reg Rn);

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

  // Extract
  void EXT(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u32 index);

  // Scalar shift by immediate
  void SHL(ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void URSHR(ARM64Reg Rd, ARM64Reg Rn, u32 shift);

  // Vector shift by immediate
  void SHL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void SSHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
  void URSHR(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
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
  void EmitScalarPairwise(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
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
  void EmitExtract(u32 imm4, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
  void EmitScalarImm(bool M, bool S, u32 type, u32 imm5, ARM64Reg Rd, u32 imm8);
  void EmitShiftImm(bool Q, bool U, u32 imm, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
  void EmitScalarShiftImm(bool U, u32 imm, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
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
