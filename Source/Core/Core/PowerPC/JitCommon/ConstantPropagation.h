// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/PowerPC.h"

#include <array>
#include <cstddef>
#include <optional>

namespace JitCommon
{
struct ConstantPropagationResult final
{
  constexpr ConstantPropagationResult() = default;

  constexpr ConstantPropagationResult(s8 gpr_, u32 gpr_value_, bool compute_rc_ = false)
      : gpr_value(gpr_value_), gpr(gpr_), instruction_fully_executed(true), compute_rc(compute_rc_)
  {
  }

  // If gpr is non-negative, this is the value the instruction writes to that GPR.
  u32 gpr_value = 0;

  // If the instruction couldn't be evaluated or doesn't output to a GPR, this is -1.
  // Otherwise, this is the GPR that the instruction writes to.
  s8 gpr = -1;

  // Whether the instruction was able to be fully evaluated with no side effects unaccounted for,
  // or in other words, whether the JIT can skip emitting code for this instruction.
  bool instruction_fully_executed = false;

  // If true, CR0 needs to be set based on gpr_value.
  bool compute_rc = false;

  // If not std::nullopt, the instruction writes this to the carry flag.
  std::optional<bool> carry = std::nullopt;

  // If not std::nullopt, the instruction writes this to the overflow flag.
  std::optional<bool> overflow = std::nullopt;
};

class ConstantPropagation final
{
public:
  ConstantPropagationResult EvaluateInstruction(UGeckoInstruction inst, u64 flags) const;

  void Apply(ConstantPropagationResult result, BitSet32 gprs_out);

  template <typename... Args>
  bool HasGPR(Args... gprs) const
  {
    BitSet32 mask{static_cast<int>(gprs)...};
    return (m_gpr_values_known & mask) == mask;
  }

  u32 GetGPR(size_t gpr) const { return m_gpr_values[gpr]; }

  void SetGPR(size_t gpr, u32 value)
  {
    m_gpr_values_known[gpr] = true;
    m_gpr_values[gpr] = value;
  }

  void ClearGPR(size_t gpr) { m_gpr_values_known[gpr] = false; }

  void Clear() { m_gpr_values_known = BitSet32{}; }

private:
  ConstantPropagationResult EvaluateMulImm(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateSubImmCarry(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateAddImm(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateAddImmCarry(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateRlwimix(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateRlwinmxRlwnmx(UGeckoInstruction inst, u32 shift) const;
  ConstantPropagationResult EvaluateBitwiseImm(UGeckoInstruction inst,
                                               u32 (*do_op)(u32, u32)) const;
  ConstantPropagationResult EvaluateTable31(UGeckoInstruction inst, u64 flags) const;
  ConstantPropagationResult EvaluateTable31Negx(UGeckoInstruction inst, u64 flags) const;
  ConstantPropagationResult EvaluateTable31S(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateTable31AB(UGeckoInstruction inst, u64 flags) const;
  ConstantPropagationResult EvaluateTable31ABOneRegisterKnown(UGeckoInstruction inst, u64 flags,
                                                              u32 value, bool known_reg_is_b) const;
  ConstantPropagationResult EvaluateTable31ABIdenticalRegisters(UGeckoInstruction inst,
                                                                u64 flags) const;
  ConstantPropagationResult EvaluateTable31SB(UGeckoInstruction inst) const;
  ConstantPropagationResult EvaluateTable31SBOneRegisterKnown(UGeckoInstruction inst, u32 value,
                                                              bool known_reg_is_b) const;
  ConstantPropagationResult EvaluateTable31SBIdenticalRegisters(UGeckoInstruction inst) const;

  static constexpr ConstantPropagationResult DO_NOTHING = [] {
    ConstantPropagationResult result;
    result.instruction_fully_executed = true;
    return result;
  }();

  static constexpr size_t GPR_COUNT = 32;

  std::array<u32, GPR_COUNT> m_gpr_values;
  BitSet32 m_gpr_values_known{};
};

}  // namespace JitCommon
