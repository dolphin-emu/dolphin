// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Arm64Emitter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/SmallVector.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#endif

namespace Arm64Gen
{
namespace
{
// For ADD/SUB
std::optional<std::pair<u32, bool>> IsImmArithmetic(uint64_t input)
{
  if (input < 4096)
    return std::pair{static_cast<u32>(input), false};

  if ((input & 0xFFF000) == input)
    return std::pair{static_cast<u32>(input >> 12), true};

  return std::nullopt;
}

float FPImm8ToFloat(u8 bits)
{
  const u32 sign = bits >> 7;
  const u32 bit6 = (bits >> 6) & 1;
  const u32 exp = ((!bit6) << 7) | (0x7C * bit6) | ((bits >> 4) & 3);
  const u32 mantissa = (bits & 0xF) << 19;
  const u32 f = (sign << 31) | (exp << 23) | mantissa;

  return std::bit_cast<float>(f);
}

std::optional<u8> FPImm8FromFloat(float value)
{
  const u32 f = std::bit_cast<u32>(value);
  const u32 mantissa4 = (f & 0x7FFFFF) >> 19;
  const u32 exponent = (f >> 23) & 0xFF;
  const u32 sign = f >> 31;

  if ((exponent >> 7) == ((exponent >> 6) & 1))
    return std::nullopt;

  const u8 imm8 = (sign << 7) | ((!(exponent >> 7)) << 6) | ((exponent & 3) << 4) | mantissa4;
  const float new_float = FPImm8ToFloat(imm8);

  if (new_float != value)
    return std::nullopt;

  return imm8;
}
}  // Anonymous namespace

void ARM64XEmitter::SetCodePtrUnsafe(u8* ptr, u8* end, bool write_failed)
{
  m_code = ptr;
  m_code_end = end;
  m_write_failed = write_failed;
}

void ARM64XEmitter::SetCodePtr(u8* ptr, u8* end, bool write_failed)
{
  SetCodePtrUnsafe(ptr, end, write_failed);
  m_lastCacheFlushEnd = ptr;
}

const u8* ARM64XEmitter::GetCodePtr() const
{
  return m_code;
}

u8* ARM64XEmitter::GetWritableCodePtr()
{
  return m_code;
}

const u8* ARM64XEmitter::GetCodeEnd() const
{
  return m_code_end;
}

u8* ARM64XEmitter::GetWritableCodeEnd()
{
  return m_code_end;
}

void ARM64XEmitter::ReserveCodeSpace(u32 bytes)
{
  for (u32 i = 0; i < bytes / 4; i++)
    BRK(0);
}

u8* ARM64XEmitter::AlignCode16()
{
  int c = int((u64)m_code & 15);
  if (c)
    ReserveCodeSpace(16 - c);
  return m_code;
}

u8* ARM64XEmitter::AlignCodePage()
{
  int c = int((u64)m_code & 4095);
  if (c)
    ReserveCodeSpace(4096 - c);
  return m_code;
}

void ARM64XEmitter::Write32(u32 value)
{
  if (m_code + sizeof(u32) > m_code_end)
  {
    m_code = m_code_end;
    m_write_failed = true;
    return;
  }

  std::memcpy(m_code, &value, sizeof(u32));
  m_code += sizeof(u32);
}

void ARM64XEmitter::FlushIcache()
{
  FlushIcacheSection(m_lastCacheFlushEnd, m_code);
  m_lastCacheFlushEnd = m_code;
}

void ARM64XEmitter::FlushIcacheSection(u8* start, u8* end)
{
  if (start == end)
    return;

#if defined(IOS) || defined(__APPLE__)
  // Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
  sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#elif defined(WIN32)
  FlushInstructionCache(GetCurrentProcess(), start, end - start);
#else
  // Don't rely on GCC's __clear_cache implementation, as it caches
  // icache/dcache cache line sizes, that can vary between cores on
  // big.LITTLE architectures.
  u64 addr, ctr_el0;
  static size_t icache_line_size = 0xffff, dcache_line_size = 0xffff;
  size_t isize, dsize;

  __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr_el0));
  isize = 4 << ((ctr_el0 >> 0) & 0xf);
  dsize = 4 << ((ctr_el0 >> 16) & 0xf);

  // use the global minimum cache line size
  icache_line_size = isize = icache_line_size < isize ? icache_line_size : isize;
  dcache_line_size = dsize = dcache_line_size < dsize ? dcache_line_size : dsize;

  addr = (u64)start & ~(u64)(dsize - 1);
  for (; addr < (u64)end; addr += dsize)
    // use "civac" instead of "cvau", as this is the suggested workaround for
    // Cortex-A53 errata 819472, 826319, 827319 and 824069.
    __asm__ volatile("dc civac, %0" : : "r"(addr) : "memory");
  __asm__ volatile("dsb ish" : : : "memory");

  addr = (u64)start & ~(u64)(isize - 1);
  for (; addr < (u64)end; addr += isize)
    __asm__ volatile("ic ivau, %0" : : "r"(addr) : "memory");

  __asm__ volatile("dsb ish" : : : "memory");
  __asm__ volatile("isb" : : : "memory");
#endif
}

// Exception generation
static const u32 ExcEnc[][3] = {
    {0, 0, 1},  // SVC
    {0, 0, 2},  // HVC
    {0, 0, 3},  // SMC
    {1, 0, 0},  // BRK
    {2, 0, 0},  // HLT
    {5, 0, 1},  // DCPS1
    {5, 0, 2},  // DCPS2
    {5, 0, 3},  // DCPS3
};

// Arithmetic generation
static const u32 ArithEnc[] = {
    0x058,  // ADD
    0x258,  // SUB
};

// Conditional Select
static const u32 CondSelectEnc[][2] = {
    {0, 0},  // CSEL
    {0, 1},  // CSINC
    {1, 0},  // CSINV
    {1, 1},  // CSNEG
};

// Data-Processing (1 source)
static const u32 Data1SrcEnc[][2] = {
    {0, 0},  // RBIT
    {0, 1},  // REV16
    {0, 2},  // REV32
    {0, 3},  // REV64
    {0, 4},  // CLZ
    {0, 5},  // CLS
};

// Data-Processing (2 source)
static const u32 Data2SrcEnc[] = {
    0x02,  // UDIV
    0x03,  // SDIV
    0x08,  // LSLV
    0x09,  // LSRV
    0x0A,  // ASRV
    0x0B,  // RORV
    0x10,  // CRC32B
    0x11,  // CRC32H
    0x12,  // CRC32W
    0x14,  // CRC32CB
    0x15,  // CRC32CH
    0x16,  // CRC32CW
    0x13,  // CRC32X (64bit Only)
    0x17,  // XRC32CX (64bit Only)
};

// Data-Processing (3 source)
static const u32 Data3SrcEnc[][2] = {
    {0, 0},  // MADD
    {0, 1},  // MSUB
    {1, 0},  // SMADDL (64Bit Only)
    {1, 1},  // SMSUBL (64Bit Only)
    {2, 0},  // SMULH (64Bit Only)
    {5, 0},  // UMADDL (64Bit Only)
    {5, 1},  // UMSUBL (64Bit Only)
    {6, 0},  // UMULH (64Bit Only)
};

// Logical (shifted register)
static const u32 LogicalEnc[][2] = {
    {0, 0},  // AND
    {0, 1},  // BIC
    {1, 0},  // OOR
    {1, 1},  // ORN
    {2, 0},  // EOR
    {2, 1},  // EON
    {3, 0},  // ANDS
    {3, 1},  // BICS
};

// Load/Store Exclusive
static const u32 LoadStoreExcEnc[][5] = {
    {0, 0, 0, 0, 0},  // STXRB
    {0, 0, 0, 0, 1},  // STLXRB
    {0, 0, 1, 0, 0},  // LDXRB
    {0, 0, 1, 0, 1},  // LDAXRB
    {0, 1, 0, 0, 1},  // STLRB
    {0, 1, 1, 0, 1},  // LDARB
    {1, 0, 0, 0, 0},  // STXRH
    {1, 0, 0, 0, 1},  // STLXRH
    {1, 0, 1, 0, 0},  // LDXRH
    {1, 0, 1, 0, 1},  // LDAXRH
    {1, 1, 0, 0, 1},  // STLRH
    {1, 1, 1, 0, 1},  // LDARH
    {2, 0, 0, 0, 0},  // STXR
    {3, 0, 0, 0, 0},  // (64bit) STXR
    {2, 0, 0, 0, 1},  // STLXR
    {3, 0, 0, 0, 1},  // (64bit) STLXR
    {2, 0, 0, 1, 0},  // STXP
    {3, 0, 0, 1, 0},  // (64bit) STXP
    {2, 0, 0, 1, 1},  // STLXP
    {3, 0, 0, 1, 1},  // (64bit) STLXP
    {2, 0, 1, 0, 0},  // LDXR
    {3, 0, 1, 0, 0},  // (64bit) LDXR
    {2, 0, 1, 0, 1},  // LDAXR
    {3, 0, 1, 0, 1},  // (64bit) LDAXR
    {2, 0, 1, 1, 0},  // LDXP
    {3, 0, 1, 1, 0},  // (64bit) LDXP
    {2, 0, 1, 1, 1},  // LDAXP
    {3, 0, 1, 1, 1},  // (64bit) LDAXP
    {2, 1, 0, 0, 1},  // STLR
    {3, 1, 0, 0, 1},  // (64bit) STLR
    {2, 1, 1, 0, 1},  // LDAR
    {3, 1, 1, 0, 1},  // (64bit) LDAR
};

void ARM64XEmitter::EncodeCompareBranchInst(u32 op, ARM64Reg Rt, const void* ptr)
{
  bool b64Bit = Is64Bit(Rt);
  s64 distance = (s64)ptr - (s64)m_code;

  ASSERT_MSG(DYNA_REC, !(distance & 0x3), "Distance must be a multiple of 4: {}", distance);

  distance >>= 2;

  ASSERT_MSG(DYNA_REC, distance >= -0x40000 && distance <= 0x3FFFF,
             "Received too large distance: {}", distance);

  Write32((b64Bit << 31) | (0x34 << 24) | (op << 24) | (((u32)distance << 5) & 0xFFFFE0) |
          DecodeReg(Rt));
}

void ARM64XEmitter::EncodeTestBranchInst(u32 op, ARM64Reg Rt, u8 bits, const void* ptr)
{
  u8 b40 = bits & 0x1F;
  u8 b5 = (bits >> 5) & 0x1;
  s64 distance = (s64)ptr - (s64)m_code;

  ASSERT_MSG(DYNA_REC, !(distance & 0x3), "distance must be a multiple of 4: {}", distance);

  distance >>= 2;

  ASSERT_MSG(DYNA_REC, distance >= -0x3FFF && distance < 0x3FFF, "Received too large distance: {}",
             distance);

  Write32((b5 << 31) | (0x36 << 24) | (op << 24) | (b40 << 19) |
          ((static_cast<u32>(distance) << 5) & 0x7FFE0) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeUnconditionalBranchInst(u32 op, const void* ptr)
{
  s64 distance = (s64)ptr - s64(m_code);

  ASSERT_MSG(DYNA_REC, !(distance & 0x3), "distance must be a multiple of 4: {}", distance);

  distance >>= 2;

  ASSERT_MSG(DYNA_REC, distance >= -0x2000000LL && distance <= 0x1FFFFFFLL,
             "Received too large distance: {}", distance);

  Write32((op << 31) | (0x5 << 26) | (distance & 0x3FFFFFF));
}

void ARM64XEmitter::EncodeUnconditionalBranchInst(u32 opc, u32 op2, u32 op3, u32 op4, ARM64Reg Rn)
{
  Write32((0x6B << 25) | (opc << 21) | (op2 << 16) | (op3 << 10) | (DecodeReg(Rn) << 5) | op4);
}

void ARM64XEmitter::EncodeExceptionInst(u32 instenc, u32 imm)
{
  ASSERT_MSG(DYNA_REC, !(imm & ~0xFFFF), "Exception instruction too large immediate: {}", imm);

  Write32((0xD4 << 24) | (ExcEnc[instenc][0] << 21) | (imm << 5) | (ExcEnc[instenc][1] << 2) |
          ExcEnc[instenc][2]);
}

void ARM64XEmitter::EncodeSystemInst(u32 op0, u32 op1, u32 CRn, u32 CRm, u32 op2, ARM64Reg Rt)
{
  Write32((0x354 << 22) | (op0 << 19) | (op1 << 16) | (CRn << 12) | (CRm << 8) | (op2 << 5) |
          DecodeReg(Rt));
}

void ARM64XEmitter::EncodeArithmeticInst(u32 instenc, bool flags, ARM64Reg Rd, ARM64Reg Rn,
                                         ARM64Reg Rm, ArithOption Option)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (flags << 29) | (ArithEnc[instenc] << 21) |
          (Option.IsExtended() ? (1 << 21) : 0) | (DecodeReg(Rm) << 16) | Option.GetData() |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeArithmeticCarryInst(u32 op, bool flags, ARM64Reg Rd, ARM64Reg Rn,
                                              ARM64Reg Rm)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (op << 30) | (flags << 29) | (0xD0 << 21) | (DecodeReg(Rm) << 16) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeCondCompareImmInst(u32 op, ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
  bool b64Bit = Is64Bit(Rn);

  ASSERT_MSG(DYNA_REC, !(imm & ~0x1F), "too large immediate: {}", imm);
  ASSERT_MSG(DYNA_REC, !(nzcv & ~0xF), "Flags out of range: {}", nzcv);

  Write32((b64Bit << 31) | (op << 30) | (1 << 29) | (0xD2 << 21) | (imm << 16) | (cond << 12) |
          (1 << 11) | (DecodeReg(Rn) << 5) | nzcv);
}

void ARM64XEmitter::EncodeCondCompareRegInst(u32 op, ARM64Reg Rn, ARM64Reg Rm, u32 nzcv,
                                             CCFlags cond)
{
  bool b64Bit = Is64Bit(Rm);

  ASSERT_MSG(DYNA_REC, !(nzcv & ~0xF), "Flags out of range: {}", nzcv);

  Write32((b64Bit << 31) | (op << 30) | (1 << 29) | (0xD2 << 21) | (DecodeReg(Rm) << 16) |
          (cond << 12) | (DecodeReg(Rn) << 5) | nzcv);
}

void ARM64XEmitter::EncodeCondSelectInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm,
                                         CCFlags cond)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (CondSelectEnc[instenc][0] << 30) | (0xD4 << 21) |
          (DecodeReg(Rm) << 16) | (cond << 12) | (CondSelectEnc[instenc][1] << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeData1SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (0x2D6 << 21) | (Data1SrcEnc[instenc][0] << 16) |
          (Data1SrcEnc[instenc][1] << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeData2SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (0x0D6 << 21) | (DecodeReg(Rm) << 16) | (Data2SrcEnc[instenc] << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeData3SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm,
                                       ARM64Reg Ra)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (0xD8 << 21) | (Data3SrcEnc[instenc][0] << 21) | (DecodeReg(Rm) << 16) |
          (Data3SrcEnc[instenc][1] << 15) | (DecodeReg(Ra) << 10) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLogicalInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm,
                                      ArithOption Shift)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (LogicalEnc[instenc][0] << 29) | (0x5 << 25) |
          (LogicalEnc[instenc][1] << 21) | Shift.GetData() | (DecodeReg(Rm) << 16) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLoadRegisterInst(u32 bitop, ARM64Reg Rt, u32 imm)
{
  bool b64Bit = Is64Bit(Rt);
  bool bVec = IsVector(Rt);

  ASSERT_MSG(DYNA_REC, !(imm & 0xFFFFF), "offset too large {}", imm);

  if (b64Bit && bitop != 0x2)  // LDRSW(0x2) uses 64bit reg, doesn't have 64bit bit set
    bitop |= 0x1;
  Write32((bitop << 30) | (bVec << 26) | (0x18 << 24) | (imm << 5) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeLoadStoreExcInst(u32 instenc, ARM64Reg Rs, ARM64Reg Rt2, ARM64Reg Rn,
                                           ARM64Reg Rt)
{
  Write32((LoadStoreExcEnc[instenc][0] << 30) | (0x8 << 24) | (LoadStoreExcEnc[instenc][1] << 23) |
          (LoadStoreExcEnc[instenc][2] << 22) | (LoadStoreExcEnc[instenc][3] << 21) |
          (DecodeReg(Rs) << 16) | (LoadStoreExcEnc[instenc][4] << 15) | (DecodeReg(Rt2) << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeLoadStorePairedInst(u32 op, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn,
                                              u32 imm)
{
  bool b64Bit = Is64Bit(Rt);
  bool b128Bit = IsQuad(Rt);
  bool bVec = IsVector(Rt);

  if (b128Bit)
  {
    ASSERT_MSG(DYNA_REC, (imm & 0xf) == 0, "128-bit load/store must use aligned offset: {}", imm);
    imm >>= 4;
  }
  else if (b64Bit)
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x7) == 0, "64-bit load/store must use aligned offset: {}", imm);
    imm >>= 3;
  }
  else
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x3) == 0, "32-bit load/store must use aligned offset: {}", imm);
    imm >>= 2;
  }

  ASSERT_MSG(DYNA_REC, (imm & ~0xF) == 0, "offset too large {}", imm);

  u32 opc = 0;
  if (b128Bit)
    opc = 2;
  else if (b64Bit && bVec)
    opc = 1;
  else if (b64Bit && !bVec)
    opc = 2;

  Write32((opc << 30) | (bVec << 26) | (op << 22) | (imm << 15) | (DecodeReg(Rt2) << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, u32 op2, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  bool b64Bit = Is64Bit(Rt);
  bool bVec = IsVector(Rt);

  u32 offset = imm & 0x1FF;

  ASSERT_MSG(DYNA_REC, !(imm < -256 || imm > 255), "offset too large {}", imm);

  Write32((b64Bit << 30) | (op << 22) | (bVec << 26) | (offset << 12) | (op2 << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm, u8 size)
{
  bool b64Bit = Is64Bit(Rt);
  bool bVec = IsVector(Rt);

  if (size == 64)
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x7) == 0, "64-bit load/store must use aligned offset: {}", imm);
    imm >>= 3;
  }
  else if (size == 32)
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x3) == 0, "32-bit load/store must use aligned offset: {}", imm);
    imm >>= 2;
  }
  else if (size == 16)
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x1) == 0, "16-bit load/store must use aligned offset: {}", imm);
    imm >>= 1;
  }

  ASSERT_MSG(DYNA_REC, imm >= 0, "(IndexType::Unsigned): offset must be positive {}", imm);
  ASSERT_MSG(DYNA_REC, !(imm & ~0xFFF), "(IndexType::Unsigned): offset too large {}", imm);

  Write32((b64Bit << 30) | (op << 22) | (bVec << 26) | (imm << 10) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rt));
}

