// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"

class Jit64;

class FPURegCache final : public RegCache
{
public:
  explicit FPURegCache(Jit64& jit);

  bool IsImm(preg_t preg) const override;
  u32 Imm32(preg_t preg) const override;
  s32 SImm32(preg_t preg) const override;

protected:
  Gen::OpArg R(preg_t preg) const override;
  Gen::OpArg GetDefaultLocation(preg_t preg) const override;
  void StoreRegister(preg_t preg, const Gen::OpArg& newLoc) override;
  void LoadRegister(preg_t preg, Gen::X64Reg newLoc) override;
  void DiscardImm(preg_t preg) override;
  std::span<const Gen::X64Reg> GetAllocationOrder() const override;
  BitSet32 GetRegUtilization() const override;
  BitSet32 CountRegsIn(preg_t preg, u32 lookahead) const override;
};
