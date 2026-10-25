// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Quantized loads and stores (psq_l and psq_st).
//
// The GQRs that a block uses but doesn't set are taken to be the values they had when it was
// compiled, as Jit64 does, and checked at the start of the block (see DoJit). With them known,
// each instruction is compiled for its type and scale, and its memory access gets fastmem.
// Otherwise, the instruction falls back to the interpreter.

#include "Core/PowerPC/JitPPC64/Jit.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

using namespace PPC64Gen;

namespace
{
struct QuantizedType
{
  bool valid = false;
  bool is_float = false;
  bool is_signed = false;
  // The size of one half, in bits
  u32 size = 0;
};

QuantizedType DecodeQuantizedType(EQuantizeType type)
{
  switch (type)
  {
  case QUANTIZE_FLOAT:
    return {true, true, false, 32};
  case QUANTIZE_U8:
    return {true, false, false, 8};
  case QUANTIZE_U16:
    return {true, false, false, 16};
  case QUANTIZE_S8:
    return {true, false, true, 8};
  case QUANTIZE_S16:
    return {true, false, true, 16};
  default:
    return {};
  }
}

// Quantizing multiplies by 2^exponent, and dequantizing by 2^-exponent, as m_quantizeTable and
// m_dequantizeTable do: the 6-bit scale is signed.
s32 ScaleExponent(u32 scale)
{
  return scale < 32 ? static_cast<s32>(scale) : static_cast<s32>(scale) - 64;
}
}  // namespace

// The double 2^exponent, with Power10's xxspltidp, which converts the single it splats.
void JitPPC64::LoadPowerOfTwo(VSR out, GPR tmp, s32 exponent, bool splat)
{
  if (m_isa_3_1)
  {
    XXSPLTIDP(out, static_cast<u32>(127 + exponent) << 23);
    return;
  }
  LI(tmp, static_cast<s16>(1023 + exponent));
  SLDI(tmp, tmp, 52);
  MTVSRD(out, tmp);
  if (splat)
    XXPERMDI(out, out, out, 0);
}

void JitPPC64::DequantizePair(VSR out, GPR hi, GPR lo, u32 type_size, bool is_signed, VSR factor)
{
  if (is_signed)
  {
    type_size == 8 ? EXTSB(hi, hi) : EXTSH(hi, hi);
    type_size == 8 ? EXTSB(lo, lo) : EXTSH(lo, lo);
  }
  MTVSRDD(out, hi, lo);
  XVCVSXDDP(out, out);
  if (factor != VSR::VS0)
    XVMULDP(out, out, factor);
}

void JitPPC64::QuantizePair(GPR out, GPR tmp, VSR in, u32 type_size, bool is_signed, VSR factor,
                            VSR scaled, VSR work)
{
  // As QuantizeHalf. The scaling is by a power of two, exact in double precision, where single
  // precision would overflow or underflow only for values that saturate or truncate to 0 anyway.
  XVCVDPSP(scaled, in);
  XVCVSPDP(scaled, scaled);
  if (factor != VSR::VS0)
    XVMULDP(scaled, scaled, factor);
  // NaNs convert to 0.
  XVCMPEQDP(work, scaled, scaled);
  XVCVDPSXWS(scaled, scaled);
  XXLAND(scaled, scaled, work);

  const s32 min = is_signed ? -(1 << (type_size - 1)) : 0;
  const u32 max = is_signed ? (1u << (type_size - 1)) - 1 : (1u << type_size) - 1;
  LI(tmp, static_cast<s16>(min));
  MTVSRWS(work, tmp);
  VMAXSW(scaled, scaled, work);
  MOVI2R(tmp, max);
  MTVSRWS(work, tmp);
  VMINSW(scaled, scaled, work);

  // Words 0 and 2, in the upper halves of the doublewords.
  MFVSRD(out, scaled);
  MFVSRLD(tmp, scaled);
  RLDICL(out, out, 32, 64 - type_size);
  RLDICL(tmp, tmp, 32, 64 - type_size);
  SLDI(out, out, type_size);
  OR(out, out, tmp);
}