void ARM64XEmitter::EncodeMOVWideInst(u32 op, ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
  bool b64Bit = Is64Bit(Rd);

  ASSERT_MSG(DYNA_REC, !(imm & ~0xFFFF), "immediate out of range: {}", imm);

  Write32((b64Bit << 31) | (op << 29) | (0x25 << 23) | (static_cast<u32>(pos) << 21) | (imm << 5) |
          DecodeReg(Rd));
}

void ARM64XEmitter::EncodeBitfieldMOVInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
  bool b64Bit = Is64Bit(Rd);

  Write32((b64Bit << 31) | (op << 29) | (0x26 << 23) | (b64Bit << 22) | (immr << 16) |
          (imms << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLoadStoreRegisterOffset(u32 size, u32 opc, ARM64Reg Rt, ARM64Reg Rn,
                                                  ArithOption Rm)
{
  const int decoded_Rm = DecodeReg(Rm.GetReg());

  Write32((size << 30) | (opc << 22) | (0x1C1 << 21) | (decoded_Rm << 16) | Rm.GetData() |
          (1 << 11) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64XEmitter::EncodeAddSubImmInst(u32 op, bool flags, u32 shift, u32 imm, ARM64Reg Rn,
                                        ARM64Reg Rd)
{
  bool b64Bit = Is64Bit(Rd);

  ASSERT_MSG(DYNA_REC, !(imm & ~0xFFF), "immediate too large: {}", imm);

  Write32((b64Bit << 31) | (op << 30) | (flags << 29) | (0x11 << 24) | (shift << 22) | (imm << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLogicalImmInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm)
{
  ASSERT_MSG(DYNA_REC, imm.valid, "Invalid logical immediate");

  // Sometimes Rd is fixed to SP, but can still be 32bit or 64bit.
  // Use Rn to determine bitness here.
  bool b64Bit = Is64Bit(Rn);

  ASSERT_MSG(DYNA_REC, b64Bit || !imm.n,
             "64-bit logical immediate does not fit in 32-bit register");

  Write32((b64Bit << 31) | (op << 29) | (0x24 << 23) | (imm.n << 22) | (imm.r << 16) |
          (imm.s << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLoadStorePair(u32 op, u32 load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2,
                                        ARM64Reg Rn, s32 imm)
{
  bool b64Bit = Is64Bit(Rt);
  u32 type_encode = 0;

  switch (type)
  {
  case IndexType::Signed:
    type_encode = 0b010;
    break;
  case IndexType::Post:
    type_encode = 0b001;
    break;
  case IndexType::Pre:
    type_encode = 0b011;
    break;
  case IndexType::Unsigned:
    ASSERT_MSG(DYNA_REC, false, "IndexType::Unsigned is not supported!");
    break;
  }

  if (b64Bit)
  {
    op |= 0b10;
    ASSERT_MSG(DYNA_REC, (imm & 0x7) == 0, "64-bit load/store must use aligned offset: {}", imm);
    imm >>= 3;
  }
  else
  {
    ASSERT_MSG(DYNA_REC, (imm & 0x3) == 0, "32-bit load/store must use aligned offset: {}", imm);
    imm >>= 2;
  }

  ASSERT_MSG(DYNA_REC, imm >= -64 && imm < 64, "imm too large for load/store pair! {}", imm);

  Write32((op << 30) | (0b101 << 27) | (type_encode << 23) | (load << 22) | ((imm & 0x7F) << 15) |
          (DecodeReg(Rt2) << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}
void ARM64XEmitter::EncodeAddressInst(u32 op, ARM64Reg Rd, s32 imm)
{
  Write32((op << 31) | ((imm & 0x3) << 29) | (0x10 << 24) | ((imm & 0x1FFFFC) << 3) |
          DecodeReg(Rd));
}

void ARM64XEmitter::EncodeLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  ASSERT_MSG(DYNA_REC, !(imm < -256 || imm > 255), "offset too large: {}", imm);

  Write32((size << 30) | (0b111 << 27) | (op << 22) | ((imm & 0x1FF) << 12) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rt));
}

static constexpr bool IsInRangeImm19(s64 distance)
{
  return (distance >= -0x40000 && distance <= 0x3FFFF);
}

static constexpr bool IsInRangeImm14(s64 distance)
{
  return (distance >= -0x2000 && distance <= 0x1FFF);
}

static constexpr bool IsInRangeImm26(s64 distance)
{
  return (distance >= -0x2000000 && distance <= 0x1FFFFFF);
}

static constexpr u32 MaskImm19(s64 distance)
{
  return distance & 0x7FFFF;
}

static constexpr u32 MaskImm14(s64 distance)
{
  return distance & 0x3FFF;
}

static constexpr u32 MaskImm26(s64 distance)
{
  return distance & 0x3FFFFFF;
}

// FixupBranch branching
void ARM64XEmitter::SetJumpTarget(FixupBranch const& branch)
{
  if (!branch.ptr)
    return;

  bool Not = false;
  u32 inst = 0;
  s64 distance = (s64)(m_code - branch.ptr);
  distance >>= 2;

  switch (branch.type)
  {
  case FixupBranch::Type::CBNZ:
    Not = true;
    [[fallthrough]];
  case FixupBranch::Type::CBZ:
  {
    ASSERT_MSG(DYNA_REC, IsInRangeImm19(distance),
               "Branch type {}: Received too large distance: {}", static_cast<int>(branch.type),
               distance);
    const bool b64Bit = Is64Bit(branch.reg);
    inst = (b64Bit << 31) | (0x1A << 25) | (Not << 24) | (MaskImm19(distance) << 5) |
           DecodeReg(branch.reg);
  }
  break;
  case FixupBranch::Type::BConditional:
    ASSERT_MSG(DYNA_REC, IsInRangeImm19(distance),
               "Branch type {}: Received too large distance: {}", static_cast<int>(branch.type),
               distance);
    inst = (0x2A << 25) | (MaskImm19(distance) << 5) | branch.cond;
    break;
  case FixupBranch::Type::TBNZ:
    Not = true;
    [[fallthrough]];
  case FixupBranch::Type::TBZ:
  {
    ASSERT_MSG(DYNA_REC, IsInRangeImm14(distance),
               "Branch type {}: Received too large distance: {}", static_cast<int>(branch.type),
               distance);
    inst = ((branch.bit & 0x20) << 26) | (0x1B << 25) | (Not << 24) | ((branch.bit & 0x1F) << 19) |
           (MaskImm14(distance) << 5) | DecodeReg(branch.reg);
  }
  break;
  case FixupBranch::Type::B:
    ASSERT_MSG(DYNA_REC, IsInRangeImm26(distance),
               "Branch type {}: Received too large distance: {}", static_cast<int>(branch.type),
               distance);
    inst = (0x5 << 26) | MaskImm26(distance);
    break;
  case FixupBranch::Type::BL:
    ASSERT_MSG(DYNA_REC, IsInRangeImm26(distance),
               "Branch type {}: Received too large distance: {}", static_cast<int>(branch.type),
               distance);
    inst = (0x25 << 26) | MaskImm26(distance);
    break;
  }

  std::memcpy(branch.ptr, &inst, sizeof(inst));
}

FixupBranch ARM64XEmitter::WriteFixupBranch()
{
  FixupBranch branch{};
  branch.ptr = m_code;
  BRK(0);

  // If we couldn't write the full jump instruction, indicate that in the returned FixupBranch by
  // setting the branch's address to null. This will prevent a later SetJumpTarget() from writing to
  // invalid memory.
  if (HasWriteFailed())
    branch.ptr = nullptr;

  return branch;
}

FixupBranch ARM64XEmitter::CBZ(ARM64Reg Rt)
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::CBZ;
  branch.reg = Rt;
  return branch;
}
FixupBranch ARM64XEmitter::CBNZ(ARM64Reg Rt)
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::CBNZ;
  branch.reg = Rt;
  return branch;
}
FixupBranch ARM64XEmitter::B(CCFlags cond)
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::BConditional;
  branch.cond = cond;
  return branch;
}
FixupBranch ARM64XEmitter::TBZ(ARM64Reg Rt, u8 bit)
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::TBZ;
  branch.reg = Rt;
  branch.bit = bit;
  return branch;
}
FixupBranch ARM64XEmitter::TBNZ(ARM64Reg Rt, u8 bit)
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::TBNZ;
  branch.reg = Rt;
  branch.bit = bit;
  return branch;
}
FixupBranch ARM64XEmitter::B()
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::B;
  return branch;
}
FixupBranch ARM64XEmitter::BL()
{
  FixupBranch branch = WriteFixupBranch();
  branch.type = FixupBranch::Type::BL;
  return branch;
}

// Compare and Branch
void ARM64XEmitter::CBZ(ARM64Reg Rt, const void* ptr)
{
  EncodeCompareBranchInst(0, Rt, ptr);
}
void ARM64XEmitter::CBNZ(ARM64Reg Rt, const void* ptr)
{
  EncodeCompareBranchInst(1, Rt, ptr);
}

// Conditional Branch
void ARM64XEmitter::B(CCFlags cond, const void* ptr)
{
  s64 distance = (s64)ptr - (s64)m_code;

  distance >>= 2;

  ASSERT_MSG(DYNA_REC, IsInRangeImm19(distance),
             "Received too large distance: {}->{} (dist {} {:#x})", fmt::ptr(m_code), fmt::ptr(ptr),
             distance, distance);
  Write32((0x54 << 24) | (MaskImm19(distance) << 5) | cond);
}

// Test and Branch
void ARM64XEmitter::TBZ(ARM64Reg Rt, u8 bits, const void* ptr)
{
  EncodeTestBranchInst(0, Rt, bits, ptr);
}
void ARM64XEmitter::TBNZ(ARM64Reg Rt, u8 bits, const void* ptr)
{
  EncodeTestBranchInst(1, Rt, bits, ptr);
}

// Unconditional Branch
void ARM64XEmitter::B(const void* ptr)
{
  EncodeUnconditionalBranchInst(0, ptr);
}
void ARM64XEmitter::BL(const void* ptr)
{
  EncodeUnconditionalBranchInst(1, ptr);
}

void ARM64XEmitter::QuickCallFunction(ARM64Reg scratchreg, const void* func)
{
  s64 distance = (s64)func - (s64)m_code;
  distance >>= 2;  // Can only branch to opcode-aligned (4) addresses
  if (!IsInRangeImm26(distance))
  {
    MOVI2R(scratchreg, (uintptr_t)func);
    BLR(scratchreg);
  }
  else
  {
    BL(func);
  }
}

// Unconditional Branch (register)
void ARM64XEmitter::BR(ARM64Reg Rn)
{
  EncodeUnconditionalBranchInst(0, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::BLR(ARM64Reg Rn)
{
  EncodeUnconditionalBranchInst(1, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::RET(ARM64Reg Rn)
{
  EncodeUnconditionalBranchInst(2, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::ERET()
{
  EncodeUnconditionalBranchInst(4, 0x1F, 0, 0, ARM64Reg::SP);
}
void ARM64XEmitter::DRPS()
{
  EncodeUnconditionalBranchInst(5, 0x1F, 0, 0, ARM64Reg::SP);
}

// Exception generation
void ARM64XEmitter::SVC(u32 imm)
{
  EncodeExceptionInst(0, imm);
}

void ARM64XEmitter::HVC(u32 imm)
{
  EncodeExceptionInst(1, imm);
}

void ARM64XEmitter::SMC(u32 imm)
{
  EncodeExceptionInst(2, imm);
}

void ARM64XEmitter::BRK(u32 imm)
{
  EncodeExceptionInst(3, imm);
}

void ARM64XEmitter::HLT(u32 imm)
{
  EncodeExceptionInst(4, imm);
}

void ARM64XEmitter::DCPS1(u32 imm)
{
  EncodeExceptionInst(5, imm);
}

void ARM64XEmitter::DCPS2(u32 imm)
{
  EncodeExceptionInst(6, imm);
}

void ARM64XEmitter::DCPS3(u32 imm)
{
  EncodeExceptionInst(7, imm);
}

// System
void ARM64XEmitter::_MSR(PStateField field, u8 imm)
{
  u32 op1 = 0, op2 = 0;
  switch (field)
  {
  case PStateField::SPSel:
    op1 = 0;
    op2 = 5;
    break;
  case PStateField::DAIFSet:
    op1 = 3;
    op2 = 6;
    break;
  case PStateField::DAIFClr:
    op1 = 3;
    op2 = 7;
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Invalid PStateField to do a imm move to");
    break;
  }
  EncodeSystemInst(0, op1, 4, imm, op2, ARM64Reg::WSP);
}

static void GetSystemReg(PStateField field, int& o0, int& op1, int& CRn, int& CRm, int& op2)
{
  switch (field)
  {
  case PStateField::NZCV:
    o0 = 3;
    op1 = 3;
    CRn = 4;
    CRm = 2;
    op2 = 0;
    break;
  case PStateField::FPCR:
    o0 = 3;
    op1 = 3;
    CRn = 4;
    CRm = 4;
    op2 = 0;
    break;
  case PStateField::FPSR:
    o0 = 3;
    op1 = 3;
    CRn = 4;
    CRm = 4;
    op2 = 1;
    break;
  case PStateField::PMCR_EL0:
    o0 = 3;
    op1 = 3;
    CRn = 9;
    CRm = 6;
    op2 = 0;
    break;
  case PStateField::PMCCNTR_EL0:
    o0 = 3;
    op1 = 3;
    CRn = 9;
    CRm = 7;
    op2 = 0;
    break;
  default:
    ASSERT_MSG(DYNA_REC, false, "Invalid PStateField to do a register move from/to");
    break;
  }
}

void ARM64XEmitter::_MSR(PStateField field, ARM64Reg Rt)
{
  int o0 = 0, op1 = 0, CRn = 0, CRm = 0, op2 = 0;
  ASSERT_MSG(DYNA_REC, Is64Bit(Rt), "MSR: Rt must be 64-bit");
  GetSystemReg(field, o0, op1, CRn, CRm, op2);
  EncodeSystemInst(o0, op1, CRn, CRm, op2, Rt);
}

void ARM64XEmitter::MRS(ARM64Reg Rt, PStateField field)
{
  int o0 = 0, op1 = 0, CRn = 0, CRm = 0, op2 = 0;
  ASSERT_MSG(DYNA_REC, Is64Bit(Rt), "MRS: Rt must be 64-bit");
  GetSystemReg(field, o0, op1, CRn, CRm, op2);
  EncodeSystemInst(o0 | 4, op1, CRn, CRm, op2, Rt);
}

void ARM64XEmitter::CNTVCT(Arm64Gen::ARM64Reg Rt)
{
  ASSERT_MSG(DYNA_REC, Is64Bit(Rt), "CNTVCT: Rt must be 64-bit");

  // MRS <Xt>, CNTVCT_EL0 ; Read CNTVCT_EL0 into Xt
  EncodeSystemInst(3 | 4, 3, 0xe, 0, 2, Rt);
}

void ARM64XEmitter::HINT(SystemHint op)
{
  EncodeSystemInst(0, 3, 2, 0, static_cast<u32>(op), ARM64Reg::WSP);
}
void ARM64XEmitter::CLREX()
{
  EncodeSystemInst(0, 3, 3, 0, 2, ARM64Reg::WSP);
}
void ARM64XEmitter::DSB(BarrierType type)
{
  EncodeSystemInst(0, 3, 3, static_cast<u32>(type), 4, ARM64Reg::WSP);
}
void ARM64XEmitter::DMB(BarrierType type)
{
  EncodeSystemInst(0, 3, 3, static_cast<u32>(type), 5, ARM64Reg::WSP);
}
void ARM64XEmitter::ISB(BarrierType type)
{
  EncodeSystemInst(0, 3, 3, static_cast<u32>(type), 6, ARM64Reg::WSP);
}

// Add/Subtract (extended register)
void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  ADD(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
}

void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(0, false, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticInst(0, true, Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
}

void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(0, true, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  SUB(Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
}

void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(1, false, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticInst(1, true, Rd, Rn, Rm, ArithOption(Rd, ShiftType::LSL, 0));
}

void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(1, true, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::CMN(ARM64Reg Rn, ARM64Reg Rm)
{
  CMN(Rn, Rm, ArithOption(Rn, ShiftType::LSL, 0));
}

void ARM64XEmitter::CMN(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(0, true, Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, Rm, Option);
}

void ARM64XEmitter::CMP(ARM64Reg Rn, ARM64Reg Rm)
{
  CMP(Rn, Rm, ArithOption(Rn, ShiftType::LSL, 0));
}

void ARM64XEmitter::CMP(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
  EncodeArithmeticInst(1, true, Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, Rm, Option);
}

// Add/Subtract (with carry)
void ARM64XEmitter::ADC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticCarryInst(0, false, Rd, Rn, Rm);
}
void ARM64XEmitter::ADCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticCarryInst(0, true, Rd, Rn, Rm);
}
void ARM64XEmitter::SBC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticCarryInst(1, false, Rd, Rn, Rm);
}
void ARM64XEmitter::SBCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeArithmeticCarryInst(1, true, Rd, Rn, Rm);
}

// Conditional Compare (immediate)
void ARM64XEmitter::CCMN(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
  EncodeCondCompareImmInst(0, Rn, imm, nzcv, cond);
}
void ARM64XEmitter::CCMP(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
  EncodeCondCompareImmInst(1, Rn, imm, nzcv, cond);
}

// Conditiona Compare (register)
void ARM64XEmitter::CCMN(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond)
{
  EncodeCondCompareRegInst(0, Rn, Rm, nzcv, cond);
}
void ARM64XEmitter::CCMP(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond)
{
  EncodeCondCompareRegInst(1, Rn, Rm, nzcv, cond);
}

// Conditional Select
void ARM64XEmitter::CSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
  EncodeCondSelectInst(0, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSINC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
  EncodeCondSelectInst(1, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSINV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
  EncodeCondSelectInst(2, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
  EncodeCondSelectInst(3, Rd, Rn, Rm, cond);
}

// Data-Processing 1 source
void ARM64XEmitter::RBIT(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(0, Rd, Rn);
}
void ARM64XEmitter::REV16(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(1, Rd, Rn);
}
void ARM64XEmitter::REV32(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(2, Rd, Rn);
}
void ARM64XEmitter::REV64(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(3, Rd, Rn);
}
void ARM64XEmitter::CLZ(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(4, Rd, Rn);
}
void ARM64XEmitter::CLS(ARM64Reg Rd, ARM64Reg Rn)
{
  EncodeData1SrcInst(5, Rd, Rn);
}

// Data-Processing 2 source
void ARM64XEmitter::UDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(0, Rd, Rn, Rm);
}
void ARM64XEmitter::SDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(1, Rd, Rn, Rm);
}
void ARM64XEmitter::LSLV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(2, Rd, Rn, Rm);
}
void ARM64XEmitter::LSRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(3, Rd, Rn, Rm);
}
void ARM64XEmitter::ASRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(4, Rd, Rn, Rm);
}
void ARM64XEmitter::RORV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(5, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32B(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(6, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32H(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(7, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32W(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(8, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(9, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(10, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CW(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(11, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32X(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(12, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData2SrcInst(13, Rd, Rn, Rm);
}

// Data-Processing 3 source
void ARM64XEmitter::MADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(0, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::MSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(1, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(2, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  SMADDL(Rd, Rn, Rm, ARM64Reg::SP);
}
void ARM64XEmitter::SMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(3, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData3SrcInst(4, Rd, Rn, Rm, ARM64Reg::SP);
}
void ARM64XEmitter::UMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(5, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  UMADDL(Rd, Rn, Rm, ARM64Reg::SP);
}
void ARM64XEmitter::UMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EncodeData3SrcInst(6, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData3SrcInst(7, Rd, Rn, Rm, ARM64Reg::SP);
}
void ARM64XEmitter::MUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData3SrcInst(0, Rd, Rn, Rm, ARM64Reg::SP);
}
void ARM64XEmitter::MNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EncodeData3SrcInst(1, Rd, Rn, Rm, ARM64Reg::SP);
}

// Logical (shifted register)
void ARM64XEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(0, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(1, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(2, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(3, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(4, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(5, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(6, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
  EncodeLogicalInst(7, Rd, Rn, Rm, Shift);
}

void ARM64XEmitter::MOV(ARM64Reg Rd, ARM64Reg Rm, ArithOption Shift)
{
  ORR(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rm, Shift);
}

void ARM64XEmitter::MOV(ARM64Reg Rd, ARM64Reg Rm)
{
  if (IsGPR(Rd) && IsGPR(Rm))
    ORR(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rm, ArithOption(Rm, ShiftType::LSL, 0));
  else
    ASSERT_MSG(DYNA_REC, false, "Non-GPRs not supported in MOV");
}
void ARM64XEmitter::MVN(ARM64Reg Rd, ARM64Reg Rm)
{
  ORN(Rd, Is64Bit(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR, Rm, ArithOption(Rm, ShiftType::LSL, 0));
}
void ARM64XEmitter::LSL(ARM64Reg Rd, ARM64Reg Rm, int shift)
{
  int bits = Is64Bit(Rd) ? 64 : 32;
  UBFM(Rd, Rm, (bits - shift) & (bits - 1), bits - shift - 1);
}
void ARM64XEmitter::LSR(ARM64Reg Rd, ARM64Reg Rm, int shift)
{
  int bits = Is64Bit(Rd) ? 64 : 32;
  UBFM(Rd, Rm, shift, bits - 1);
}
void ARM64XEmitter::ASR(ARM64Reg Rd, ARM64Reg Rm, int shift)
{
  int bits = Is64Bit(Rd) ? 64 : 32;
  SBFM(Rd, Rm, shift, bits - 1);
}
void ARM64XEmitter::ROR(ARM64Reg Rd, ARM64Reg Rm, int shift)
{
  EXTR(Rd, Rm, Rm, shift);
}

// Logical (immediate)
void ARM64XEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm)
{
  EncodeLogicalImmInst(0, Rd, Rn, imm);
}
void ARM64XEmitter::ANDS(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm)
{
  EncodeLogicalImmInst(3, Rd, Rn, imm);
}
void ARM64XEmitter::EOR(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm)
{
  EncodeLogicalImmInst(2, Rd, Rn, imm);
}
void ARM64XEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, LogicalImm imm)
{
  EncodeLogicalImmInst(1, Rd, Rn, imm);
}
void ARM64XEmitter::TST(ARM64Reg Rn, LogicalImm imm)
{
  EncodeLogicalImmInst(3, Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, imm);
}

// Add/subtract (immediate)
void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(0, false, shift, imm, Rn, Rd);
}
void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(0, true, shift, imm, Rn, Rd);
}
void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(1, false, shift, imm, Rn, Rd);
}
void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(1, true, shift, imm, Rn, Rd);
}
void ARM64XEmitter::CMP(ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(1, true, shift, imm, Rn, Is64Bit(Rn) ? ARM64Reg::SP : ARM64Reg::WSP);
}
void ARM64XEmitter::CMN(ARM64Reg Rn, u32 imm, bool shift)
{
  EncodeAddSubImmInst(0, true, shift, imm, Rn, Is64Bit(Rn) ? ARM64Reg::SP : ARM64Reg::WSP);
}

// Data Processing (Immediate)
void ARM64XEmitter::MOVZ(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
  EncodeMOVWideInst(2, Rd, imm, pos);
}
void ARM64XEmitter::MOVN(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
  EncodeMOVWideInst(0, Rd, imm, pos);
}
void ARM64XEmitter::MOVK(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
  EncodeMOVWideInst(3, Rd, imm, pos);
}

// Bitfield move
void ARM64XEmitter::BFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
  EncodeBitfieldMOVInst(1, Rd, Rn, immr, imms);
}
void ARM64XEmitter::SBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
  EncodeBitfieldMOVInst(0, Rd, Rn, immr, imms);
}
void ARM64XEmitter::UBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
  EncodeBitfieldMOVInst(2, Rd, Rn, immr, imms);
}

void ARM64XEmitter::BFI(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width)
{
  u32 size = Is64Bit(Rn) ? 64 : 32;
  ASSERT_MSG(DYNA_REC, lsb < size && width >= 1 && width <= size - lsb,
             "lsb {} and width {} is greater than the register size {}!", lsb, width, size);
  BFM(Rd, Rn, (size - lsb) % size, width - 1);
}
void ARM64XEmitter::BFXIL(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width)
{
  u32 size = Is64Bit(Rn) ? 64 : 32;
  ASSERT_MSG(DYNA_REC, lsb < size && width >= 1 && width <= size - lsb,
             "lsb {} and width {} is greater than the register size {}!", lsb, width, size);
  BFM(Rd, Rn, lsb, lsb + width - 1);
}
void ARM64XEmitter::UBFIZ(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width)
{
  u32 size = Is64Bit(Rn) ? 64 : 32;
  ASSERT_MSG(DYNA_REC, lsb < size && width >= 1 && width <= size - lsb,
             "lsb {} and width {} is greater than the register size {}!", lsb, width, size);
  UBFM(Rd, Rn, (size - lsb) % size, width - 1);
}
void ARM64XEmitter::EXTR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u32 shift)
{
  bool sf = Is64Bit(Rd);
  bool N = sf;

  Write32((sf << 31) | (0x27 << 23) | (N << 22) | (DecodeReg(Rm) << 16) | (shift << 10) |
          (DecodeReg(Rm) << 5) | DecodeReg(Rd));
}
void ARM64XEmitter::SXTB(ARM64Reg Rd, ARM64Reg Rn)
{
  SBFM(Rd, Rn, 0, 7);
}
void ARM64XEmitter::SXTH(ARM64Reg Rd, ARM64Reg Rn)
{
  SBFM(Rd, Rn, 0, 15);
}
void ARM64XEmitter::SXTW(ARM64Reg Rd, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, Is64Bit(Rd), "64bit register required as destination");
  SBFM(Rd, Rn, 0, 31);
}
void ARM64XEmitter::UXTB(ARM64Reg Rd, ARM64Reg Rn)
{
  UBFM(Rd, Rn, 0, 7);
}
void ARM64XEmitter::UXTH(ARM64Reg Rd, ARM64Reg Rn)
{
  UBFM(Rd, Rn, 0, 15);
}

// Load Register (Literal)
void ARM64XEmitter::LDR(ARM64Reg Rt, u32 imm)
{
  EncodeLoadRegisterInst(0, Rt, imm);
}
void ARM64XEmitter::LDRSW(ARM64Reg Rt, u32 imm)
{
  EncodeLoadRegisterInst(2, Rt, imm);
}
void ARM64XEmitter::PRFM(ARM64Reg Rt, u32 imm)
{
  EncodeLoadRegisterInst(3, Rt, imm);
}

// Load/Store pair
void ARM64XEmitter::LDP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStorePair(0, 1, type, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::LDPSW(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStorePair(1, 1, type, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::STP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStorePair(0, 0, type, Rt, Rt2, Rn, imm);
}

// Load/Store Exclusive
void ARM64XEmitter::STXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(0, Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STLXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(1, Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDXRB(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(2, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDAXRB(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(3, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STLRB(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(4, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDARB(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(5, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(6, Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STLXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(7, Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDXRH(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(8, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDAXRH(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(9, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STLRH(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(10, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDARH(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(11, ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(12 + Is64Bit(Rt), Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STLXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(14 + Is64Bit(Rt), Rs, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::STXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(16 + Is64Bit(Rt), Rs, Rt2, Rt, Rn);
}
void ARM64XEmitter::STLXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(18 + Is64Bit(Rt), Rs, Rt2, Rt, Rn);
}
void ARM64XEmitter::LDXR(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(20 + Is64Bit(Rt), ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDAXR(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(22 + Is64Bit(Rt), ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(24 + Is64Bit(Rt), ARM64Reg::SP, Rt2, Rt, Rn);
}
void ARM64XEmitter::LDAXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(26 + Is64Bit(Rt), ARM64Reg::SP, Rt2, Rt, Rn);
}
void ARM64XEmitter::STLR(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(28 + Is64Bit(Rt), ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}
void ARM64XEmitter::LDAR(ARM64Reg Rt, ARM64Reg Rn)
{
  EncodeLoadStoreExcInst(30 + Is64Bit(Rt), ARM64Reg::SP, ARM64Reg::SP, Rt, Rn);
}

// Load/Store no-allocate pair (offset)
void ARM64XEmitter::STNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm)
{
  EncodeLoadStorePairedInst(0xA0, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::LDNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm)
{
  EncodeLoadStorePairedInst(0xA1, Rt, Rt2, Rn, imm);
}

// Load/Store register (immediate post-indexed)
// XXX: Most of these support vectors
void ARM64XEmitter::STRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(0x0E4, Rt, Rn, imm, 8);
  else
    EncodeLoadStoreIndexedInst(0x0E0, type == IndexType::Post ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(0x0E5, Rt, Rn, imm, 8);
  else
    EncodeLoadStoreIndexedInst(0x0E1, type == IndexType::Post ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E6 : 0x0E7, Rt, Rn, imm, 8);
  else
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E2 : 0x0E3, type == IndexType::Post ? 1 : 3, Rt, Rn,
                               imm);
}
void ARM64XEmitter::STRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(0x1E4, Rt, Rn, imm, 16);
  else
    EncodeLoadStoreIndexedInst(0x1E0, type == IndexType::Post ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(0x1E5, Rt, Rn, imm, 16);
  else
    EncodeLoadStoreIndexedInst(0x1E1, type == IndexType::Post ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E6 : 0x1E7, Rt, Rn, imm, 16);
  else
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E2 : 0x1E3, type == IndexType::Post ? 1 : 3, Rt, Rn,
                               imm);
}
void ARM64XEmitter::STR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E4 : 0x2E4, Rt, Rn, imm, Is64Bit(Rt) ? 64 : 32);
  else
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E0 : 0x2E0, type == IndexType::Post ? 1 : 3, Rt, Rn,
                               imm);
}
void ARM64XEmitter::LDR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E5 : 0x2E5, Rt, Rn, imm, Is64Bit(Rt) ? 64 : 32);
  else
    EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E1 : 0x2E1, type == IndexType::Post ? 1 : 3, Rt, Rn,
                               imm);
}
void ARM64XEmitter::LDRSW(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  if (type == IndexType::Unsigned)
    EncodeLoadStoreIndexedInst(0x2E6, Rt, Rn, imm, 32);
  else
    EncodeLoadStoreIndexedInst(0x2E2, type == IndexType::Post ? 1 : 3, Rt, Rn, imm);
}

// Load/Store register (register offset)
void ARM64XEmitter::STRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(0, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(0, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  bool b64Bit = Is64Bit(Rt);
  EncodeLoadStoreRegisterOffset(0, 3 - b64Bit, Rt, Rn, Rm);
}
void ARM64XEmitter::STRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(1, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(1, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  bool b64Bit = Is64Bit(Rt);
  EncodeLoadStoreRegisterOffset(1, 3 - b64Bit, Rt, Rn, Rm);
}
void ARM64XEmitter::STR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  bool b64Bit = Is64Bit(Rt);
  EncodeLoadStoreRegisterOffset(2 + b64Bit, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  bool b64Bit = Is64Bit(Rt);
  EncodeLoadStoreRegisterOffset(2 + b64Bit, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSW(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(2, 2, Rt, Rn, Rm);
}
void ARM64XEmitter::PRFM(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(3, 2, Rt, Rn, Rm);
}

// Load/Store register (unscaled offset)
void ARM64XEmitter::STURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(0, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(0, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(0, Is64Bit(Rt) ? 2 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(1, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(1, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(1, Is64Bit(Rt) ? 2 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(Is64Bit(Rt) ? 3 : 2, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EncodeLoadStoreUnscaled(Is64Bit(Rt) ? 3 : 2, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSW(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  ASSERT_MSG(DYNA_REC, !Is64Bit(Rt), "Must have a 64bit destination register!");
  EncodeLoadStoreUnscaled(2, 2, Rt, Rn, imm);
}

// Address of label/page PC-relative
void ARM64XEmitter::ADR(ARM64Reg Rd, s32 imm)
{
  EncodeAddressInst(0, Rd, imm);
}
void ARM64XEmitter::ADRP(ARM64Reg Rd, s64 imm)
{
  EncodeAddressInst(1, Rd, static_cast<s32>(imm >> 12));
}

// This is using a hand-rolled algorithm. The goal is zero memory allocations, not necessarily
// the best JIT-time time complexity. (The number of moves is usually very small.)
void ARM64XEmitter::ParallelMoves(RegisterMove* begin, RegisterMove* end,
                                  std::array<u8, 32>* source_gpr_usages)
{
  // X0-X7 are used for passing arguments.
  // X18-X31 are either callee saved or used for special purposes.
  constexpr size_t temp_reg_begin = 8;
  constexpr size_t temp_reg_end = 18;

  while (begin != end)
  {
    bool removed_moves_during_this_loop_iteration = false;

    RegisterMove* current_move = end;
    while (current_move != begin)
    {
      RegisterMove* prev_move = current_move;
      --current_move;
      if ((*source_gpr_usages)[DecodeReg(current_move->dst)] == 0)
      {
        MOV(current_move->dst, current_move->src);
        (*source_gpr_usages)[DecodeReg(current_move->src)]--;
        std::move(prev_move, end, current_move);
        --end;
        removed_moves_during_this_loop_iteration = true;
      }
    }

    if (!removed_moves_during_this_loop_iteration)
    {
      // We need to break a cycle using a temporary register.

      size_t temp_reg = temp_reg_begin;
      while ((*source_gpr_usages)[temp_reg] != 0)
      {
        ++temp_reg;
        ASSERT_MSG(DYNA_REC, temp_reg != temp_reg_end, "Out of registers");
      }

      const ARM64Reg src = begin->src;
      const ARM64Reg dst =
          (Is64Bit(src) ? EncodeRegTo64 : EncodeRegTo32)(static_cast<ARM64Reg>(temp_reg));

      MOV(dst, src);
      (*source_gpr_usages)[DecodeReg(dst)] = (*source_gpr_usages)[DecodeReg(src)];
      (*source_gpr_usages)[DecodeReg(src)] = 0;

      std::for_each(begin, end, [src, dst](RegisterMove& move) {
        if (move.src == src)
          move.src = dst;
      });
    }
  }
}

template <typename T>
void ARM64XEmitter::MOVI2RImpl(ARM64Reg Rd, T imm)
{
  enum class Approach
  {
    MOVZBase,
    MOVNBase,
    ADRBase,
    ADRPBase,
    ORRBase,
  };

  struct Part
  {
    Part() = default;
    Part(u16 imm_, ShiftAmount shift_) : imm(imm_), shift(shift_) {}

    u16 imm;
    ShiftAmount shift;
  };

  constexpr size_t max_parts = sizeof(T) / 2;

  Common::SmallVector<Part, max_parts> best_parts;
  Approach best_approach;
  u64 best_base;

  const auto instructions_required = [](const Common::SmallVector<Part, max_parts>& parts,
                                        Approach approach) {
    return parts.size() + (approach > Approach::MOVNBase);
  };

  const auto try_base = [&](T base, Approach approach, bool first_time) {
    Common::SmallVector<Part, max_parts> parts;

    for (size_t i = 0; i < max_parts; ++i)
    {
      const size_t shift = i * 16;
      const u16 imm_shifted = static_cast<u16>(imm >> shift);
      const u16 base_shifted = static_cast<u16>(base >> shift);
      if (imm_shifted != base_shifted)
        parts.emplace_back(imm_shifted, static_cast<ShiftAmount>(i));
    }

    if (first_time ||
        instructions_required(parts, approach) < instructions_required(best_parts, best_approach))
    {
      best_parts = std::move(parts);
      best_approach = approach;
      best_base = base;
    }
  };

  // Try MOVZ/MOVN
  try_base(T(0), Approach::MOVZBase, true);
  try_base(~T(0), Approach::MOVNBase, false);

  // Try PC-relative approaches
  const auto sext_21_bit = [](u64 x) {
    return static_cast<s64>((x & 0x1FFFFF) | (x & 0x100000 ? ~0x1FFFFF : 0));
  };
  const u64 pc = reinterpret_cast<u64>(GetCodePtr());
  const s64 adrp_offset = sext_21_bit((imm >> 12) - (pc >> 12)) << 12;
  const s64 adr_offset = sext_21_bit(imm - pc);
  const u64 adrp_base = (pc & ~0xFFF) + adrp_offset;
  const u64 adr_base = pc + adr_offset;
  if constexpr (sizeof(T) == 8)
  {
    try_base(adrp_base, Approach::ADRPBase, false);
    try_base(adr_base, Approach::ADRBase, false);
  }

  // Try ORR (or skip it if we already have a 1-instruction encoding - these tests are non-trivial)
  if (instructions_required(best_parts, best_approach) > 1)
  {
    if constexpr (sizeof(T) == 8)
    {
      for (u64 orr_imm : {(imm << 32) | (imm & 0x0000'0000'FFFF'FFFF),
                          (imm & 0xFFFF'FFFF'0000'0000) | (imm >> 32),
                          (imm << 48) | (imm & 0x0000'FFFF'FFFF'0000) | (imm >> 48)})
      {
        if (LogicalImm(orr_imm, GPRSize::B64))
          try_base(orr_imm, Approach::ORRBase, false);
      }
    }
    else
    {
      if (LogicalImm(imm, GPRSize::B32))
        try_base(imm, Approach::ORRBase, false);
    }
  }

  size_t parts_uploaded = 0;

  // To kill any dependencies, we start with an instruction that overwrites the entire register
  switch (best_approach)
  {
  case Approach::MOVZBase:
    if (best_parts.empty())
      best_parts.emplace_back(u16(0), ShiftAmount::Shift0);

    MOVZ(Rd, best_parts[0].imm, best_parts[0].shift);
    ++parts_uploaded;
    break;

  case Approach::MOVNBase:
    if (best_parts.empty())
      best_parts.emplace_back(u16(0xFFFF), ShiftAmount::Shift0);

    MOVN(Rd, static_cast<u16>(~best_parts[0].imm), best_parts[0].shift);
    ++parts_uploaded;
    break;

  case Approach::ADRBase:
    ADR(Rd, adr_offset);
    break;

  case Approach::ADRPBase:
    ADRP(Rd, adrp_offset);
    break;

  case Approach::ORRBase:
    constexpr ARM64Reg zero_reg = sizeof(T) == 8 ? ARM64Reg::ZR : ARM64Reg::WZR;
    const bool success = TryORRI2R(Rd, zero_reg, best_base);
    ASSERT(success);
    break;
  }

  // And then we use MOVK for the remaining parts
  for (; parts_uploaded < best_parts.size(); ++parts_uploaded)
  {
    const Part& part = best_parts[parts_uploaded];

    if (best_approach == Approach::ADRPBase && part.shift == ShiftAmount::Shift0)
    {
      // The combination of ADRP followed by ADD immediate is specifically optimized in hardware
      ASSERT(part.imm == (adrp_base & 0xF000) + (part.imm & 0xFFF));
      ADD(Rd, Rd, part.imm & 0xFFF);
    }
    else
    {
      MOVK(Rd, part.imm, part.shift);
    }
  }
}

template void ARM64XEmitter::MOVI2RImpl(ARM64Reg Rd, u64 imm);
template void ARM64XEmitter::MOVI2RImpl(ARM64Reg Rd, u32 imm);

void ARM64XEmitter::MOVI2R(ARM64Reg Rd, u64 imm)
{
  if (Is64Bit(Rd))
    MOVI2RImpl<u64>(Rd, imm);
  else
    MOVI2RImpl<u32>(Rd, static_cast<u32>(imm));
}

bool ARM64XEmitter::MOVI2R2(ARM64Reg Rd, u64 imm1, u64 imm2)
{
  // TODO: Also optimize for performance, not just for code size.
  u8* start_pointer = GetWritableCodePtr();

  MOVI2R(Rd, imm1);
  int size1 = GetCodePtr() - start_pointer;

  m_code = start_pointer;

  MOVI2R(Rd, imm2);
  int size2 = GetCodePtr() - start_pointer;

  m_code = start_pointer;

  bool element = size1 > size2;

  MOVI2R(Rd, element ? imm2 : imm1);

  return element;
}

void ARM64XEmitter::ABI_PushRegisters(BitSet32 registers)
{
  int num_regs = registers.Count();
  int stack_size = (num_regs + (num_regs & 1)) * 8;
  auto it = registers.begin();

  if (!num_regs)
    return;

  // 8 byte per register, but 16 byte alignment, so we may have to padd one register.
  // Only update the SP on the last write to avoid the dependency between those stores.

  // The first push must adjust the SP, else a context switch may invalidate everything below SP.
  if (num_regs & 1)
  {
    STR(IndexType::Pre, ARM64Reg::X0 + *it++, ARM64Reg::SP, -stack_size);
  }
  else
  {
    ARM64Reg first_reg = ARM64Reg::X0 + *it++;
    ARM64Reg second_reg = ARM64Reg::X0 + *it++;
    STP(IndexType::Pre, first_reg, second_reg, ARM64Reg::SP, -stack_size);
  }

  // Fast store for all other registers, this is always an even number.
  for (int i = 0; i < (num_regs - 1) / 2; i++)
  {
    ARM64Reg odd_reg = ARM64Reg::X0 + *it++;
    ARM64Reg even_reg = ARM64Reg::X0 + *it++;
    STP(IndexType::Signed, odd_reg, even_reg, ARM64Reg::SP, 16 * (i + 1));
  }

  ASSERT_MSG(DYNA_REC, it == registers.end(), "Registers don't match: {:b}", registers.m_val);
}

void ARM64XEmitter::ABI_PopRegisters(BitSet32 registers, BitSet32 ignore_mask)
{
  int num_regs = registers.Count();
  int stack_size = (num_regs + (num_regs & 1)) * 8;
  auto it = registers.begin();

  if (!num_regs)
    return;

  // We must adjust the SP in the end, so load the first (two) registers at least.
  ARM64Reg first = ARM64Reg::X0 + *it++;
  ARM64Reg second;
  if (!(num_regs & 1))
    second = ARM64Reg::X0 + *it++;
  else
    second = {};

  // 8 byte per register, but 16 byte alignment, so we may have to padd one register.
  // Only update the SP on the last load to avoid the dependency between those loads.

  // Fast load for all but the first (two) registers, this is always an even number.
  for (int i = 0; i < (num_regs - 1) / 2; i++)
  {
    ARM64Reg odd_reg = ARM64Reg::X0 + *it++;
    ARM64Reg even_reg = ARM64Reg::X0 + *it++;
    LDP(IndexType::Signed, odd_reg, even_reg, ARM64Reg::SP, 16 * (i + 1));
  }

  // Post loading the first (two) registers.
  if (num_regs & 1)
    LDR(IndexType::Post, first, ARM64Reg::SP, stack_size);
  else
    LDP(IndexType::Post, first, second, ARM64Reg::SP, stack_size);

  ASSERT_MSG(DYNA_REC, it == registers.end(), "Registers don't match: {:b}", registers.m_val);
}

// Float Emitter
void ARM64FloatEmitter::EmitLoadStoreImmediate(u8 size, u32 opc, IndexType type, ARM64Reg Rt,
                                               ARM64Reg Rn, s32 imm)
{
  u32 encoded_size = 0;
  u32 encoded_imm = 0;

  if (size == 8)
    encoded_size = 0;
  else if (size == 16)
    encoded_size = 1;
  else if (size == 32)
    encoded_size = 2;
  else if (size == 64)
    encoded_size = 3;
  else if (size == 128)
    encoded_size = 0;

  if (type == IndexType::Unsigned)
  {
    ASSERT_MSG(DYNA_REC, imm >= 0, "(IndexType::Unsigned) immediate offset must be positive! ({})",
               imm);
    if (size == 16)
    {
      ASSERT_MSG(DYNA_REC, (imm & 0x1) == 0, "16-bit load/store must use aligned offset: {}", imm);
      imm >>= 1;
    }
    else if (size == 32)
    {
      ASSERT_MSG(DYNA_REC, (imm & 0x3) == 0, "32-bit load/store must use aligned offset: {}", imm);
      imm >>= 2;
    }
    else if (size == 64)
    {
      ASSERT_MSG(DYNA_REC, (imm & 0x7) == 0, "64-bit load/store must use aligned offset: {}", imm);
      imm >>= 3;
    }
    else if (size == 128)
    {
      ASSERT_MSG(DYNA_REC, (imm & 0xf) == 0, "128-bit load/store must use aligned offset: {}", imm);
      imm >>= 4;
    }
    ASSERT_MSG(DYNA_REC, imm <= 0xFFF, "Immediate value is too big: {}", imm);
    encoded_imm = (imm & 0xFFF);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, !(imm < -256 || imm > 255),
               "immediate offset must be within range of -256 to 256! {}", imm);
    encoded_imm = (imm & 0x1FF) << 2;
    if (type == IndexType::Post)
      encoded_imm |= 1;
    else
      encoded_imm |= 3;
  }

  Write32((encoded_size << 30) | (0xF << 26) | (type == IndexType::Unsigned ? (1 << 24) : 0) |
          (size == 128 ? (1 << 23) : 0) | (opc << 22) | (encoded_imm << 10) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rt));
}

void ARM64FloatEmitter::EmitScalar2Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd,
                                          ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Only double and single registers are supported!");

  Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (type << 22) | (DecodeReg(Rm) << 16) |
          (opcode << 12) | (1 << 11) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitScalarThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn,
                                            ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Only double and single registers are supported!");

  Write32((1 << 30) | (U << 29) | (0b11110001 << 21) | (size << 22) | (DecodeReg(Rm) << 16) |
          (opcode << 11) | (1 << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn,
                                      ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rd), "Singles are not supported!");
  bool quad = IsQuad(Rd);

  Write32((quad << 30) | (U << 29) | (0b1110001 << 21) | (size << 22) | (DecodeReg(Rm) << 16) |
          (opcode << 11) | (1 << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitCopy(bool Q, u32 op, u32 imm5, u32 imm4, ARM64Reg Rd, ARM64Reg Rn)
{
  Write32((Q << 30) | (op << 29) | (0b111 << 25) | (imm5 << 16) | (imm4 << 11) | (1 << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitScalar2RegMisc(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  Write32((1 << 30) | (U << 29) | (0b11110001 << 21) | (size << 22) | (opcode << 12) | (1 << 11) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitScalarPairwise(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  Write32((1 << 30) | (U << 29) | (0b111100011 << 20) | (size << 22) | (opcode << 12) | (1 << 11) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::Emit2RegMisc(bool Q, bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rd), "Singles are not supported!");

  Write32((Q << 30) | (U << 29) | (0b1110001 << 21) | (size << 22) | (opcode << 12) | (1 << 11) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size,
                                                     ARM64Reg Rt, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rt), "Singles are not supported!");
  bool quad = IsQuad(Rt);

  Write32((quad << 30) | (0b1101 << 24) | (L << 22) | (R << 21) | (opcode << 13) | (S << 12) |
          (size << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size,
                                                     ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rt), "Singles are not supported!");
  bool quad = IsQuad(Rt);

  Write32((quad << 30) | (0x1B << 23) | (L << 22) | (R << 21) | (DecodeReg(Rm) << 16) |
          (opcode << 13) | (S << 12) | (size << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::Emit1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Vector is not supported!");

  Write32((M << 31) | (S << 29) | (0xF1 << 21) | (type << 22) | (opcode << 15) | (1 << 14) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitConversion(bool sf, bool S, u32 type, u32 rmode, u32 opcode,
                                       ARM64Reg Rd, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, Rn <= ARM64Reg::SP, "Only GPRs are supported as source!");

  Write32((sf << 31) | (S << 29) | (0xF1 << 21) | (type << 22) | (rmode << 19) | (opcode << 16) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitConvertScalarToInt(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round,
                                               bool sign)
{
  DEBUG_ASSERT_MSG(DYNA_REC, IsScalar(Rn), "fcvts: Rn must be floating point");
  if (IsGPR(Rd))
  {
    // Use the encoding that transfers the result to a GPR.
    const bool sf = Is64Bit(Rd);
    const int type = IsDouble(Rn) ? 1 : 0;
    int opcode = (sign ? 1 : 0);
    int rmode = 0;
    switch (round)
    {
    case RoundingMode::A:
      rmode = 0;
      opcode |= 4;
      break;
    case RoundingMode::P:
      rmode = 1;
      break;
    case RoundingMode::M:
      rmode = 2;
      break;
    case RoundingMode::Z:
      rmode = 3;
      break;
    case RoundingMode::N:
      rmode = 0;
      break;
    }
    EmitConversion2(sf, 0, true, type, rmode, opcode, 0, Rd, Rn);
  }
  else
  {
    // Use the encoding (vector, single) that keeps the result in the fp register.
    int sz = IsDouble(Rn);
    int opcode = 0;
    switch (round)
    {
    case RoundingMode::A:
      opcode = 0x1C;
      break;
    case RoundingMode::N:
      opcode = 0x1A;
      break;
    case RoundingMode::M:
      opcode = 0x1B;
      break;
    case RoundingMode::P:
      opcode = 0x1A;
      sz |= 2;
      break;
    case RoundingMode::Z:
      opcode = 0x1B;
      sz |= 2;
      break;
    }
    Write32((0x5E << 24) | (sign << 29) | (sz << 22) | (1 << 21) | (opcode << 12) | (2 << 10) |
            (DecodeReg(Rn) << 5) | DecodeReg(Rd));
  }
}

void ARM64FloatEmitter::FCVTS(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round)
{
  EmitConvertScalarToInt(Rd, Rn, round, false);
}

void ARM64FloatEmitter::FCVTU(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round)
{
  EmitConvertScalarToInt(Rd, Rn, round, true);
}

void ARM64FloatEmitter::EmitConversion2(bool sf, bool S, bool direction, u32 type, u32 rmode,
                                        u32 opcode, int scale, ARM64Reg Rd, ARM64Reg Rn)
{
  Write32((sf << 31) | (S << 29) | (0xF0 << 21) | (direction << 21) | (type << 22) | (rmode << 19) |
          (opcode << 16) | (scale << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitCompare(bool M, bool S, u32 op, u32 opcode2, ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rn), "Vector is not supported!");
  bool is_double = IsDouble(Rn);

  Write32((M << 31) | (S << 29) | (0xF1 << 21) | (is_double << 22) | (DecodeReg(Rm) << 16) |
          (op << 14) | (1 << 13) | (DecodeReg(Rn) << 5) | opcode2);
}

void ARM64FloatEmitter::EmitCondSelect(bool M, bool S, CCFlags cond, ARM64Reg Rd, ARM64Reg Rn,
                                       ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Vector is not supported!");
  bool is_double = IsDouble(Rd);

  Write32((M << 31) | (S << 29) | (0xF1 << 21) | (is_double << 22) | (DecodeReg(Rm) << 16) |
          (cond << 12) | (3 << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitPermute(u32 size, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rd), "Singles are not supported!");

  bool quad = IsQuad(Rd);

  u32 encoded_size = 0;
  if (size == 16)
    encoded_size = 1;
  else if (size == 32)
    encoded_size = 2;
  else if (size == 64)
    encoded_size = 3;

  Write32((quad << 30) | (7 << 25) | (encoded_size << 22) | (DecodeReg(Rm) << 16) | (op << 12) |
          (1 << 11) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitExtract(u32 imm4, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !IsSingle(Rd), "Singles are not supported!");

  bool quad = IsQuad(Rd);

  Write32((quad << 30) | (23 << 25) | (op << 22) | (DecodeReg(Rm) << 16) | (imm4 << 11) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitScalarImm(bool M, bool S, u32 type, u32 imm5, ARM64Reg Rd, u32 imm8)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Vector is not supported!");

  bool is_double = !IsSingle(Rd);

  Write32((M << 31) | (S << 29) | (0xF1 << 21) | (is_double << 22) | (type << 22) | (imm8 << 13) |
          (1 << 12) | (imm5 << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitShiftImm(bool Q, bool U, u32 imm, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, (imm & 0b1111000) != 0, "Can't have zero immh");

  Write32((Q << 30) | (U << 29) | (0xF << 24) | (imm << 16) | (opcode << 11) | (1 << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitScalarShiftImm(bool U, u32 imm, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
  Write32((1 << 30) | (U << 29) | (0x3E << 23) | (imm << 16) | (opcode << 11) | (1 << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitLoadStoreMultipleStructure(u32 size, bool L, u32 opcode, ARM64Reg Rt,
                                                       ARM64Reg Rn)
{
  bool quad = IsQuad(Rt);
  u32 encoded_size = 0;

  if (size == 16)
    encoded_size = 1;
  else if (size == 32)
    encoded_size = 2;
  else if (size == 64)
    encoded_size = 3;

  Write32((quad << 30) | (3 << 26) | (L << 22) | (opcode << 12) | (encoded_size << 10) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::EmitLoadStoreMultipleStructurePost(u32 size, bool L, u32 opcode,
                                                           ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
  bool quad = IsQuad(Rt);
  u32 encoded_size = 0;

  if (size == 16)
    encoded_size = 1;
  else if (size == 32)
    encoded_size = 2;
  else if (size == 64)
    encoded_size = 3;

  Write32((quad << 30) | (0b11001 << 23) | (L << 22) | (DecodeReg(Rm) << 16) | (opcode << 12) |
          (encoded_size << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::EmitScalar1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd,
                                          ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !IsQuad(Rd), "Vector is not supported!");

  Write32((M << 31) | (S << 29) | (0xF1 << 21) | (type << 22) | (opcode << 15) | (1 << 14) |
          (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitVectorxElement(bool U, u32 size, bool L, u32 opcode, bool H,
                                           ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  bool quad = IsQuad(Rd);

  Write32((quad << 30) | (U << 29) | (0xF << 24) | (size << 22) | (L << 21) |
          (DecodeReg(Rm) << 16) | (opcode << 12) | (H << 11) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rd));
}

void ARM64FloatEmitter::EmitLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  ASSERT_MSG(DYNA_REC, !(imm < -256 || imm > 255), "received too large offset: {}", imm);

  Write32((size << 30) | (0xF << 26) | (op << 22) | ((imm & 0x1FF) << 12) | (DecodeReg(Rn) << 5) |
          DecodeReg(Rt));
}

void ARM64FloatEmitter::EncodeLoadStorePair(u32 size, bool load, IndexType type, ARM64Reg Rt,
                                            ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
  u32 type_encode = 0;
  u32 opc = 0;

  switch (type)
  {
  case IndexType::Signed:
    type_encode = 0b010;
    break;
  case IndexType::Post:
    type_encode = 0b001;
    break;
  case IndexType::Pre:
    type_encode = 0b011;
    break;
  case IndexType::Unsigned:
    ASSERT_MSG(DYNA_REC, false, "IndexType::Unsigned is unsupported!");
    break;
  }

  if (size == 128)
  {
    ASSERT_MSG(DYNA_REC, !(imm & 0xF), "Invalid offset {:#x}! (size {})", imm, size);
    opc = 2;
    imm >>= 4;
  }
  else if (size == 64)
  {
    ASSERT_MSG(DYNA_REC, !(imm & 0x7), "Invalid offset {:#x}! (size {})", imm, size);
    opc = 1;
    imm >>= 3;
  }
  else if (size == 32)
  {
    ASSERT_MSG(DYNA_REC, !(imm & 0x3), "Invalid offset {:#x}! (size {})", imm, size);
    opc = 0;
    imm >>= 2;
  }

  ASSERT_MSG(DYNA_REC, imm >= -64 && imm < 64, "imm too large for load/store pair! {}", imm);

  Write32((opc << 30) | (0b1011 << 26) | (type_encode << 23) | (load << 22) | ((imm & 0x7F) << 15) |
          (DecodeReg(Rt2) << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::EncodeLoadStoreRegisterOffset(u32 size, bool load, ARM64Reg Rt, ARM64Reg Rn,
                                                      ArithOption Rm)
{
  ASSERT_MSG(DYNA_REC, Rm.IsExtended(), "Must contain an extended reg as Rm!");

  u32 encoded_size = 0;
  u32 encoded_op = 0;

  if (size == 8)
  {
    encoded_size = 0;
    encoded_op = 0;
  }
  else if (size == 16)
  {
    encoded_size = 1;
    encoded_op = 0;
  }
  else if (size == 32)
  {
    encoded_size = 2;
    encoded_op = 0;
  }
  else if (size == 64)
  {
    encoded_size = 3;
    encoded_op = 0;
  }
  else if (size == 128)
  {
    encoded_size = 0;
    encoded_op = 2;
  }

  if (load)
    encoded_op |= 1;

  const int decoded_Rm = DecodeReg(Rm.GetReg());

  Write32((encoded_size << 30) | (encoded_op << 22) | (0b111100001 << 21) | (decoded_Rm << 16) |
          Rm.GetData() | (1 << 11) | (DecodeReg(Rn) << 5) | DecodeReg(Rt));
}

void ARM64FloatEmitter::EncodeModImm(bool Q, u8 op, u8 cmode, u8 o2, ARM64Reg Rd, u8 abcdefgh)
{
  union
  {
    u8 hex;
    struct
    {
      unsigned defgh : 5;
      unsigned abc : 3;
    };
  } v;
  v.hex = abcdefgh;
  Write32((Q << 30) | (op << 29) | (0xF << 24) | (v.abc << 16) | (cmode << 12) | (o2 << 11) |
          (1 << 10) | (v.defgh << 5) | DecodeReg(Rd));
}

void ARM64FloatEmitter::LDR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EmitLoadStoreImmediate(size, 1, type, Rt, Rn, imm);
}
void ARM64FloatEmitter::STR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  EmitLoadStoreImmediate(size, 0, type, Rt, Rn, imm);
}

// Loadstore unscaled
void ARM64FloatEmitter::LDUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  u32 encoded_size = 0;
  u32 encoded_op = 0;

  if (size == 8)
  {
    encoded_size = 0;
    encoded_op = 1;
  }
  else if (size == 16)
  {
    encoded_size = 1;
    encoded_op = 1;
  }
  else if (size == 32)
  {
    encoded_size = 2;
    encoded_op = 1;
  }
  else if (size == 64)
  {
    encoded_size = 3;
    encoded_op = 1;
  }
  else if (size == 128)
  {
    encoded_size = 0;
    encoded_op = 3;
  }

  EmitLoadStoreUnscaled(encoded_size, encoded_op, Rt, Rn, imm);
}
void ARM64FloatEmitter::STUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
  u32 encoded_size = 0;
  u32 encoded_op = 0;

  if (size == 8)
  {
    encoded_size = 0;
    encoded_op = 0;
  }
  else if (size == 16)
  {
    encoded_size = 1;
    encoded_op = 0;
  }
  else if (size == 32)
  {
    encoded_size = 2;
    encoded_op = 0;
  }
  else if (size == 64)
  {
    encoded_size = 3;
    encoded_op = 0;
  }
  else if (size == 128)
  {
    encoded_size = 0;
    encoded_op = 2;
  }

  EmitLoadStoreUnscaled(encoded_size, encoded_op, Rt, Rn, imm);
}

// Loadstore single structure
void ARM64FloatEmitter::LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn)
{
  bool S = 0;
  u32 opcode = 0;
  u32 encoded_size = 0;
  ARM64Reg encoded_reg = ARM64Reg::INVALID_REG;

  if (size == 8)
  {
    S = (index & 4) != 0;
    opcode = 0;
    encoded_size = index & 3;
    if (index & 8)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 16)
  {
    S = (index & 2) != 0;
    opcode = 2;
    encoded_size = (index & 1) << 1;
    if (index & 4)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 32)
  {
    S = (index & 1) != 0;
    opcode = 4;
    encoded_size = 0;
    if (index & 2)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 64)
  {
    S = 0;
    opcode = 4;
    encoded_size = 1;
    if (index == 1)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }

  EmitLoadStoreSingleStructure(1, 0, opcode, S, encoded_size, encoded_reg, Rn);
}

void ARM64FloatEmitter::LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm)
{
  bool S = 0;
  u32 opcode = 0;
  u32 encoded_size = 0;
  ARM64Reg encoded_reg = ARM64Reg::INVALID_REG;

  if (size == 8)
  {
    S = (index & 4) != 0;
    opcode = 0;
    encoded_size = index & 3;
    if (index & 8)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 16)
  {
    S = (index & 2) != 0;
    opcode = 2;
    encoded_size = (index & 1) << 1;
    if (index & 4)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 32)
  {
    S = (index & 1) != 0;
    opcode = 4;
    encoded_size = 0;
    if (index & 2)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 64)
  {
    S = 0;
    opcode = 4;
    encoded_size = 1;
    if (index == 1)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }

  EmitLoadStoreSingleStructure(1, 0, opcode, S, encoded_size, encoded_reg, Rn, Rm);
}

void ARM64FloatEmitter::LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn)
{
  EmitLoadStoreSingleStructure(1, 0, 6, 0, size >> 4, Rt, Rn);
}
void ARM64FloatEmitter::LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn)
{
  EmitLoadStoreSingleStructure(1, 1, 6, 0, size >> 4, Rt, Rn);
}
void ARM64FloatEmitter::LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitLoadStoreSingleStructure(1, 0, 6, 0, size >> 4, Rt, Rn, Rm);
}
void ARM64FloatEmitter::LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitLoadStoreSingleStructure(1, 1, 6, 0, size >> 4, Rt, Rn, Rm);
}

void ARM64FloatEmitter::ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn)
{
  bool S = 0;
  u32 opcode = 0;
  u32 encoded_size = 0;
  ARM64Reg encoded_reg = ARM64Reg::INVALID_REG;

  if (size == 8)
  {
    S = (index & 4) != 0;
    opcode = 0;
    encoded_size = index & 3;
    if (index & 8)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 16)
  {
    S = (index & 2) != 0;
    opcode = 2;
    encoded_size = (index & 1) << 1;
    if (index & 4)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 32)
  {
    S = (index & 1) != 0;
    opcode = 4;
    encoded_size = 0;
    if (index & 2)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 64)
  {
    S = 0;
    opcode = 4;
    encoded_size = 1;
    if (index == 1)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }

  EmitLoadStoreSingleStructure(0, 0, opcode, S, encoded_size, encoded_reg, Rn);
}

void ARM64FloatEmitter::ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm)
{
  bool S = 0;
  u32 opcode = 0;
  u32 encoded_size = 0;
  ARM64Reg encoded_reg = ARM64Reg::INVALID_REG;

  if (size == 8)
  {
    S = (index & 4) != 0;
    opcode = 0;
    encoded_size = index & 3;
    if (index & 8)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 16)
  {
    S = (index & 2) != 0;
    opcode = 2;
    encoded_size = (index & 1) << 1;
    if (index & 4)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 32)
  {
    S = (index & 1) != 0;
    opcode = 4;
    encoded_size = 0;
    if (index & 2)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }
  else if (size == 64)
  {
    S = 0;
    opcode = 4;
    encoded_size = 1;
    if (index == 1)
      encoded_reg = EncodeRegToQuad(Rt);
    else
      encoded_reg = EncodeRegToDouble(Rt);
  }

  EmitLoadStoreSingleStructure(0, 0, opcode, S, encoded_size, encoded_reg, Rn, Rm);
}

// Loadstore multiple structure
void ARM64FloatEmitter::LD1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !(count == 0 || count > 4), "Must have a count of 1 to 4 registers! ({})",
             count);
  u32 opcode = 0;
  if (count == 1)
    opcode = 0b111;
  else if (count == 2)
    opcode = 0b1010;
  else if (count == 3)
    opcode = 0b0110;
  else if (count == 4)
    opcode = 0b0010;
  EmitLoadStoreMultipleStructure(size, 1, opcode, Rt, Rn);
}
void ARM64FloatEmitter::LD1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn,
                            ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !(count == 0 || count > 4), "Must have a count of 1 to 4 registers! ({})",
             count);
  ASSERT_MSG(DYNA_REC, type == IndexType::Post, "Only post indexing is supported!");

  u32 opcode = 0;
  if (count == 1)
    opcode = 0b111;
  else if (count == 2)
    opcode = 0b1010;
  else if (count == 3)
    opcode = 0b0110;
  else if (count == 4)
    opcode = 0b0010;
  EmitLoadStoreMultipleStructurePost(size, 1, opcode, Rt, Rn, Rm);
}
void ARM64FloatEmitter::ST1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn)
{
  ASSERT_MSG(DYNA_REC, !(count == 0 || count > 4), "Must have a count of 1 to 4 registers! ({})",
             count);
  u32 opcode = 0;
  if (count == 1)
    opcode = 0b111;
  else if (count == 2)
    opcode = 0b1010;
  else if (count == 3)
    opcode = 0b0110;
  else if (count == 4)
    opcode = 0b0010;
  EmitLoadStoreMultipleStructure(size, 0, opcode, Rt, Rn);
}
void ARM64FloatEmitter::ST1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn,
                            ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, !(count == 0 || count > 4), "Must have a count of 1 to 4 registers! ({})",
             count);
  ASSERT_MSG(DYNA_REC, type == IndexType::Post, "Only post indexing is supporte!");

  u32 opcode = 0;
  if (count == 1)
    opcode = 0b111;
  else if (count == 2)
    opcode = 0b1010;
  else if (count == 3)
    opcode = 0b0110;
  else if (count == 4)
    opcode = 0b0010;
  EmitLoadStoreMultipleStructurePost(size, 0, opcode, Rt, Rn, Rm);
}

// Scalar - 1 Source
void ARM64FloatEmitter::FMOV(ARM64Reg Rd, ARM64Reg Rn, bool top)
{
  if (IsScalar(Rd) && IsScalar(Rn))
  {
    EmitScalar1Source(0, 0, IsDouble(Rd), 0, Rd, Rn);
  }
  else if (IsGPR(Rd) != IsGPR(Rn))
  {
    const ARM64Reg gpr = IsGPR(Rn) ? Rn : Rd;
    const ARM64Reg fpr = IsGPR(Rn) ? Rd : Rn;

    const int sf = Is64Bit(gpr) ? 1 : 0;
    const int type = Is64Bit(gpr) ? (top ? 2 : 1) : 0;
    const int rmode = top ? 1 : 0;
    const int opcode = IsGPR(Rn) ? 7 : 6;

    ASSERT_MSG(DYNA_REC, !top || IsQuad(fpr), "FMOV: top can only be used with quads");

    // TODO: Should this check be more lenient? Sometimes you do want to do things like
    // read the lower 32 bits of a double
    ASSERT_MSG(DYNA_REC,
               (!Is64Bit(gpr) && IsSingle(fpr)) ||
                   (Is64Bit(gpr) && ((IsDouble(fpr) && !top) || (IsQuad(fpr) && top))),
               "FMOV: Mismatched sizes");

    Write32((sf << 31) | (0x1e << 24) | (type << 22) | (1 << 21) | (rmode << 19) | (opcode << 16) |
            (DecodeReg(Rn) << 5) | DecodeReg(Rd));
  }
  else
  {
    ASSERT_MSG(DYNA_REC, 0, "FMOV: Unsupported case");
  }
}

// Loadstore paired
void ARM64FloatEmitter::LDP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn,
                            s32 imm)
{
  EncodeLoadStorePair(size, true, type, Rt, Rt2, Rn, imm);
}
void ARM64FloatEmitter::STP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn,
                            s32 imm)
{
  EncodeLoadStorePair(size, false, type, Rt, Rt2, Rn, imm);
}

// Loadstore register offset
void ARM64FloatEmitter::STR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(size, false, Rt, Rn, Rm);
}
void ARM64FloatEmitter::LDR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
  EncodeLoadStoreRegisterOffset(size, true, Rt, Rn, Rm);
}

void ARM64FloatEmitter::FABS(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar1Source(0, 0, IsDouble(Rd), 1, Rd, Rn);
}
void ARM64FloatEmitter::FNEG(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar1Source(0, 0, IsDouble(Rd), 2, Rd, Rn);
}
void ARM64FloatEmitter::FSQRT(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar1Source(0, 0, IsDouble(Rd), 3, Rd, Rn);
}
void ARM64FloatEmitter::FRINTI(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar1Source(0, 0, IsDouble(Rd), 15, Rd, Rn);
}

void ARM64FloatEmitter::FRECPE(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar2RegMisc(0, IsDouble(Rd) ? 3 : 2, 0x1D, Rd, Rn);
}
void ARM64FloatEmitter::FRSQRTE(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalar2RegMisc(1, IsDouble(Rd) ? 3 : 2, 0x1D, Rd, Rn);
}

// Scalar - pairwise
void ARM64FloatEmitter::FADDP(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalarPairwise(1, IsDouble(Rd), 0b01101, Rd, Rn);
}
void ARM64FloatEmitter::FMAXP(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalarPairwise(1, IsDouble(Rd), 0b01111, Rd, Rn);
}
void ARM64FloatEmitter::FMINP(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalarPairwise(1, IsDouble(Rd) ? 3 : 2, 0b01111, Rd, Rn);
}
void ARM64FloatEmitter::FMAXNMP(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalarPairwise(1, IsDouble(Rd), 0b01100, Rd, Rn);
}
void ARM64FloatEmitter::FMINNMP(ARM64Reg Rd, ARM64Reg Rn)
{
  EmitScalarPairwise(1, IsDouble(Rd) ? 3 : 2, 0b01100, Rd, Rn);
}

// Scalar - 2 Source
void ARM64FloatEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  ASSERT_MSG(DYNA_REC, IsDouble(Rd), "Only double registers are supported!");
  EmitScalarThreeSame(0, 3, 0b10000, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 2, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 0, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 1, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMAX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 4, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMIN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 5, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMAXNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 6, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMINNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 7, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FNMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitScalar2Source(0, 0, IsDouble(Rd), 8, Rd, Rn, Rm);
}

void ARM64FloatEmitter::FMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EmitScalar3Source(IsDouble(Rd), Rd, Rn, Rm, Ra, 0);
}
void ARM64FloatEmitter::FMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EmitScalar3Source(IsDouble(Rd), Rd, Rn, Rm, Ra, 1);
}
void ARM64FloatEmitter::FNMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EmitScalar3Source(IsDouble(Rd), Rd, Rn, Rm, Ra, 2);
}
void ARM64FloatEmitter::FNMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
  EmitScalar3Source(IsDouble(Rd), Rd, Rn, Rm, Ra, 3);
}

void ARM64FloatEmitter::EmitScalar3Source(bool isDouble, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm,
                                          ARM64Reg Ra, int opcode)
{
  int type = isDouble ? 1 : 0;
  int o1 = opcode >> 1;
  int o0 = opcode & 1;
  m_emit->Write32((0x1F << 24) | (type << 22) | (o1 << 21) | (DecodeReg(Rm) << 16) | (o0 << 15) |
                  (DecodeReg(Ra) << 10) | (DecodeReg(Rn) << 5) | DecodeReg(Rd));
}

// Scalar floating point immediate
void ARM64FloatEmitter::FMOV(ARM64Reg Rd, uint8_t imm8)
{
  EmitScalarImm(0, 0, 0, 0, Rd, imm8);
}

// Vector
void ARM64FloatEmitter::ADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, MathUtil::IntLog2(size) - 3, 0b10000, Rd, Rn, Rm);
}
void ARM64FloatEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 0, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 1, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::BIF(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, 3, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::BIT(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, 2, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::BSL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, 1, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
  u32 imm5 = 0;

  if (size == 8)
  {
    imm5 = 1;
    imm5 |= index << 1;
  }
  else if (size == 16)
  {
    imm5 = 2;
    imm5 |= index << 2;
  }
  else if (size == 32)
  {
    imm5 = 4;
    imm5 |= index << 3;
  }
  else if (size == 64)
  {
    imm5 = 8;
    imm5 |= index << 4;
  }

  EmitCopy(IsQuad(Rd), 0, imm5, 0, Rd, Rn);
}
void ARM64FloatEmitter::FABS(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0xF, Rd, Rn);
}
void ARM64FloatEmitter::FADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, size >> 6, 0x1A, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMAX(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, size >> 6, 0b11110, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMLA(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, size >> 6, 0x19, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMIN(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 2 | size >> 6, 0b11110, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCVTL(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(false, 0, size >> 6, 0x17, Rd, Rn);
}
void ARM64FloatEmitter::FCVTL2(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(true, 0, size >> 6, 0x17, Rd, Rn);
}
void ARM64FloatEmitter::FCVTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, dest_size >> 5, 0x16, Rd, Rn);
}
void ARM64FloatEmitter::FCVTZS(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0x1B, Rd, Rn);
}
void ARM64FloatEmitter::FCVTZU(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 2 | (size >> 6), 0x1B, Rd, Rn);
}
void ARM64FloatEmitter::FDIV(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, size >> 6, 0x1F, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, size >> 6, 0x1B, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FNEG(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 2 | (size >> 6), 0xF, Rd, Rn);
}
void ARM64FloatEmitter::FRECPE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0x1D, Rd, Rn);
}
void ARM64FloatEmitter::FRSQRTE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 2 | (size >> 6), 0x1D, Rd, Rn);
}
void ARM64FloatEmitter::FSUB(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 2 | (size >> 6), 0x1A, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMLS(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 2 | (size >> 6), 0x19, Rd, Rn, Rm);
}
void ARM64FloatEmitter::NOT(ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 0, 5, Rd, Rn);
}
void ARM64FloatEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 2, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, 3, 3, Rd, Rn, Rm);
}
void ARM64FloatEmitter::REV16(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, size >> 4, 1, Rd, Rn);
}
void ARM64FloatEmitter::REV32(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, size >> 4, 0, Rd, Rn);
}
void ARM64FloatEmitter::REV64(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, size >> 4, 0, Rd, Rn);
}
void ARM64FloatEmitter::SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, size >> 6, 0x1D, Rd, Rn);
}
void ARM64FloatEmitter::UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, size >> 6, 0x1D, Rd, Rn);
}
void ARM64FloatEmitter::SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale)
{
  EmitShiftImm(IsQuad(Rd), 0, size * 2 - scale, 0x1C, Rd, Rn);
}
void ARM64FloatEmitter::UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale)
{
  EmitShiftImm(IsQuad(Rd), 1, size * 2 - scale, 0x1C, Rd, Rn);
}
void ARM64FloatEmitter::SQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(false, 0, dest_size >> 4, 0b10100, Rd, Rn);
}
void ARM64FloatEmitter::SQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(true, 0, dest_size >> 4, 0b10100, Rd, Rn);
}
void ARM64FloatEmitter::UQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(false, 1, dest_size >> 4, 0b10100, Rd, Rn);
}
void ARM64FloatEmitter::UQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(true, 1, dest_size >> 4, 0b10100, Rd, Rn);
}
void ARM64FloatEmitter::XTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(false, 0, dest_size >> 4, 0b10010, Rd, Rn);
}
void ARM64FloatEmitter::XTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(true, 0, dest_size >> 4, 0b10010, Rd, Rn);
}

// Move
void ARM64FloatEmitter::DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  u32 imm5 = 0;

  if (size == 8)
    imm5 = 1;
  else if (size == 16)
    imm5 = 2;
  else if (size == 32)
    imm5 = 4;
  else if (size == 64)
    imm5 = 8;

  EmitCopy(IsQuad(Rd), 0, imm5, 1, Rd, Rn);
}
void ARM64FloatEmitter::INS(u8 size, ARM64Reg Rd, u8 index, ARM64Reg Rn)
{
  u32 imm5 = 0;

  if (size == 8)
  {
    imm5 = 1;
    imm5 |= index << 1;
  }
  else if (size == 16)
  {
    imm5 = 2;
    imm5 |= index << 2;
  }
  else if (size == 32)
  {
    imm5 = 4;
    imm5 |= index << 3;
  }
  else if (size == 64)
  {
    imm5 = 8;
    imm5 |= index << 4;
  }

  EmitCopy(1, 0, imm5, 3, Rd, Rn);
}
void ARM64FloatEmitter::INS(u8 size, ARM64Reg Rd, u8 index1, ARM64Reg Rn, u8 index2)
{
  u32 imm5 = 0, imm4 = 0;

  if (size == 8)
  {
    imm5 = 1;
    imm5 |= index1 << 1;
    imm4 = index2;
  }
  else if (size == 16)
  {
    imm5 = 2;
    imm5 |= index1 << 2;
    imm4 = index2 << 1;
  }
  else if (size == 32)
  {
    imm5 = 4;
    imm5 |= index1 << 3;
    imm4 = index2 << 2;
  }
  else if (size == 64)
  {
    imm5 = 8;
    imm5 |= index1 << 4;
    imm4 = index2 << 3;
  }

  EmitCopy(1, 1, imm5, imm4, Rd, Rn);
}

void ARM64FloatEmitter::UMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
  bool b64Bit = Is64Bit(Rd);
  ASSERT_MSG(DYNA_REC, Rd < ARM64Reg::SP, "Destination must be a GPR!");
  ASSERT_MSG(DYNA_REC, !(b64Bit && size != 64),
             "Must have a size of 64 when destination is 64bit!");
  u32 imm5 = 0;

  if (size == 8)
  {
    imm5 = 1;
    imm5 |= index << 1;
  }
  else if (size == 16)
  {
    imm5 = 2;
    imm5 |= index << 2;
  }
  else if (size == 32)
  {
    imm5 = 4;
    imm5 |= index << 3;
  }
  else if (size == 64)
  {
    imm5 = 8;
    imm5 |= index << 4;
  }

  EmitCopy(b64Bit, 0, imm5, 7, Rd, Rn);
}
void ARM64FloatEmitter::SMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
  bool b64Bit = Is64Bit(Rd);
  ASSERT_MSG(DYNA_REC, Rd < ARM64Reg::SP, "Destination must be a GPR!");
  ASSERT_MSG(DYNA_REC, size != 64, "SMOV doesn't support 64bit destination. Use UMOV!");
  u32 imm5 = 0;

  if (size == 8)
  {
    imm5 = 1;
    imm5 |= index << 1;
  }
  else if (size == 16)
  {
    imm5 = 2;
    imm5 |= index << 2;
  }
  else if (size == 32)
  {
    imm5 = 4;
    imm5 |= index << 3;
  }

  EmitCopy(b64Bit, 0, imm5, 5, Rd, Rn);
}

// One source
void ARM64FloatEmitter::FCVT(u8 size_to, u8 size_from, ARM64Reg Rd, ARM64Reg Rn)
{
  u32 dst_encoding = 0;
  u32 src_encoding = 0;

  if (size_to == 16)
    dst_encoding = 3;
  else if (size_to == 32)
    dst_encoding = 0;
  else if (size_to == 64)
    dst_encoding = 1;

  if (size_from == 16)
    src_encoding = 3;
  else if (size_from == 32)
    src_encoding = 0;
  else if (size_from == 64)
    src_encoding = 1;

  Emit1Source(0, 0, src_encoding, 4 | dst_encoding, Rd, Rn);
}

void ARM64FloatEmitter::SCVTF(ARM64Reg Rd, ARM64Reg Rn)
{
  if (IsScalar(Rn))
  {
    // Source is in FP register (like destination!). We must use a vector encoding.
    bool sign = false;
    int sz = IsDouble(Rn);
    Write32((0x5e << 24) | (sign << 29) | (sz << 22) | (0x876 << 10) | (DecodeReg(Rn) << 5) |
            DecodeReg(Rd));
  }
  else
  {
    bool sf = Is64Bit(Rn);
    u32 type = 0;
    if (IsDouble(Rd))
      type = 1;
    EmitConversion(sf, 0, type, 0, 2, Rd, Rn);
  }
}

void ARM64FloatEmitter::UCVTF(ARM64Reg Rd, ARM64Reg Rn)
{
  if (IsScalar(Rn))
  {
    // Source is in FP register (like destination!). We must use a vector encoding.
    bool sign = true;
    int sz = IsDouble(Rn);
    Write32((0x5e << 24) | (sign << 29) | (sz << 22) | (0x876 << 10) | (DecodeReg(Rn) << 5) |
            DecodeReg(Rd));
  }
  else
  {
    bool sf = Is64Bit(Rn);
    u32 type = 0;
    if (IsDouble(Rd))
      type = 1;

    EmitConversion(sf, 0, type, 0, 3, Rd, Rn);
  }
}

void ARM64FloatEmitter::SCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale)
{
  bool sf = Is64Bit(Rn);
  u32 type = 0;
  if (IsDouble(Rd))
    type = 1;

  EmitConversion2(sf, 0, false, type, 0, 2, 64 - scale, Rd, Rn);
}

void ARM64FloatEmitter::UCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale)
{
  bool sf = Is64Bit(Rn);
  u32 type = 0;
  if (IsDouble(Rd))
    type = 1;

  EmitConversion2(sf, 0, false, type, 0, 3, 64 - scale, Rd, Rn);
}

void ARM64FloatEmitter::FCMP(ARM64Reg Rn, ARM64Reg Rm)
{
  EmitCompare(0, 0, 0, 0, Rn, Rm);
}
void ARM64FloatEmitter::FCMP(ARM64Reg Rn)
{
  EmitCompare(0, 0, 0, 8, Rn, (ARM64Reg)0);
}
void ARM64FloatEmitter::FCMPE(ARM64Reg Rn, ARM64Reg Rm)
{
  EmitCompare(0, 0, 0, 0x10, Rn, Rm);
}
void ARM64FloatEmitter::FCMPE(ARM64Reg Rn)
{
  EmitCompare(0, 0, 0, 0x18, Rn, (ARM64Reg)0);
}
void ARM64FloatEmitter::FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(0, size >> 6, 0x1C, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0xD, Rd, Rn);
}
void ARM64FloatEmitter::FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, size >> 6, 0x1C, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 2 | (size >> 6), 0x0C, Rd, Rn);
}
void ARM64FloatEmitter::FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, 2 | (size >> 6), 0x1C, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0x0C, Rd, Rn);
}
void ARM64FloatEmitter::FCMLE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 1, 2 | (size >> 6), 0xD, Rd, Rn);
}
void ARM64FloatEmitter::FCMLT(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
  Emit2RegMisc(IsQuad(Rd), 0, 2 | (size >> 6), 0xE, Rd, Rn);
}
void ARM64FloatEmitter::FACGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, size >> 6, 0x1D, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FACGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitThreeSame(1, 2 | (size >> 6), 0x1D, Rd, Rn, Rm);
}

void ARM64FloatEmitter::FCSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
  EmitCondSelect(0, 0, cond, Rd, Rn, Rm);
}

// Permute
void ARM64FloatEmitter::UZP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b001, Rd, Rn, Rm);
}
void ARM64FloatEmitter::TRN1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b010, Rd, Rn, Rm);
}
void ARM64FloatEmitter::ZIP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::UZP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b101, Rd, Rn, Rm);
}
void ARM64FloatEmitter::TRN2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b110, Rd, Rn, Rm);
}
void ARM64FloatEmitter::ZIP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
  EmitPermute(size, 0b111, Rd, Rn, Rm);
}

