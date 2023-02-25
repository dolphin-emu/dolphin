// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"

#include <cmath>
#include <optional>

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
    : ReshapableInput(name_, ui_name_, GroupType::Stick), m_stick_gate(std::move(stick_gate))
{
  for (auto& named_direction : named_directions)
    AddInput(Translate, named_direction);

  AddInput(Translate, _trans("Modifier"));
}

AnalogStick::ReshapeData AnalogStick::GetReshapableState(bool adjusted) const
{
  const ControlState y = controls[0]->GetState() - controls[1]->GetState();
  const ControlState x = controls[3]->GetState() - controls[2]->GetState();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  return Reshape(x, y, GetModifierInput()->GetState());
}

AnalogStick::StateData AnalogStick::GetState() const
{
  return GetReshapableState(true);
}

AnalogStick::StateData AnalogStick::GetState(const InputOverrideFunction& override_func) const
{
  bool override_occurred = false;
  return GetState(override_func, &override_occurred);
}

AnalogStick::StateData AnalogStick::GetState(const InputOverrideFunction& override_func,
                                             bool* override_occurred) const
{
  StateData state = GetState();
  if (!override_func)
    return state;

  if (const std::optional<ControlState> x_override = override_func(name, X_INPUT_OVERRIDE, state.x))
  {
    state.x = *x_override;
    *override_occurred = true;
  }

  if (const std::optional<ControlState> y_override = override_func(name, Y_INPUT_OVERRIDE, state.y))
  {
    state.y = *y_override;
    *override_occurred = true;
  }

  return state;
}

ControlState AnalogStick::GetGateRadiusAtAngle(double ang) const
{
  return m_stick_gate->GetRadiusAtAngle(ang);
}

Control* AnalogStick::GetModifierInput() const
{
  return controls[4].get();
}

OctagonAnalogStick::OctagonAnalogStick(const char* name_, ControlState gate_radius)
    : OctagonAnalogStick(name_, name_, gate_radius)
{
}

OctagonAnalogStick::OctagonAnalogStick(const char* name_, const char* ui_name_,
                                       ControlState gate_radius)
    : AnalogStick(name_, ui_name_, std::make_unique<ControllerEmu::OctagonStickGate>(1.0))
{
  AddVirtualNotchSetting(&m_virtual_notch_setting, 45);

  AddSetting(
      &m_gate_size_setting,
      {_trans("Gate Size"),
       // i18n: The percent symbol.
       _trans("%"),
       // i18n: Refers to plastic shell of game controller (stick gate) that limits stick movements.
       _trans("Adjusts target radius of simulated stick gate."), nullptr,
       SettingVisibility::Advanced},
      gate_radius * 100, 0.01, 100);
}

ControlState OctagonAnalogStick::GetVirtualNotchSize() const
{
  return m_virtual_notch_setting.GetValue() * MathUtil::TAU / 360;
}

ControlState OctagonAnalogStick::GetGateRadiusAtAngle(double ang) const
{
  return AnalogStick::GetGateRadiusAtAngle(ang) * m_gate_size_setting.GetValue() / 100;
}

}  // namespace ControllerEmu
