// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControlReference/ControlReference.h"

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

}  // namespace ControllerEmu
