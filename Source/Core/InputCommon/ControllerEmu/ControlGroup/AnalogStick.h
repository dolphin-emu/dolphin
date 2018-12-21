// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class AnalogStick : public ControlGroup
{
public:
  enum
  {
    SETTING_INPUT_RADIUS,
    SETTING_INPUT_SHAPE,
    SETTING_DEADZONE,
  };

  struct StateData
  {
    ControlState x{};
    ControlState y{};
  };

  AnalogStick(const char* name, std::unique_ptr<StickGate>&& stick_gate);
  AnalogStick(const char* name, const char* ui_name, std::unique_ptr<StickGate>&& stick_gate);

  StateData GetState(bool adjusted = true);

  // Angle is in radians and should be non-negative
  ControlState GetGateRadiusAtAngle(double ang) const;
  ControlState GetDeadzoneRadiusAtAngle(double ang) const;
  ControlState GetInputRadiusAtAngle(double ang) const;

private:
  ControlState CalculateInputShapeRadiusAtAngle(double ang) const;

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
