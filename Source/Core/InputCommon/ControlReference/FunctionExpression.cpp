// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControlReference/FunctionExpression.h"

#include "Common/MathUtil.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ciface::ExpressionParser
{
using Clock = std::chrono::steady_clock;
using FSec = std::chrono::duration<ControlState>;

// usage: min(expression1, expression2)
class MinExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression1, expression2"};
  }

  ControlState GetValue() const override
  {
    const ControlState val1 = GetArg(0).GetValue();
    const ControlState val2 = GetArg(1).GetValue();

    return std::min(val1, val2);
  }
};

// usage: max(expression1, expression2)
class MaxExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression1, expression2"};
  }

  ControlState GetValue() const override
  {
    const ControlState val1 = GetArg(0).GetValue();
    const ControlState val2 = GetArg(1).GetValue();

    return std::max(val1, val2);
  }
};

// usage: minus(expression)
class UnaryMinusExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
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

// usage: pow(base, exponent)
class PowExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"base, exponent"};
  }

  ControlState GetValue() const override
  {
    const ControlState val1 = GetArg(0).GetValue();
    const ControlState val2 = GetArg(1).GetValue();

    return std::pow(val1, val2);
  }
};

// usage: not(expression)
class NotExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
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
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::sin(GetArg(0).GetValue()); }
};

// usage: cos(expression)
class CosExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::cos(GetArg(0).GetValue()); }
};

// usage: tan(expression)
class TanExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::tan(GetArg(0).GetValue()); }
};

// usage: asin(expression)
class ASinExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::asin(GetArg(0).GetValue()); }
};

// usage: acos(expression)
class ACosExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::acos(GetArg(0).GetValue()); }
};

// usage: atan(expression)
class ATanExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::atan(GetArg(0).GetValue()); }
};

// usage: atan2(y, x)
class ATan2Expression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"y, x"};
  }

  ControlState GetValue() const override
  {
    return std::atan2(GetArg(0).GetValue(), GetArg(1).GetValue());
  }
};

// usage: sqrt(expression)
class SqrtExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression"};
  }

  ControlState GetValue() const override { return std::sqrt(GetArg(0).GetValue()); }
};

// usage: clamp(value, min, max)
class ClampExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 3)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"value, min, max"};
  }

  ControlState GetValue() const override
  {
    return std::clamp(GetArg(0).GetValue(), GetArg(1).GetValue(), GetArg(2).GetValue());
  }
};

// usage: timer(seconds)
class TimerExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
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
      // User configured a non-positive timer. Reset the timer and return 0.
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

  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: if(condition, true_expression, false_expression)
class IfExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 3)
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

// usage: deadzone(input, amount)
class DeadzoneExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
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

// usage: antiDeadzone(input, amount)
// Goes against a game built in deadzone, like if it will treat 0.2 (amount) as 0,
// but will treat 0.3 as 0.125. It should be applied after deadzone
class AntiDeadzoneExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, amount"};
  }

  ControlState GetValue() const override
  {
    const ControlState val = GetArg(0).GetValue();
    const ControlState anti_deadzone = GetArg(1).GetValue();
    // Fixes the problem where 2 opposite axis have the same anti deadzone
    // so they would cancel each other out if we didn't check for this
    if (val == 0.0)
    {
      return 0.0;
    }
    const ControlState abs_val = std::abs(val);
    return std::copysign(anti_deadzone + abs_val * (1.0 - anti_deadzone), val);
  }
};

// usage: bezierCurve(input, x1, x2)
// 2 control points bezier. Basically just a fancy "remap" in one dimension.
// Useful to go against games analog stick response curve when using the mouse as
// as axis, or when games have a linear response curve to an analog stick and
// you'd like to change it. Mostly used on the x axis (left/right).
class BezierCurveExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 3)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, x1, x2"};
  }

  ControlState GetValue() const override
  {
    const ControlState val = GetArg(0).GetValue();
    const ControlState x1 = GetArg(1).GetValue();
    const ControlState x2 = GetArg(2).GetValue();

    const ControlState t = std::abs(val);
    const ControlState t2 = t * t;
    const ControlState t3 = t2 * t;
    const ControlState u = 1.0 - t;
    const ControlState u2 = u * u;

    // Don't clamp between 0 and 1.
    // Formula actually is like below but control point x0 and x3 are 0 and 1.
    // u3*x0 + 3*u2*t*x1 + 3*u*t2*x2 + t3*x3
    const ControlState bezier = (3.0 * u2 * t * x1) + (3.0 * u * t2 * x2) + t3;
    return std::copysign(bezier, val);
  }
};

