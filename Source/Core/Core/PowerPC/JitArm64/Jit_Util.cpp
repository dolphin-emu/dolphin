// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/HW/MMIO.h"

#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/Jit_Util.h"

using namespace Arm64Gen;

template <typename T>
class MMIOWriteCodeGenerator : public MMIO::WriteHandlerVisitor<T>
{
public:
  MMIOWriteCodeGenerator(ARM64XEmitter* emit, BitSet32 gprs_in_use, BitSet32 fprs_in_use,
                         ARM64Reg src_reg, u32 address)
      : m_emit(emit), m_gprs_in_use(gprs_in_use), m_fprs_in_use(fprs_in_use), m_src_reg(src_reg),
        m_address(address)
  {
  }

  void VisitNop() override
  {
    // Do nothing
  }
  void VisitDirect(T* addr, u32 mask) override { WriteRegToAddr(8 * sizeof(T), addr, mask); }
  void VisitComplex(const std::function<void(u32, T)>* lambda) override
  {
    CallLambda(8 * sizeof(T), lambda);
  }

private:
  void StoreFromRegister(int sbits, ARM64Reg reg)
  {
    switch (sbits)
    {
    case 8:
      m_emit->STRB(INDEX_UNSIGNED, reg, X0, 0);
      break;
    case 16:
      m_emit->STRH(INDEX_UNSIGNED, reg, X0, 0);
      break;
    case 32:
      m_emit->STR(INDEX_UNSIGNED, reg, X0, 0);
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unknown size %d passed to MMIOWriteCodeGenerator!", sbits);
      break;
    }
  }

  void WriteRegToAddr(int sbits, const void* ptr, u32 mask)
  {
    m_emit->MOVP2R(X0, ptr);

    // If we do not need to mask, we can do the sign extend while loading
    // from memory. If masking is required, we have to first zero extend,
    // then mask, then sign extend if needed (1 instr vs. ~4).
    u32 all_ones = (1ULL << sbits) - 1;
    if ((all_ones & mask) == all_ones)
    {
      StoreFromRegister(sbits, m_src_reg);
    }
    else
    {
      m_emit->ANDI2R(W1, m_src_reg, mask, W1);
      StoreFromRegister(sbits, W1);
    }
  }

  void CallLambda(int sbits, const std::function<void(u32, T)>* lambda)
  {
    ARM64FloatEmitter float_emit(m_emit);

    m_emit->ABI_PushRegisters(m_gprs_in_use);
    float_emit.ABI_PushRegisters(m_fprs_in_use, X1);
    m_emit->MOVI2R(W1, m_address);
    m_emit->MOV(W2, m_src_reg);
    m_emit->BLR(m_emit->ABI_SetupLambda(lambda));
    float_emit.ABI_PopRegisters(m_fprs_in_use, X1);
    m_emit->ABI_PopRegisters(m_gprs_in_use);
  }

  ARM64XEmitter* m_emit;
  BitSet32 m_gprs_in_use;
  BitSet32 m_fprs_in_use;
  ARM64Reg m_src_reg;
  u32 m_address;
};
// Visitor that generates code to read a MMIO value.
template <typename T>
class MMIOReadCodeGenerator : public MMIO::ReadHandlerVisitor<T>
{
public:
  MMIOReadCodeGenerator(ARM64XEmitter* emit, BitSet32 gprs_in_use, BitSet32 fprs_in_use,
                        ARM64Reg dst_reg, u32 address, bool sign_extend)
      : m_emit(emit), m_gprs_in_use(gprs_in_use), m_fprs_in_use(fprs_in_use), m_dst_reg(dst_reg),
        m_address(address), m_sign_extend(sign_extend)
  {
  }

  void VisitConstant(T value) override { LoadConstantToReg(8 * sizeof(T), value); }
  void VisitDirect(const T* addr, u32 mask) override
  {
    LoadAddrMaskToReg(8 * sizeof(T), addr, mask);
  }
  void VisitComplex(const std::function<T(u32)>* lambda) override
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

