// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"

#include <array>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/FloatUtils.h"
#include "Common/Intrinsics.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PowerPC.h"

#define QUANTIZED_REGS_TO_SAVE                                                                     \
  (ABI_ALL_CALLER_SAVED & ~BitSet32{RSCRATCH, RSCRATCH2, RSCRATCH_EXTRA, XMM0 + 16, XMM1 + 16})

#define QUANTIZED_REGS_TO_SAVE_LOAD (QUANTIZED_REGS_TO_SAVE | BitSet32{RSCRATCH2})

using namespace Gen;

alignas(16) static const __m128i double_fraction = _mm_set_epi64x(0, 0x000fffffffffffff);
alignas(16) static const __m128i double_sign_bit = _mm_set_epi64x(0, 0x8000000000000000);
alignas(16) static const __m128i double_explicit_top_bit = _mm_set_epi64x(0, 0x0010000000000000);
alignas(16) static const __m128i double_top_two_bits = _mm_set_epi64x(0, 0xc000000000000000);
alignas(16) static const __m128i double_bottom_bits = _mm_set_epi64x(0, 0x07ffffffe0000000);

// Since the following float conversion functions are used in non-arithmetic PPC float
// instructions, they must convert floats bitexact and never flush denormals to zero or turn SNaNs
// into QNaNs. This means we can't use CVTSS2SD/CVTSD2SS.

// Another problem is that officially, converting doubles to single format results in undefined
// behavior.  Relying on undefined behavior is a bug so no software should ever do this.
// Super Mario 64 (on Wii VC) accidentally relies on this behavior.  See issue #11173

// This is the same algorithm used in the interpreter (and actual hardware)
// The documentation states that the conversion of a double with an outside the
// valid range for a single (or a single denormal) is undefined.
// But testing on actual hardware shows it always picks bits 0..1 and 5..34
// unless the exponent is in the range of 874 to 896.

void CommonAsmRoutines::GenConvertDoubleToSingle()
{
  // Input in XMM0, output to RSCRATCH
  // Clobbers RSCRATCH/RSCRATCH2/XMM0/XMM1

  const void* start = GetCodePtr();

  // Grab Exponent
  MOVQ_xmm(R(RSCRATCH), XMM0);
  MOV(64, R(RSCRATCH2), R(RSCRATCH));
  SHR(64, R(RSCRATCH), Imm8(52));
  AND(16, R(RSCRATCH), Imm16(0x7ff));

  // Check if the double is in the range of valid single subnormal
  SUB(16, R(RSCRATCH), Imm16(874));
  CMP(16, R(RSCRATCH), Imm16(896 - 874));
  FixupBranch Denormalize = J_CC(CC_NA);

  // Don't Denormalize

  if (cpu_info.bBMI2FastParallelBitOps)
  {
    // Extract bits 0-1 and 5-34
    MOV(64, R(RSCRATCH), Imm64(0xc7ffffffe0000000));
    PEXT(64, RSCRATCH, RSCRATCH2, R(RSCRATCH));
  }
  else
  {
    // We want bits 0, 1
    avx_op(&XEmitter::VPAND, &XEmitter::PAND, XMM1, R(XMM0), MConst(double_top_two_bits));
    PSRLQ(XMM1, 32);

    // And 5 through to 34
    PAND(XMM0, MConst(double_bottom_bits));
    PSRLQ(XMM0, 29);

    // OR them togther
    POR(XMM0, R(XMM1));
    MOVD_xmm(R(RSCRATCH), XMM0);
  }
  RET();

  // Denormalise
  SetJumpTarget(Denormalize);

  // shift = (905 - Exponent) plus the 21 bit double to single shift
  NEG(16, R(RSCRATCH));
  ADD(16, R(RSCRATCH), Imm16((905 + 21) - 874));
  MOVQ_xmm(XMM1, R(RSCRATCH));

  // XMM0 = fraction | 0x0010000000000000
  PAND(XMM0, MConst(double_fraction));
  POR(XMM0, MConst(double_explicit_top_bit));

  // fraction >> shift
  PSRLQ(XMM0, R(XMM1));
  MOVD_xmm(R(RSCRATCH), XMM0);

  // OR the sign bit in.
  SHR(64, R(RSCRATCH2), Imm8(32));
  AND(32, R(RSCRATCH2), Imm32(0x80000000));

  OR(32, R(RSCRATCH), R(RSCRATCH2));
  RET();

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_cdts");
}

