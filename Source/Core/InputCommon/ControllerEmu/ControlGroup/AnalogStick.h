// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
// An abstract class representing the plastic shell that limits an analog stick's movement.
class StickGate
{
public:
  // Angle is in radians and should be non-negative
  virtual ControlState GetRadiusAtAngle(double ang) const = 0;
};

// An octagon-shaped stick gate is found on most Nintendo GC/Wii analog sticks.
class OctagonStickGate : public StickGate
{
public:
  explicit OctagonStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override;

  static ControlState ComputeRadiusAtAngle(double ang);

  const ControlState m_radius;
};

// A round-shaped stick gate. Possibly found on 3rd-party accessories.
class RoundStickGate : public StickGate
{
public:
  explicit RoundStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override;

private:
  const ControlState m_radius;
};

// A square-shaped stick gate. e.g. keyboard input.
class SquareStickGate : public StickGate
{
public:
  explicit SquareStickGate(ControlState half_width);
  ControlState GetRadiusAtAngle(double ang) const override;

  static ControlState ComputeRadiusAtAngle(double ang);

private:
  const ControlState m_half_width;
};

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