  void LoadToRegister(int sbits, bool dont_extend)
  {
    switch (sbits)
    {
    case 8:
      if (m_sign_extend && !dont_extend)
        m_emit->LDRSB(INDEX_UNSIGNED, m_dst_reg, X0, 0);
      else
        m_emit->LDRB(INDEX_UNSIGNED, m_dst_reg, X0, 0);
      break;
    case 16:
      if (m_sign_extend && !dont_extend)
        m_emit->LDRSH(INDEX_UNSIGNED, m_dst_reg, X0, 0);
      else
        m_emit->LDRH(INDEX_UNSIGNED, m_dst_reg, X0, 0);
      break;
    case 32:
      m_emit->LDR(INDEX_UNSIGNED, m_dst_reg, X0, 0);
      break;
    default:
      ASSERT_MSG(DYNA_REC, false, "Unknown size %d passed to MMIOReadCodeGenerator!", sbits);
      break;
    }
  }

  void LoadAddrMaskToReg(int sbits, const void* ptr, u32 mask)
  {
    m_emit->MOVP2R(X0, ptr);

    // If we do not need to mask, we can do the sign extend while loading
    // from memory. If masking is required, we have to first zero extend,
    // then mask, then sign extend if needed (1 instr vs. ~4).
    u32 all_ones = (1ULL << sbits) - 1;
    if ((all_ones & mask) == all_ones)
    {
      LoadToRegister(sbits, false);
    }
    else
    {
      LoadToRegister(sbits, true);
      m_emit->ANDI2R(m_dst_reg, m_dst_reg, mask, W0);
      if (m_sign_extend)
        m_emit->SBFM(m_dst_reg, m_dst_reg, 0, sbits - 1);
    }
  }

  void CallLambda(int sbits, const std::function<T(u32)>* lambda)
  {
    ARM64FloatEmitter float_emit(m_emit);

    m_emit->ABI_PushRegisters(m_gprs_in_use);
    float_emit.ABI_PushRegisters(m_fprs_in_use, X1);
    m_emit->MOVI2R(W1, m_address);
    m_emit->BLR(m_emit->ABI_SetupLambda(lambda));
    float_emit.ABI_PopRegisters(m_fprs_in_use, X1);
    m_emit->ABI_PopRegisters(m_gprs_in_use);

    if (m_sign_extend)
      m_emit->SBFM(m_dst_reg, W0, 0, sbits - 1);
    else
      m_emit->UBFM(m_dst_reg, W0, 0, sbits - 1);
  }

  ARM64XEmitter* m_emit;
  BitSet32 m_gprs_in_use;
  BitSet32 m_fprs_in_use;
  ARM64Reg m_dst_reg;
  u32 m_address;
  bool m_sign_extend;
};

void MMIOLoadToReg(MMIO::Mapping* mmio, Arm64Gen::ARM64XEmitter* emit, BitSet32 gprs_in_use,
                   BitSet32 fprs_in_use, ARM64Reg dst_reg, u32 address, u32 flags)
{
  if (flags & BackPatchInfo::FLAG_SIZE_8)
  {
    MMIOReadCodeGenerator<u8> gen(emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                  flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u8>(address)->Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_16)
  {
    MMIOReadCodeGenerator<u16> gen(emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                   flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u16>(address)->Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_32)
  {
    MMIOReadCodeGenerator<u32> gen(emit, gprs_in_use, fprs_in_use, dst_reg, address,
                                   flags & BackPatchInfo::FLAG_EXTEND);
    mmio->GetHandlerForRead<u32>(address)->Visit(gen);
  }
}

void MMIOWriteRegToAddr(MMIO::Mapping* mmio, Arm64Gen::ARM64XEmitter* emit, BitSet32 gprs_in_use,
                        BitSet32 fprs_in_use, ARM64Reg src_reg, u32 address, u32 flags)
{
  if (flags & BackPatchInfo::FLAG_SIZE_8)
  {
    MMIOWriteCodeGenerator<u8> gen(emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u8>(address)->Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_16)
  {
    MMIOWriteCodeGenerator<u16> gen(emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u16>(address)->Visit(gen);
  }
  else if (flags & BackPatchInfo::FLAG_SIZE_32)
  {
    MMIOWriteCodeGenerator<u32> gen(emit, gprs_in_use, fprs_in_use, src_reg, address);
    mmio->GetHandlerForWrite<u32>(address)->Visit(gen);
  }
}
