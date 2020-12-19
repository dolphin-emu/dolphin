// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct expr;
struct expr_var_list;

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

  double Evaluate() const;

  std::string GetText() const;

private:
  Expression(std::string_view text, ExprPointer ex, ExprVarListPointer vars);

  std::string m_text;
  ExprPointer m_expr;
  ExprVarListPointer m_vars;
};

inline bool EvaluateCondition(const std::optional<Expression>& condition)
{
  return !condition || condition->Evaluate() != 0;
}
