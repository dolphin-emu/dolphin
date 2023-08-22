// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/ConstantPropagation.h"

namespace JitCommon
{
ConstantPropagationResult ConstantPropagation::EvaluateInstruction(UGeckoInstruction inst) const
{
  switch (inst.OPCD)
  {
  default:
    return {};
  }
}

void ConstantPropagation::Apply(ConstantPropagationResult result, BitSet32 gprs_out)
{
  if (!result.instruction_fully_executed)
    m_gpr_values_known &= ~gprs_out;

  if (result.gpr >= 0)
    SetGPR(result.gpr, result.gpr_value);
}

}  // namespace JitCommon
