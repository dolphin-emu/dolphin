// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControlReference/FunctionExpression.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/Core.h"
#include "Core/HW/VideoInterface.h"
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
    const ControlState true_state = GetArg(1).GetValue();
    const ControlState false_state = GetArg(2).GetValue();
    return (GetArg(0).GetValue() > CONDITION_THRESHOLD) ? true_state : false_state;
  }
};

// usage: interval(delay_frames, duration_frames)
class IntervalExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"delay_frames, duration_frames"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans(
        "Returns 1 for \"duration_frames\" every \"delay_frames\" (while it keeps counting them)");
  }

  ControlState GetValue() const override
  {
    const u32 delay_frames = u32(std::max(GetArg(0).GetValue() + 0.5, 0.0));
    const u32 duration_frames = u32(std::max(GetArg(1).GetValue() + 0.5, 0.0));

    if (duration_frames_counter > 0)
    {
      --duration_frames_counter;
    }

    if (delay_frames_counter >= delay_frames)
    {
      duration_frames_counter = duration_frames;
      delay_frames_counter = 0;
    }
    else
    {
      delay_frames_counter++;
    }

    return duration_frames_counter > 0;
  }

  mutable u32 delay_frames_counter = 0;
  mutable u32 duration_frames_counter = 0;
};

// usage: sequence(input, value_0, value_1, ...)
class SequenceExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 3)  // Enforce at least two values, use onPress() otherwise
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, value_0, value_1, ..."};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Will start playing back the specified sequence (frame by frame) every time the "
                  "input is pressed");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    // We need to retrieve all the states for consistency, though for this
    // we should "enforce" literal expressions which don't have a state
    std::vector<ControlState> sequence;
    const u32 sequence_length = GetArgCount() - 1;
    sequence.reserve(sequence_length);
    for (u32 i = 1; i < GetArgCount(); ++i)
      sequence.push_back(GetArg(i).GetValue());

    if (input > CONDITION_THRESHOLD && !m_pressed)
    {
      m_started = true;
      m_index = 0;
    }
    m_pressed = input > CONDITION_THRESHOLD;

    if (!m_started)
      return 0;

    if (m_index >= sequence_length)
    {
      m_started = false;
      return 0;
    }

    return sequence[m_index++];  // Increase the index
  }

  mutable u32 m_index = 0;
  mutable bool m_pressed = false;
  mutable bool m_started = false;
};

// usage: record(input, record_input, playback_input)
class RecordExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 3)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, record_input, playback_input"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("It will record \"input\" while \"record_input\" is held down, and "
                  "playback when \"playback_input\" is pressed. \"input\" is passed through "
                  "otherwise. Recordings are not saved");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const ControlState record_input = GetArg(1).GetValue();
    const ControlState playback_input = GetArg(2).GetValue();

    if (record_input > CONDITION_THRESHOLD && !m_record_pressed)
    {
      m_recording = true;
      m_playing_back = false;
      m_recorded_states.clear();
    }
    else if (record_input <= CONDITION_THRESHOLD && m_record_pressed)
    {
      m_recording = false;
    }
    m_record_pressed = record_input > CONDITION_THRESHOLD;

    if (!m_recording && m_recorded_states.size() > 0 && playback_input > CONDITION_THRESHOLD &&
        !m_playback_pressed)
    {
      m_playing_back = true;
      m_index = 0;
    }
    m_playback_pressed = playback_input > CONDITION_THRESHOLD;

    if (m_recording)
    {
      m_recorded_states.push_back(input);
    }
    else if (m_playing_back)
    {
      ++m_index;
      if (m_index >= m_recorded_states.size())
      {
        m_playing_back = false;
      }
      return m_recorded_states[m_index - 1];
    }
    return input;
  }

  mutable std::vector<ControlState> m_recorded_states;
  mutable u32 m_index = 0;
  mutable bool m_record_pressed = false;
  mutable bool m_playback_pressed = false;
  mutable bool m_recording = false;
  mutable bool m_playing_back = false;
};