void JitPPC64::psq_lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  const bool indexed = inst.OPCD == 4;
  const bool update = inst.OPCD == 57 || (indexed && (inst.SUBOP6 & 32));
  const u32 i = indexed ? inst.Ix : inst.I;
  const bool single = indexed ? inst.Wx : inst.W;
  // The invalid form is left to the interpreter.
  FALLBACK_IF(update && inst.RA == 0);

  if (!js.constantGqrValid[i])
  {
    QuantizedLoadStoreWithRoutine(inst, false, indexed, update, i, single);
    return;
  }

  const UGQR gqr(js.constantGqr[i]);
  const QuantizedType type = DecodeQuantizedType(gqr.ld_type);
  FALLBACK_IF(!type.valid);
  const s32 exponent = ScaleExponent(gqr.ld_scale);
  // With Power ISA 3.0, both halves of integers are converted at once.
  const bool integer_pair = !single && !type.is_float && m_isa_3_0;

  const auto addr = gpr.GetScopedReg();
  const auto value = gpr.GetScopedReg();
  if (indexed)
  {
    if (inst.RA == 0)
      CLRLDI(addr, gpr.R(inst.RB), 32);
    else
      ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  }
  else
  {
    if (inst.RA == 0)
      MOVI2R(addr, static_cast<u32>(static_cast<s32>(inst.SIMM_12)));
    else
      ADDI(addr, gpr.R(inst.RA), inst.SIMM_12);
  }
  CLRLDI(addr, addr, 32);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  // Floats are loaded into VS0, which isn't allocated, half 0 in word 0 and half 1 in word 1.
  if (type.is_float)
    EmitLoadToVSR(single ? 32 : 64, VSR::VS0, value, addr);
  else
    EmitMemoryAccess({.size = single ? type.size : 2 * type.size}, value, addr);

  // Integers' halves are in big-endian order in value: half 0 in the upper bits.
  const auto half0 = fpr.GetScopedReg();
  const auto half1 = fpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();
  if (type.is_float)
  {
    // As ConvertToDouble, from the single in word 0, and half 1 shifted there.
    XSCVSPDPN(half0, VSR::VS0);
    if (!single)
    {
      XXSLDWI(half1, VSR::VS0, VSR::VS0, 1);
      XSCVSPDPN(half1, half1);
    }
  }
  else
  {
    // float(T(value)) * m_dequantizeTable[scale]: exact, as is doing it in double precision.
    const auto convert = [&](VSR out, GPR integer) {
      if (type.is_signed)
        type.size == 8 ? EXTSB(integer, integer) : EXTSH(integer, integer);
      MTVSRD(out, integer);
      XSCVSXDDP(out, out);
      if (exponent != 0)
      {
        const auto factor = fpr.GetScopedReg();
        LoadPowerOfTwo(factor, integer, -exponent);
        XSMULDP(out, out, factor);
      }
    };
    if (single)
    {
      convert(half0, value);
    }
    else if (integer_pair)
    {
      // Into FD, with the factor in half1.
      if (exponent != 0)
        LoadPowerOfTwo(half1, tmp, -exponent, true);
      CLRLDI(tmp, value, 64 - type.size);
      SRDI(value, value, type.size);
      DequantizePair(fpr.RW(inst.FD, false), value, tmp, type.size, type.is_signed,
                     exponent != 0 ? VSR(half1) : VSR::VS0);
    }
    else
    {
      CLRLDI(tmp, value, 64 - type.size);
      SRDI(value, value, type.size);
      convert(half0, value);
      convert(half1, tmp);
    }
  }

  if (single)
  {
    // ps1 is 1.0.
    LoadPowerOfTwo(half1, tmp, 0);
  }
  if (!integer_pair)
    XXPERMDI(fpr.RW(inst.FD, false), half0, half1, 0);

  if (update)
    MR(a, addr);
}

