// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QuartzCore/QuartzCore.h>

#include "Common/Matrix.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

#ifdef __OBJC__
@class DolWindowPositionObserver;
#else
class DolWindowPositionObserver;
#endif

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
    bool IsDetectable() const override { return false; }
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

  explicit KeyboardAndMouse(void* view);
  ~KeyboardAndMouse() override;

  std::string GetName() const override;
  std::string GetSource() const override;

private:
  void MainThreadInitialization(void* view);

  Common::Vec2 m_cursor;

  DolWindowPositionObserver* m_window_pos_observer;
};
}  // namespace ciface::Quartz
