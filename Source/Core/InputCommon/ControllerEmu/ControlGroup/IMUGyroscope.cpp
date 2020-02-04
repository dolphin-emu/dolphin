// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"

#include <memory>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
IMUGyroscope::IMUGyroscope(std::string name, std::string ui_name)
    : ControlGroup(std::move(name), std::move(ui_name), GroupType::IMUGyroscope)
{
  AddInput(Translate, _trans("Pitch Up"));
  AddInput(Translate, _trans("Pitch Down"));
  AddInput(Translate, _trans("Roll Left"));
  AddInput(Translate, _trans("Roll Right"));
  AddInput(Translate, _trans("Yaw Left"));
  AddInput(Translate, _trans("Yaw Right"));

  AddSetting(&m_deadzone_setting,
             {_trans("Dead Zone"),
              // i18n: "°/s" is the symbol for degrees (angular measurement) divided by seconds.
              _trans("°/s"),
              // i18n: Refers to the dead-zone setting of gyroscope input.
              _trans("Angular velocity to ignore.")},
             1, 0, 180);
}

auto IMUGyroscope::GetRawState() const -> StateData
{
  return StateData(controls[1]->GetState() - controls[0]->GetState(),
                   controls[2]->GetState() - controls[3]->GetState(),
                   controls[4]->GetState() - controls[5]->GetState());
}

std::optional<IMUGyroscope::StateData> IMUGyroscope::GetState() const
{
  if (controls[0]->control_ref->BoundCount() == 0)
    return std::nullopt;

  auto state = GetRawState();

  // Apply "deadzone".
  for (auto& c : state.data)
    c *= std::abs(c) > GetDeadzone();

  return state;
}

ControlState IMUGyroscope::GetDeadzone() const
{
  return m_deadzone_setting.GetValue() / 360 * MathUtil::TAU;
}

}  // namespace ControllerEmu
