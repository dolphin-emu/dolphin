// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

  bool AreInputsBound() const;
};
}  // namespace ControllerEmu