// usage: lag(input, lag_frames)
class LagExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, lag_frames"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Will lag (delay) the input by \"lag_frames\"");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const size_t lag_frames = size_t(std::max(GetArg(1).GetValue() + 0.5, 0.0));

    m_cached_states.reserve(lag_frames + 1);

    while (m_cached_states.size() < lag_frames)
    {
      m_cached_states.push_back(0);
    }

    m_cached_states.insert(m_cached_states.begin(), input);

    const ControlState oldest_state = m_cached_states[lag_frames];
    while (m_cached_states.size() > lag_frames)
    {
      m_cached_states.pop_back();
    }

    return oldest_state;
  }

  mutable std::vector<ControlState> m_cached_states;
};

// usage: average(input, frames)
class AverageExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, frames"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Returns the average of the input over the specified number of frames");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    // Note that if you increase this at runtime, we won't have the corresponding values
    const size_t frames = size_t(std::max(GetArg(1).GetValue() + 0.5, 1.0));

    m_cached_states.reserve(frames + 1);

    m_cached_states.insert(m_cached_states.begin(), input);
    while (m_cached_states.size() > frames)
    {
      m_cached_states.pop_back();
    }

    ControlState sum = 0.0;
    for (ControlState cached_state : m_cached_states)
    {
      sum += cached_state;
    }

    return sum / std::min(m_cached_states.size(), frames);
  }

  mutable std::vector<ControlState> m_cached_states;
};

// usage: sum(input, frames)
// E.g. GC SerialInterface updates at twice the video refresh rate of the game, it's very unlikely
// that games will consider/read both inputs so we sum the last two to not miss values
class SumExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, frames"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Returns the sum of the input over the specified number of frames. Use "
                  "this on relative inputs to avoid losing values not read by the game");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    // Note that if you increase this at runtime, we won't have the corresponding values
    const size_t frames = size_t(std::max(GetArg(1).GetValue() + 0.5, 1.0));

    m_cached_states.reserve(frames + 1);

    m_cached_states.insert(m_cached_states.begin(), input);
    while (m_cached_states.size() > frames)
    {
      m_cached_states.pop_back();
    }

    ControlState sum = 0.0;
    for (ControlState cached_state : m_cached_states)
    {
      sum += cached_state;
    }
    return sum;
  }

  mutable std::vector<ControlState> m_cached_states;
};

// usage: timer(seconds, reset = false)
class TimerExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1 || args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"seconds, reset = false"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Returns the progress to reach the specified time (you can change it on the go "
                  "to change the progress speed). Loops around once reached");
  }

  ControlState GetValue() const override
  {
    const ControlState seconds = GetArg(0).GetValue();
    const bool reset = GetArgCount() == 2 ? (GetArg(1).GetValue() > CONDITION_THRESHOLD) : false;

    if (reset)
    {
      m_progress = 0.0;
    }

    if (seconds <= 0.0)
    {
      // User configured a non-positive timer. Reset the timer and return 0.
      m_progress = 0.0;
      m_has_started = false;
      return m_progress;
    }

    // The very first frame needs to return 0
    if (m_has_started)
      m_progress += ControllerInterface::GetCurrentInputDeltaSeconds() / seconds;

    if (m_progress > 1.0)
    {
      const double reset_count = std::floor(m_progress);
      m_progress -= reset_count;
    }
    else if (m_progress <= 0.0)
    {
      m_has_started = true;
    }

    return m_progress;
  }

  mutable bool m_has_started = false;
  mutable double m_progress = 0.0;
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Goes against a game built in deadzone, like if it will treat 0.2 (amount) as "
                  "0, but will treat 0.3 as 0.125. It should be applied after deadzone");
  }

  ControlState GetValue() const override
  {
    const ControlState val = GetArg(0).GetValue();
    const ControlState anti_deadzone = GetArg(1).GetValue();
    // We don't want to return a min of anti_deadzone if our val is 0.
    // It fixes the problem where 2 opposite axes (e.g. R stick X+ and R stick Y-) have the same
    // anti deadzone expression so they would cancel each other out if we didn't check for this.
    if (val == 0.0)
      return 0;
    const ControlState abs_val = std::abs(val);
    return std::copysign(anti_deadzone + abs_val * (1.0 - anti_deadzone), val);
  }
};

// usage: bezierCurve(input, x1, x2)
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("2 control points bezier. Basically just a fancy \"remap\"in one dimension. "
                  "Useful to go against games analog stick response curve when using the"
                  "mouse as as axis, or when games have a linear response curve to an analog stick"
                  " and you'd like to change it. Mostly used on the x axis (left/right)");
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