// Extract
void ARM64FloatEmitter::EXT(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u32 index)
{
  EmitExtract(index, 0, Rd, Rn, Rm);
}

// Scalar shift by immediate
void ARM64FloatEmitter::SHL(ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  constexpr size_t src_size = 64;
  ASSERT_MSG(DYNA_REC, IsDouble(Rd), "Only double registers are supported!");
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitScalarShiftImm(0, src_size | shift, 0b01010, Rd, Rn);
}

void ARM64FloatEmitter::URSHR(ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  constexpr size_t src_size = 64;
  ASSERT_MSG(DYNA_REC, IsDouble(Rd), "Only double registers are supported!");
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitScalarShiftImm(1, src_size * 2 - shift, 0b00100, Rd, Rn);
}

// Vector shift by immediate
void ARM64FloatEmitter::SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  SSHLL(src_size, Rd, Rn, shift, false);
}
void ARM64FloatEmitter::SSHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  SSHLL(src_size, Rd, Rn, shift, true);
}
void ARM64FloatEmitter::SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  SHRN(dest_size, Rd, Rn, shift, false);
}
void ARM64FloatEmitter::SHRN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  SHRN(dest_size, Rd, Rn, shift, true);
}
void ARM64FloatEmitter::USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  USHLL(src_size, Rd, Rn, shift, false);
}
void ARM64FloatEmitter::USHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  USHLL(src_size, Rd, Rn, shift, true);
}
void ARM64FloatEmitter::SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
  SXTL(src_size, Rd, Rn, false);
}
void ARM64FloatEmitter::SXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
  SXTL(src_size, Rd, Rn, true);
}
void ARM64FloatEmitter::UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
  UXTL(src_size, Rd, Rn, false);
}
void ARM64FloatEmitter::UXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
  UXTL(src_size, Rd, Rn, true);
}

