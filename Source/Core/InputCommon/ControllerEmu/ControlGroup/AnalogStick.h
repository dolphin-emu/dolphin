// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class AnalogStick : public ReshapableInput
{
public:
  using StateData = ReshapeData;

  AnalogStick(const char* name, std::unique_ptr<StickGate>&& stick_gate);
  AnalogStick(const char* name, const char* ui_name, std::unique_ptr<StickGate>&& stick_gate);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  StateData GetState();

private:
  std::unique_ptr<StickGate> m_stick_gate;
};

// An AnalogStick with an OctagonStickGate
class OctagonAnalogStick : public AnalogStick
{
public:
  OctagonAnalogStick(const char* name, ControlState gate_radius);
  OctagonAnalogStick(const char* name, const char* ui_name, ControlState gate_radius);
};

}  // namespace ControllerEmu