// usage: acceleration(input, max_acceleration_time, acceleration, min_time_input,
// min_acceleration_input, max_acceleration_input = 1)
// Useful when using the mouse as an axis and you don't want the game
// to gradually accelerate its response with time. It should help in
// keeping a response curve closer to 1:1 (linear).
// The way this work is by "predicting" the acceleration the game would have with time,
// and increasing the input value so you won't feel any acceleration,
// the drawback is that you might see some velocity snapping, and this doesn't
// work when passing in a maxed out input of course.
// An alternative approach would be to actually try to shrink the input as time passed,
// to counteract the acceleration, but that would strongly limit the max output.
// Use after AntiDeadzone and BezierCurve. In some games acceleration is only on the X axis.
class AntiAccelerationExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 5 || args.size() == 6)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, max_acceleration_time, acceleration, min_time_input, "
                               "min_acceleration_input, max_acceleration_input = 1"};
  }

  ControlState GetValue() const override
  {
    // Always get all the values as other expressions might change values in their GetValue() method

    const ControlState val = GetArg(0).GetValue();
    // After this time, the game won't accelerate anymore
    const ControlState max_acc_time = 1.0 + GetArg(1).GetValue();
    // Just a random (guessed) value to accelerate our input while we are contrasting the
    // acceleration that is happening in the game
    const ControlState acc = GetArg(2).GetValue();
    // Elapsed time, which influences the output, is counted if the input is >= than this value
    const ControlState min_time_input = GetArg(3).GetValue();
    // Input at which the game would start accelerating
    const ControlState min_acc_input = GetArg(4).GetValue();
    // Input at which the game would reach the maximum acceleration (usually 1)
    const ControlState max_acc_input = GetArgCount() == 6 ? GetArg(5).GetValue() : 1.0;

    const ControlState abs_val = std::abs(val);
    if (abs_val < min_acc_input)
    {
      m_elapsed = 0.0;
      m_last_update = Clock::now();
      return val;
    }

    const ControlState diff = max_acc_input - min_acc_input;
    const ControlState acc_alpha =
        diff != 0.0 ? std::clamp((abs_val - min_acc_input) / diff, 0.0, 1.0) : 1.0;
    const ControlState time_alpha = std::min((max_acc_time - m_elapsed) / max_acc_time, 0.0);

    const ControlState multiplier = MathUtil::Lerp(1.0, acc * acc_alpha, time_alpha * time_alpha);

    const auto now = Clock::now();
    if (abs_val >= min_time_input)
    {
      // This should be multiplied by the actual emulation speed of course but it can't here
      m_elapsed += std::chrono::duration_cast<FSec>(now - m_last_update).count();
    }
    else
    {
      m_elapsed = 0.0;
    }
    m_last_update = now;

    return std::copysign(abs_val * multiplier, val);
  }

  mutable Clock::time_point m_last_update = Clock::now();
  mutable ControlState m_elapsed = 0.0;
};

// usage: smooth(input, seconds_up, seconds_down = seconds_up)
// seconds is seconds to change from 0.0 to 1.0
class SmoothExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2 || args.size() == 3)
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
    const ControlState smooth_down = GetArgCount() == 3 ? GetArg(2).GetValue() : smooth_up;

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

  mutable ControlState m_value = 0.0;
  mutable Clock::time_point m_last_update = Clock::now();
};

// usage: toggle(input, [clear])
// Toggled on press
class ToggleExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    // Optional 2nd argument for clearing state:
    if (args.size() == 1 || args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, [clear]"};
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();

    if (input > CONDITION_THRESHOLD)
    {
      if (!m_pressed)
      {
        m_pressed = true;
        m_state ^= true;
      }
    }
    else
    {
      m_pressed = false;
    }

    if (GetArgCount() == 2 && GetArg(1).GetValue() > CONDITION_THRESHOLD)
    {
      m_state = false;
    }

    return m_state;
  }

  mutable bool m_pressed{};
  mutable bool m_state{};
};

// usage: onPress(input)
// Once, every time it starts being pressed
class OnPressExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input"};
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();

    if (input > CONDITION_THRESHOLD)
    {
      if (!m_pressed)
      {
        m_pressed = true;
        return 1.0;
      }
    }
    else
    {
      m_pressed = false;
    }

    return 0.0;
  }

  mutable bool m_pressed{};
};

