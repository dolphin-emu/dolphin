// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"

#include <array>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/MathUtil.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/HW/GPFifo.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64Common/Jit64Base.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PowerPC.h"

#define QUANTIZED_REGS_TO_SAVE                                                                     \
  (ABI_ALL_CALLER_SAVED & ~BitSet32{RSCRATCH, RSCRATCH2, RSCRATCH_EXTRA, XMM0 + 16, XMM1 + 16})

#define QUANTIZED_REGS_TO_SAVE_LOAD (QUANTIZED_REGS_TO_SAVE | BitSet32{RSCRATCH2})

using namespace Gen;

void CommonAsmRoutines::GenFifoWrite(int size)
{
  const void* start = GetCodePtr();

  // Assume value in RSCRATCH
  MOV(64, R(RSCRATCH2), ImmPtr(&GPFifo::g_gather_pipe_ptr));
  MOV(64, R(RSCRATCH2), MatR(RSCRATCH2));
  SwapAndStore(size, MatR(RSCRATCH2), RSCRATCH);
  MOV(64, R(RSCRATCH), ImmPtr(&GPFifo::g_gather_pipe_ptr));
  ADD(64, R(RSCRATCH2), Imm8(size >> 3));
  MOV(64, MatR(RSCRATCH), R(RSCRATCH2));
  RET();

  JitRegister::Register(start, GetCodePtr(), "JIT_FifoWrite_%i", size);
}

void CommonAsmRoutines::GenFrsqrte()
{
  const void* start = GetCodePtr();

  // Assume input in XMM0.
  // This function clobbers all three RSCRATCH.
  MOVQ_xmm(R(RSCRATCH), XMM0);

  // Negative and zero inputs set an exception and take the complex path.
  TEST(64, R(RSCRATCH), R(RSCRATCH));
  FixupBranch zero = J_CC(CC_Z, true);
  FixupBranch negative = J_CC(CC_S, true);
  MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHR(64, R(RSCRATCH_EXTRA), Imm8(52));

  // Zero and max exponents (non-normal floats) take the complex path.
  FixupBranch complex1 = J_CC(CC_Z, true);
  CMP(32, R(RSCRATCH_EXTRA), Imm32(0x7FF));
  FixupBranch complex2 = J_CC(CC_E, true);

  SUB(32, R(RSCRATCH_EXTRA), Imm32(0x3FD));
  SAR(32, R(RSCRATCH_EXTRA), Imm8(1));
  MOV(32, R(RSCRATCH2), Imm32(0x3FF));
  SUB(32, R(RSCRATCH2), R(RSCRATCH_EXTRA));
  SHL(64, R(RSCRATCH2), Imm8(52));  // exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) /
                                    // 2)) & (0x7FFLL << 52);

  MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHR(64, R(RSCRATCH_EXTRA), Imm8(48));
  AND(32, R(RSCRATCH_EXTRA), Imm8(0x1F));
  XOR(32, R(RSCRATCH_EXTRA), Imm8(0x10));  // int index = i / 2048 + (odd_exponent ? 16 : 0);

  PUSH(RSCRATCH2);
  MOV(64, R(RSCRATCH2), ImmPtr(GetConstantFromPool(MathUtil::frsqrte_expected)));
  static_assert(sizeof(MathUtil::BaseAndDec) == 8, "Unable to use SCALE_8; incorrect size");

  SHR(64, R(RSCRATCH), Imm8(37));
  AND(32, R(RSCRATCH), Imm32(0x7FF));
  IMUL(32, RSCRATCH,
       MComplex(RSCRATCH2, RSCRATCH_EXTRA, SCALE_8, offsetof(MathUtil::BaseAndDec, m_dec)));
  MOV(32, R(RSCRATCH_EXTRA),
      MComplex(RSCRATCH2, RSCRATCH_EXTRA, SCALE_8, offsetof(MathUtil::BaseAndDec, m_base)));
  SUB(32, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHL(64, R(RSCRATCH_EXTRA), Imm8(26));

  POP(RSCRATCH2);
  OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));  // vali |= (s64)(frsqrte_expected_base[index] -
                                            // frsqrte_expected_dec[index] * (i % 2048)) << 26;
  MOVQ_xmm(XMM0, R(RSCRATCH2));
  RET();

  // Exception flags for zero input.
  SetJumpTarget(zero);
  TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_ZX));
  FixupBranch skip_set_fx1 = J_CC(CC_NZ);
  OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_ZX));
  FixupBranch complex3 = J();

  // Exception flags for negative input.
  SetJumpTarget(negative);
  TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_VXSQRT));
  FixupBranch skip_set_fx2 = J_CC(CC_NZ);
  OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_VXSQRT));

  SetJumpTarget(skip_set_fx1);
  SetJumpTarget(skip_set_fx2);
  SetJumpTarget(complex1);
  SetJumpTarget(complex2);
  SetJumpTarget(complex3);
  ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  ABI_CallFunction(MathUtil::ApproximateReciprocalSquareRoot);
  ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  RET();

  JitRegister::Register(start, GetCodePtr(), "JIT_Frsqrte");
}