void ARM64FloatEmitter::SHL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitShiftImm(1, 0, src_size | shift, 0b01010, Rd, Rn);
}

void ARM64FloatEmitter::SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper)
{
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitShiftImm(upper, 0, src_size | shift, 0b10100, Rd, Rn);
}

void ARM64FloatEmitter::URSHR(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitShiftImm(1, 1, src_size * 2 - shift, 0b00100, Rd, Rn);
}

void ARM64FloatEmitter::USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper)
{
  ASSERT_MSG(DYNA_REC, shift < src_size, "Shift amount must be less than the element size! {} {}",
             shift, src_size);
  EmitShiftImm(upper, 1, src_size | shift, 0b10100, Rd, Rn);
}

void ARM64FloatEmitter::SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper)
{
  ASSERT_MSG(DYNA_REC, shift < dest_size, "Shift amount must be less than the element size! {} {}",
             shift, dest_size);
  EmitShiftImm(upper, 1, dest_size * 2 - shift, 0b10000, Rd, Rn);
}

void ARM64FloatEmitter::SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper)
{
  SSHLL(src_size, Rd, Rn, 0, upper);
}

void ARM64FloatEmitter::UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper)
{
  USHLL(src_size, Rd, Rn, 0, upper);
}

// vector x indexed element
void ARM64FloatEmitter::FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index)
{
  ASSERT_MSG(DYNA_REC, size == 32 || size == 64, "Only 32bit or 64bit sizes are supported! {}",
             size);

  bool L = false;
  bool H = false;
  if (size == 32)
  {
    L = index & 1;
    H = (index >> 1) & 1;
  }
  else if (size == 64)
  {
    H = index == 1;
  }

  EmitVectorxElement(0, 2 | (size >> 6), L, 0x9, H, Rd, Rn, Rm);
}

