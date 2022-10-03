// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class AnalogStick : public ReshapableInput
{
public:
  using StateData = ReshapeData;

  AnalogStick(const char* name, std::unique_ptr<StickGate>&& stick_gate);
  AnalogStick(const char* name, const char* ui_name, std::unique_ptr<StickGate>&& stick_gate);

  ReshapeData GetReshapableState(bool adjusted) const final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  StateData GetState() const;
  StateData GetState(const InputOverrideFunction& override_func) const;
  StateData GetState(const InputOverrideFunction& override_func, bool* override_occurred) const;

private:
  Control* GetModifierInput() const override;

  std::unique_ptr<StickGate> m_stick_gate;
};

// An AnalogStick with an OctagonStickGate
class OctagonAnalogStick : public AnalogStick
{
public:
  OctagonAnalogStick(const char* name, ControlState gate_radius);
  OctagonAnalogStick(const char* name, const char* ui_name, ControlState gate_radius);

  ControlState GetVirtualNotchSize() const override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

private:
  SettingValue<double> m_virtual_notch_setting;
  SettingValue<double> m_gate_size_setting;
};

}  // namespace ControllerEmu