void CommonAsmRoutines::GenFres()
{
  const void* start = GetCodePtr();

  // Assume input in XMM0.
  // This function clobbers all three RSCRATCH.
  MOVQ_xmm(R(RSCRATCH), XMM0);

  // Zero inputs set an exception and take the complex path.
  TEST(64, R(RSCRATCH), R(RSCRATCH));
  FixupBranch zero = J_CC(CC_Z);

  MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHR(64, R(RSCRATCH_EXTRA), Imm8(52));
  MOV(32, R(RSCRATCH2), R(RSCRATCH_EXTRA));
  AND(32, R(RSCRATCH_EXTRA), Imm32(0x7FF));  // exp
  AND(32, R(RSCRATCH2), Imm32(0x800));       // sign
  SUB(32, R(RSCRATCH_EXTRA), Imm32(895));
  CMP(32, R(RSCRATCH_EXTRA), Imm32(1149 - 895));
  // Take the complex path for very large/small exponents.
  FixupBranch complex = J_CC(CC_AE);  // if (exp < 895 || exp >= 1149)

  SUB(32, R(RSCRATCH_EXTRA), Imm32(0x7FD - 895));
  NEG(32, R(RSCRATCH_EXTRA));
  OR(32, R(RSCRATCH_EXTRA), R(RSCRATCH2));
  SHL(64, R(RSCRATCH_EXTRA), Imm8(52));  // vali = sign | exponent

  MOV(64, R(RSCRATCH2), R(RSCRATCH));
  SHR(64, R(RSCRATCH), Imm8(37));
  SHR(64, R(RSCRATCH2), Imm8(47));
  AND(32, R(RSCRATCH), Imm32(0x3FF));  // i % 1024
  AND(32, R(RSCRATCH2), Imm8(0x1F));   // i / 1024

  PUSH(RSCRATCH_EXTRA);
  MOV(64, R(RSCRATCH_EXTRA), ImmPtr(GetConstantFromPool(MathUtil::fres_expected)));
  static_assert(sizeof(MathUtil::BaseAndDec) == 8, "Unable to use SCALE_8; incorrect size");

  IMUL(32, RSCRATCH,
       MComplex(RSCRATCH_EXTRA, RSCRATCH2, SCALE_8, offsetof(MathUtil::BaseAndDec, m_dec)));
  ADD(32, R(RSCRATCH), Imm8(1));
  SHR(32, R(RSCRATCH), Imm8(1));

  MOV(32, R(RSCRATCH2),
      MComplex(RSCRATCH_EXTRA, RSCRATCH2, SCALE_8, offsetof(MathUtil::BaseAndDec, m_base)));
  SUB(32, R(RSCRATCH2), R(RSCRATCH));
  SHL(64, R(RSCRATCH2), Imm8(29));

  POP(RSCRATCH_EXTRA);

  OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));  // vali |= (s64)(fres_expected_base[i / 1024] -
                                            // (fres_expected_dec[i / 1024] * (i % 1024) + 1) / 2)
                                            // << 29
  MOVQ_xmm(XMM0, R(RSCRATCH2));
  RET();

  // Exception flags for zero input.
  SetJumpTarget(zero);
  TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_ZX));
  FixupBranch skip_set_fx1 = J_CC(CC_NZ);
  OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_ZX));
  SetJumpTarget(skip_set_fx1);

  SetJumpTarget(complex);
  ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  ABI_CallFunction(MathUtil::ApproximateReciprocal);
  ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  RET();

  JitRegister::Register(start, GetCodePtr(), "JIT_Fres");
}

