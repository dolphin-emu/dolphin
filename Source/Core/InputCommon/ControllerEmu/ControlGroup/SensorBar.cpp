// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/SensorBar.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Control/Input.h"

namespace ControllerEmu
{
SensorBar::SensorBar(std::string name_, std::string ui_name_)
    : ControlGroup(std::move(name_), std::move(ui_name_), GroupType::SensorBar,
      ControlGroup::DefaultValue::Enabled)
{
  // First point
  AddInput(Translate, "Point 1 Size");
  AddInput(Translate, "Point 1 Up");
  AddInput(Translate, "Point 1 Down");
  AddInput(Translate, "Point 1 Left");
  AddInput(Translate, "Point 1 Right");

  // Second point
  AddInput(Translate, "Point 2 Size");
  AddInput(Translate, "Point 2 Up");
  AddInput(Translate, "Point 2 Down");
  AddInput(Translate, "Point 2 Left");
  AddInput(Translate, "Point 2 Right");

}

SensorBar::StateData SensorBar::GetState()
{
  StateData state;
  state.y1 = controls[2]->GetState() - controls[1]->GetState();
  state.x1 = controls[4]->GetState() - controls[3]->GetState();
  state.size1 = std::clamp(controls[0]->GetState(), 0.0, 1.0);
  state.y2 = controls[7]->GetState() - controls[6]->GetState();
  state.x2 = controls[9]->GetState() - controls[8]->GetState();
  state.size2 = std::clamp(controls[5]->GetState(), 0.0, 1.0);

  return state;
}
}  // namespace ControllerEmu
