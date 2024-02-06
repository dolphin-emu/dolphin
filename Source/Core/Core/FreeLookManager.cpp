// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FreeLookManager.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/ScopeGuard.h"

#include "Core/Config/FreeLookSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/FreeLookConfig.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"
#include "InputCommon/InputConfig.h"

#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace
{
namespace MoveButtons
{
enum MoveButtons
{
  Up,
  Down,
  Left,
  Right,
  Forward,
  Backward,
};
}

namespace SpeedButtons
{
enum SpeedButtons
{
  Decrease,
  Increase,
  Reset,
};
}

namespace OtherButtons
{
enum OtherButtons
{
  ResetView,
};
}

namespace FieldOfViewButtons
{
enum FieldOfViewButtons
{
  IncreaseX,
  DecreaseX,
  IncreaseY,
  DecreaseY,
};
}

namespace GyroButtons
{
enum GyroButtons
{
  PitchUp,
  PitchDown,
  RollLeft,
  RollRight,
  YawLeft,
  YawRight,
};
}
}  // namespace

FreeLookController::FreeLookController(const unsigned int index) : m_index(index)
{
  using Translatability = ControllerEmu::Translatability;

  groups.emplace_back(m_move_buttons = new ControllerEmu::Buttons(_trans("Move")));

  m_move_buttons->AddInput(Translatability::Translate, _trans("Up"));
  m_move_buttons->AddInput(Translatability::Translate, _trans("Down"));
  m_move_buttons->AddInput(Translatability::Translate, _trans("Left"));
  m_move_buttons->AddInput(Translatability::Translate, _trans("Right"));
  m_move_buttons->AddInput(Translatability::Translate, _trans("Forward"));
  m_move_buttons->AddInput(Translatability::Translate, _trans("Backward"));

  groups.emplace_back(m_speed_buttons = new ControllerEmu::Buttons(_trans("Speed")));

  m_speed_buttons->AddInput(Translatability::Translate, _trans("Decrease"));
  m_speed_buttons->AddInput(Translatability::Translate, _trans("Increase"));
  m_speed_buttons->AddInput(Translatability::Translate, _trans("Reset"));

  groups.emplace_back(m_other_buttons = new ControllerEmu::Buttons(_trans("Other")));

  m_other_buttons->AddInput(Translatability::Translate, _trans("Reset View"));

  groups.emplace_back(m_fov_buttons = new ControllerEmu::Buttons(_trans("Field of View")));

  m_fov_buttons->AddInput(Translatability::Translate, _trans("Increase X"));
  m_fov_buttons->AddInput(Translatability::Translate, _trans("Decrease X"));
  m_fov_buttons->AddInput(Translatability::Translate, _trans("Increase Y"));
  m_fov_buttons->AddInput(Translatability::Translate, _trans("Decrease Y"));

  groups.emplace_back(m_rotation_gyro = new ControllerEmu::IMUGyroscope(
                          _trans("Incremental Rotation"), _trans("Incremental Rotation")));
}

std::string FreeLookController::GetName() const
{
  return std::string("FreeLook") + char('1' + m_index);
}

InputConfig* FreeLookController::GetConfig() const
{
  return FreeLook::GetInputConfig();
}

void FreeLookController::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

#ifndef ANDROID
  auto hotkey_string = [](std::vector<std::string> inputs) {
    return "@(" + JoinStrings(inputs, "+") + ')';
  };

  m_move_buttons->SetControlExpression(MoveButtons::Up, hotkey_string({"Shift", "E"}));
  m_move_buttons->SetControlExpression(MoveButtons::Down, hotkey_string({"Shift", "Q"}));
  m_move_buttons->SetControlExpression(MoveButtons::Left, hotkey_string({"Shift", "A"}));
  m_move_buttons->SetControlExpression(MoveButtons::Right, hotkey_string({"Shift", "D"}));
  m_move_buttons->SetControlExpression(MoveButtons::Forward, hotkey_string({"Shift", "W"}));
  m_move_buttons->SetControlExpression(MoveButtons::Backward, hotkey_string({"Shift", "S"}));

  m_speed_buttons->SetControlExpression(SpeedButtons::Decrease, hotkey_string({"Shift", "`1`"}));
  m_speed_buttons->SetControlExpression(SpeedButtons::Increase, hotkey_string({"Shift", "`2`"}));
  m_speed_buttons->SetControlExpression(SpeedButtons::Reset, hotkey_string({"Shift", "F"}));

  m_other_buttons->SetControlExpression(OtherButtons::ResetView, hotkey_string({"Shift", "R"}));

  m_fov_buttons->SetControlExpression(FieldOfViewButtons::IncreaseX,
                                      hotkey_string({"Shift", "`Axis Z+`"}));
  m_fov_buttons->SetControlExpression(FieldOfViewButtons::DecreaseX,
                                      hotkey_string({"Shift", "`Axis Z-`"}));
  m_fov_buttons->SetControlExpression(FieldOfViewButtons::IncreaseY,
                                      hotkey_string({"Shift", "`Axis Z+`"}));
  m_fov_buttons->SetControlExpression(FieldOfViewButtons::DecreaseY,
                                      hotkey_string({"Shift", "`Axis Z-`"}));

  // Left Click
