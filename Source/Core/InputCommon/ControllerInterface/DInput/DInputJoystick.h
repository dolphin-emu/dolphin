// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

namespace ciface::DInput
{
void InitJoystick(IDirectInput8* const idi8, HWND hwnd);

class Joystick : public ForceFeedback::ForceFeedbackDevice
{
private:
  class Button : public Input
  {
  public:
    Button(u8 index, const BYTE& button) : m_button(button), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const BYTE& m_button;
    const u8 m_index;
  };

  class Axis : public Input
  {
  public:
    Axis(u8 index, const LONG& axis, LONG base, LONG range)
        : m_axis(axis)
        , m_base(base)
        , m_range(range)
        , m_index(index)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const LONG& m_axis;
    const LONG m_base, m_range;
    const u8 m_index;
  };

  class Hat : public Input
  {
  public:
    Hat(u8 index, const DWORD& hat, u8 direction)
        : m_hat(hat)
        , m_direction(direction)
        , m_index(index)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const DWORD& m_hat;
    const u8 m_index, m_direction;
  };

public:
  Core::DeviceRemoval UpdateInput() override;

  Joystick(const LPDIRECTINPUTDEVICE8 device);
  ~Joystick() override;

  std::string GetName() const override;
  std::string GetSource() const override;
  int GetSortPriority() const override { return -3; }

  bool IsValid() const final;

private:
  const LPDIRECTINPUTDEVICE8 m_device;

  DIJOYSTATE m_state_in{};

  bool m_buffered;
};
}  // namespace ciface::DInput
