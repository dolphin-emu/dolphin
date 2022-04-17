// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCPad.h"

#include <cstring>

#include "Common/Common.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Pad
{
static InputConfig s_config("GCPadNew", _trans("Pad"), "GCPad");
InputConfig* GetConfig()
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
    for (unsigned int i = 0; i < 4; ++i)
      s_config.CreateController<GCPad>(i);
  }

  s_config.RegisterHotplugCallback();

  // Load the saved controller config
  s_config.LoadConfig(InputConfig::InputClass::GC);
}

void LoadConfig()
{
  s_config.LoadConfig(InputConfig::InputClass::GC);
}

bool IsInitialized()
{
  return !s_config.ControllersNeedToBeCreated();
}

GCPadStatus GetStatus(int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetInput();
}

ControllerEmu::ControlGroup* GetGroup(int pad_num, PadGroup group)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetGroup(group);
}

void Rumble(const int pad_num, const ControlState strength)
{
  static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(strength);
}

void ResetRumble(const int pad_num)
{
  static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(0.0);
}

bool GetMicButton(const int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetMicButton();
}

void ChangeUIPrimeHack(int number, bool useMetroidUI)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(number));

  gcpad->ChangeUIPrimeHack(useMetroidUI);
}

bool CheckPitchRecentre()
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->CheckPitchRecentre();
}

bool PrimeUseController()
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->PrimeControllerMode();
}

void PrimeSetMode(bool useController)
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  gcpad->SetPrimeMode(useController);
}

std::tuple<double, double> GetPrimeStickXY()
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->GetPrimeStickXY();
}

bool CheckForward() {
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->groups[1]->controls[0].get()->control_ref->State() > 0.5;
}

bool CheckBack() {
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->groups[1]->controls[1].get()->control_ref->State() > 0.5;
}

bool CheckLeft() {
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->groups[1]->controls[2].get()->control_ref->State() > 0.5;
}

bool CheckRight() {
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->groups[1]->controls[3].get()->control_ref->State() > 0.5;
}

bool CheckJump() {
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->groups[0]->controls[1]->control_ref->State() > 0.5;
}

std::tuple<double, double, bool, bool, bool> PrimeSettings()
{
  GCPad* gcpad = static_cast<GCPad*>(s_config.GetController(0));

  return gcpad->GetPrimeSettings();
}

}  // namespace Pad