#if defined HAVE_X11 && HAVE_X11
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchUp,
                                        "if(`Click 3`,`RelativeMouse Y-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchDown,
                                        "if(`Click 3`,`RelativeMouse Y+` * 0.10, 0)");
#elif defined(__APPLE__)
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchUp,
                                        "if(`Left Click`,`RelativeMouse Y-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchDown,
                                        "if(`Left Click`,`RelativeMouse Y+` * 0.10, 0)");
#else
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchUp,
                                        "if(`Click 1`,`RelativeMouse Y-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::PitchDown,
                                        "if(`Click 1`,`RelativeMouse Y+` * 0.10, 0)");
#endif

  // Middle Click
#ifdef __APPLE__
  m_rotation_gyro->SetControlExpression(GyroButtons::RollLeft,
                                        "if(`Middle Click`,`RelativeMouse X-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::RollRight,
                                        "if(`Middle Click`,`RelativeMouse X+` * 0.10, 0)");
#else
  m_rotation_gyro->SetControlExpression(GyroButtons::RollLeft,
                                        "if(`Click 2`,`RelativeMouse X-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::RollRight,
                                        "if(`Click 2`,`RelativeMouse X+` * 0.10, 0)");
#endif

  // Right Click
#if defined HAVE_X11 && HAVE_X11
  m_rotation_gyro->SetControlExpression(GyroButtons::YawLeft,
                                        "if(`Click 3`,`RelativeMouse X-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::YawRight,
                                        "if(`Click 3`,`RelativeMouse X+` * 0.10, 0)");
#elif defined(__APPLE__)
  m_rotation_gyro->SetControlExpression(GyroButtons::YawLeft,
                                        "if(`Right Click`,`RelativeMouse X-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::YawRight,
                                        "if(`Right Click`,`RelativeMouse X+` * 0.10, 0)");
#else
  m_rotation_gyro->SetControlExpression(GyroButtons::YawLeft,
                                        "if(`Click 1`,`RelativeMouse X-` * 0.10, 0)");
  m_rotation_gyro->SetControlExpression(GyroButtons::YawRight,
                                        "if(`Click 1`,`RelativeMouse X+` * 0.10, 0)");
#endif
#endif
}

ControllerEmu::ControlGroup* FreeLookController::GetGroup(FreeLookGroup group) const
{
  switch (group)
  {
  case FreeLookGroup::Move:
    return m_move_buttons;
  case FreeLookGroup::Speed:
    return m_speed_buttons;
  case FreeLookGroup::FieldOfView:
    return m_fov_buttons;
  case FreeLookGroup::Other:
    return m_other_buttons;
  case FreeLookGroup::Rotation:
    return m_rotation_gyro;
  default:
    return nullptr;
  }
}

void FreeLookController::Update()
{
  if (!g_freelook_camera.IsActive())
    return;

  auto* camera_controller = g_freelook_camera.GetController();
  if (camera_controller->SupportsInput())
  {
    UpdateInput(static_cast<CameraControllerInput*>(camera_controller));
  }
}

