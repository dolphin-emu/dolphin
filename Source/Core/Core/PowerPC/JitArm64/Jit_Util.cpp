// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit_Util.h"

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/HW/MMIO.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/System.h"

using namespace Arm64Gen;

template <typename T>
class MMIOWriteCodeGenerator : public MMIO::WriteHandlingMethodVisitor<T>
{
public:
  MMIOWriteCodeGenerator(Core::System* system, ARM64XEmitter* emit, BitSet32 gprs_in_use,
                         BitSet32 fprs_in_use, ARM64Reg src_reg, u32 address)
      : m_system(system), m_emit(emit), m_gprs_in_use(gprs_in_use), m_fprs_in_use(fprs_in_use),
        m_src_reg(src_reg), m_address(address)
  {
  }

  void VisitNop() override
  {
    // Do nothing
  }
  void VisitDirect(T* addr, u32 mask) override { WriteRegToAddr(8 * sizeof(T), addr, mask); }
  void VisitComplex(const std::function<void(Core::System&, u32, T)>* lambda) override
  {
    CallLambda(8 * sizeof(T), lambda);
  }

private:
  void StoreFromRegister(int sbits, ARM64Reg reg, s32 offset)
  {
    switch (sbits)
    {
    case 8:
      m_emit->STRB(IndexType::Unsigned, reg, ARM64Reg::X0, offset);
      break;
    case 16:
      m_emit->STRH(IndexType::Unsigned, reg, ARM64Reg::X0, offset);
      break;
    case 32:
      m_emit->STR(IndexType::Unsigned, reg, ARM64Reg::X0, offset);
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unknown size {} passed to MMIOWriteCodeGenerator!", sbits);
      break;
    }
  }

  void WriteRegToAddr(int sbits, const void* ptr, u32 mask)
  {
    const s32 offset = m_emit->MOVPage2R(ARM64Reg::X0, ptr);

    // If we do not need to mask, we can do the sign extend while loading
    // from memory. If masking is required, we have to first zero extend,
    // then mask, then sign extend if needed (1 instr vs. ~4).
    const u32 all_ones = (1ULL << sbits) - 1;
    if ((all_ones & mask) == all_ones)
    {
      StoreFromRegister(sbits, m_src_reg, offset);
    }
    else
    {
      m_emit->ANDI2R(ARM64Reg::W1, m_src_reg, mask, ARM64Reg::W1);
      StoreFromRegister(sbits, ARM64Reg::W1, offset);
    }
  }

  void CallLambda(int sbits, const std::function<void(Core::System&, u32, T)>* lambda)
  {
    ARM64FloatEmitter float_emit(m_emit);

    m_emit->ABI_PushRegisters(m_gprs_in_use);
    float_emit.ABI_PushRegisters(m_fprs_in_use, ARM64Reg::X1);
    m_emit->MOVP2R(ARM64Reg::X1, m_system);
    m_emit->MOVI2R(ARM64Reg::W2, m_address);
    m_emit->MOV(ARM64Reg::W3, m_src_reg);
    m_emit->BLR(m_emit->ABI_SetupLambda(lambda));

    float_emit.ABI_PopRegisters(m_fprs_in_use, ARM64Reg::X1);
    m_emit->ABI_PopRegisters(m_gprs_in_use);
  }

  Core::System* m_system;
  ARM64XEmitter* m_emit;
  BitSet32 m_gprs_in_use;
  BitSet32 m_fprs_in_use;
  ARM64Reg m_src_reg;
  u32 m_address;
};
// Visitor that generates code to read a MMIO value.
template <typename T>
class MMIOReadCodeGenerator : public MMIO::ReadHandlingMethodVisitor<T>
{
public:
  MMIOReadCodeGenerator(Core::System* system, ARM64XEmitter* emit, BitSet32 gprs_in_use,
                        BitSet32 fprs_in_use, ARM64Reg dst_reg, u32 address, bool sign_extend)
      : m_system(system), m_emit(emit), m_gprs_in_use(gprs_in_use), m_fprs_in_use(fprs_in_use),
        m_dst_reg(dst_reg), m_address(address), m_sign_extend(sign_extend)
  {
  }

