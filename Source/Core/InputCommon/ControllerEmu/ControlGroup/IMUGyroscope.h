// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "Common/MathUtil.h"
#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
class IMUGyroscope : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  IMUGyroscope(std::string name, std::string ui_name);

  StateData GetRawState() const;
  std::optional<StateData> GetState() const;

  // Value is in rad/s.
  ControlState GetDeadzone() const;

  bool IsCalibrating() const;

private:
  using Clock = std::chrono::steady_clock;

  void RestartCalibration() const;
  void UpdateCalibration(const StateData&) const;

  SettingValue<double> m_deadzone_setting;
  SettingValue<double> m_calibration_period_setting;

  mutable StateData m_calibration = {};
  mutable MathUtil::RunningMean<StateData> m_running_calibration;
  mutable Clock::time_point m_calibration_period_start = Clock::now();
};
}  // namespace ControllerEmu
