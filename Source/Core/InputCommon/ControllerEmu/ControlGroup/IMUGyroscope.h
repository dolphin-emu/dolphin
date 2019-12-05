// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class IMUGyroscope : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  IMUGyroscope(std::string name, std::string ui_name);

  std::optional<StateData> GetState() const;
};
}  // namespace ControllerEmu
