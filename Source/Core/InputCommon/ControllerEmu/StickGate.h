// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
// An abstract class representing the plastic shell that limits an analog stick's movement.
class StickGate
{
public:
  // Angle is in radians and should be non-negative
  virtual ControlState GetRadiusAtAngle(double ang) const = 0;

  virtual ~StickGate() = default;
};

// An octagon-shaped stick gate is found on most Nintendo GC/Wii analog sticks.
class OctagonStickGate : public StickGate
{
public:
  // Radius of circumscribed circle
  explicit OctagonStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override final;

private:
  const ControlState m_radius;
};

// A round-shaped stick gate. Possibly found on 3rd-party accessories.
class RoundStickGate : public StickGate
{
public:
  explicit RoundStickGate(ControlState radius);
  ControlState GetRadiusAtAngle(double ang) const override final;

private:
  const ControlState m_radius;
};

// A square-shaped stick gate. e.g. keyboard input.
class SquareStickGate : public StickGate
{
public:
  explicit SquareStickGate(ControlState half_width);
  ControlState GetRadiusAtAngle(double ang) const override final;

private:
  const ControlState m_half_width;
};

class ReshapableInput : public ControlGroup
{
public:
  ReshapableInput(std::string name, std::string ui_name, GroupType type);

  struct ReshapeData
  {
    ControlState x{};
    ControlState y{};
  };

  enum
  {
    SETTING_INPUT_RADIUS,
    SETTING_INPUT_SHAPE,
    SETTING_DEADZONE,
    SETTING_COUNT,
  };

  // Angle is in radians and should be non-negative
  ControlState GetDeadzoneRadiusAtAngle(double ang) const;
  ControlState GetInputRadiusAtAngle(double ang) const;

  virtual ControlState GetGateRadiusAtAngle(double ang) const = 0;
  virtual ReshapeData GetReshapableState(bool adjusted) = 0;

protected:
  void AddReshapingSettings(ControlState default_radius, ControlState default_shape,
                            int max_deadzone);

  ReshapeData Reshape(ControlState x, ControlState y, ControlState modifier = 0.0);

private:
  ControlState CalculateInputShapeRadiusAtAngle(double ang) const;
};

}  // namespace ControllerEmu
