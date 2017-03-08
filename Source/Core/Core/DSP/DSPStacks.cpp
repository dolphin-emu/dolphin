// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPStacks.h"

#include <cstddef>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPCore.h"

// Stacks. The stacks are outside the DSP RAM, in dedicated hardware.
namespace DSP
{
static void dsp_reg_stack_push(size_t stack_reg)
{
  g_dsp.reg_stack_ptr[stack_reg]++;
  g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
  g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]] = g_dsp.r.st[stack_reg];
}

static void dsp_reg_stack_pop(size_t stack_reg)
{
  g_dsp.r.st[stack_reg] = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]];
  g_dsp.reg_stack_ptr[stack_reg]--;
  g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
}

void dsp_reg_store_stack(StackRegister stack_reg, u16 val)
{
  const auto reg_index = static_cast<size_t>(stack_reg);

  dsp_reg_stack_push(reg_index);
  g_dsp.r.st[reg_index] = val;
}

u16 dsp_reg_load_stack(StackRegister stack_reg)
{
  const auto reg_index = static_cast<size_t>(stack_reg);

  const u16 val = g_dsp.r.st[reg_index];
  dsp_reg_stack_pop(reg_index);
  return val;
}
}  // namespace DSP