// usage: acceleration(input, max_acc_time, acc, min_time_input, min_acc_input, max_acc_input = 1)
// The way this work is by "predicting" the acceleration the game would have with time, and
// increasing the input value so you won't feel any acceleration, the drawback is that you might see
// some velocity snapping, and this doesn't work when passing in a maxed out input of course. An
// alternative approach would be to actually try to shrink the input as time passed, to counteract
// the acceleration, but that would strongly limit the max output. Use after AntiDeadzone and
// BezierCurve. In some games acceleration is only on the X axis.
class AntiAccelerationExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 5 || args.size() == 6)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, max_acc_time, acc, min_time_input, "
                               "min_acc_input, max_acc_input = 1"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans(
        "Useful when using the mouse as an axis and you don't want the game to gradually "
        "accelerate its response with time. It should help in keeping a response curve closer to "
        "1:1 (linear)");
  }

  ControlState GetValue() const override
  {
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
      return val;
    }

    const ControlState diff = max_acc_input - min_acc_input;
    const ControlState acc_alpha =
        diff != 0.0 ? std::clamp((abs_val - min_acc_input) / diff, 0.0, 1.0) : 1.0;
    const ControlState time_alpha = std::min((max_acc_time - m_elapsed) / max_acc_time, 0.0);

    const ControlState multiplier = MathUtil::Lerp(1.0, acc * acc_alpha, time_alpha * time_alpha);

    if (abs_val >= min_time_input)
      m_elapsed += ControllerInterface::GetCurrentInputDeltaSeconds();
    else
      m_elapsed = 0.0;

    return std::copysign(abs_val * multiplier, val);
  }

  mutable double m_elapsed = 0.0;
};

// usage: smooth(input, seconds_up, seconds_down = seconds_up)
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Gradually moves its internal value toward its target value (input). It asks for "
                  "the times to go between 0 and 1. Can be used to simulate a relative axis");
  }

  ControlState GetValue() const override
  {
    const ControlState desired_value = GetArg(0).GetValue();

    const ControlState smooth_up = GetArg(1).GetValue();
    const ControlState smooth_down = GetArgCount() == 3 ? GetArg(2).GetValue() : smooth_up;

    const ControlState smooth = (desired_value < m_state) ? smooth_down : smooth_up;
    const ControlState max_move = ControllerInterface::GetCurrentInputDeltaSeconds() / smooth;

    if (std::isinf(max_move))
    {
      m_state = desired_value;
    }
    else
    {
      const ControlState diff = desired_value - m_state;
      m_state += std::copysign(std::min(max_move, std::abs(diff)), diff);
    }

    return m_state;
  }

  mutable ControlState m_state = 0;
};

// usage: toggle(input, [clear])
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

  const char* GetDescription(bool for_input) const override { return _trans("Toggle on press"); }

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
      m_state = false;

    return m_state;
  }

  mutable bool m_pressed = false;
  mutable bool m_state = false;
};

// usage: onPress(input, duration_frames = 1)
class OnPressExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, duration_frames = 1"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return "Returns 1 when the input has been pressed, for "
           "the specified number of frames";
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const u32 duration_frames =
        GetArgCount() >= 2 ? u32(std::max(GetArg(1).GetValue() + 0.5, 0.0)) : 1;

    if (m_frames_left > 0)
      m_frames_left--;

    const bool was_pressed = m_pressed;
    m_pressed = input > CONDITION_THRESHOLD;

    // Start again on every new press, don't restart on release
    if (!was_pressed && m_pressed)
      m_frames_left = duration_frames;

    return m_frames_left > 0;
  }

  mutable bool m_pressed = false;
  mutable u32 m_frames_left = 0;
};

// usage: onRelease(input, duration_frames = 1)
class OnReleaseExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, duration_frames = 1"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return "Returns 1 when the input has been released, for "
           "the specified number of frames";
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const u32 duration_frames =
        GetArgCount() >= 2 ? u32(std::max(GetArg(1).GetValue() + 0.5, 0.0)) : 1;

    if (m_frames_left > 0)
      m_frames_left--;

    const bool was_pressed = m_pressed;
    m_pressed = input > CONDITION_THRESHOLD;

    // Start again on every new release, don't restart on press
    if (was_pressed && !m_pressed)
      m_frames_left = duration_frames;

    return m_frames_left > 0;
  }

  mutable bool m_pressed = false;
  mutable u32 m_frames_left = 0;
};