void CommonAsmRoutines::GenFrsqrte()
{
  const void* start = GetCodePtr();

  // Assume input in XMM0.
  // This function clobbers all three RSCRATCH.
  MOVQ_xmm(R(RSCRATCH), XMM0);

  // Extract exponent
  MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHR(64, R(RSCRATCH_EXTRA), Imm8(52));

  // Negatives, zeros, denormals, infinities and NaNs take the complex path.
  LEA(32, RSCRATCH2, MDisp(RSCRATCH_EXTRA, -1));
  CMP(32, R(RSCRATCH2), Imm32(0x7FE));
  FixupBranch complex = J_CC(CC_AE, Jump::Near);

  SUB(32, R(RSCRATCH_EXTRA), Imm32(0x3FD));
  SAR(32, R(RSCRATCH_EXTRA), Imm8(1));
  MOV(32, R(RSCRATCH2), Imm32(0x3FF));
  SUB(32, R(RSCRATCH2), R(RSCRATCH_EXTRA));
  SHL(64, R(RSCRATCH2), Imm8(52));  // exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) /
                                    // 2)) & (0x7FFLL << 52);

  MOV(64, R(RSCRATCH_EXTRA), R(RSCRATCH));
  SHR(64, R(RSCRATCH_EXTRA), Imm8(48));
  AND(32, R(RSCRATCH_EXTRA), Imm8(0x1F));

  PUSH(RSCRATCH2);
  MOV(64, R(RSCRATCH2), ImmPtr(GetConstantFromPool(Common::frsqrte_expected)));
  static_assert(sizeof(Common::BaseAndDec) == 8, "Unable to use SCALE_8; incorrect size");

  SHR(64, R(RSCRATCH), Imm8(37));
  AND(32, R(RSCRATCH), Imm32(0x7FF));
  IMUL(32, RSCRATCH,
       MComplex(RSCRATCH2, RSCRATCH_EXTRA, SCALE_8, offsetof(Common::BaseAndDec, m_dec)));
  ADD(32, R(RSCRATCH),
      MComplex(RSCRATCH2, RSCRATCH_EXTRA, SCALE_8, offsetof(Common::BaseAndDec, m_base)));
  SHL(64, R(RSCRATCH), Imm8(26));

  POP(RSCRATCH2);
  OR(64, R(RSCRATCH2), R(RSCRATCH));  // vali |= (s64)(frsqrte_expected_base[index] +
                                      // frsqrte_expected_dec[index] * (i % 2048)) << 26;
  MOVQ_xmm(XMM0, R(RSCRATCH2));
  RET();

  SetJumpTarget(complex);
  AND(32, R(RSCRATCH_EXTRA), Imm32(0x7FF));
  CMP(32, R(RSCRATCH_EXTRA), Imm32(0x7FF));
  FixupBranch nan_or_inf = J_CC(CC_E);

  MOV(64, R(RSCRATCH2), R(RSCRATCH));
  SHL(64, R(RSCRATCH2), Imm8(1));
  FixupBranch nonzero = J_CC(CC_NZ);

  // +0.0 or -0.0
  TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_ZX));
  FixupBranch skip_set_fx1 = J_CC(CC_NZ);
  OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_ZX));
  SetJumpTarget(skip_set_fx1);
  MOV(64, R(RSCRATCH2), Imm64(0x7FF0'0000'0000'0000));
  OR(64, R(RSCRATCH2), R(RSCRATCH));
  MOVQ_xmm(XMM0, R(RSCRATCH2));
  RET();

  // SNaN or QNaN or +Inf or -Inf
  SetJumpTarget(nan_or_inf);
  MOV(64, R(RSCRATCH2), R(RSCRATCH));
  SHL(64, R(RSCRATCH2), Imm8(12));
  FixupBranch inf = J_CC(CC_Z);
  BTS(64, R(RSCRATCH), Imm8(51));
  MOVQ_xmm(XMM0, R(RSCRATCH));
  RET();
  SetJumpTarget(inf);
  TEST(64, R(RSCRATCH), R(RSCRATCH));
  FixupBranch negative = J_CC(CC_S);
  XORPD(XMM0, R(XMM0));
  RET();

  SetJumpTarget(nonzero);
  FixupBranch denormal = J_CC(CC_NC);

  // Negative sign
  SetJumpTarget(negative);
  TEST(32, PPCSTATE(fpscr), Imm32(FPSCR_VXSQRT));
  FixupBranch skip_set_fx2 = J_CC(CC_NZ);
  OR(32, PPCSTATE(fpscr), Imm32(FPSCR_FX | FPSCR_VXSQRT));
  SetJumpTarget(skip_set_fx2);
  MOV(64, R(RSCRATCH2), Imm64(0x7FF8'0000'0000'0000));
  MOVQ_xmm(XMM0, R(RSCRATCH2));
  RET();

  SetJumpTarget(denormal);
  ABI_PushRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  ABI_CallFunction(Common::ApproximateReciprocalSquareRoot);
  ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  RET();

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_Frsqrte");
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
  MOV(64, R(RSCRATCH_EXTRA), ImmPtr(GetConstantFromPool(Common::fres_expected)));
  static_assert(sizeof(Common::BaseAndDec) == 8, "Unable to use SCALE_8; incorrect size");

  IMUL(32, RSCRATCH,
       MComplex(RSCRATCH_EXTRA, RSCRATCH2, SCALE_8, offsetof(Common::BaseAndDec, m_dec)));
  ADD(32, R(RSCRATCH), Imm8(1));
  SHR(32, R(RSCRATCH), Imm8(1));

  MOV(32, R(RSCRATCH2),
      MComplex(RSCRATCH_EXTRA, RSCRATCH2, SCALE_8, offsetof(Common::BaseAndDec, m_base)));
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
  ABI_CallFunction(Common::ApproximateReciprocal);
  ABI_PopRegistersAndAdjustStack(QUANTIZED_REGS_TO_SAVE, 8);
  RET();

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_Fres");
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
  // Upper bits of tmp need to be zeroed.
  XOR(32, R(tmp), R(tmp));
  for (int i = 0; i < 8; i++)
  {
    if (i != 0)
      SHL(32, R(dst), Imm8(4));

    MOV(64, R(cr_val), PPCSTATE_CR(i));

    // EQ: Bits 31-0 == 0; set flag bit 1
    TEST(32, R(cr_val), R(cr_val));
    SETcc(CC_Z, R(tmp));
    LEA(32, dst, MComplex(dst, tmp, SCALE_2, 0));

    // GT: Value > 0; set flag bit 2
    TEST(64, R(cr_val), R(cr_val));
    SETcc(CC_G, R(tmp));
    LEA(32, dst, MComplex(dst, tmp, SCALE_4, 0));

    // SO: Bit 59 set; set flag bit 0
    // LT: Bit 62 set; set flag bit 3
    SHR(64, R(cr_val), Imm8(PowerPC::CR_EMU_SO_BIT));
    AND(32, R(cr_val), Imm8(PowerPC::CR_LT | PowerPC::CR_SO));
    OR(32, R(dst), R(cr_val));
  }
  RET();

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_Mfcr");
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

// TODO: Use the actual instruction being emulated (needed for alignment exception emulation)
static const UGeckoInstruction ps_placeholder_instruction = 0;

void CommonAsmRoutines::GenQuantizedStores()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_stXX).
  paired_store_quantized = reinterpret_cast<const u8**>(AlignCodeTo(256));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
  {
    paired_store_quantized[type] =
        GenQuantizedStoreRuntime(false, static_cast<EQuantizeType>(type));
  }
}

// See comment in header for in/outs.
void CommonAsmRoutines::GenQuantizedSingleStores()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_stXX).
  single_store_quantized = reinterpret_cast<const u8**>(AlignCodeTo(256));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    single_store_quantized[type] = GenQuantizedStoreRuntime(true, static_cast<EQuantizeType>(type));
}