// usage: onRelease(input)
class OnReleaseExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input"};
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();

    if (input > CONDITION_THRESHOLD)
    {
      m_pressed = true;
    }
    else if (m_pressed)
    {
      m_pressed = false;
      return 1.0;
    }

    return 0.0;
  }

  mutable bool m_pressed{};
};

// usage: onChange(input)
// Returns 1 when the input has changed from the last cached value (has a threshold)
class OnChangeExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input"};
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();

    bool pressed = false;
    if (input > CONDITION_THRESHOLD)
    {
      pressed = true;
    }

    if (m_pressed != pressed)
    {
      m_pressed = pressed;
      return 1.0;
    }

    return 0.0;
  }

  mutable bool m_pressed{};
};

// usage: cache(input, condition)
// Caches and returns the input when the condition is true,
// returns the last cached value when the condition is false
class CacheExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, condition"};
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const ControlState condition = GetArg(1).GetValue();

    if (condition > CONDITION_THRESHOLD)
    {
      m_state = input;
    }

    return m_state;
  }

  mutable ControlState m_state = 0.0;
};

// usage: hold(input, seconds)
class HoldExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const ControlState input = GetArg(0).GetValue();

    if (input <= CONDITION_THRESHOLD)
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

  mutable bool m_state = false;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: tap(input, seconds, taps = 2)
// Double+ click detection
class TapExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2 || args.size() == 3)
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

    const u32 desired_taps = GetArgCount() == 3 ? u32(GetArg(2).GetValue() + 0.5) : 2;

    if (input <= CONDITION_THRESHOLD)
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

  mutable bool m_released = true;
  mutable u32 m_taps = 0;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: relative(input, speed, [max_abs_value], [shared_state])
// speed is max movement per second
class RelativeExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 2 && args.size() <= 4)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, speed, [max_abs_value], [shared_state]"};
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

  mutable ControlState m_state = 0.0;
  mutable Clock::time_point m_last_update = Clock::now();
};

// usage: pulse(input, seconds)
class PulseExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds"};
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const ControlState input = GetArg(0).GetValue();

    if (input <= CONDITION_THRESHOLD)
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

  mutable bool m_released = false;
  mutable bool m_state = false;
  mutable Clock::time_point m_release_time = Clock::now();
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string_view name)
{
  // Logic/Math:
  if (name == "if")
    return std::make_unique<IfExpression>();
  if (name == "not")
    return std::make_unique<NotExpression>();
  if (name == "min")
    return std::make_unique<MinExpression>();
  if (name == "max")
    return std::make_unique<MaxExpression>();
  if (name == "clamp")
    return std::make_unique<ClampExpression>();
  if (name == "minus")
    return std::make_unique<UnaryMinusExpression>();
  if (name == "pow")
    return std::make_unique<PowExpression>();
  if (name == "sqrt")
    return std::make_unique<SqrtExpression>();
  if (name == "sin")
    return std::make_unique<SinExpression>();
  if (name == "cos")
    return std::make_unique<CosExpression>();
  if (name == "tan")
    return std::make_unique<TanExpression>();
  if (name == "asin")
    return std::make_unique<ASinExpression>();
  if (name == "acos")
    return std::make_unique<ACosExpression>();
  if (name == "atan")
    return std::make_unique<ATanExpression>();
  if (name == "atan2")
    return std::make_unique<ATan2Expression>();
  // State/time based:
  if (name == "onPress")
    return std::make_unique<OnPressExpression>();
  if (name == "onRelease")
    return std::make_unique<OnReleaseExpression>();
  if (name == "onChange")
    return std::make_unique<OnChangeExpression>();
  if (name == "cache")
    return std::make_unique<CacheExpression>();
  if (name == "hold")
    return std::make_unique<HoldExpression>();
  if (name == "toggle")
    return std::make_unique<ToggleExpression>();
  if (name == "tap")
    return std::make_unique<TapExpression>();
  if (name == "relative")
    return std::make_unique<RelativeExpression>();
  if (name == "smooth")
    return std::make_unique<SmoothExpression>();
  if (name == "pulse")
    return std::make_unique<PulseExpression>();
  if (name == "timer")
    return std::make_unique<TimerExpression>();
  // Stick helpers:
  if (name == "deadzone")
    return std::make_unique<DeadzoneExpression>();
  if (name == "antiDeadzone")
    return std::make_unique<AntiDeadzoneExpression>();
  if (name == "bezierCurve")
    return std::make_unique<BezierCurveExpression>();
  if (name == "antiAcceleration")
    return std::make_unique<AntiAccelerationExpression>();

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

}  // namespace ciface::ExpressionParser
