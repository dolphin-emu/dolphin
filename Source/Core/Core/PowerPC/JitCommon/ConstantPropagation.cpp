// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/ConstantPropagation.h"

namespace JitCommon
{
ConstantPropagationResult ConstantPropagation::EvaluateInstruction(UGeckoInstruction inst) const
{
  return {};
}

void ConstantPropagation::Apply(ConstantPropagationResult result)
{
  if (result.gpr >= 0)
    SetGPR(result.gpr, result.gpr_value);
}

}  // namespace JitCommon
