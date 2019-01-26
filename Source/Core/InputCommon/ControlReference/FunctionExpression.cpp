// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <cmath>

#include "InputCommon/ControlReference/FunctionExpression.h"

namespace ciface
{
namespace ExpressionParser
{
constexpr int LOOP_MAX_REPS = 10000;
constexpr ControlState CONDITION_THRESHOLD = 0.5;

// TODO: Return an oscillating value to make it apparent something was spelled wrong?
class UnknownFunctionExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return false;
  }
  ControlState GetValue() const override { return 0.0; }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "unknown"; }
};

class ToggleExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    // Optional 2nd argument for clearing state:
    return 1 == args.size() || 2 == args.size();
  }

  ControlState GetValue() const override
  {
    const ControlState inner_value = GetArg(0).GetValue();

    if (inner_value < CONDITION_THRESHOLD)
    {
      m_released = true;
    }
    else if (m_released && inner_value > CONDITION_THRESHOLD)
    {
      m_released = false;
      m_state ^= true;
    }

    if (2 == GetArgCount() && GetArg(1).GetValue() > CONDITION_THRESHOLD)
    {
      m_state = false;
    }

    return m_state;
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "toggle"; }

  mutable bool m_released{};
  mutable bool m_state{};
};

class NotExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override { return 1.0 - GetArg(0).GetValue(); }
  void SetValue(ControlState value) override { GetArg(0).SetValue(1.0 - value); }
  std::string GetFuncName() const override { return ""; }
};

class SinExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override { return std::sin(GetArg(0).GetValue()); }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "sin"; }
};

class TimerExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();
    const auto elapsed = now - m_start_time;

    using FSec = std::chrono::duration<ControlState>;

    const ControlState val = GetArg(0).GetValue();

    ControlState progress = std::chrono::duration_cast<FSec>(elapsed).count() / val;

    if (std::isinf(progress))
    {
      // User configured a 0.0 length timer. Reset the timer and return 0.0.
      progress = 0.0;
      m_start_time = now;
    }
    else if (progress >= 1.0)
    {
      const ControlState reset_count = std::floor(progress);

      m_start_time += std::chrono::duration_cast<Clock::duration>(FSec(val * reset_count));
      progress -= reset_count;
    }

    return progress;
  }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "timer"; }

private:
  using Clock = std::chrono::steady_clock;
  mutable Clock::time_point m_start_time = Clock::now();
};

class IfExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 3 == args.size();
  }

  ControlState GetValue() const override
  {
    return (GetArg(0).GetValue() > CONDITION_THRESHOLD) ? GetArg(1).GetValue() :
                                                          GetArg(2).GetValue();
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "if"; }
};

class UnaryMinusExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override
  {
    // Subtraction for clarity:
    return 0.0 - GetArg(0).GetValue();
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "minus"; }
};

class WhileExpression : public FunctionExpression
{
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 2 == args.size();
  }

  ControlState GetValue() const override
  {
    // Returns 1.0 on successful loop, 0.0 on reps exceeded. Sensible?

    for (int i = 0; i != LOOP_MAX_REPS; ++i)
    {
      // Check condition of 1st argument:
      const ControlState val = GetArg(0).GetValue();
      if (val < CONDITION_THRESHOLD)
        return 1.0;

      // Evaluate 2nd argument:
      GetArg(1).GetValue();
    }

    // Exceeded max reps:
    return 0.0;
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "while"; }
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string name)
{
  if (name.empty())
    return std::make_unique<NotExpression>();
  else if ("if" == name)
    return std::make_unique<IfExpression>();
  else if ("sin" == name)
    return std::make_unique<SinExpression>();
  else if ("timer" == name)
    return std::make_unique<TimerExpression>();
  else if ("toggle" == name)
    return std::make_unique<ToggleExpression>();
  else if ("while" == name)
    return std::make_unique<WhileExpression>();
  else if ("minus" == name)
    return std::make_unique<UnaryMinusExpression>();
  else
    return std::make_unique<UnknownFunctionExpression>();
}

int FunctionExpression::CountNumControls() const
{
  int result = 0;

  for (auto& arg : m_args)
    result += arg->CountNumControls();

  return result;
}

void FunctionExpression::UpdateReferences(ControlEnvironment& env)
{
  for (auto& arg : m_args)
    arg->UpdateReferences(env);
}

FunctionExpression::operator std::string() const
{
  std::string result = '!' + GetFuncName();

  for (auto& arg : m_args)
    result += ' ' + static_cast<std::string>(*arg);

  return result;
}

bool FunctionExpression::SetArguments(std::vector<std::unique_ptr<Expression>>&& args)
{
  m_args = std::move(args);

  return ValidateArguments(m_args);
}

Expression& FunctionExpression::GetArg(u32 number)
{
  return *m_args[number];
}

const Expression& FunctionExpression::GetArg(u32 number) const
{
  return *m_args[number];
}

u32 FunctionExpression::GetArgCount() const
{
  return u32(m_args.size());
}

}  // namespace ExpressionParser
}  // namespace ciface