void JitPPC64::QuantizeHalf(GPR out, VSR in, u32 type_size, bool is_signed, s32 exponent)
{
  const auto scaled = fpr.GetScopedReg();
  const auto bound = gpr.GetScopedReg();
  if (exponent == 0)
  {
    QuantizeHalf(out, in, type_size, is_signed, VSR::VS0, scaled, bound);
    return;
  }
  const auto factor = fpr.GetScopedReg();
  LoadPowerOfTwo(factor, out, exponent);
  QuantizeHalf(out, in, type_size, is_signed, factor, scaled, bound);
}

void JitPPC64::QuantizeHalf(GPR out, VSR in, u32 type_size, bool is_signed, VSR factor, VSR scaled,
                            GPR bound)
{
  // As ScaleAndClamp: SType(std::clamp(float(ps) * m_quantizeTable[scale], min, max)), with the
  // factor 2^scale, or no factor for 2^0 (VS0). Clamping and truncating toward zero can go either
  // way round, as the bounds are integers; a NaN, which std::clamp passes through, converts to 0.
  XSRSP(scaled, in);
  if (factor != VSR::VS0)
    XSMULSP(scaled, scaled, factor);
  XSCMPUDP(CRF::CR1, scaled, scaled);
  // Truncates, saturating to 32 bits.
  XSCVDPSXWS(scaled, scaled);
  MFVSRD(out, scaled);
  EXTSW(out, out);
  ISEL(out, GPR::R0, out, CRBit(CRF::CR1, CCFlags::SO));

  const s32 min = is_signed ? -(1 << (type_size - 1)) : 0;
  const u32 max = is_signed ? (1u << (type_size - 1)) - 1 : (1u << type_size) - 1;
  LI(bound, static_cast<s16>(min));
  CMPW(CRF::CR1, out, bound);
  ISEL(out, bound, out, CRBit(CRF::CR1, CCFlags::LT));
  MOVI2R(bound, max);
  CMPW(CRF::CR1, out, bound);
  ISEL(out, bound, out, CRBit(CRF::CR1, CCFlags::GT));
}

void JitPPC64::ConvertDoubleToSingleFTZ(GPR value)
{
  const auto exp = gpr.GetScopedReg();
  const auto high = gpr.GetScopedReg();
  ConvertDoubleToSingleFTZ(value, exp, high);
}

void JitPPC64::ConvertDoubleToSingleFTZ(GPR value, GPR exp, GPR high)
{
  // As ConvertToSingleFTZ: like ConvertToSingle, but values too small for normal singles become
  // zeroes of the same sign.
  RLDICL(exp, value, 12, 53);
  CMPLWI(CRF::CR1, exp, 896);
  SRDI(high, value, 32);
  SRDI(value, value, 29);
  RLWIMI(value, high, 0, 0, 1);
  RLWINM(high, high, 0, 0, 0);
  ISEL(value, value, high, CRBit(CRF::CR1, CCFlags::GT));
  CLRLDI(value, value, 32);
}

