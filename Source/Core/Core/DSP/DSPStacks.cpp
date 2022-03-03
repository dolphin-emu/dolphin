// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/DSPCore.h"

#include <cstddef>

#include "Common/CommonTypes.h"

// Stacks. The stacks are outside the DSP RAM, in dedicated hardware.
namespace DSP
{
void SDSP::StoreStack(StackRegister stack_reg, u16 val)
{
  const auto reg_index = static_cast<size_t>(stack_reg);

  reg_stack_ptrs[reg_index]++;
  reg_stack_ptrs[reg_index] &= DSP_STACK_MASK;
  reg_stacks[reg_index][reg_stack_ptrs[reg_index]] = r.st[reg_index];

  r.st[reg_index] = val;
}

u16 SDSP::PopStack(StackRegister stack_reg)
{
  const auto reg_index = static_cast<size_t>(stack_reg);
  const u16 val = r.st[reg_index];

  r.st[reg_index] = reg_stacks[reg_index][reg_stack_ptrs[reg_index]];
  reg_stack_ptrs[reg_index]--;
  reg_stack_ptrs[reg_index] &= DSP_STACK_MASK;

  return val;
}
}  // namespace DSP