// usage: onChange(input, duration_frames = 1)
class OnChangeExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, duration_frames = 1"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return "Returns 1 when the input detection threshold has changed from the previous value, for "
           "the specified number of frames";
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const u32 duration_frames =
        GetArgCount() >= 2 ? u32(std::max(GetArg(1).GetValue() + 0.5, 0.0)) : 1;

    if (m_frames_left > 0)
      m_frames_left--;

    const bool was_pressed = m_pressed;
    m_pressed = input > CONDITION_THRESHOLD;

    // Start again on every new press or release
    if (was_pressed != m_pressed)
      m_frames_left = duration_frames;

    return m_frames_left > 0;
  }

  mutable bool m_pressed = false;
  mutable u32 m_frames_left = 0;
};

// usage: cache(expression, condition)
// Note that resetting/releasing the condition input does not reset the cached value
class CacheExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"expression, condition"};
  }

  const char* GetDescription(bool for_input) const override
  {
    if (for_input)
    {
      return _trans("Caches and returns the input when the condition is true, returns the "
                    "cached value when the condition is false");
    }
    return _trans("Sets and caches the value when the condition is true, keeps settings the cached "
                  "value when the condition is false");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const ControlState condition = GetArg(1).GetValue();

    if (condition > CONDITION_THRESHOLD)
      m_state = input;

    return m_state;
  }

  void SetValue(ControlState value) override
  {
    const ControlState condition = GetArg(1).GetValue();

    if (condition > CONDITION_THRESHOLD)
      m_state = value;

    GetArg(0).SetValue(m_state);
  }

  mutable ControlState m_state = 0;
};

// usage: onHold(input, seconds)
// Use real world time for this one as it makes more sense
class OnHoldExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Returns true after being held for the specified amount of time, until released");
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const ControlState input = GetArg(0).GetValue();
    const ControlState seconds = GetArg(1).GetValue();

    if (input <= CONDITION_THRESHOLD)
    {
      m_state = false;
      m_start_time = Clock::now();
    }
    else if (!m_state)
    {
      const auto hold_time = now - m_start_time;

      if (std::chrono::duration_cast<FSec>(hold_time).count() >= seconds)
        m_state = true;
    }

    return m_state;
  }

  mutable bool m_state = false;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: onTap(input, seconds, taps = 2)
// Use real world time for this one as it makes more sense.
// It would be nice to add a setting to restart the detection time on every tap,
// for more flexibility
class OnTapExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Double+ tap detection within the specified time from the first tap. Keeps "
                  "returning 1 until you release the last tap");
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();

    const auto elapsed = std::chrono::duration_cast<FSec>(now - m_start_time).count();

    const ControlState input = GetArg(0).GetValue();
    const ControlState seconds = GetArg(1).GetValue();

    const bool is_time_up = elapsed >= seconds;

    const u32 desired_taps =
        GetArgCount() == 3 ? u32(std::max(GetArg(2).GetValue() + 0.5, 0.0)) : 2;

    if (input <= CONDITION_THRESHOLD)
    {
      m_released = true;

      // Reset taps if enough time has passed from the first one
      if (m_taps > 0 && is_time_up)
        m_taps = 0;

      return false;
    }

    if (m_released)
    {
      if (m_taps == 0)
        m_start_time = now;

      ++m_taps;
      m_released = false;
    }

    return desired_taps == m_taps;
  }

  mutable bool m_released = true;
  mutable u32 m_taps = 0;
  mutable Clock::time_point m_start_time = Clock::now();
};