void CommonAsmRoutines::GenMfcr()
{
  const void* start = GetCodePtr();

  // Input: none
  // Output: RSCRATCH
  // This function clobbers all three RSCRATCH.
  X64Reg dst = RSCRATCH;
  X64Reg tmp = RSCRATCH2;
  X64Reg cr_val = RSCRATCH_EXTRA;
  XOR(32, R(dst), R(dst));
  for (int i = 0; i < 8; i++)
  {
    static const u32 m_flagTable[8] = {0x0, 0x1, 0x8, 0x9, 0x0, 0x1, 0x8, 0x9};
    if (i != 0)
      SHL(32, R(dst), Imm8(4));

    MOV(64, R(cr_val), PPCSTATE(cr_val[i]));

    // Upper bits of tmp need to be zeroed.
    // Note: tmp is used later for address calculations and thus
    //       can't be zero-ed once. This also prevents partial
    //       register stalls due to SETcc.
    XOR(32, R(tmp), R(tmp));
    // EQ: Bits 31-0 == 0; set flag bit 1
    TEST(32, R(cr_val), R(cr_val));
    SETcc(CC_Z, R(tmp));
    LEA(32, dst, MComplex(dst, tmp, SCALE_2, 0));

    // GT: Value > 0; set flag bit 2
    TEST(64, R(cr_val), R(cr_val));
    SETcc(CC_G, R(tmp));
    LEA(32, dst, MComplex(dst, tmp, SCALE_4, 0));

    // SO: Bit 61 set; set flag bit 0
    // LT: Bit 62 set; set flag bit 3
    SHR(64, R(cr_val), Imm8(61));
    LEA(64, tmp, MConst(m_flagTable));
    OR(32, R(dst), MComplex(tmp, cr_val, SCALE_4, 0));
  }
  RET();

  JitRegister::Register(start, GetCodePtr(), "JIT_Mfcr");
}

// Safe + Fast Quantizers, originally from JITIL by magumagu
alignas(16) static const float m_65535[4] = {65535.0f, 65535.0f, 65535.0f, 65535.0f};
alignas(16) static const float m_32767 = 32767.0f;
alignas(16) static const float m_m32768 = -32768.0f;
alignas(16) static const float m_255 = 255.0f;
alignas(16) static const float m_127 = 127.0f;
alignas(16) static const float m_m128 = -128.0f;

// Sizes of the various quantized store types
constexpr std::array<u8, 8> sizes{{32, 0, 0, 0, 8, 16, 8, 16}};

void CommonAsmRoutines::GenQuantizedStores()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_stXX).
  pairedStoreQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCodeTo(256)));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    pairedStoreQuantized[type] = GenQuantizedStoreRuntime(false, static_cast<EQuantizeType>(type));
}

// See comment in header for in/outs.
void CommonAsmRoutines::GenQuantizedSingleStores()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_stXX).
  singleStoreQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCodeTo(256)));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    singleStoreQuantized[type] = GenQuantizedStoreRuntime(true, static_cast<EQuantizeType>(type));
}

const u8* CommonAsmRoutines::GenQuantizedStoreRuntime(bool single, EQuantizeType type)
{
  const void* start = GetCodePtr();
  const u8* load = AlignCode4();
  GenQuantizedStore(single, type, -1);
  RET();
  JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedStore_%i_%i", type, single);

  return load;
}

void CommonAsmRoutines::GenQuantizedLoads()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_lXX).
  pairedLoadQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCodeTo(256)));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    pairedLoadQuantized[type] = GenQuantizedLoadRuntime(false, static_cast<EQuantizeType>(type));
}

void CommonAsmRoutines::GenQuantizedSingleLoads()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_lXX).
  singleLoadQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCodeTo(256)));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    singleLoadQuantized[type] = GenQuantizedLoadRuntime(true, static_cast<EQuantizeType>(type));
}

const u8* CommonAsmRoutines::GenQuantizedLoadRuntime(bool single, EQuantizeType type)
{
  const void* start = GetCodePtr();
  const u8* load = AlignCode4();
  GenQuantizedLoad(single, type, -1);
  RET();
  JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedLoad_%i_%i", type, single);

  return load;
}