void ARM64FloatEmitter::FMLA(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index)
{
  ASSERT_MSG(DYNA_REC, size == 32 || size == 64, "Only 32bit or 64bit sizes are supported! {}",
             size);

  bool L = false;
  bool H = false;
  if (size == 32)
  {
    L = index & 1;
    H = (index >> 1) & 1;
  }
  else if (size == 64)
  {
    H = index == 1;
  }

  EmitVectorxElement(0, 2 | (size >> 6), L, 1, H, Rd, Rn, Rm);
}

// Modified Immediate
void ARM64FloatEmitter::MOVI(u8 size, ARM64Reg Rd, u64 imm, u8 shift)
{
  bool Q = IsQuad(Rd);
  u8 cmode = 0;
  u8 op = 0;
  u8 abcdefgh = imm & 0xFF;
  if (size == 8)
  {
    ASSERT_MSG(DYNA_REC, shift == 0, "size8 doesn't support shift! ({})", shift);
    ASSERT_MSG(DYNA_REC, !(imm & ~0xFFULL), "size8 only supports 8bit values! ({})", imm);
  }
  else if (size == 16)
  {
    ASSERT_MSG(DYNA_REC, shift == 0 || shift == 8, "size16 only supports shift of 0 or 8! ({})",
               shift);
    ASSERT_MSG(DYNA_REC, !(imm & ~0xFFULL), "size16 only supports 8bit values! ({})", imm);

    if (shift == 8)
      cmode |= 2;
  }
  else if (size == 32)
  {
    ASSERT_MSG(DYNA_REC, shift == 0 || shift == 8 || shift == 16 || shift == 24,
               "size32 only supports shift of 0, 8, 16, or 24! ({})", shift);
    // XXX: Implement support for MOVI - shifting ones variant
    ASSERT_MSG(DYNA_REC, !(imm & ~0xFFULL), "size32 only supports 8bit values! ({})", imm);
    switch (shift)
    {
    case 8:
      cmode |= 2;
      break;
    case 16:
      cmode |= 4;
      break;
    case 24:
      cmode |= 6;
      break;
    default:
      break;
    }
  }
  else  // 64
  {
    ASSERT_MSG(DYNA_REC, shift == 0, "size64 doesn't support shift! ({})", shift);

    op = 1;
    cmode = 0xE;
    abcdefgh = 0;
    for (int i = 0; i < 8; ++i)
    {
      u8 tmp = (imm >> (i << 3)) & 0xFF;
      ASSERT_MSG(DYNA_REC, tmp == 0xFF || tmp == 0, "size64 Invalid immediate! ({} -> {})", imm,
                 tmp);
      if (tmp == 0xFF)
        abcdefgh |= (1 << i);
    }
  }
  EncodeModImm(Q, op, cmode, 0, Rd, abcdefgh);
}