// usage: sharedRelative(input, speed = 0, max_abs_value = 1, [shared_state])
class SharedRelativeExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() >= 1 && args.size() <= 4)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, speed = 0, max_abs_value = 1, [shared_state]"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans(
        "Adds the value of an input to a state, multiplied by a speed per second if it's > 0. You"
        " can add a shared state with $unique_name to link functions from two opposite mappings");
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
    //
    // Note that this won't work if directly mapped to a single button as it won't go back.

    const ControlState input = GetArg(0).GetValue();
    const ControlState speed = (GetArgCount() >= 2) ? GetArg(1).GetValue() : 0.0;

    const ControlState max_abs_value = (GetArgCount() >= 3) ? GetArg(2).GetValue() : 1.0;

    // This will likely (hopefully) be a VariableExpression.
    if (GetArgCount() >= 4)
      m_state = GetArg(3).GetValue();

    const ControlState max_move =
        input * (speed > 0.f ? (ControllerInterface::GetCurrentInputDeltaSeconds() * speed) : 1.0);
    const ControlState diff_from_zero = std::abs(m_state);
    const ControlState diff_from_max = std::abs(max_abs_value - m_state);

    m_state += std::min(std::max(max_move, -diff_from_zero), diff_from_max) *
               std::copysign(1.0, max_abs_value);

    // Break const correctness but GetValue() is kind of meant like an update method now
    if (GetArgCount() >= 4)
      const_cast<Expression&>(GetArg(3)).SetValue(m_state);

    return std::max(0.0, m_state * std::copysign(1.0, max_abs_value));
  }

  mutable ControlState m_state = 0;
};

// usage: relativeToSpeed(input)
// Input polling is not done at irregular intervals, both in real/world time,
// and in emulation time (irregular only on the SI).
// Meaning that relative axis movement would be vastly different between input frames,
// so we need to normalize it.
// Note that sometimes 2 input updates are so close to each other that relative axes
// might not have had time to move precisely, so our output won't be linear with their speed.
class RelativeToSpeedExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Turns a relative axis into a rate of change (speed). Can be used to map a "
                  "mouse axis to an analog stick for example");
  }

  ControlState GetValue() const override
  {
    const ControlState base_value = GetArg(0).GetValue();
    // This normalizes the values by:
    // -the video refresh rate (50 or 60)
    // -the emulation target speed
    // -the emulation achieved speed
    // -the real time oscillations between input polling
    return base_value *
           (ControllerInterface::GetTargetInputDeltaSeconds() /
            ControllerInterface::GetCurrentRealInputDeltaSeconds()) /
           double(ControllerInterface::GetInputUpdatesPerTarget());
  }
};

// usage: pulse(input, seconds, accumulate = true)
class PulseExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2 || args.size() == 3)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"input, seconds, accumulate = true"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Keeps the value true for \"seconds\" (increasingly if asked) every time the "
                  "input starts being true");
  }

  ControlState GetValue() const override
  {
    const ControlState input = GetArg(0).GetValue();
    const ControlState seconds = GetArg(1).GetValue();
    const bool accumulate =
        GetArgCount() == 3 ? (GetArg(2).GetValue() > CONDITION_THRESHOLD) : true;

    if (input <= CONDITION_THRESHOLD)
    {
      m_released = true;
    }
    else if (m_released)
    {
      m_released = false;

      if (m_state)
      {
        if (accumulate)
          m_release_time += seconds;
        else
          m_release_time = seconds;
      }
      else
      {
        m_state = true;
        m_release_time = seconds;
      }
    }

    if (m_state && m_release_time <= 0.0)
    {
      m_state = false;
    }
    m_release_time =
        std::max(0.0, m_release_time - ControllerInterface::GetCurrentInputDeltaSeconds());

    return m_state;
  }

  mutable bool m_released = false;
  mutable bool m_state = false;
  mutable double m_release_time = 0.0;
};

// usage: ignoreFocus(expression)
class IgnoreFocusExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Forces the focus requirements of the expression to be ignored");
  }

  // No need to keep the inner expression flags as they'd be ignored anyway
  Device::FocusFlags GetFocusFlags() const override { return Device::FocusFlags::IgnoreFocus; }

  ControlState GetValue() const override { return GetArg(0).GetValue(); }
};

// usage: ignoreOnFocusChange(expression)
class IgnoreOnFocusChangeExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Forces the expression to be ignored until on focus acquired");
  }

  Device::FocusFlags GetFocusFlags() const override
  {
    // Add the required flag, keep the previous ones, as long as they don't conflict
    u8 focus_flags = u8(FunctionExpression::GetFocusFlags());
    focus_flags &= ~u8(Device::FocusFlags::IgnoreFocus);
    return Device::FocusFlags(focus_flags | u8(Device::FocusFlags::IgnoreOnFocusChanged));
  }

  ControlState GetValue() const override { return GetArg(0).GetValue(); }
};

