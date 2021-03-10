#pragma once
// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class SensorBar : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  SensorBar(std::string name, std::string ui_name);

  std::optional<StateData> GetDisplacement() const;
  std::optional<StateData> GetOrientation() const;
};
}  // namespace ControllerEmu
