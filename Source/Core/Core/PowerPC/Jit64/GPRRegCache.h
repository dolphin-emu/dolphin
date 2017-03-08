// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Jit64/JitRegCache.h"

class Jit64;

class GPRRegCache final : public RegCache
{
public:
  explicit GPRRegCache(Jit64& jit);

  void StoreRegister(size_t preg, const Gen::OpArg& new_loc) override;
  void LoadRegister(size_t preg, Gen::X64Reg new_loc) override;
  Gen::OpArg GetDefaultLocation(size_t reg) const override;
  const Gen::X64Reg* GetAllocationOrder(size_t* count) override;
  void SetImmediate32(size_t preg, u32 imm_value, bool dirty = true);
  BitSet32 GetRegUtilization() override;
  BitSet32 CountRegsIn(size_t preg, u32 lookahead) override;
};
