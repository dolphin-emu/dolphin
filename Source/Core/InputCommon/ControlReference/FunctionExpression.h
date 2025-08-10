// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/ControlReference/ExpressionParser.h"

namespace ciface::ExpressionParser
{
constexpr ControlState CONDITION_THRESHOLD = 0.5;

class FunctionExpression : public Expression
{
public:
  struct ArgumentsAreValid
  {
  };

  struct ExpectedArguments
  {
    std::string text;
  };

  using ArgumentValidation = std::variant<ArgumentsAreValid, ExpectedArguments>;

  int CountNumControls() const override;
  void UpdateReferences(ControlEnvironment& env) override;

  void SetArguments(std::vector<std::unique_ptr<Expression>>&& args);
  virtual ArgumentValidation ValidateArguments() = 0;

  void SetValue(ControlState value) override;

protected:
  Expression& GetArg(u32 number);
  u32 GetArgCount() const;

private:
  std::vector<std::unique_ptr<Expression>> m_args;
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string_view name);

}  // namespace ciface::ExpressionParser