const u8* CommonAsmRoutines::GenQuantizedStoreRuntime(bool single, EQuantizeType type)
{
  const void* start = GetCodePtr();
  const u8* load = AlignCode4();
  GenQuantizedStore(single, type, -1);
  RET();
  Common::JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedStore_{}_{}",
                                Common::ToUnderlying(type), single);

  return load;
}

void CommonAsmRoutines::GenQuantizedLoads()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_lXX).
  paired_load_quantized = reinterpret_cast<const u8**>(AlignCodeTo(256));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    paired_load_quantized[type] = GenQuantizedLoadRuntime(false, static_cast<EQuantizeType>(type));
}

void CommonAsmRoutines::GenQuantizedSingleLoads()
{
  // Aligned to 256 bytes as least significant byte needs to be zero (See: Jit64::psq_lXX).
  single_load_quantized = reinterpret_cast<const u8**>(AlignCodeTo(256));
  ReserveCodeSpace(8 * sizeof(u8*));

  for (int type = 0; type < 8; type++)
    single_load_quantized[type] = GenQuantizedLoadRuntime(true, static_cast<EQuantizeType>(type));
}

const u8* CommonAsmRoutines::GenQuantizedLoadRuntime(bool single, EQuantizeType type)
{
  const void* start = GetCodePtr();
  const u8* load = AlignCode4();
  GenQuantizedLoad(single, type, -1);
  RET();
  Common::JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedLoad_{}_{}",
                                Common::ToUnderlying(type), single);

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

  int flags = isInline ? 0 :
                         SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG |
                             SAFE_LOADSTORE_DR_ON | SAFE_LOADSTORE_NO_UPDATE_PC;
  if (!single)
    flags |= SAFE_LOADSTORE_NO_SWAP;

  SafeWriteRegToReg(RSCRATCH, RSCRATCH_EXTRA, size, 0, ps_placeholder_instruction,
                    QUANTIZED_REGS_TO_SAVE, flags);
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

  BitSet32 regsToSave = QUANTIZED_REGS_TO_SAVE_LOAD;
  int flags = isInline ? 0 :
                         SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG |
                             SAFE_LOADSTORE_DR_ON | SAFE_LOADSTORE_NO_UPDATE_PC;
  SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), size, 0, ps_placeholder_instruction, regsToSave,
                extend, flags);
  if (!single && (type == QUANTIZE_U8 || type == QUANTIZE_S8))
  {
    // TODO: Support not swapping in safeLoadToReg to avoid bswapping twice
    ROR(16, R(RSCRATCH_EXTRA), Imm8(8));
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

  BitSet32 regsToSave = QUANTIZED_REGS_TO_SAVE;
  int flags = isInline ? 0 :
                         SAFE_LOADSTORE_NO_FASTMEM | SAFE_LOADSTORE_NO_PROLOG |
                             SAFE_LOADSTORE_DR_ON | SAFE_LOADSTORE_NO_UPDATE_PC;
  SafeLoadToReg(RSCRATCH_EXTRA, R(RSCRATCH_EXTRA), size, 0, ps_placeholder_instruction, regsToSave,
                extend, flags);

  if (single)
  {
    MOVD_xmm(XMM0, R(RSCRATCH_EXTRA));
    UNPCKLPS(XMM0, MConst(m_one));
  }
  else
  {
    ROL(64, R(RSCRATCH_EXTRA), Imm8(32));
    MOVQ_xmm(XMM0, R(RSCRATCH_EXTRA));
  }
}
