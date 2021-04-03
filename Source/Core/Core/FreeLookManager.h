// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <optional>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

class InputConfig;

namespace ControllerEmu
{
class ControlGroup;
class Buttons;
class IMUGyroscope;
}  // namespace ControllerEmu

enum class FreeLookGroup
{
  Move,
  Speed,
  FieldOfView,
  Other,
  RotationIncremental,
  RotationExact,
};

namespace FreeLook
{
void Shutdown();
void Initialize();
void LoadInputConfig();
bool IsInitialized();
void UpdateInput();

InputConfig* GetInputConfig();
ControllerEmu::ControlGroup* GetInputGroup(int pad_num, FreeLookGroup group);

}  // namespace FreeLook

class FreeLookController final : public ControllerEmu::EmulatedController
{
public:
  explicit FreeLookController(unsigned int index);

  std::string GetName() const override;
  void LoadDefaults(const ControllerInterface& ciface) override;

  ControllerEmu::ControlGroup* GetGroup(FreeLookGroup group) const;
  void Update();

private:
  ControllerEmu::Buttons* m_move_buttons;
  ControllerEmu::Buttons* m_speed_buttons;
  ControllerEmu::Buttons* m_fov_buttons;
  ControllerEmu::Buttons* m_other_buttons;
  ControllerEmu::IMUGyroscope* m_incremental_rotation_gyro;
  ControllerEmu::ControlGroup* m_exact_rotation_group;

  ControllerEmu::SettingValue<double> m_pitch;
  double m_last_pitch;

  ControllerEmu::SettingValue<double> m_yaw;
  double m_last_yaw;

  ControllerEmu::SettingValue<double> m_roll;
  double m_last_roll;

  const unsigned int m_index;
  std::optional<std::chrono::steady_clock::time_point> m_last_free_look_rotate_time;
};
