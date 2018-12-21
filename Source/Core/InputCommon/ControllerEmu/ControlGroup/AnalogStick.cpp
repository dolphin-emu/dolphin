// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"

#include <cmath>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
AnalogStick::AnalogStick(const char* const name_, std::unique_ptr<StickGate>&& stick_gate)
    : AnalogStick(name_, name_, std::move(stick_gate))
{
}

AnalogStick::AnalogStick(const char* const name_, const char* const ui_name_,
                         std::unique_ptr<StickGate>&& stick_gate)
    : ControlGroup(name_, ui_name_, GroupType::Stick), m_stick_gate(std::move(stick_gate))
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(Translate, named_direction));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Modifier")));

  // Set default input radius to that of the gate radius (no resizing)
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Input Radius"), GetGateRadiusAtAngle(0.0), 0, 100));
  // Set default input shape to an octagon (no reshaping)
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Input Shape"), 0.0, 0, 50));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

AnalogStick::StateData AnalogStick::GetState(bool adjusted)
{
  ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  // TODO: make the AtAngle functions work with negative angles:
  const ControlState ang = std::atan2(y, x) + MathUtil::TAU;

  const ControlState gate_max_dist = GetGateRadiusAtAngle(ang);
  const ControlState input_max_dist = GetInputRadiusAtAngle(ang);

  // If input radius is zero we apply no scaling.
  // This is useful when mapping native controllers without knowing intimate radius details.
  const ControlState max_dist = input_max_dist ? input_max_dist : gate_max_dist;

  ControlState dist = std::sqrt(x * x + y * y) / max_dist;

  // If the modifier is pressed, scale the distance by the modifier's value.
  // This is affected by the modifier's "range" setting which defaults to 50%.
  const ControlState modifier = controls[4]->control_ref->State();
  if (modifier)
  {
    // TODO: Modifier's range setting gets reset to 100% when the clear button is clicked.
    // This causes the modifier to not behave how a user might suspect.
    // Retaining the old scale-by-50% behavior until range is fixed to clear to 50%.
    dist *= 0.5;
    // dist *= modifier;
  }

  // Apply deadzone as a percentage of the user-defined radius/shape:
  const ControlState deadzone = GetDeadzoneRadiusAtAngle(ang);
  dist = std::max(0.0, dist - deadzone) / (1.0 - deadzone);

  // Scale to the gate shape/radius:
  dist = dist *= gate_max_dist;

  x = MathUtil::Clamp(std::cos(ang) * dist, -1.0, 1.0);
  y = MathUtil::Clamp(std::sin(ang) * dist, -1.0, 1.0);
  return {x, y};
}

ControlState AnalogStick::GetGateRadiusAtAngle(double ang) const
{
  return m_stick_gate->GetRadiusAtAngle(ang);
}

ControlState AnalogStick::GetDeadzoneRadiusAtAngle(double ang) const
{
  return CalculateInputShapeRadiusAtAngle(ang) * numeric_settings[SETTING_DEADZONE]->GetValue();
}

ControlState AnalogStick::GetInputRadiusAtAngle(double ang) const
{
  return CalculateInputShapeRadiusAtAngle(ang) * numeric_settings[SETTING_INPUT_RADIUS]->GetValue();
}

ControlState AnalogStick::CalculateInputShapeRadiusAtAngle(double ang) const
{
  const auto shape = numeric_settings[SETTING_INPUT_SHAPE]->GetValue() * 4.0;

  if (shape < 1.0)
  {
    // Between 0 and 25 return a shape between octagon and circle
    const auto amt = shape;
    return OctagonStickGate(1).GetRadiusAtAngle(ang) * (1 - amt) + amt;
  }
  else
  {
    // Between 25 and 50 return a shape between circle and square
    const auto amt = shape - 1.0;
    return (1 - amt) + SquareStickGate(1).GetRadiusAtAngle(ang) * amt;
  }
}

OctagonAnalogStick::OctagonAnalogStick(const char* name, ControlState gate_radius)
    : OctagonAnalogStick(name, name, gate_radius)
{
}

OctagonAnalogStick::OctagonAnalogStick(const char* name, const char* ui_name,
                                       ControlState gate_radius)
    : AnalogStick(name, ui_name, std::make_unique<ControllerEmu::OctagonStickGate>(gate_radius))
{
}

}  // namespace ControllerEmu