void ARM64FloatEmitter::ORR_BIC(u8 size, ARM64Reg Rd, u8 imm, u8 shift, u8 op)
{
  bool Q = IsQuad(Rd);
  u8 cmode = 1;
  if (size == 16)
  {
    ASSERT_MSG(DYNA_REC, shift == 0 || shift == 8, "size16 only supports shift of 0 or 8! {}",
               shift);

    if (shift == 8)
      cmode |= 2;
  }
  else if (size == 32)
  {
    ASSERT_MSG(DYNA_REC, shift == 0 || shift == 8 || shift == 16 || shift == 24,
               "size32 only supports shift of 0, 8, 16, or 24! ({})", shift);
    // XXX: Implement support for MOVI - shifting ones variant
    switch (shift)
    {
    case 8:
      cmode |= 2;
      break;
    case 16:
      cmode |= 4;
      break;
    case 24:
      cmode |= 6;
      break;
    default:
      break;
    }
  }
  else
  {
    ASSERT_MSG(DYNA_REC, false, "Only size of 16 or 32 is supported! ({})", size);
  }
  EncodeModImm(Q, op, cmode, 0, Rd, imm);
}

void ARM64FloatEmitter::ORR(u8 size, ARM64Reg Rd, u8 imm, u8 shift)
{
  ORR_BIC(size, Rd, imm, shift, 0);
}

