// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"

#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
IMUCursor::IMUCursor(std::string name, std::string ui_name)
    : ControlGroup(std::move(name), std::move(ui_name), GroupType::IMUCursor,
                   ControlGroup::CanBeDisabled::Yes)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Recenter")));

  // Default values are optimized for "Super Mario Galaxy 2".
  // This seems to be acceptable for a good number of games.

  AddSetting(&m_yaw_setting,
             // i18n: Refers to an amount of rotational movement about the "yaw" axis.
             {_trans("Total Yaw"),
              // i18n: The symbol/abbreviation for degrees (unit of angular measure).
              _trans("°"),
              // i18n: Refers to emulated wii remote movements.
              _trans("Total rotation about the yaw axis.")},
             15, 0, 360);
}

ControlState IMUCursor::GetTotalYaw() const
{
  return m_yaw_setting.GetValue() * MathUtil::TAU / 360;
}

}  // namespace ControllerEmu
