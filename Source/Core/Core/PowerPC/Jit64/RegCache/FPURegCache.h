// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"

class Jit64;

class FPURegCache final : public RegCache
{
public:
  explicit FPURegCache(Jit64& jit);

  RCRepr GetRepr(preg_t preg) const { return m_regs[preg].GetRepr(); }

  template <typename... Args>
  bool IsSingle(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsRCReprSingle(m_regs[pregs].GetRepr()) && ...);
  }

  template <typename... Args>
  bool IsRounded(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsRCReprRounded(m_regs[pregs].GetRepr()) && ...);
  }

  template <typename... Args>
  bool IsAnyDup(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsRCReprAnyDup(m_regs[pregs].GetRepr()) && ...);
  }

  template <typename... Args>
  bool IsDup(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsRCReprDup(m_regs[pregs].GetRepr()) && ...);
  }

  template <typename... Args>
  bool IsDupPhysical(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsRCReprDupPhysical(m_regs[pregs].GetRepr()) && ...);
  }

  RCRepr Convert(Gen::X64Reg reg, RCRepr old, RCRepr request);

protected:
  Gen::OpArg GetDefaultLocation(preg_t preg) const override;
  void StoreRegister(preg_t preg, const Gen::OpArg& newLoc) override;
  void LoadRegister(preg_t preg, Gen::X64Reg newLoc) override;
  void ConvertRegister(preg_t preg, RCRepr requested_repr) override;
  const Gen::X64Reg* GetAllocationOrder(size_t* count) const override;
  BitSet32 GetRegUtilization() const override;
  BitSet32 CountRegsIn(preg_t preg, u32 lookahead) const override;
};
