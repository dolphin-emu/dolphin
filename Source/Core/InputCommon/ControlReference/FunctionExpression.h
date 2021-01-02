// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
// if input > this, then it passes
constexpr ControlState CONDITION_THRESHOLD = 0.5;

// [arg] means that it's optional and can be not inserted.
// arg = x means that it will default to that value if you don't specify it.
// ... means that unlimited args are supported.
// Make sure to call GetValue() on every arg within your GetValue()
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
  Device::FocusFlags GetFocusFlags() const override;
  void UpdateReferences(ControlEnvironment& env, bool is_input) override;

  ArgumentValidation SetArguments(std::vector<std::unique_ptr<Expression>>&& args);

  // These will be shown in the UI so wrap them in _trans(). Automatically word wrapped.
  virtual const char* GetDescription(bool for_input) const { return nullptr; };

  void SetValue(ControlState value) override;

protected:
  virtual ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) = 0;

  Expression& GetArg(u32 number);
  const Expression& GetArg(u32 number) const;
  u32 GetArgCount() const;

private:
  std::vector<std::unique_ptr<Expression>> m_args;
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string_view name);

}  // namespace ciface::ExpressionParser