void ARM64FloatEmitter::BIC(u8 size, ARM64Reg Rd, u8 imm, u8 shift)
{
  ORR_BIC(size, Rd, imm, shift, 1);
}

void ARM64FloatEmitter::ABI_PushRegisters(BitSet32 registers, ARM64Reg tmp)
{
  bool bundled_loadstore = false;

  for (int i = 0; i < 32; ++i)
  {
    if (!registers[i])
      continue;

    int count = 0;
    while (++count < 4 && (i + count) < 32 && registers[i + count])
    {
    }
    if (count > 1)
    {
      bundled_loadstore = true;
      break;
    }
  }

  if (bundled_loadstore && tmp != ARM64Reg::INVALID_REG)
  {
    DEBUG_ASSERT_MSG(DYNA_REC, Is64Bit(tmp), "Expected a 64-bit temporary register!");

    int num_regs = registers.Count();
    m_emit->SUB(ARM64Reg::SP, ARM64Reg::SP, num_regs * 16);
    m_emit->ADD(tmp, ARM64Reg::SP, 0);
    std::vector<ARM64Reg> island_regs;
    for (int i = 0; i < 32; ++i)
    {
      if (!registers[i])
        continue;

      int count = 0;

      // 0 = true
      // 1 < 4 && registers[i + 1] true!
      // 2 < 4 && registers[i + 2] true!
      // 3 < 4 && registers[i + 3] true!
      // 4 < 4 && registers[i + 4] false!
      while (++count < 4 && (i + count) < 32 && registers[i + count])
      {
      }

      if (count == 1)
        island_regs.push_back(ARM64Reg::Q0 + i);
      else
        ST1(64, count, IndexType::Post, ARM64Reg::Q0 + i, tmp);

      i += count - 1;
    }

    // Handle island registers
    std::vector<ARM64Reg> pair_regs;
    for (auto& it : island_regs)
    {
      pair_regs.push_back(it);
      if (pair_regs.size() == 2)
      {
        STP(128, IndexType::Post, pair_regs[0], pair_regs[1], tmp, 32);
        pair_regs.clear();
      }
    }
    if (pair_regs.size())
      STR(128, IndexType::Post, pair_regs[0], tmp, 16);
  }
  else
  {
    std::vector<ARM64Reg> pair_regs;
    for (auto it : registers)
    {
      pair_regs.push_back(ARM64Reg::Q0 + it);
      if (pair_regs.size() == 2)
      {
        STP(128, IndexType::Pre, pair_regs[0], pair_regs[1], ARM64Reg::SP, -32);
        pair_regs.clear();
      }
    }
    if (pair_regs.size())
      STR(128, IndexType::Pre, pair_regs[0], ARM64Reg::SP, -16);
  }
}
void ARM64FloatEmitter::ABI_PopRegisters(BitSet32 registers, ARM64Reg tmp)
{
  bool bundled_loadstore = false;
  int num_regs = registers.Count();

  for (int i = 0; i < 32; ++i)
  {
    if (!registers[i])
      continue;

    int count = 0;
    while (++count < 4 && (i + count) < 32 && registers[i + count])
    {
    }
    if (count > 1)
    {
      bundled_loadstore = true;
      break;
    }
  }

  if (bundled_loadstore && tmp != ARM64Reg::INVALID_REG)
  {
    // The temporary register is only used to indicate that we can use this code path
    std::vector<ARM64Reg> island_regs;
    for (int i = 0; i < 32; ++i)
    {
      if (!registers[i])
        continue;

      int count = 0;
      while (++count < 4 && (i + count) < 32 && registers[i + count])
      {
      }

      if (count == 1)
        island_regs.push_back(ARM64Reg::Q0 + i);
      else
        LD1(64, count, IndexType::Post, ARM64Reg::Q0 + i, ARM64Reg::SP);

      i += count - 1;
    }

    // Handle island registers
    std::vector<ARM64Reg> pair_regs;
    for (auto& it : island_regs)
    {
      pair_regs.push_back(it);
      if (pair_regs.size() == 2)
      {
        LDP(128, IndexType::Post, pair_regs[0], pair_regs[1], ARM64Reg::SP, 32);
        pair_regs.clear();
      }
    }
    if (pair_regs.size())
      LDR(128, IndexType::Post, pair_regs[0], ARM64Reg::SP, 16);
  }
  else
  {
    bool odd = num_regs % 2;
    std::vector<ARM64Reg> pair_regs;
    for (int i = 31; i >= 0; --i)
    {
      if (!registers[i])
        continue;

      if (odd)
      {
        // First load must be a regular LDR if odd
        odd = false;
        LDR(128, IndexType::Post, ARM64Reg::Q0 + i, ARM64Reg::SP, 16);
      }
      else
      {
        pair_regs.push_back(ARM64Reg::Q0 + i);
        if (pair_regs.size() == 2)
        {
          LDP(128, IndexType::Post, pair_regs[1], pair_regs[0], ARM64Reg::SP, 32);
          pair_regs.clear();
        }
      }
    }
  }
}

