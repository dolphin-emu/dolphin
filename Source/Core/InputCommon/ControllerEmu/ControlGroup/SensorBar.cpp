// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/SensorBar.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
SensorBar::SensorBar(std::string name_, std::string ui_name_)
    : ControlGroup(std::move(name_), std::move(ui_name_), GroupType::SensorBar,
#ifdef ANDROID
          // Enabling this on Android devices which have an accelerometer and gyroscope prevents
          // touch controls from being used for pointing, and touch controls generally work better
          ControlGroup::DefaultValue::Disabled)
#else
          // Most users probably won't have the setup for this
          ControlGroup::DefaultValue::Disabled)
#endif
{
  AddInput(Translate, _trans("Displacement X"));
  AddInput(Translate, _trans("Displacement Y"));
  AddInput(Translate, _trans("Displacement Z"));
  AddInput(Translate, _trans("Pitch"));
  AddInput(Translate, _trans("Roll"));
  AddInput(Translate, _trans("Yaw"));
}

auto SensorBar::GetDisplacement() const -> StateData
{
  return StateData(controls[0]->GetState(), controls[1]->GetState(), controls[2]->GetState());
}

auto SensorBar::GetOrientation() const -> StateData
{
  return StateData(controls[3]->GetState(), controls[4]->GetState(), controls[5]->GetState());
}

}  // namespace ControllerEmu