void QuantizedMemoryRoutines::GenQuantizedStore(bool single, EQuantizeType type, int quantize)
{
  // In: one or two single floats in XMM0, if quantize is -1, a quantization factor in RSCRATCH2

  int size = sizes[type] * (single ? 1 : 2);
  bool isInline = quantize != -1;

  // illegal
  if (type == QUANTIZE_INVALID1 || type == QUANTIZE_INVALID2 || type == QUANTIZE_INVALID3)
  {
    UD2();
    return;
  }

  if (type == QUANTIZE_FLOAT)
  {
    GenQuantizedStoreFloat(single, isInline);
  }
  else if (single)
  {
    if (quantize == -1)
    {
      SHR(32, R(RSCRATCH2), Imm8(5));
      LEA(64, RSCRATCH, MConst(m_quantizeTableS));
      MULSS(XMM0, MRegSum(RSCRATCH2, RSCRATCH));
    }
    else if (quantize > 0)
    {
      MULSS(XMM0, MConst(m_quantizeTableS, quantize * 2));
    }

    switch (type)
    {
    case QUANTIZE_U8:
      XORPS(XMM1, R(XMM1));
      MAXSS(XMM0, R(XMM1));
      MINSS(XMM0, MConst(m_255));
      break;
    case QUANTIZE_S8:
      MAXSS(XMM0, MConst(m_m128));
      MINSS(XMM0, MConst(m_127));
      break;
    case QUANTIZE_U16:
      XORPS(XMM1, R(XMM1));
      MAXSS(XMM0, R(XMM1));
      MINSS(XMM0, MConst(m_65535));
      break;
    case QUANTIZE_S16:
      MAXSS(XMM0, MConst(m_m32768));
      MINSS(XMM0, MConst(m_32767));
      break;
    default:
      break;
    }

    CVTTSS2SI(RSCRATCH, R(XMM0));
  }
  else
  {
    if (quantize == -1)
    {
      SHR(32, R(RSCRATCH2), Imm8(5));
      LEA(64, RSCRATCH, MConst(m_quantizeTableS));
      MOVQ_xmm(XMM1, MRegSum(RSCRATCH2, RSCRATCH));
      MULPS(XMM0, R(XMM1));
    }
    else if (quantize > 0)
    {
      MOVQ_xmm(XMM1, MConst(m_quantizeTableS, quantize * 2));
      MULPS(XMM0, R(XMM1));
    }

    bool hasPACKUSDW = cpu_info.bSSE4_1;

    // Special case: if we don't have PACKUSDW we need to clamp to zero as well so the shuffle
    // below can work
    if (type == QUANTIZE_U16 && !hasPACKUSDW)
    {
      XORPS(XMM1, R(XMM1));
      MAXPS(XMM0, R(XMM1));
    }

    // According to Intel Docs CVTPS2DQ writes 0x80000000 if the source floating point value
    // is out of int32 range while it's OK for large negatives, it isn't for positives
    // I don't know whether the overflow actually happens in any games but it potentially can
    // cause problems, so we need some clamping
    MINPS(XMM0, MConst(m_65535));
    CVTTPS2DQ(XMM0, R(XMM0));

    switch (type)
    {
    case QUANTIZE_U8:
      PACKSSDW(XMM0, R(XMM0));
      PACKUSWB(XMM0, R(XMM0));
      MOVD_xmm(R(RSCRATCH), XMM0);
      break;
    case QUANTIZE_S8:
      PACKSSDW(XMM0, R(XMM0));
      PACKSSWB(XMM0, R(XMM0));
      MOVD_xmm(R(RSCRATCH), XMM0);
      break;
    case QUANTIZE_U16:
      if (hasPACKUSDW)
      {
        PACKUSDW(XMM0, R(XMM0));         // AAAABBBB CCCCDDDD ... -> AABBCCDD ...
        MOVD_xmm(R(RSCRATCH), XMM0);     // AABBCCDD ... -> AABBCCDD
        BSWAP(32, RSCRATCH);             // AABBCCDD -> DDCCBBAA
        ROL(32, R(RSCRATCH), Imm8(16));  // DDCCBBAA -> BBAADDCC
      }
      else
      {
        // We don't have PACKUSDW so we'll shuffle instead (assumes 32-bit values >= 0 and < 65536)
        PSHUFLW(XMM0, R(XMM0), 2);    // AABB0000 CCDD0000 ... -> CCDDAABB ...
        MOVD_xmm(R(RSCRATCH), XMM0);  // CCDDAABB ... -> CCDDAABB
        BSWAP(32, RSCRATCH);          // CCDDAABB -> BBAADDCC
      }
      break;
    case QUANTIZE_S16:
      PACKSSDW(XMM0, R(XMM0));
      MOVD_xmm(R(RSCRATCH), XMM0);
      BSWAP(32, RSCRATCH);
      ROL(32, R(RSCRATCH), Imm8(16));
      break;
    default:
      break;
    }
  }

  int flags =
      isInline ? 0 : SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_DR_ON;
  if (!single)
    flags |= SAFE_LOADSTORE_NO_SWAP;

  SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, size, 0, QUANTIZED_REGS_TO_SAVE, flags);
}