// usage: requireFocus(expression)
// This already enforces check for Full Focus, we shouldn't need one for Focus only.
class RequireFocusExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Forces focus to be checked on the expression. This is mostly needed for "
                  "outputs which ignore focus by default");
  }

  Device::FocusFlags GetFocusFlags() const override
  {
    // Add the required flag, keep the previous ones, as long as they don't conflict
    u8 focus_flags = u8(FunctionExpression::GetFocusFlags());
    focus_flags &= ~u8(Device::FocusFlags::IgnoreFocus);
    return Device::FocusFlags(focus_flags | u8(Device::FocusFlags::RequireFullFocus));
  }

  ControlState GetValue() const override { return GetArg(0).GetValue(); }
};

// usage: (return value)gameSpeed()
// We don't want this to be used as control so we don't return 1 to CountNumControls()
class GameSpeedExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 0)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"none"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Returns the game speed from the last frame (1 is full speed). Can be used "
                  "similarly to a time delta");
  }

  ControlState GetValue() const override
  {
    if ((ControllerInterface::GetCurrentInputChannel() != InputChannel::SerialInterface &&
         ControllerInterface::GetCurrentInputChannel() != InputChannel::Bluetooth) ||
        ::Core::GetState() == ::Core::State::Uninitialized)
      return 1;
    return ::Core::GetActualEmulationSpeed();
  }
};

// usage: timeToInputFrames(seconds)
class TimeToInputFramesExpression : public FunctionExpression
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

  const char* GetDescription(bool for_input) const override
  {
    return _trans("If you don't want to deal with input update calculations you can use "
                  "this function to convert seconds to input frames. You can't preview "
                  "values when the emulation is not running");
  }

  ControlState GetValue() const override
  {
    return GetArg(0).GetValue() / ControllerInterface::GetTargetInputDeltaSeconds();
  }
};

// usage: videoToInputFrames(frames)
class VideoToInputFramesExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 1)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"frames"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans(
        "Input isn't updated with the same frequency as the video, so some functions ask "
        "for a duration in input frames. Use this to convert to it as not all input frames are "
        "guaranteed to be read by the game. The actual game frame rate won't influence this, only "
        "the video refresh rate will. You can't preview values when the emulation is not running."
        " Ignored for non game mappings");
  }

  ControlState GetValue() const override
  {
    if ((ControllerInterface::GetCurrentInputChannel() != InputChannel::SerialInterface &&
         ControllerInterface::GetCurrentInputChannel() != InputChannel::Bluetooth) ||
        ::Core::GetState() == ::Core::State::Uninitialized)
      return GetArg(0).GetValue();
    return GetArg(0).GetValue() * (ControllerInterface::GetTargetInputDeltaSeconds() *
                                   VideoInterface::GetTargetRefreshRate());
  }
};

// usage: (return value)hasFocus()
// We don't want this to be used as control so we don't return 1 to CountNumControls()
class HasFocusExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 0)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"none"};
  }

  const char* GetDescription(bool for_input) const override
  {
    return _trans("Can be used to cache inputs values on focus loss, or to trigger inputs the "
                  "window loses focus, like pausing the game pause or changing the emulation speed."
                  " Only applies to the emulation window");
  }

  Device::FocusFlags GetFocusFlags() const override { return Device::FocusFlags::IgnoreFocus; }

  ControlState GetValue() const override
  {
    // Ignore this for non game channels, it's not necessary (also couldn't be tested in the UI)
    if (ControllerInterface::GetCurrentInputChannel() != InputChannel::SerialInterface &&
        ControllerInterface::GetCurrentInputChannel() != InputChannel::Bluetooth)
      return true;
    // There is no need to cache this, nor to return the control gate cached version of focus,
    // we want this to be as up to date as possible
    return Host_RendererHasFocus();
  }
};

