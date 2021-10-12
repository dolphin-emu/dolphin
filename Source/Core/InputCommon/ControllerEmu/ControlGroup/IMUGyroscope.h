// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  // Also updates the state by default
  std::optional<StateData> GetState(bool update = true);

  // Value is in rad/s.
  ControlState GetDeadzone() const;

  bool IsCalibrating() const;

private:
  using Clock = std::chrono::steady_clock;

  bool AreInputsBound() const;
  bool CanCalibrate() const;
  void RestartCalibration();
  void UpdateCalibration(const StateData&);

  SettingValue<double> m_deadzone_setting;
  SettingValue<double> m_calibration_period_setting;

  StateData m_calibration = {};
  MathUtil::RunningMean<StateData> m_running_calibration;
  Clock::time_point m_calibration_period_start = Clock::now();
};
}  // namespace ControllerEmu