void JitPPC64::psq_stXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);

  const bool indexed = inst.OPCD == 4;
  const bool update = inst.OPCD == 61 || (indexed && (inst.SUBOP6 & 32));
  const u32 i = indexed ? inst.Ix : inst.I;
  const bool single = indexed ? inst.Wx : inst.W;
  // The invalid form is left to the interpreter.
  FALLBACK_IF(update && inst.RA == 0);

  if (!js.constantGqrValid[i])
  {
    QuantizedLoadStoreWithRoutine(inst, true, indexed, update, i, single);
    return;
  }

  const UGQR gqr(js.constantGqr[i]);
  const QuantizedType type = DecodeQuantizedType(gqr.st_type);
  FALLBACK_IF(!type.valid);
  const s32 exponent = ScaleExponent(gqr.st_scale);
  // With Power ISA 3.0, both halves of integers are quantized at once.
  const bool integer_pair = !single && !type.is_float && m_isa_3_0;

  const auto addr = gpr.GetScopedReg();
  const auto value = gpr.GetScopedReg();
  if (indexed)
  {
    if (inst.RA == 0)
      CLRLDI(addr, gpr.R(inst.RB), 32);
    else
      ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  }
  else
  {
    if (inst.RA == 0)
      MOVI2R(addr, static_cast<u32>(static_cast<s32>(inst.SIMM_12)));
    else
      ADDI(addr, gpr.R(inst.RA), inst.SIMM_12);
  }
  CLRLDI(addr, addr, 32);
  const VSR s = fpr.R(inst.FS);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;

  {
    const auto value1 = gpr.GetScopedReg();
    // ps1 swapped into doubleword 0. With Power ISA 3.0, ps1 of floats goes to a GPR with mfvsrld
    // instead, and integers are quantized both at once.
    PPC64FPRCache::ScopedReg half1;
    if (!single && !m_isa_3_0)
    {
      half1 = fpr.GetScopedReg();
      XXSWAPD(half1, s);
    }

    if (type.is_float)
    {
      MFVSRD(value, s);
      ConvertDoubleToSingleFTZ(value);
      if (!single)
      {
        if (m_isa_3_0)
          MFVSRLD(value1, s);
        else
          MFVSRD(value1, half1);
        ConvertDoubleToSingleFTZ(value1);
      }
    }
    else
    {
      if (integer_pair)
      {
        const auto factor = fpr.GetScopedReg();
        const auto scaled = fpr.GetScopedReg();
        const auto work = fpr.GetScopedReg();
        if (exponent != 0)
          LoadPowerOfTwo(factor, value1, exponent, true);
        QuantizePair(value, value1, s, type.size, type.is_signed,
                     exponent != 0 ? VSR(factor) : VSR::VS0, scaled, work);
      }
      else
      {
        QuantizeHalf(value, s, type.size, type.is_signed, exponent);
        if (!single)
          QuantizeHalf(value1, half1, type.size, type.is_signed, exponent);
      }
    }

    // Half 0 goes in the upper bits, which are stored first; QuantizePair put it there.
    if (!single && !integer_pair)
    {
      CLRLDI(value1, value1, 64 - type.size);
      SLDI(value, value, type.size);
      OR(value, value, value1);
    }
  }

  EmitMemoryAccess({.store = true, .size = single ? type.size : 2 * type.size}, value, addr);

  if (update)
    MR(a, addr);
}

void JitPPC64::QuantizedLoadStoreWithRoutine(UGeckoInstruction inst, bool store, bool indexed,
                                             bool update, u32 gqr_index, bool single)
{
  // The GQR may change while the block runs, so call the routine for its type, with its scale.
  // They take the address in R3, the scale in R4 and the value in VS32, preserve the scratch
  // GPRs, and may clobber R0, R3, R4, R12, CTR, VS32-VS35.
  ASSERT(!fpr.AnyScratchRegsInUse());
  const auto addr = gpr.GetScopedReg();
  if (indexed)
  {
    if (inst.RA == 0)
      CLRLDI(addr, gpr.R(inst.RB), 32);
    else
      ADD(addr, gpr.R(inst.RA), gpr.R(inst.RB));
  }
  else
  {
    if (inst.RA == 0)
      MOVI2R(addr, static_cast<u32>(static_cast<s32>(inst.SIMM_12)));
    else
      ADDI(addr, gpr.R(inst.RA), inst.SIMM_12);
  }
  CLRLDI(addr, addr, 32);
  const GPR a = update ? gpr.RW(inst.RA, true) : GPR::R0;
  if (store)
  {
    const VSR s = fpr.R(inst.FS);
    XXLOR(VSR::VS32, s, s);
  }

  // The PC is used by memory checks, by GPFifo to find stores to the gather pipe, and in logs.
  MOVI2R(GPR::R12, js.compilerPC);
  STW(GPR::R12, PPC_REG, PPCSTATE_OFF(pc));

  // UGQR: st_type in bits 0-2, st_scale in 8-13, ld_type in 16-18, ld_scale in 24-29. The
  // routines' index is W << 3 | type, in bytes << 3.
  LWZ(GPR::R12, PPC_REG, PPCSTATE_OFF_SPR(SPR_GQR0 + gqr_index));
  if (store)
  {
    RLWINM(GPR::R4, GPR::R12, 24, 26, 31);
    RLWINM(GPR::R12, GPR::R12, 3, 26, 28);
  }
  else
  {
    RLWINM(GPR::R4, GPR::R12, 8, 26, 31);
    RLWINM(GPR::R12, GPR::R12, 32 - 13, 26, 28);
  }
  const auto& routines = store ? m_quantized_store_routines : m_quantized_load_routines;
  MOVP2R(GPR::R3, routines.data() + (single ? 8 : 0));
  LDX(GPR::R12, GPR::R3, GPR::R12);
  MTCTR(GPR::R12);
  MR(GPR::R3, addr);
  BCTRL();

  if (jo.memcheck)
    WriteConditionalExceptionExit(EXCEPTION_DSI, GPR::R12);

  if (!store)
    XXLOR(fpr.RW(inst.FD, false), VSR::VS32, VSR::VS32);
  if (update)
    MR(a, addr);
}