  void VisitConstant(T value) override { LoadConstantToReg(8 * sizeof(T), value); }
  void VisitDirect(const T* addr, u32 mask) override
  {
    LoadAddrMaskToReg(8 * sizeof(T), addr, mask);
  }
  void VisitComplex(const std::function<T(Core::System&, u32)>* lambda) override
  {
    CallLambda(8 * sizeof(T), lambda);
  }

private:
  void LoadConstantToReg(int sbits, u32 value)
  {
    m_emit->MOVI2R(m_dst_reg, value);
    if (m_sign_extend)
      m_emit->SBFM(m_dst_reg, m_dst_reg, 0, sbits - 1);
  }

  void LoadToRegister(int sbits, bool dont_extend, s32 offset)
  {
    switch (sbits)
    {
    case 8:
      if (m_sign_extend && !dont_extend)
        m_emit->LDRSB(IndexType::Unsigned, m_dst_reg, ARM64Reg::X0, offset);
      else
        m_emit->LDRB(IndexType::Unsigned, m_dst_reg, ARM64Reg::X0, offset);
      break;
    case 16:
      if (m_sign_extend && !dont_extend)
        m_emit->LDRSH(IndexType::Unsigned, m_dst_reg, ARM64Reg::X0, offset);
      else
        m_emit->LDRH(IndexType::Unsigned, m_dst_reg, ARM64Reg::X0, offset);
      break;
    case 32:
      m_emit->LDR(IndexType::Unsigned, m_dst_reg, ARM64Reg::X0, offset);
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unknown size {} passed to MMIOReadCodeGenerator!", sbits);
      break;
    }
  }

  void LoadAddrMaskToReg(int sbits, const void* ptr, u32 mask)
  {
    const s32 offset = m_emit->MOVPage2R(ARM64Reg::X0, ptr);

    // If we do not need to mask, we can do the sign extend while loading
    // from memory. If masking is required, we have to first zero extend,
    // then mask, then sign extend if needed (1 instr vs. ~4).
    const u32 all_ones = (1ULL << sbits) - 1;
    if ((all_ones & mask) == all_ones)
    {
      LoadToRegister(sbits, false, offset);
    }
    else
    {
      LoadToRegister(sbits, true, offset);
      m_emit->ANDI2R(m_dst_reg, m_dst_reg, mask, ARM64Reg::W0);
      if (m_sign_extend)
        m_emit->SBFM(m_dst_reg, m_dst_reg, 0, sbits - 1);
    }
  }

  void CallLambda(int sbits, const std::function<T(Core::System&, u32)>* lambda)
  {
    ARM64FloatEmitter float_emit(m_emit);

    m_emit->ABI_PushRegisters(m_gprs_in_use);
    float_emit.ABI_PushRegisters(m_fprs_in_use, ARM64Reg::X1);
    m_emit->MOVP2R(ARM64Reg::X1, m_system);
    m_emit->MOVI2R(ARM64Reg::W2, m_address);
    m_emit->BLR(m_emit->ABI_SetupLambda(lambda));
    if (m_sign_extend)
      m_emit->SBFM(m_dst_reg, ARM64Reg::W0, 0, sbits - 1);
    else
      m_emit->UBFM(m_dst_reg, ARM64Reg::W0, 0, sbits - 1);

    float_emit.ABI_PopRegisters(m_fprs_in_use, ARM64Reg::X1);
    m_emit->ABI_PopRegisters(m_gprs_in_use);
  }

  Core::System* m_system;
  ARM64XEmitter* m_emit;
  BitSet32 m_gprs_in_use;
  BitSet32 m_fprs_in_use;
  ARM64Reg m_dst_reg;
  u32 m_address;
  bool m_sign_extend;
};

void SwapPairs(ARM64XEmitter* emit, ARM64Reg dst_reg, ARM64Reg src_reg, u32 flags)
{
  if (flags & BackPatchInfo::FLAG_SIZE_32)
    emit->ROR(dst_reg, src_reg, 32);
  else if (flags & BackPatchInfo::FLAG_SIZE_16)
    emit->ROR(dst_reg, src_reg, 16);
  else
    emit->REV16(dst_reg, src_reg);
}