void ARM64XEmitter::ANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  if (!Is64Bit(Rn))
  {
    // To handle 32-bit logical immediates, the very easiest thing is to repeat
    // the input value twice to make a 64-bit word. The correct encoding of that
    // as a logical immediate will also be the correct encoding of the 32-bit
    // value.
    //
    // Doing this here instead of in the LogicalImm constructor makes it easier
    // to check if the input is all ones.

    imm = (imm << 32) | (imm & 0xFFFFFFFF);
  }

  if (imm == 0)
  {
    MOVZ(Rd, 0);
  }
  else if ((~imm) == 0)
  {
    if (Rd != Rn)
      MOV(Rd, Rn);
  }
  else if (const auto result = LogicalImm(imm, GPRSize::B64))
  {
    AND(Rd, Rn, result);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, scratch != ARM64Reg::INVALID_REG,
               "ANDI2R - failed to construct logical immediate value from {:#10x}, need scratch",
               imm);
    MOVI2R(scratch, imm);
    AND(Rd, Rn, scratch);
  }
}

void ARM64XEmitter::ORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  if (!Is64Bit(Rn))
  {
    // To handle 32-bit logical immediates, the very easiest thing is to repeat
    // the input value twice to make a 64-bit word. The correct encoding of that
    // as a logical immediate will also be the correct encoding of the 32-bit
    // value.
    //
    // Doing this here instead of in the LogicalImm constructor makes it easier
    // to check if the input is all ones.

    imm = (imm << 32) | (imm & 0xFFFFFFFF);
  }

  if (imm == 0)
  {
    if (Rd != Rn)
      MOV(Rd, Rn);
  }
  else if ((~imm) == 0)
  {
    MOVN(Rd, 0);
  }
  else if (const auto result = LogicalImm(imm, GPRSize::B64))
  {
    ORR(Rd, Rn, result);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, scratch != ARM64Reg::INVALID_REG,
               "ORRI2R - failed to construct logical immediate value from {:#10x}, need scratch",
               imm);
    MOVI2R(scratch, imm);
    ORR(Rd, Rn, scratch);
  }
}

void ARM64XEmitter::EORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  if (!Is64Bit(Rn))
  {
    // To handle 32-bit logical immediates, the very easiest thing is to repeat
    // the input value twice to make a 64-bit word. The correct encoding of that
    // as a logical immediate will also be the correct encoding of the 32-bit
    // value.
    //
    // Doing this here instead of in the LogicalImm constructor makes it easier
    // to check if the input is all ones.

    imm = (imm << 32) | (imm & 0xFFFFFFFF);
  }

  if (imm == 0)
  {
    if (Rd != Rn)
      MOV(Rd, Rn);
  }
  else if ((~imm) == 0)
  {
    MVN(Rd, Rn);
  }
  else if (const auto result = LogicalImm(imm, GPRSize::B64))
  {
    EOR(Rd, Rn, result);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, scratch != ARM64Reg::INVALID_REG,
               "EORI2R - failed to construct logical immediate value from {:#10x}, need scratch",
               imm);
    MOVI2R(scratch, imm);
    EOR(Rd, Rn, scratch);
  }
}

void ARM64XEmitter::ANDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  if (!Is64Bit(Rn))
  {
    // To handle 32-bit logical immediates, the very easiest thing is to repeat
    // the input value twice to make a 64-bit word. The correct encoding of that
    // as a logical immediate will also be the correct encoding of the 32-bit
    // value.
    //
    // Doing this here instead of in the LogicalImm constructor makes it easier
    // to check if the input is all ones.

    imm = (imm << 32) | (imm & 0xFFFFFFFF);
  }

  if (imm == 0)
  {
    ANDS(Rd, Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR,
         Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR);
  }
  else if ((~imm) == 0)
  {
    ANDS(Rd, Rn, Rn);
  }
  else if (const auto result = LogicalImm(imm, GPRSize::B64))
  {
    ANDS(Rd, Rn, result);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, scratch != ARM64Reg::INVALID_REG,
               "ANDSI2R - failed to construct logical immediate value from {:#10x}, need scratch",
               imm);
    MOVI2R(scratch, imm);
    ANDS(Rd, Rn, scratch);
  }
}

void ARM64XEmitter::AddImmediate(ARM64Reg Rd, ARM64Reg Rn, u64 imm, bool shift, bool negative,
                                 bool flags)
{
  if (!negative)
  {
    if (!flags)
      ADD(Rd, Rn, imm, shift);
    else
      ADDS(Rd, Rn, imm, shift);
  }
  else
  {
    if (!flags)
      SUB(Rd, Rn, imm, shift);
    else
      SUBS(Rd, Rn, imm, shift);
  }
}

void ARM64XEmitter::ADDI2R_internal(ARM64Reg Rd, ARM64Reg Rn, u64 imm, bool negative, bool flags,
                                    ARM64Reg scratch)
{
  DEBUG_ASSERT(Is64Bit(Rd) == Is64Bit(Rn));

  if (!Is64Bit(Rd))
    imm &= 0xFFFFFFFFULL;

  bool has_scratch = scratch != ARM64Reg::INVALID_REG;
  u64 imm_neg = Is64Bit(Rd) ? u64(-s64(imm)) : u64(-s64(imm)) & 0xFFFFFFFFuLL;
  bool neg_neg = negative ? false : true;

  // Special path for zeroes
  if (imm == 0 && !flags)
  {
    if (Rd == Rn)
    {
      return;
    }
    else if (DecodeReg(Rd) != DecodeReg(ARM64Reg::SP) && DecodeReg(Rn) != DecodeReg(ARM64Reg::SP))
    {
      MOV(Rd, Rn);
      return;
    }
  }

  // Regular fast paths, aarch64 immediate instructions
  // Try them all first
  if (imm <= 0xFFF)
  {
    AddImmediate(Rd, Rn, imm, false, negative, flags);
    return;
  }
  if (imm <= 0xFFFFFF && (imm & 0xFFF) == 0)
  {
    AddImmediate(Rd, Rn, imm >> 12, true, negative, flags);
    return;
  }
  if (imm_neg <= 0xFFF)
  {
    AddImmediate(Rd, Rn, imm_neg, false, neg_neg, flags);
    return;
  }
  if (imm_neg <= 0xFFFFFF && (imm_neg & 0xFFF) == 0)
  {
    AddImmediate(Rd, Rn, imm_neg >> 12, true, neg_neg, flags);
    return;
  }

  // ADD+ADD is slower than MOVK+ADD, but inplace.
  // But it supports a few more bits, so use it to avoid MOVK+MOVK+ADD.
  // As this splits the addition in two parts, this must not be done on setting flags.
  if (!flags && (imm >= 0x10000u || !has_scratch) && imm < 0x1000000u)
  {
    AddImmediate(Rd, Rn, imm & 0xFFF, false, negative, false);
    AddImmediate(Rd, Rd, imm >> 12, true, negative, false);
    return;
  }
  if (!flags && (imm_neg >= 0x10000u || !has_scratch) && imm_neg < 0x1000000u)
  {
    AddImmediate(Rd, Rn, imm_neg & 0xFFF, false, neg_neg, false);
    AddImmediate(Rd, Rd, imm_neg >> 12, true, neg_neg, false);
    return;
  }

  ASSERT_MSG(DYNA_REC, has_scratch,
             "ADDI2R - failed to construct arithmetic immediate value from {:#10x}, need scratch",
             imm);

  negative ^= MOVI2R2(scratch, imm, imm_neg);
  if (!negative)
  {
    if (!flags)
      ADD(Rd, Rn, scratch);
    else
      ADDS(Rd, Rn, scratch);
  }
  else
  {
    if (!flags)
      SUB(Rd, Rn, scratch);
    else
      SUBS(Rd, Rn, scratch);
  }
}

void ARM64XEmitter::ADDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  ADDI2R_internal(Rd, Rn, imm, false, false, scratch);
}

void ARM64XEmitter::ADDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  ADDI2R_internal(Rd, Rn, imm, false, true, scratch);
}

void ARM64XEmitter::SUBI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  ADDI2R_internal(Rd, Rn, imm, true, false, scratch);
}

void ARM64XEmitter::SUBSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  ADDI2R_internal(Rd, Rn, imm, true, true, scratch);
}

void ARM64XEmitter::CMPI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch)
{
  ADDI2R_internal(Is64Bit(Rn) ? ARM64Reg::ZR : ARM64Reg::WZR, Rn, imm, true, true, scratch);
}

bool ARM64XEmitter::TryADDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm)
{
  if (const auto result = IsImmArithmetic(imm))
  {
    const auto [val, shift] = *result;
    ADD(Rd, Rn, val, shift);
    return true;
  }

  return false;
}

bool ARM64XEmitter::TrySUBI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm)
{
  if (const auto result = IsImmArithmetic(imm))
  {
    const auto [val, shift] = *result;
    SUB(Rd, Rn, val, shift);
    return true;
  }

  return false;
}

bool ARM64XEmitter::TryCMPI2R(ARM64Reg Rn, u64 imm)
{
  if (const auto result = IsImmArithmetic(imm))
  {
    const auto [val, shift] = *result;
    CMP(Rn, val, shift);
    return true;
  }

  return false;
}

bool ARM64XEmitter::TryANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm)
{
  if (const auto result = LogicalImm(imm, Is64Bit(Rd) ? GPRSize::B64 : GPRSize::B32))
  {
    AND(Rd, Rn, result);
    return true;
  }

  return false;
}

bool ARM64XEmitter::TryORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm)
{
  if (const auto result = LogicalImm(imm, Is64Bit(Rd) ? GPRSize::B64 : GPRSize::B32))
  {
    ORR(Rd, Rn, result);
    return true;
  }

  return false;
}

bool ARM64XEmitter::TryEORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm)
{
  if (const auto result = LogicalImm(imm, Is64Bit(Rd) ? GPRSize::B64 : GPRSize::B32))
  {
    EOR(Rd, Rn, result);
    return true;
  }

  return false;
}

void ARM64FloatEmitter::MOVI2F(ARM64Reg Rd, float value, ARM64Reg scratch, bool negate)
{
  ASSERT_MSG(DYNA_REC, !IsDouble(Rd), "MOVI2F does not yet support double precision");

  if (value == 0.0f)
  {
    FMOV(Rd, IsDouble(Rd) ? ARM64Reg::ZR : ARM64Reg::WZR);
    if (negate)
      FNEG(Rd, Rd);
    // TODO: There are some other values we could generate with the float-imm instruction, like
    // 1.0...
  }
  else if (const auto imm = FPImm8FromFloat(value))
  {
    FMOV(Rd, *imm);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, scratch != ARM64Reg::INVALID_REG,
               "Failed to find a way to generate FP immediate {} without scratch", value);
    if (negate)
      value = -value;

    const u32 ival = std::bit_cast<u32>(value);
    m_emit->MOVI2R(scratch, ival);
    FMOV(Rd, scratch);
  }
}

// TODO: Quite a few values could be generated easily using the MOVI instruction and friends.
void ARM64FloatEmitter::MOVI2FDUP(ARM64Reg Rd, float value, ARM64Reg scratch)
{
  // TODO: Make it work with more element sizes
  // TODO: Optimize - there are shorter solution for many values
  ARM64Reg s = ARM64Reg::S0 + DecodeReg(Rd);
  MOVI2F(s, value, scratch);
  DUP(32, Rd, Rd, 0);
}

}  // namespace Arm64Gen
