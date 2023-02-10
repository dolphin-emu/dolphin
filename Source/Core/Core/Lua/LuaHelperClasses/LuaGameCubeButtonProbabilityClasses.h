#pragma once

#include <random>
#include <vector>

#include "Core/Movie.h"
#include "LuaGCButtons.h"

class LuaGameCubeButtonProbabilityEvent
{
public:
  static std::mt19937_64 num_generator_64_bits;
  static void SetRngSeeding();

  virtual void ApplyProbability(Movie::ControllerState& controller_state) = 0;
  virtual ~LuaGameCubeButtonProbabilityEvent() {}
  void CheckIfEventHappened(double probability)
  {
    if (probability >= 100.0)
      event_happened = true;
    else if (probability <= 0.0)
      event_happened = false;
    else
      event_happened =
           ((probability/100.0) >= (static_cast<double>(num_generator_64_bits()) /
                                    static_cast<double>(num_generator_64_bits.max())));
  }

  bool event_happened;
  GcButtonName button_effected;
};

class LuaGCButtonFlipProbabilityEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonFlipProbabilityEvent(double probability, GcButtonName new_button_effected)
  {
    button_effected = new_button_effected;
    CheckIfEventHappened(probability);
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    switch (button_effected)
    {
    case GcButtonName::A:
      controller_state.A = !controller_state.A;
      return;
    case GcButtonName::B:
      controller_state.B = !controller_state.B;
      return;
    case GcButtonName::X:
      controller_state.X = !controller_state.X;
      return;
    case GcButtonName::Y:
      controller_state.Y = !controller_state.Y;
      return;
    case GcButtonName::Z:
      controller_state.Z = !controller_state.Z;
      return;
    case GcButtonName::L:
      controller_state.L = !controller_state.L;
      return;
    case GcButtonName::R:
      controller_state.R = !controller_state.R;
      return;
    case GcButtonName::DPadUp:
      controller_state.DPadUp = !controller_state.DPadUp;
      return;
    case GcButtonName::DPadDown:
      controller_state.DPadDown = !controller_state.DPadDown;
      return;
    case GcButtonName::DPadLeft:
      controller_state.DPadLeft = !controller_state.DPadLeft;
      return;
    case GcButtonName::DPadRight:
      controller_state.DPadRight = !controller_state.DPadRight;
      return;
    case GcButtonName::Start:
      controller_state.Start = !controller_state.Start;
      return;
    case GcButtonName::Reset:
      controller_state.reset = !controller_state.reset;
      return;
    default:
      return;
    }
  }
};

class LuaGCButtonReleaseEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonReleaseEvent(double probability, GcButtonName new_button_name)
  {
    button_effected = new_button_name;
    CheckIfEventHappened(probability);
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    switch (button_effected)
    {
    case GcButtonName::A:
      controller_state.A = false;
      return;
    case GcButtonName::B:
      controller_state.B = false;
      return;
    case GcButtonName::X:
      controller_state.X = false;
      return;
    case GcButtonName::Y:
      controller_state.Y = false;
      return;
    case GcButtonName::Z:
      controller_state.Z = false;
      return;
    case GcButtonName::L:
      controller_state.L = false;
      return;
    case GcButtonName::R:
      controller_state.R = false;
      return;
    case GcButtonName::DPadUp:
      controller_state.DPadUp = false;
      return;
    case GcButtonName::DPadDown:
      controller_state.DPadDown = false;
      return;
    case GcButtonName::DPadLeft:
      controller_state.DPadLeft = false;
      return;
    case GcButtonName::DPadRight:
      controller_state.DPadRight = false;
      return;
    case GcButtonName::Start:
      controller_state.Start = false;
      return;
    case GcButtonName::Reset:
      controller_state.reset = false;
      return;
    default:
      return;
    }
  }
};

class LuaGCButtonPressEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonPressEvent(double probability, GcButtonName new_button)
  {
    button_effected = new_button;
    CheckIfEventHappened(probability);
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    switch (button_effected)
    {
    case GcButtonName::A:
      controller_state.A = true;
      return;
    case GcButtonName::B:
      controller_state.B = true;
      return;
    case GcButtonName::X:
      controller_state.X = true;
      return;
    case GcButtonName::Y:
      controller_state.Y = true;
      return;
    case GcButtonName::Z:
      controller_state.Z = true;
      return;
    case GcButtonName::L:
      controller_state.L = true;
      return;
    case GcButtonName::R:
      controller_state.R = true;
      return;
    case GcButtonName::DPadUp:
      controller_state.DPadUp = true;
      return;
    case GcButtonName::DPadDown:
      controller_state.DPadDown = true;
      return;
    case GcButtonName::DPadLeft:
      controller_state.DPadLeft = true;
      return;
    case GcButtonName::DPadRight:
      controller_state.DPadRight = true;
      return;
    case GcButtonName::Start:
      controller_state.Start = true;
      return;
    case GcButtonName::Reset:
      controller_state.reset = true;
      return;
    default:
      return;
    }
  }
};

class LuaGCAlterAnalogInputFromCurrentValue : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCAlterAnalogInputFromCurrentValue(double probability, GcButtonName new_button_name,
                                        u32 new_max_lower_difference, u32 new_max_upper_difference)
  {
    button_effected = new_button_name;
    max_lower_difference = new_max_lower_difference;
    CheckIfEventHappened(probability);
    CalculateOffsetFromLowerBound(new_max_lower_difference, new_max_upper_difference);
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    switch (button_effected)
    {
    case GcButtonName::AnalogStickX:
      controller_state.AnalogStickX = GetNewValue(controller_state.AnalogStickX);
      return;
    case GcButtonName::AnalogStickY:
      controller_state.AnalogStickY = GetNewValue(controller_state.AnalogStickY);
      return;
    case GcButtonName::CStickX:
      controller_state.CStickX = GetNewValue(controller_state.CStickX);
      return;
    case GcButtonName::CStickY:
      controller_state.CStickY = GetNewValue(controller_state.CStickY);
      return;
    case GcButtonName::TriggerL:
      controller_state.TriggerL = GetNewValue(controller_state.TriggerL);
      return;
    case GcButtonName::TriggerR:
      controller_state.TriggerR = GetNewValue(controller_state.TriggerR);
      return;
    default:
      return;
    }
  }

private:
  u32 offset_from_lower_bound;
  u8 max_lower_difference;

  void CalculateOffsetFromLowerBound(u32 lower_difference, u32 upper_difference)
  {
    if (upper_difference == 0 && lower_difference == 0)
    {
      offset_from_lower_bound = 0;
      return;
    }

    offset_from_lower_bound = (rand() % (lower_difference + upper_difference + 1));
  }

  u8 GetNewValue(u8 current_value)
  {
    int lower_bound = static_cast<int>(current_value) - static_cast<int>(max_lower_difference);
    int return_value = lower_bound + static_cast<int>(offset_from_lower_bound);
    if (return_value < 0)
      return_value = 0;
    else if (return_value > 255)
      return_value = 255;
    return static_cast<u8>(return_value);
  }
};

class LuaGCAlterAnalogInputFromFixedValue : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCAlterAnalogInputFromFixedValue(double probability, GcButtonName new_button_name, u32 value,
                                      u32 max_lower_difference, u32 max_upper_difference)
  {
    CheckIfEventHappened(probability);
    button_effected = new_button_name;
    if (max_lower_difference == 0 && max_upper_difference == 0)
      altered_button_value = static_cast<u8>(value);
    else
    {
      int lower_bound = static_cast<int>(value) - static_cast<int>(max_lower_difference);
      int temp_result =
          lower_bound +
          static_cast<int>((rand() % (max_lower_difference + max_upper_difference + 1)));
      if (temp_result < 0)
        temp_result = 0;
      if (temp_result > 255)
        temp_result = 255;
      altered_button_value = static_cast<u8>(temp_result);
    }
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    switch (button_effected)
    {
    case GcButtonName::AnalogStickX:
      controller_state.AnalogStickX = altered_button_value;
      return;
    case GcButtonName::AnalogStickY:
      controller_state.AnalogStickY = altered_button_value;
      return;
    case GcButtonName::CStickX:
      controller_state.CStickX = altered_button_value;
      return;
    case GcButtonName::CStickY:
      controller_state.CStickY = altered_button_value;
      return;
    case GcButtonName::TriggerL:
      controller_state.TriggerL = altered_button_value;
      return;
    case GcButtonName::TriggerR:
      controller_state.TriggerR = altered_button_value;
      return;
    default:
      return;
    }
  }

