// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <optional>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

class CameraControllerInput;
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
  void UpdateInput(CameraControllerInput* camera_controller);
  ControllerEmu::Buttons* m_move_buttons;
  ControllerEmu::Buttons* m_speed_buttons;
  ControllerEmu::Buttons* m_fov_buttons;
  ControllerEmu::Buttons* m_other_buttons;
  ControllerEmu::IMUGyroscope* m_rotation_gyro;

  const unsigned int m_index;
  std::optional<std::chrono::steady_clock::time_point> m_last_free_look_rotate_time;
};
