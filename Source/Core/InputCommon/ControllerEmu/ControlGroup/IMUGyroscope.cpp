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
  AddInput(Translate, _trans("Pitch Up"));
  AddInput(Translate, _trans("Pitch Down"));
  AddInput(Translate, _trans("Roll Left"));
  AddInput(Translate, _trans("Roll Right"));
  AddInput(Translate, _trans("Yaw Left"));
  AddInput(Translate, _trans("Yaw Right"));
}

std::optional<IMUGyroscope::StateData> IMUGyroscope::GetState() const
{
  if (controls[0]->control_ref->BoundCount() == 0)
    return std::nullopt;

  StateData state;
  state.x = (controls[1]->GetState() - controls[0]->GetState());
  state.y = (controls[2]->GetState() - controls[3]->GetState());
  state.z = (controls[4]->GetState() - controls[5]->GetState());
  return state;
}

}  // namespace ControllerEmu