void QuantizedMemoryRoutines::GenQuantizedStoreFloat(bool single, bool isInline)
{
  if (single)
  {
    // Easy!
    MOVD_xmm(R(RSCRATCH), XMM0);
  }
  else
  {
    if (cpu_info.bSSSE3)
    {
      PSHUFB(XMM0, MConst(pbswapShuffle2x4));
      MOVQ_xmm(R(RSCRATCH), XMM0);
    }
    else
    {
      MOVQ_xmm(R(RSCRATCH), XMM0);
      ROL(64, R(RSCRATCH), Imm8(32));
      BSWAP(64, RSCRATCH);
    }
  }
}

void QuantizedMemoryRoutines::GenQuantizedLoad(bool single, EQuantizeType type, int quantize)
{
  // Note that this method assumes that inline methods know the value of quantize ahead of
  // time. The methods generated AOT assume that the quantize flag is placed in RSCRATCH in
  // the second lowest byte, ie: 0x0000xx00

  int size = sizes[type] * (single ? 1 : 2);
  bool isInline = quantize != -1;

  // illegal
  if (type == QUANTIZE_INVALID1 || type == QUANTIZE_INVALID2 || type == QUANTIZE_INVALID3)
  {
    UD2();
    return;
  }

  // Floats don't use quantization and can generate more optimal code
  if (type == QUANTIZE_FLOAT)
  {
    GenQuantizedLoadFloat(single, isInline);
    return;
  }

  bool extend = single && (type == QUANTIZE_S8 || type == QUANTIZE_S16);

  if (g_jit->jo.memcheck)
  {
    BitSet32 regsToSave = QUANTIZED_REGS_TO_SAVE_LOAD;
    int flags =
        isInline ? 0 : SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_DR_ON;
    SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), size, 0, regsToSave, extend, flags);
    if (!single && (type == QUANTIZE_U8 || type == QUANTIZE_S8))
    {
      // TODO: Support not swapping in safeLoadToReg to avoid bswapping twice
      ROR(16, R(RSCRATCH_EXTRA), Imm8(8));
    }
  }
  else
  {
    switch (type)
    {
    case QUANTIZE_U8:
    case QUANTIZE_S8:
      UnsafeLoadRegToRegNoSwap(RSCRATCH_EXTRA, RSCRATCH_EXTRA, size, 0, extend);
      break;
    case QUANTIZE_U16:
    case QUANTIZE_S16:
      UnsafeLoadRegToReg(RSCRATCH_EXTRA, RSCRATCH_EXTRA, size, 0, extend);
      break;
    default:
      break;
    }
  }

  if (single)
  {
    CVTSI2SS(XMM0, R(RSCRATCH_EXTRA));

    if (quantize == -1)
    {
      SHR(32, R(RSCRATCH2), Imm8(5));
      LEA(64, RSCRATCH, MConst(m_dequantizeTableS));
      MULSS(XMM0, MRegSum(RSCRATCH2, RSCRATCH));
    }
    else if (quantize > 0)
    {
      MULSS(XMM0, MConst(m_dequantizeTableS, quantize * 2));
    }
    UNPCKLPS(XMM0, MConst(m_one));
  }
  else
  {
    switch (type)
    {
    case QUANTIZE_U8:
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
      if (cpu_info.bSSE4_1)
      {
        PMOVZXBD(XMM0, R(XMM0));
      }
      else
      {
        PXOR(XMM1, R(XMM1));
        PUNPCKLBW(XMM0, R(XMM1));
        PUNPCKLWD(XMM0, R(XMM1));
      }
      break;
    case QUANTIZE_S8:
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
      if (cpu_info.bSSE4_1)
      {
        PMOVSXBD(XMM0, R(XMM0));
      }
      else
      {
        PUNPCKLBW(XMM0, R(XMM0));
        PUNPCKLWD(XMM0, R(XMM0));
        PSRAD(XMM0, 24);
      }
      break;
    case QUANTIZE_U16:
      ROL(32, R(RSCRATCH_EXTRA), Imm8(16));
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
      if (cpu_info.bSSE4_1)
      {
        PMOVZXWD(XMM0, R(XMM0));
      }
      else
      {
        PXOR(XMM1, R(XMM1));
        PUNPCKLWD(XMM0, R(XMM1));
      }
      break;
    case QUANTIZE_S16:
      ROL(32, R(RSCRATCH_EXTRA), Imm8(16));
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
      if (cpu_info.bSSE4_1)
      {
        PMOVSXWD(XMM0, R(XMM0));
      }
      else
      {
        PUNPCKLWD(XMM0, R(XMM0));
        PSRAD(XMM0, 16);
      }
      break;
    default:
      break;
    }
    CVTDQ2PS(XMM0, R(XMM0));

    if (quantize == -1)
    {
      SHR(32, R(RSCRATCH2), Imm8(5));
      LEA(64, RSCRATCH, MConst(m_dequantizeTableS));
      MOVQ_xmm(XMM1, MRegSum(RSCRATCH2, RSCRATCH));
      MULPS(XMM0, R(XMM1));
    }
    else if (quantize > 0)
    {
      MOVQ_xmm(XMM1, MConst(m_dequantizeTableS, quantize * 2));
      MULPS(XMM0, R(XMM1));
    }
  }
}

