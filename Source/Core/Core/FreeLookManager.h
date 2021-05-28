// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <optional>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

class CameraController2DInput;
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

enum class FreeLook2DGroup
{
  Move,
  Speed,
  Stretch,
  TextureLayer,
  Other,
};

namespace FreeLook
{
void Shutdown();
void Initialize();
bool IsInitialized();
void UpdateInput();

void LoadInputConfig();
InputConfig* GetInputConfig();
ControllerEmu::ControlGroup* GetInputGroup(int pad_num, FreeLookGroup group);

void Load2DInputConfig();
InputConfig* Get2DInputConfig();
ControllerEmu::ControlGroup* Get2DInputGroup(int pad_num, FreeLook2DGroup group);

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
  ControllerEmu::IMUGyroscope* m_rotation_gyro;

  const unsigned int m_index;
  std::optional<std::chrono::steady_clock::time_point> m_last_free_look_rotate_time;
};

class FreeLook2DController final : public ControllerEmu::EmulatedController
{
public:
  explicit FreeLook2DController(unsigned int index);

  std::string GetName() const override;

  ControllerEmu::ControlGroup* GetGroup(FreeLook2DGroup group) const;
  void Update();

private:
  void UpdateInput(CameraController2DInput* camera_controller);
  ControllerEmu::Buttons* m_move_buttons;
  ControllerEmu::Buttons* m_speed_buttons;
  ControllerEmu::Buttons* m_stretch_buttons;
  ControllerEmu::Buttons* m_texturelayer_buttons;
  ControllerEmu::Buttons* m_other_buttons;

  const unsigned int m_index;
  std::optional<std::chrono::steady_clock::time_point> m_last_free_look_rotate_time;
};
