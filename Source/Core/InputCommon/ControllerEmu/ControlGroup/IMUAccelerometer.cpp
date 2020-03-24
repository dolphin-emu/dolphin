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
  AddInput(Translate, _trans("Up"));
  AddInput(Translate, _trans("Down"));
  AddInput(Translate, _trans("Left"));
  AddInput(Translate, _trans("Right"));
  AddInput(Translate, _trans("Forward"));
  AddInput(Translate, _trans("Backward"));
}

std::optional<IMUAccelerometer::StateData> IMUAccelerometer::GetState() const
{
  if (controls[0]->control_ref->BoundCount() == 0)
    return std::nullopt;

  StateData state;
  state.x = (controls[2]->GetState() - controls[3]->GetState());
  state.y = (controls[5]->GetState() - controls[4]->GetState());
  state.z = (controls[0]->GetState() - controls[1]->GetState());
  return state;
}

}  // namespace ControllerEmu
