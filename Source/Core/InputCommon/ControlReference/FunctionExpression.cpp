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

using Clock = std::chrono::steady_clock;
using FSec = std::chrono::duration<ControlState>;

// usage: toggle(toggle_state_input, [clear_state_input])
class ToggleExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    // Optional 2nd argument for clearing state:
    if (1 == args.size() || 2 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"toggle_state_input, [clear_state_input]"};
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

  mutable bool m_released{};
  mutable bool m_state{};
};

// usage: not(expression)
class NotExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (1 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return 1.0 - GetArg(0).GetValue(); }
  void SetValue(ControlState value) override { GetArg(0).SetValue(1.0 - value); }
};

// usage: sin(expression)
class SinExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (1 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::sin(GetArg(0).GetValue()); }
};

// usage: timer(seconds)
class TimerExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (1 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"seconds"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();
    const auto elapsed = now - m_start_time;

    const ControlState val = GetArg(0).GetValue();

    ControlState progress = std::chrono::duration_cast<FSec>(elapsed).count() / val;

    if (std::isinf(progress) || progress < 0.0)
    {
      // User configured a non-positive timer. Reset the timer and return 0.0.
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

private:
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: if(condition, true_expression, false_expression)
class IfExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (3 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"condition, true_expression, false_expression"};
  }

  ControlState GetValue() const override
  {
    return (GetArg(0).GetValue() > CONDITION_THRESHOLD) ? GetArg(1).GetValue() :
                                                          GetArg(2).GetValue();
  }
};

// usage: minus(expression)
class UnaryMinusExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (1 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override
  {
    // Subtraction for clarity:
    return 0.0 - GetArg(0).GetValue();
  }
};

// usage: deadzone(input, amount)
class DeadzoneExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (2 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, amount"};
  }

  ControlState GetValue() const override
  {
    const ControlState val = GetArg(0).GetValue();
    const ControlState deadzone = GetArg(1).GetValue();
    return std::copysign(std::max(0.0, std::abs(val) - deadzone) / (1.0 - deadzone), val);
  }
};

// usage: smooth(input, seconds_up, seconds_down = seconds_up)
// seconds is seconds to change from 0.0 to 1.0
class SmoothExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (2 == args.size() || 3 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds_up, seconds_down = seconds_up"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();
    const auto elapsed = now - m_last_update;
    m_last_update = now;

    const ControlState desired_value = GetArg(0).GetValue();

    const ControlState smooth_up = GetArg(1).GetValue();
    const ControlState smooth_down = (3 == GetArgCount() ? GetArg(2).GetValue() : smooth_up);

    const ControlState smooth = (desired_value < m_value) ? smooth_down : smooth_up;
    const ControlState max_move = std::chrono::duration_cast<FSec>(elapsed).count() / smooth;

    if (std::isinf(max_move))
    {
      m_value = desired_value;
    }
    else
    {
      const ControlState diff = desired_value - m_value;
      m_value += std::copysign(std::min(max_move, std::abs(diff)), diff);
    }

    return m_value;
  }

private:
  mutable ControlState m_value = 0.0;
  mutable Clock::time_point m_last_update = Clock::now();
};

// usage: hold(input, seconds)
class HoldExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (2 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const ControlState input = GetArg(0).GetValue();

    if (input < CONDITION_THRESHOLD)
    {
      m_state = false;
      m_start_time = Clock::now();
    }
    else if (!m_state)
    {
      const auto hold_time = now - m_start_time;

      if (std::chrono::duration_cast<FSec>(hold_time).count() >= GetArg(1).GetValue())
        m_state = true;
    }

    return m_state;
  }

private:
  mutable bool m_state = false;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: tap(input, seconds, taps=2)
class TapExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (2 == args.size() || 3 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds, taps = 2"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const auto elapsed = std::chrono::duration_cast<FSec>(now - m_start_time).count();

    const ControlState input = GetArg(0).GetValue();
    const ControlState seconds = GetArg(1).GetValue();

    const bool is_time_up = elapsed > seconds;

    const u32 desired_taps = (3 == GetArgCount()) ? u32(GetArg(2).GetValue() + 0.5) : 2;

    if (input < CONDITION_THRESHOLD)
    {
      m_released = true;

      if (m_taps > 0 && is_time_up)
      {
        m_taps = 0;
      }
    }
    else
    {
      if (m_released)
      {
        if (!m_taps)
        {
          m_start_time = now;
        }

        ++m_taps;
        m_released = false;
      }

      return desired_taps == m_taps;
    }

    return 0.0;
  }