private:
  u8 altered_button_value;
};

class LuaGCButtonComboEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCButtonComboEvent(double probability, Movie::ControllerState new_button_presses,
                        std::vector<GcButtonName> new_buttons_to_look_at,
                        bool new_set_other_buttons_to_blank)
  {
    CheckIfEventHappened(probability);
    button_presses = new_button_presses;
    buttons_to_look_at = new_buttons_to_look_at;
    set_other_buttons_to_blank = new_set_other_buttons_to_blank;
  }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    if (set_other_buttons_to_blank)
    {
      memset(&controller_state, 0, sizeof(Movie::ControllerState));
      controller_state.is_connected = true;
      controller_state.CStickX = 128;
      controller_state.CStickY = 128;
      controller_state.AnalogStickX = 128;
      controller_state.AnalogStickY = 128;
    }

    for (size_t i = 0; i < buttons_to_look_at.size(); ++i)
    {
      switch (buttons_to_look_at[i])
      {
      case GcButtonName::A:
        controller_state.A = button_presses.A;
        break;
      case GcButtonName::B:
        controller_state.B = button_presses.B;
        break;
      case GcButtonName::X:
        controller_state.X = button_presses.X;
        break;
      case GcButtonName::Y:
        controller_state.Y = button_presses.Y;
        break;
      case GcButtonName::Z:
        controller_state.Z = button_presses.Z;
        break;
      case GcButtonName::L:
        controller_state.L = button_presses.L;
        break;
      case GcButtonName::R:
        controller_state.R = button_presses.R;
        break;
      case GcButtonName::DPadUp:
        controller_state.DPadUp = button_presses.DPadUp;
        break;
      case GcButtonName::DPadDown:
        controller_state.DPadDown = button_presses.DPadDown;
        break;
      case GcButtonName::DPadLeft:
        controller_state.DPadLeft = button_presses.DPadLeft;
        break;
      case GcButtonName::DPadRight:
        controller_state.DPadRight = button_presses.DPadRight;
        break;
      case GcButtonName::Start:
        controller_state.Start = button_presses.Start;
        break;
      case GcButtonName::Reset:
        controller_state.reset = button_presses.reset;
        break;
      case GcButtonName::TriggerL:
        controller_state.TriggerL = button_presses.TriggerL;
        break;
      case GcButtonName::TriggerR:
        controller_state.TriggerR = button_presses.TriggerR;
        break;
      case GcButtonName::AnalogStickX:
        controller_state.AnalogStickX = button_presses.AnalogStickX;
        break;
      case GcButtonName::AnalogStickY:
        controller_state.AnalogStickY = button_presses.AnalogStickY;
        break;
      case GcButtonName::CStickX:
        controller_state.CStickX = button_presses.CStickX;
        break;
      case GcButtonName::CStickY:
        controller_state.CStickY = button_presses.CStickY;
        break;
      default:
        break;
      }
    }
  }

private:
  Movie::ControllerState button_presses;
  std::vector<GcButtonName> buttons_to_look_at;
  bool set_other_buttons_to_blank;  
};

class LuaGCClearControllerEvent : public LuaGameCubeButtonProbabilityEvent
{
public:
  LuaGCClearControllerEvent(double probability) { CheckIfEventHappened(probability); }

  void ApplyProbability(Movie::ControllerState& controller_state)
  {
    if (!event_happened)
      return;

    memset(&controller_state, 0, sizeof(Movie::ControllerState));
    controller_state.is_connected = true;
    controller_state.CStickX = 128;
    controller_state.CStickY = 128;
    controller_state.AnalogStickX = 128;
    controller_state.AnalogStickY = 128;
    return;
  }
};