// usage: scaleSet(output, scale)
class ScaleSetExpression : public FunctionExpression
{
private:
  ArgumentValidation
  ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    if (args.size() == 2)
      return ArgumentsAreValid{};
    else
      return ExpectedArguments{"output, scale"};
  }

  const char* GetDescription(bool for_input) const override
  {
    if (for_input)
      return nullptr;
    return _trans("Scales an output value");
  }

  void SetValue(ControlState value) override { GetArg(0).SetValue(value * GetArg(1).GetValue()); }

  // Just return a multiplication if this was accidentally used on inputs
  ControlState GetValue() const override { return GetArg(0).GetValue() * GetArg(1).GetValue(); }
};

// Where there are two names it's to maintain compatibility with previous names
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
  if (name == "cache")  // Output as well
    return std::make_unique<CacheExpression>();
  if (name == "onHold" || name == "hold")
    return std::make_unique<OnHoldExpression>();
  if (name == "toggle")
    return std::make_unique<ToggleExpression>();
  if (name == "onTap" || name == "tap")
    return std::make_unique<OnTapExpression>();
  // It would be nice to remove the old name "relative" from here but we'd break existing functions
  if (name == "sharedRelative" || name == "relative")
    return std::make_unique<SharedRelativeExpression>();
  if (name == "relativeToSpeed")
    return std::make_unique<RelativeToSpeedExpression>();
  if (name == "smooth" || name == "toRelative")
    return std::make_unique<SmoothExpression>();
  if (name == "pulse")
    return std::make_unique<PulseExpression>();
  if (name == "timer")
    return std::make_unique<TimerExpression>();
  if (name == "interval")
    return std::make_unique<IntervalExpression>();
  // Recordings:
  if (name == "average")
    return std::make_unique<AverageExpression>();
  if (name == "sum")
    return std::make_unique<SumExpression>();
  if (name == "record")
    return std::make_unique<RecordExpression>();
  if (name == "sequence")
    return std::make_unique<SequenceExpression>();
  if (name == "lag")
    return std::make_unique<LagExpression>();
  // Stick helpers:
  if (name == "deadzone")
    return std::make_unique<DeadzoneExpression>();
  if (name == "antiDeadzone")
    return std::make_unique<AntiDeadzoneExpression>();
  if (name == "bezierCurve")
    return std::make_unique<BezierCurveExpression>();
  if (name == "antiAcceleration")
    return std::make_unique<AntiAccelerationExpression>();
  // Meta/Focus:
  if (name == "gameSpeed")
    return std::make_unique<GameSpeedExpression>();
  if (name == "timeToInputFrames")
    return std::make_unique<TimeToInputFramesExpression>();
  if (name == "videoToInputFrames")
    return std::make_unique<VideoToInputFramesExpression>();
  if (name == "hasFocus")
    return std::make_unique<HasFocusExpression>();
  if (name == "ignoreFocus")
    return std::make_unique<IgnoreFocusExpression>();
  if (name == "ignoreOnFocusChange")
    return std::make_unique<IgnoreOnFocusChangeExpression>();
  if (name == "requireFocus")
    return std::make_unique<RequireFocusExpression>();
  // Setter/Output:
  if (name == "scaleSet")
    return std::make_unique<ScaleSetExpression>();

  return nullptr;
}

int FunctionExpression::CountNumControls() const
{
  int result = 0;

  for (auto& arg : m_args)
    result += arg->CountNumControls();

  return result;
}

Device::FocusFlags FunctionExpression::GetFocusFlags() const
{
  u8 focus_flags = 0;
  if (m_args.size() == 0)
    focus_flags = u8(Device::FocusFlags::Default);
  // By default functions sum up all the focus requirements of their args
  for (u32 i = 0; i < u32(m_args.size()); ++i)
    focus_flags |= u8(GetArg(i).GetFocusFlags());
  return Device::FocusFlags(focus_flags);
}

void FunctionExpression::UpdateReferences(ControlEnvironment& env, bool is_input)
{
  for (auto& arg : m_args)
    arg->UpdateReferences(env, is_input);
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

void FunctionExpression::SetValue(ControlState value)
{
  // By default just pass along all the values and ignore the function as
  // the expression will build fine even if it's an output reference
  for (auto& arg : m_args)
  {
    arg->SetValue(value);
  }
}
}  // namespace ciface::ExpressionParser