std::array<FixupBranch, 3> JitPPC64::EmitQuantizedRAMAddress(u32 size)
{
  // From the address in R3, leaves the host address of RAM in R12, or branches away if the access
  // isn't to cached RAM by a DBAT: page table translations, MMIO, real mode and accesses that cross
  // 8-byte boundaries take the slow path. Uses R0.
  LWZ(GPR::R12, PPC_REG, PPCSTATE_OFF(msr));
  ANDI_(GPR::R12, GPR::R12, 1 << UReg_MSR{}.DR.StartBit());
  const FixupBranch real_mode = BC(CCFlags::EQ, CRF::CR0);

  MOVP2R(GPR::R12, m_mmu.GetDBATTable().data());
  RLWINM(GPR::R0, GPR::R3, 32 - PowerPC::BAT_INDEX_SHIFT + 2, PowerPC::BAT_INDEX_SHIFT - 2, 29);
  LWZX(GPR::R12, GPR::R12, GPR::R0);
  ANDI_(GPR::R0, GPR::R12, PowerPC::BAT_PHYSICAL_BIT);
  const FixupBranch not_physical = BC(CCFlags::EQ, CRF::CR0);
  RLWIMI(GPR::R12, GPR::R3, 0, 32 - PowerPC::BAT_INDEX_SHIFT, 31);

  RLWINM(GPR::R0, GPR::R3, 0, 29, 31);
  CMPLWI(CRF::CR1, GPR::R0, 8 - size);
  auto& memory = m_system.GetMemory();
  MOVI2R(GPR::R0, memory.GetRamSizeReal());
  CMPLW(CRF::CR0, GPR::R12, GPR::R0);
  // Misaligned, or at or beyond the end of RAM.
  CRORC(CRBit(CRF::CR1, CCFlags::GT), CRBit(CRF::CR1, CCFlags::GT), CRBit(CRF::CR0, CCFlags::LT));
  const FixupBranch not_ram = BC(CCFlags::GT, CRF::CR1);

  ASSERT(memory.GetRAM() != nullptr);
  MOVP2R(GPR::R0, memory.GetRAM());
  ADD(GPR::R12, GPR::R12, GPR::R0);
  return {real_mode, not_physical, not_ram};
}

void JitPPC64::GenerateQuantizedRoutines()
{
  for (const bool single : {false, true})
  {
    for (u32 type = 0; type < 8; type++)
    {
      m_quantized_load_routines[(single ? 8 : 0) | type] = GetCodePtr();
      GenerateQuantizedLoadRoutine(type, single);
      m_quantized_store_routines[(single ? 8 : 0) | type] = GetCodePtr();
      GenerateQuantizedStoreRoutine(type, single);
    }
  }
}

