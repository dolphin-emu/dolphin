// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Jit64/JitRegCache.h"

class Jit64;

class FPURegCache final : public RegCache
{
public:
  explicit FPURegCache(Jit64& jit);

  void StoreRegister(size_t preg, const Gen::OpArg& newLoc) override;
  void LoadRegister(size_t preg, Gen::X64Reg newLoc) override;
  const Gen::X64Reg* GetAllocationOrder(size_t* count) override;
  Gen::OpArg GetDefaultLocation(size_t reg) const override;
  BitSet32 GetRegUtilization() override;
  BitSet32 CountRegsIn(size_t preg, u32 lookahead) override;
};
