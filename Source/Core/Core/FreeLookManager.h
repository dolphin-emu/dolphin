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
  Rotation,
  PositionOffset,
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

  // The following position offsets are doubles
  // but used as floats in the camera system
  // This is done because the settings and qt
  // controls are in double format
  ControllerEmu::SettingValue<double> m_pos_x;
  double m_pos_last_x;
  ControllerEmu::SettingValue<double> m_pos_y;
  double m_pos_last_y;
  ControllerEmu::SettingValue<double> m_pos_z;
  double m_pos_last_z;
  ControllerEmu::ControlGroup* m_position_offset_group;

  ControllerEmu::Buttons* m_speed_buttons;
  ControllerEmu::Buttons* m_fov_buttons;
  ControllerEmu::Buttons* m_other_buttons;
  ControllerEmu::IMUGyroscope* m_rotation_gyro;

  const unsigned int m_index;
  std::optional<std::chrono::steady_clock::time_point> m_last_free_look_rotate_time;
};