void FreeLookController::UpdateInput(CameraControllerInput* camera_controller)
{
  const auto lock = GetStateLock();
  // Preserve the old controller gate state
  const auto old_gate = ControlReference::GetInputGate();
  Common::ScopeGuard gate_guard{[old_gate] { ControlReference::SetInputGate(old_gate); }};
  // Switch to the free look focus gate
  Core::UpdateInputGate(!Config::Get(Config::FREE_LOOK_BACKGROUND_INPUT));

  float dt = 1.0;
  if (m_last_free_look_rotate_time)
  {
    using seconds = std::chrono::duration<float, std::ratio<1>>;
    dt = std::chrono::duration_cast<seconds>(std::chrono::steady_clock::now() -
                                             *m_last_free_look_rotate_time)
             .count();
  }
  m_last_free_look_rotate_time = std::chrono::steady_clock::now();

  const auto gyro_motion_rad_velocity =
      m_rotation_gyro->GetState() ? *m_rotation_gyro->GetState() : Common::Vec3{};

  // Due to gyroscope implementation we need to swap the yaw and roll values
  // and because of the different axis used for Wii and the PS3 motion directions,
  // we need to invert the yaw and roll as well
  const Common::Vec3 gyro_motion_rad_velocity_converted{
      gyro_motion_rad_velocity.x, gyro_motion_rad_velocity.z * -1, gyro_motion_rad_velocity.y * -1};
  const auto gyro_motion_quat =
      Common::Quaternion::RotateXYZ(gyro_motion_rad_velocity_converted * dt);

  camera_controller->Rotate(gyro_motion_quat);
  if (m_move_buttons->controls[MoveButtons::Up]->GetState<bool>())
    camera_controller->MoveVertical(-camera_controller->GetSpeed() * dt);

  if (m_move_buttons->controls[MoveButtons::Down]->GetState<bool>())
    camera_controller->MoveVertical(camera_controller->GetSpeed() * dt);

  if (m_move_buttons->controls[MoveButtons::Left]->GetState<bool>())
    camera_controller->MoveHorizontal(camera_controller->GetSpeed() * dt);

  if (m_move_buttons->controls[MoveButtons::Right]->GetState<bool>())
    camera_controller->MoveHorizontal(-camera_controller->GetSpeed() * dt);

  if (m_move_buttons->controls[MoveButtons::Forward]->GetState<bool>())
    camera_controller->MoveForward(camera_controller->GetSpeed() * dt);

  if (m_move_buttons->controls[MoveButtons::Backward]->GetState<bool>())
    camera_controller->MoveForward(-camera_controller->GetSpeed() * dt);

  if (m_fov_buttons->controls[FieldOfViewButtons::IncreaseX]->GetState<bool>())
    camera_controller->IncreaseFovX(camera_controller->GetFovStepSize() * dt);

  if (m_fov_buttons->controls[FieldOfViewButtons::DecreaseX]->GetState<bool>())
    camera_controller->IncreaseFovX(-1.0f * camera_controller->GetFovStepSize() * dt);

  if (m_fov_buttons->controls[FieldOfViewButtons::IncreaseY]->GetState<bool>())
    camera_controller->IncreaseFovY(camera_controller->GetFovStepSize() * dt);

  if (m_fov_buttons->controls[FieldOfViewButtons::DecreaseY]->GetState<bool>())
    camera_controller->IncreaseFovY(-1.0f * camera_controller->GetFovStepSize() * dt);

  if (m_speed_buttons->controls[SpeedButtons::Decrease]->GetState<bool>())
    camera_controller->ModifySpeed(camera_controller->GetSpeed() * -0.9 * dt);

  if (m_speed_buttons->controls[SpeedButtons::Increase]->GetState<bool>())
    camera_controller->ModifySpeed(camera_controller->GetSpeed() * 1.1 * dt);

  if (m_speed_buttons->controls[SpeedButtons::Reset]->GetState<bool>())
    camera_controller->ResetSpeed();

  if (m_other_buttons->controls[OtherButtons::ResetView]->GetState<bool>())
    camera_controller->Reset();
}

namespace FreeLook
{
static InputConfig s_config("FreeLookController", _trans("FreeLook"), "FreeLookController",
                            "FreeLookController");
InputConfig* GetInputConfig()
{
  return &s_config;
}

void Shutdown()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
  {
    s_config.CreateController<FreeLookController>(0);
  }

  s_config.RegisterHotplugCallback();

  FreeLook::GetConfig().Refresh();

  s_config.LoadConfig();
}

void LoadInputConfig()
{
  s_config.LoadConfig();
}

bool IsInitialized()
{
  return !s_config.ControllersNeedToBeCreated();
}

ControllerEmu::ControlGroup* GetInputGroup(int pad_num, FreeLookGroup group)
{
  return static_cast<FreeLookController*>(s_config.GetController(pad_num))->GetGroup(group);
}

void UpdateInput()
{
  for (int i = 0; i < s_config.GetControllerCount(); i++)
  {
    static_cast<FreeLookController*>(s_config.GetController(i))->Update();
  }
}

}  // namespace FreeLook