void ByteswapAfterLoad(ARM64XEmitter* emit, ARM64FloatEmitter* float_emit, ARM64Reg dst_reg,
                       ARM64Reg src_reg, u32 flags, bool is_reversed, bool is_extended)
{
  if (is_reversed == !(flags & BackPatchInfo::FLAG_REVERSE))
  {
    if (flags & BackPatchInfo::FLAG_SIZE_64)
    {
      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV64(8, dst_reg, src_reg);
      else
        emit->REV64(dst_reg, src_reg);

      src_reg = dst_reg;
    }
    else if (flags & BackPatchInfo::FLAG_SIZE_32)
    {
      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV32(8, dst_reg, src_reg);
      else
        emit->REV32(dst_reg, src_reg);

      src_reg = dst_reg;
    }
    else if (flags & BackPatchInfo::FLAG_SIZE_16)
    {
      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV16(8, dst_reg, src_reg);
      else
        emit->REV16(dst_reg, src_reg);

      src_reg = dst_reg;
    }
  }

  if (!is_extended && (flags & BackPatchInfo::FLAG_EXTEND))
  {
    emit->SXTH(dst_reg, src_reg);
    src_reg = dst_reg;
  }

  if (dst_reg != src_reg)
  {
    if (flags & BackPatchInfo::FLAG_FLOAT)
      float_emit->ORR(dst_reg, src_reg, src_reg);
    else
      emit->MOV(dst_reg, src_reg);
  }
}

ARM64Reg ByteswapBeforeStore(ARM64XEmitter* emit, ARM64FloatEmitter* float_emit, ARM64Reg tmp_reg,
                             ARM64Reg src_reg, u32 flags, bool want_reversed)
{
  ARM64Reg dst_reg = src_reg;

  if (want_reversed == !(flags & BackPatchInfo::FLAG_REVERSE))
  {
    if (flags & BackPatchInfo::FLAG_SIZE_64)
    {
      dst_reg = tmp_reg;

      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV64(8, dst_reg, src_reg);
      else
        emit->REV64(dst_reg, src_reg);
    }
    else if (flags & BackPatchInfo::FLAG_SIZE_32)
    {
      dst_reg = tmp_reg;

      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV32(8, dst_reg, src_reg);
      else
        emit->REV32(dst_reg, src_reg);
    }
    else if (flags & BackPatchInfo::FLAG_SIZE_16)
    {
      dst_reg = tmp_reg;

      if (flags & BackPatchInfo::FLAG_FLOAT)
        float_emit->REV16(8, dst_reg, src_reg);
      else
        emit->REV16(dst_reg, src_reg);
    }
  }

  return dst_reg;
}

void MMIOLoadToReg(Core::System& system, MMIO::Mapping* mmio, ARM64XEmitter* emit,
                   ARM64FloatEmitter* float_emit, BitSet32 gprs_in_use, BitSet32 fprs_in_use,
                   ARM64Reg dst_reg, u32 address, u32 flags)
{
  ASSERT(!(flags & BackPatchInfo::FLAG_FLOAT));

  if (flags & BackPatchInfo::FLAG_SIZE_8)
  {
    MMIOReadCodeGenerator<u8> gen(&system, emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                  flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u8>(address).Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_16)
  {
    MMIOReadCodeGenerator<u16> gen(&system, emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                   flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u16>(address).Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_32)
  {
    MMIOReadCodeGenerator<u32> gen(&system, emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                   flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u32>(address).Visit(gen);
  }

  ByteswapAfterLoad(emit, float_emit, dst_reg, dst_reg, flags, false, true);
}

void MMIOWriteRegToAddr(Core::System& system, MMIO::Mapping* mmio, ARM64XEmitter* emit,
                        ARM64FloatEmitter* float_emit, BitSet32 gprs_in_use, BitSet32 fprs_in_use,
                        ARM64Reg src_reg, u32 address, u32 flags)
{
  ASSERT(!(flags & BackPatchInfo::FLAG_FLOAT));

  src_reg = ByteswapBeforeStore(emit, float_emit, ARM64Reg::W1, src_reg, flags, false);

  if (flags & BackPatchInfo::FLAG_SIZE_8)
  {
    MMIOWriteCodeGenerator<u8> gen(&system, emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u8>(address).Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_16)
  {
    MMIOWriteCodeGenerator<u16> gen(&system, emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u16>(address).Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_32)
  {
    MMIOWriteCodeGenerator<u32> gen(&system, emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u32>(address).Visit(gen);
  }
}
