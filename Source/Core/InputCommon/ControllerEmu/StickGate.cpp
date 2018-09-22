// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/StickGate.h"

#include <cmath>

#include "Common/MathUtil.h"

namespace ControllerEmu
{
OctagonStickGate::OctagonStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState OctagonStickGate::GetRadiusAtAngle(double ang) const
{
  constexpr int sides = 8;
  constexpr double sum_int_angles = (sides - 2) * MathUtil::PI;
  constexpr double half_int_angle = sum_int_angles / sides / 2;

  ang = std::fmod(ang, MathUtil::TAU / sides);
  // Solve ASA triangle using The Law of Sines:
  return m_radius / std::sin(MathUtil::PI - ang - half_int_angle) * std::sin(half_int_angle);
}

RoundStickGate::RoundStickGate(ControlState radius) : m_radius(radius)
{
}

ControlState RoundStickGate::GetRadiusAtAngle(double) const
{
  return m_radius;
}

SquareStickGate::SquareStickGate(ControlState half_width) : m_half_width(half_width)
{
}

ControlState SquareStickGate::GetRadiusAtAngle(double ang) const
{
  constexpr double section_ang = MathUtil::TAU / 4;
  return m_half_width / std::cos(std::fmod(ang + section_ang / 2, section_ang) - section_ang / 2);
}

}  // namespace ControllerEmu
