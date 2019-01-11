// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/StickGate.h"

#include <cmath>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
OctagonStickGate::OctagonStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState OctagonStickGate::GetRadiusAtAngle(double ang) const
{
  constexpr int sides = 8;
  constexpr double sum_int_angles = (sides - 2) * MathUtil::PI;
  constexpr double half_int_angle = sum_int_angles / sides / 2;

  ang = std::fmod(ang, MathUtil::TAU / sides);
  // Solve ASA triangle using The Law of Sines:
  return m_radius / std::sin(MathUtil::PI - ang - half_int_angle) * std::sin(half_int_angle);
}

RoundStickGate::RoundStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState RoundStickGate::GetRadiusAtAngle(double) const
{
  return m_radius;
}

SquareStickGate::SquareStickGate(ControlState half_width) : m_half_width(half_width)
{
}

ControlState SquareStickGate::GetRadiusAtAngle(double ang) const
{
  constexpr double section_ang = MathUtil::TAU / 4;
  return m_half_width / std::cos(std::fmod(ang + section_ang / 2, section_ang) - section_ang / 2);
}

ReshapableInput::ReshapableInput(std::string name, std::string ui_name, GroupType type)
    : ControlGroup(std::move(name), std::move(ui_name), type)
{
}

ControlState ReshapableInput::GetDeadzoneRadiusAtAngle(double ang) const
{
  return CalculateInputShapeRadiusAtAngle(ang) * numeric_settings[SETTING_DEADZONE]->GetValue();
}

ControlState ReshapableInput::GetInputRadiusAtAngle(double ang) const
{
  const ControlState radius =
      CalculateInputShapeRadiusAtAngle(ang) * numeric_settings[SETTING_INPUT_RADIUS]->GetValue();
  // Clamp within the -1 to +1 square as input radius may be greater than 1.0:
  return std::min(radius, SquareStickGate(1).GetRadiusAtAngle(ang));
}

void ReshapableInput::AddReshapingSettings(ControlState default_radius, ControlState default_shape,
                                           int max_deadzone)
{
  // Allow radius greater than 1.0 for definitions of rounded squares
  // This is ideal for Xbox controllers (and probably others)
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Input Radius"), default_radius, 0, 140));
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Input Shape"), default_shape, 0, 50));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

ReshapableInput::ReshapeData ReshapableInput::Reshape(ControlState x, ControlState y,
                                                      ControlState modifier)
{
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

ControlState ReshapableInput::CalculateInputShapeRadiusAtAngle(double ang) const
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

}  // namespace ControllerEmu
