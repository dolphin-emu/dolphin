// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QuartzCore/QuartzCore.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface::Quartz
{
std::string KeycodeToName(const CGKeyCode keycode);

class KeyboardAndMouse : public Core::Device
{
private:
  class Key : public Input
  {
  public:
    explicit Key(CGKeyCode keycode);
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    CGKeyCode m_keycode;
    std::string m_name;
  };

  class Cursor : public Input
  {
  public:
    Cursor(u8 index, const float& axis, const bool positive)
        : m_axis(axis), m_index(index), m_positive(positive)
    {
    }
    std::string GetName() const override;
    bool IsDetectable() override { return false; }
    ControlState GetState() const override;

  private:
    const float& m_axis;
    const u8 m_index;
    const bool m_positive;
  };

  class Button : public Input
  {
  public:
    explicit Button(CGMouseButton button) : m_button(button) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    CGMouseButton m_button;
  };

public:
  void UpdateInput() override;

  explicit KeyboardAndMouse(void* window);

  std::string GetName() const override;
  std::string GetSource() const override;

private:
  struct
  {
    float x, y;
  } m_cursor;

  uint32_t m_windowid;
};
}  // namespace ciface::Quartz