private:
  mutable bool m_released = true;
  mutable u32 m_taps = 0;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: relative(input, speed, [max_abs_value, [shared_state]])
// speed is max movement per second
class RelativeExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 2 && args.size() <= 4)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, speed, [max_abs_value, [shared_state]]"};
  }

  ControlState GetValue() const override
  {
    // There is a lot of funky math in this function but it allows for a variety of uses:
    //
    // e.g. A single mapping with a relatively adjusted value between 0.0 and 1.0
    // Potentially useful for a trigger input
    //  relative(`Up` - `Down`, 2.0)
    //
    // e.g. A value with two mappings (such as analog stick Up/Down)
    // The shared state allows the two mappings to work together.
    // This mapping (for up) returns a value clamped between 0.0 and 1.0
    //  relative(`Up`, 2.0, 1.0, $y)
    // This mapping (for down) returns the negative value clamped between 0.0 and 1.0
    // (Adjustments created by `Down` are applied negatively to the shared state)
    //  relative(`Down`, 2.0, -1.0, $y)

    const auto now = Clock::now();

    if (GetArgCount() >= 4)
      m_state = GetArg(3).GetValue();

    const auto elapsed = std::chrono::duration_cast<FSec>(now - m_last_update).count();
    m_last_update = now;

    const ControlState input = GetArg(0).GetValue();
    const ControlState speed = GetArg(1).GetValue();

    const ControlState max_abs_value = (GetArgCount() >= 3) ? GetArg(2).GetValue() : 1.0;

    const ControlState max_move = input * elapsed * speed;
    const ControlState diff_from_zero = std::abs(0.0 - m_state);
    const ControlState diff_from_max = std::abs(max_abs_value - m_state);

    m_state += std::min(std::max(max_move, -diff_from_zero), diff_from_max) *
               std::copysign(1.0, max_abs_value);

    if (GetArgCount() >= 4)
      const_cast<Expression&>(GetArg(3)).SetValue(m_state);

    return std::max(0.0, m_state * std::copysign(1.0, max_abs_value));
  }

private:
  mutable ControlState m_state = 0.0;
  mutable Clock::time_point m_last_update = Clock::now();
};

// usage: pulse(input, seconds)
class PulseExpression : public FunctionExpression
{
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (2 == args.size())
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const ControlState input = GetArg(0).GetValue();

    if (input < CONDITION_THRESHOLD)
    {
      m_released = true;
    }
    else if (m_released)
    {
      m_released = false;

      const auto seconds = std::chrono::duration_cast<Clock::duration>(FSec(GetArg(1).GetValue()));

      if (m_state)
      {
        m_release_time += seconds;
      }
      else
      {
        m_state = true;
        m_release_time = now + seconds;
      }
    }

    if (m_state && now >= m_release_time)
    {
      m_state = false;
    }

    return m_state;
  }

private:
  mutable bool m_released = false;
  mutable bool m_state = false;
  mutable Clock::time_point m_release_time = Clock::now();
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string name)
{
  if ("not" == name)
    return std::make_unique<NotExpression>();
  else if ("if" == name)
    return std::make_unique<IfExpression>();
  else if ("sin" == name)
    return std::make_unique<SinExpression>();
  else if ("timer" == name)
    return std::make_unique<TimerExpression>();
  else if ("toggle" == name)
    return std::make_unique<ToggleExpression>();
  else if ("minus" == name)
    return std::make_unique<UnaryMinusExpression>();
  else if ("deadzone" == name)
    return std::make_unique<DeadzoneExpression>();
  else if ("smooth" == name)
    return std::make_unique<SmoothExpression>();
  else if ("hold" == name)
    return std::make_unique<HoldExpression>();
  else if ("tap" == name)
    return std::make_unique<TapExpression>();
  else if ("relative" == name)
    return std::make_unique<RelativeExpression>();
  else if ("pulse" == name)
    return std::make_unique<PulseExpression>();
  else
    return nullptr;
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

FunctionExpression::ArgumentValidation
FunctionExpression::SetArguments(std::vector<std::unique_ptr<Expression>>&& args)
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

void FunctionExpression::SetValue(ControlState)
{
}

}  // namespace ExpressionParser
}  // namespace ciface