void QuantizedMemoryRoutines::GenQuantizedLoadFloat(bool single, bool isInline)
{
  int size = single ? 32 : 64;
  bool extend = false;

  if (g_jit->jo.memcheck)
  {
    BitSet32 regsToSave = QUANTIZED_REGS_TO_SAVE;
    int flags =
        isInline ? 0 : SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG | SAFE_LOADSTORE_DR_ON;
    SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), size, 0, regsToSave, extend, flags);
  }

  if (single)
  {
    if (g_jit->jo.memcheck)
    {
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
    }
    else if (cpu_info.bSSSE3)
    {
      MOVD_xmm(XMM0, MRegSum(RMEM, RSCRATCH_EXTRA));
      PSHUFB(XMM0, MConst(pbswapShuffle1x4));
    }
    else
    {
      LoadAndSwap(32, RSCRATCH_EXTRA, MRegSum(RMEM, RSCRATCH_EXTRA));
      MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
    }

    UNPCKLPS(XMM0, MConst(m_one));
  }
  else
  {
    // FIXME? This code (in non-MMU mode) assumes all accesses are directly to RAM, i.e.
    // don't need hardware access handling. This will definitely crash if paired loads occur
    // from non-RAM areas, but as far as I know, this never happens. I don't know if this is
    // for a good reason, or merely because no game does this.
    // If we find something that actually does do this, maybe this should be changed. How
    // much of a performance hit would it be?
    if (g_jit->jo.memcheck)
    {
      ROL(64, R(RSCRATCH_EXTRA), Imm8(32));
      MOVQ_xmm(XMM0, R(RSCRATCH_EXTRA));
    }
    else if (cpu_info.bSSSE3)
    {
      MOVQ_xmm(XMM0, MRegSum(RMEM, RSCRATCH_EXTRA));
      PSHUFB(XMM0, MConst(pbswapShuffle2x4));
    }
    else
    {
      LoadAndSwap(64, RSCRATCH_EXTRA, MRegSum(RMEM, RSCRATCH_EXTRA));
      ROL(64, R(RSCRATCH_EXTRA), Imm8(32));
      MOVQ_xmm(XMM0, R(RSCRATCH_EXTRA));
    }
  }
}
