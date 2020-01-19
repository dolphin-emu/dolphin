// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/IMUAccelerometer.h"

#include <memory>

#include "Common/Common.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
IMUAccelerometer::IMUAccelerometer(std::string name, std::string ui_name)
    : ControlGroup(std::move(name), std::move(ui_name), GroupType::IMUAccelerometer)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Up")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Down")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));
}

std::optional<IMUAccelerometer::StateData> IMUAccelerometer::GetState() const
{
  if (controls[0]->control_ref->BoundCount() == 0)
    return std::nullopt;

  StateData state;
  state.x = (controls[2]->control_ref->State() - controls[3]->control_ref->State());
  state.y = (controls[5]->control_ref->State() - controls[4]->control_ref->State());
  state.z = (controls[0]->control_ref->State() - controls[1]->control_ref->State());
  return state;
}

}  // namespace ControllerEmu
