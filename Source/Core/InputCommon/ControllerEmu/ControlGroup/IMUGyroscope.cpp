// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"

#include <memory>

#include "Common/Common.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
IMUGyroscope::IMUGyroscope(std::string name, std::string ui_name)
    : ControlGroup(std::move(name), std::move(ui_name), GroupType::IMUGyroscope)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Pitch Up")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Pitch Down")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Roll Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Roll Right")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Yaw Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Yaw Right")));
}

std::optional<IMUGyroscope::StateData> IMUGyroscope::GetState() const
{
  StateData state;
  state.x = (controls[1]->control_ref->State() - controls[0]->control_ref->State());
  state.y = (controls[2]->control_ref->State() - controls[3]->control_ref->State());
  state.z = (controls[4]->control_ref->State() - controls[5]->control_ref->State());

  if (controls[0]->control_ref->BoundCount() != 0)
    return state;
  else
    return std::nullopt;
}

}  // namespace ControllerEmu
