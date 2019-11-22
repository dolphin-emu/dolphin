// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
class IMUAccelerometer : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  IMUAccelerometer(std::string name, std::string ui_name);

  std::optional<StateData> GetState() const;
};
}  // namespace ControllerEmu
