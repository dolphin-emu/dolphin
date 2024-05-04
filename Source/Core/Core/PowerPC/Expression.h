// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct expr;
struct expr_var_list;

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

struct ExprDeleter
{
  void operator()(expr* expression) const;
};

using ExprPointer = std::unique_ptr<expr, ExprDeleter>;

struct ExprVarListDeleter
{
  void operator()(expr_var_list* vars) const;
};

using ExprVarListPointer = std::unique_ptr<expr_var_list, ExprVarListDeleter>;

class Expression
{
public:
  static std::optional<Expression> TryParse(std::string_view text);

  double Evaluate(Core::System& system) const;

  std::string GetText() const;

private:
  enum class SynchronizeDirection
  {
    From,
    To,
  };

  enum class VarBindingType
  {
    Zero,
    GPR,
    FPR,
    SPR,
    PCtr,
    MSR,
  };

  struct VarBinding
  {
    VarBindingType type = VarBindingType::Zero;
    int index = -1;
  };

  Expression(std::string_view text, ExprPointer ex, ExprVarListPointer vars);

  void SynchronizeBindings(Core::System& system, SynchronizeDirection dir) const;
  void Reporting(const double result) const;

  std::string m_text;
  ExprPointer m_expr;
  ExprVarListPointer m_vars;
  std::vector<VarBinding> m_binds;
};

inline bool EvaluateCondition(Core::System& system, const std::optional<Expression>& condition)
{
  return !condition || condition->Evaluate(system) != 0.0;
}