void JitPPC64::GenerateQuantizedLoadRoutine(u32 type, bool single)
{
  const QuantizedType decoded = DecodeQuantizedType(static_cast<EQuantizeType>(type));
  const bool integer_pair = !single && !decoded.is_float && m_isa_3_0;
  if (!decoded.valid)
  {
    // As Helper_Dequantize: zeroes, without an access.
    XXLXOR(VSR::VS32, VSR::VS32, VSR::VS32);
    BLR();
    return;
  }

  const u32 size = (single ? 1 : 2) * decoded.size / 8;
  MFLR(GPR::R0);
  STD(GPR::R0, GPR::SP, QUANTIZED_LR_SAVE);
  STD(GPR::R4, GPR::SP, QUANTIZED_VALUE_SAVE);

  const auto slow = EmitQuantizedRAMAddress(size);
  switch (size)
  {
  case 8:
    LDBRX(GPR::R3, GPR::R0, GPR::R12);
    break;
  case 4:
    LWBRX(GPR::R3, GPR::R0, GPR::R12);
    break;
  case 2:
    LHBRX(GPR::R3, GPR::R0, GPR::R12);
    break;
  default:
    LBZX(GPR::R3, GPR::R0, GPR::R12);
    break;
  }
  const FixupBranch loaded = B();
  for (const FixupBranch& branch : slow)
    SetJumpTarget(branch);
  BL(Routine(m_slow_access_routines[SlowAccessRoutineIndex({.size = size * 8})]));
  SetJumpTarget(loaded);

  // The halves are in big-endian order in R3: half 0 in the upper bits.
  if (decoded.is_float)
  {
    // As ConvertToDouble, from the single in word 0, and half 1 shifted there.
    if (single && m_isa_3_0)
    {
      MTVSRWS(VSR::VS34, GPR::R3);
    }
    else
    {
      if (single)
        SLDI(GPR::R3, GPR::R3, 32);
      MTVSRD(VSR::VS34, GPR::R3);
    }
    XSCVSPDPN(VSR::VS33, VSR::VS34);
    if (!single)
    {
      XXSLDWI(VSR::VS34, VSR::VS34, VSR::VS34, 1);
      XSCVSPDPN(VSR::VS34, VSR::VS34);
    }
  }
  else
  {
    // float(T(value)) * m_dequantizeTable[scale], with the factor 2^-scale, from the sign-extended
    // 6-bit scale.
    LD(GPR::R4, GPR::SP, QUANTIZED_VALUE_SAVE);
    SLDI(GPR::R4, GPR::R4, 58);
    SRADI(GPR::R4, GPR::R4, 58);
    SUBFIC(GPR::R4, GPR::R4, 1023);
    SLDI(GPR::R4, GPR::R4, 52);
    MTVSRD(VSR::VS35, GPR::R4);
    const auto convert = [&](VSR out, GPR integer) {
      if (decoded.is_signed)
        decoded.size == 8 ? EXTSB(integer, integer) : EXTSH(integer, integer);
      MTVSRD(out, integer);
      XSCVSXDDP(out, out);
      XSMULDP(out, out, VSR::VS35);
    };
    if (single)
    {
      convert(VSR::VS33, GPR::R3);
    }
    else
    {
      CLRLDI(GPR::R12, GPR::R3, 64 - decoded.size);
      SRDI(GPR::R3, GPR::R3, decoded.size);
      if (integer_pair)
      {
        // Both at once, into VS32.
        XXPERMDI(VSR::VS35, VSR::VS35, VSR::VS35, 0);
        DequantizePair(VSR::VS32, GPR::R3, GPR::R12, decoded.size, decoded.is_signed, VSR::VS35);
      }
      else
      {
        convert(VSR::VS33, GPR::R3);
        convert(VSR::VS34, GPR::R12);
      }
    }
  }

  if (single)
  {
    // ps1 is 1.0.
    LoadPowerOfTwo(VSR::VS34, GPR::R12, 0);
  }
  if (!integer_pair)
    XXPERMDI(VSR::VS32, VSR::VS33, VSR::VS34, 0);

  LD(GPR::R0, GPR::SP, QUANTIZED_LR_SAVE);
  MTLR(GPR::R0);
  BLR();
}

