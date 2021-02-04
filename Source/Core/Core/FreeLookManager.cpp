// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FreeLookManager.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

#include "Core/ConfigManager.h"
#include "Core/FreeLookConfig.h"

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
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
}  // namespace

FreeLookController::FreeLookController(const unsigned int index) : m_index(index)
{
  groups.emplace_back(m_move_buttons = new ControllerEmu::Buttons(_trans("Move")));

  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Up"));
  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Down"));
  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Left"));
  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Right"));
  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Forward"));
  m_move_buttons->AddInput(ControllerEmu::Translate, _trans("Backward"));

  groups.emplace_back(m_speed_buttons = new ControllerEmu::Buttons(_trans("Speed")));

  m_speed_buttons->AddInput(ControllerEmu::Translate, _trans("Decrease"));
  m_speed_buttons->AddInput(ControllerEmu::Translate, _trans("Increase"));
  m_speed_buttons->AddInput(ControllerEmu::Translate, _trans("Reset"));

  groups.emplace_back(m_other_buttons = new ControllerEmu::Buttons(_trans("Other")));

  m_other_buttons->AddInput(ControllerEmu::Translate, _trans("Reset View"));

  groups.emplace_back(m_fov_buttons = new ControllerEmu::Buttons(_trans("Field of View")));

  m_fov_buttons->AddInput(ControllerEmu::Translate, _trans("Increase X"));
  m_fov_buttons->AddInput(ControllerEmu::Translate, _trans("Decrease X"));
  m_fov_buttons->AddInput(ControllerEmu::Translate, _trans("Increase Y"));
  m_fov_buttons->AddInput(ControllerEmu::Translate, _trans("Decrease Y"));
}

std::string FreeLookController::GetName() const
{
  return std::string("FreeLook") + char('1' + m_index);
}

void FreeLookController::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

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
  default:
    return nullptr;
  }
}

void FreeLookController::Update()
{
  if (!g_freelook_camera.IsActive())
    return;

  const auto lock = GetStateLock();

  if (m_move_buttons->controls[MoveButtons::Up]->GetState<bool>())
    g_freelook_camera.MoveVertical(-g_freelook_camera.GetSpeed());

  if (m_move_buttons->controls[MoveButtons::Down]->GetState<bool>())
    g_freelook_camera.MoveVertical(g_freelook_camera.GetSpeed());

  if (m_move_buttons->controls[MoveButtons::Left]->GetState<bool>())
    g_freelook_camera.MoveHorizontal(g_freelook_camera.GetSpeed());

  if (m_move_buttons->controls[MoveButtons::Right]->GetState<bool>())
    g_freelook_camera.MoveHorizontal(-g_freelook_camera.GetSpeed());

  if (m_move_buttons->controls[MoveButtons::Forward]->GetState<bool>())
    g_freelook_camera.MoveForward(g_freelook_camera.GetSpeed());

  if (m_move_buttons->controls[MoveButtons::Backward]->GetState<bool>())
    g_freelook_camera.MoveForward(-g_freelook_camera.GetSpeed());

  if (m_fov_buttons->controls[FieldOfViewButtons::IncreaseX]->GetState<bool>())
    g_freelook_camera.IncreaseFovX(g_freelook_camera.GetFovStepSize());

  if (m_fov_buttons->controls[FieldOfViewButtons::DecreaseX]->GetState<bool>())
    g_freelook_camera.IncreaseFovX(-1.0f * g_freelook_camera.GetFovStepSize());

  if (m_fov_buttons->controls[FieldOfViewButtons::IncreaseY]->GetState<bool>())
    g_freelook_camera.IncreaseFovY(g_freelook_camera.GetFovStepSize());

  if (m_fov_buttons->controls[FieldOfViewButtons::DecreaseY]->GetState<bool>())
    g_freelook_camera.IncreaseFovY(-1.0f * g_freelook_camera.GetFovStepSize());

  if (m_speed_buttons->controls[SpeedButtons::Decrease]->GetState<bool>())
    g_freelook_camera.ModifySpeed(1.0f / 1.1f);

  if (m_speed_buttons->controls[SpeedButtons::Increase]->GetState<bool>())
    g_freelook_camera.ModifySpeed(1.1f);

  if (m_speed_buttons->controls[SpeedButtons::Reset]->GetState<bool>())
    g_freelook_camera.ResetSpeed();

  if (m_other_buttons->controls[OtherButtons::ResetView]->GetState<bool>())
    g_freelook_camera.Reset();
}

namespace FreeLook
{
static InputConfig s_config("FreeLookController", _trans("FreeLook"), "FreeLookController");
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

  s_config.LoadConfig(true);
}

void LoadInputConfig()
{
  s_config.LoadConfig(true);
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
