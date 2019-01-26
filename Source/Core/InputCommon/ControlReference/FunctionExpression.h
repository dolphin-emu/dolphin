// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControlReference/FunctionExpression.h"

namespace ciface
{
namespace ExpressionParser
{
class FunctionExpression : public Expression
{
public:
  int CountNumControls() const override;
  void UpdateReferences(ControlEnvironment& env) override;
  operator std::string() const override;

  bool SetArguments(std::vector<std::unique_ptr<Expression>>&& args);

protected:
  virtual std::string GetFuncName() const = 0;
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) = 0;

  Expression& GetArg(u32 number);
  const Expression& GetArg(u32 number) const;
  u32 GetArgCount() const;

private:
  std::vector<std::unique_ptr<Expression>> m_args;
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string name);

}  // namespace ExpressionParser
}  // namespace ciface