void JitPPC64::GenerateQuantizedStoreRoutine(u32 type, bool single)
{
  const QuantizedType decoded = DecodeQuantizedType(static_cast<EQuantizeType>(type));
  const bool integer_pair = !single && !decoded.is_float && m_isa_3_0;
  if (!decoded.valid)
  {
    // As Helper_Quantize: nothing.
    BLR();
    return;
  }

  const u32 size = (single ? 1 : 2) * decoded.size / 8;
  MFLR(GPR::R0);
  STD(GPR::R0, GPR::SP, QUANTIZED_LR_SAVE);
  STD(GPR::R3, GPR::SP, QUANTIZED_VALUE_SAVE);

  // The value, in R4, with half 0 in the upper bits. With Power ISA 3.0, ps1 of floats goes to a
  // GPR with mfvsrld, and integers are quantized both at once.
  if (!single && !m_isa_3_0)
    XXSWAPD(VSR::VS33, VSR::VS32);
  if (decoded.is_float)
  {
    MFVSRD(GPR::R4, VSR::VS32);
    ConvertDoubleToSingleFTZ(GPR::R4, GPR::R0, GPR::R12);
    if (!single)
    {
      if (m_isa_3_0)
        MFVSRLD(GPR::R3, VSR::VS32);
      else
        MFVSRD(GPR::R3, VSR::VS33);
      ConvertDoubleToSingleFTZ(GPR::R3, GPR::R0, GPR::R12);
    }
  }
  else
  {
    // The factor 2^scale, from the sign-extended 6-bit scale.
    SLDI(GPR::R4, GPR::R4, 58);
    SRADI(GPR::R4, GPR::R4, 58);
    ADDI(GPR::R4, GPR::R4, 1023);
    SLDI(GPR::R4, GPR::R4, 52);
    MTVSRD(VSR::VS35, GPR::R4);
    if (integer_pair)
    {
      XXPERMDI(VSR::VS35, VSR::VS35, VSR::VS35, 0);
      QuantizePair(GPR::R4, GPR::R3, VSR::VS32, decoded.size, decoded.is_signed, VSR::VS35,
                   VSR::VS34, VSR::VS33);
    }
    else
    {
      QuantizeHalf(GPR::R4, VSR::VS32, decoded.size, decoded.is_signed, VSR::VS35, VSR::VS34,
                   GPR::R12);
      if (!single)
      {
        QuantizeHalf(GPR::R3, VSR::VS33, decoded.size, decoded.is_signed, VSR::VS35, VSR::VS34,
                     GPR::R12);
      }
    }
  }
  if (!single && !integer_pair)
  {
    CLRLDI(GPR::R3, GPR::R3, 64 - decoded.size);
    SLDI(GPR::R4, GPR::R4, decoded.size);
    OR(GPR::R4, GPR::R4, GPR::R3);
  }
  // The slow path takes the value zero-extended.
  if (size < 8)
    CLRLDI(GPR::R4, GPR::R4, 64 - size * 8);

  LD(GPR::R3, GPR::SP, QUANTIZED_VALUE_SAVE);
  const auto slow = EmitQuantizedRAMAddress(size);
  switch (size)
  {
  case 8:
    STDBRX(GPR::R4, GPR::R0, GPR::R12);
    break;
  case 4:
    STWBRX(GPR::R4, GPR::R0, GPR::R12);
    break;
  case 2:
    STHBRX(GPR::R4, GPR::R0, GPR::R12);
    break;
  default:
    STBX(GPR::R4, GPR::R0, GPR::R12);
    break;
  }
  const FixupBranch stored = B();
  for (const FixupBranch& branch : slow)
    SetJumpTarget(branch);
  BL(Routine(m_slow_access_routines[SlowAccessRoutineIndex({.store = true, .size = size * 8})]));
  SetJumpTarget(stored);

  LD(GPR::R0, GPR::SP, QUANTIZED_LR_SAVE);
  MTLR(GPR::R0);
  BLR();
}
