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
    : ReshapableInput(name_, ui_name_, GroupType::Stick), m_stick_gate(std::move(stick_gate))
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(Translate, named_direction));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Modifier")));
}

AnalogStick::ReshapeData AnalogStick::GetReshapableState(bool adjusted)
{
  const ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  const ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  const ControlState modifier = controls[4]->control_ref->State();

  return Reshape(x, y, modifier);
}

AnalogStick::StateData AnalogStick::GetState()
{
  return GetReshapableState(true);
}

ControlState AnalogStick::GetGateRadiusAtAngle(double ang) const
{
  return m_stick_gate->GetRadiusAtAngle(ang);
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
