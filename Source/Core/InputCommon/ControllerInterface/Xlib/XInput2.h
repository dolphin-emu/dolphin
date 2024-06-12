// Copyright 2013 Max Eliaser
// SPDX-License-Identifier: GPL-2.0-or-later

// See XInput2.cpp for extensive documentation.

#pragma once

#include <array>

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
}

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface::XInput2
{
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

class KeyboardMouse : public Core::Device
{
private:
  struct State
  {
    std::array<char, 32> keyboard;
    u32 buttons;
    Common::Vec2 cursor;
    Common::Vec3 axis;
    Common::Vec3 relative_mouse;
  };

  class Key : public Input
  {
    friend class KeyboardMouse;

  public:
    std::string GetName() const override { return m_keyname; }
    Key(Display* display, KeyCode keycode, const char* keyboard);
    ControlState GetState() const override;

  private:
    std::string m_keyname;
    Display* const m_display;
    const char* const m_keyboard;
    const KeyCode m_keycode;
  };

  class Button : public Input
  {
  public:
    std::string GetName() const override { return name; }
    Button(unsigned int index, u32* buttons);
    ControlState GetState() const override;

  private:
    const u32* m_buttons;
    const unsigned int m_index;
    std::string name;
  };

  class Cursor : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() const override { return false; }
    Cursor(u8 index, bool positive, const float* cursor);
    ControlState GetState() const override;

  private:
    const float* m_cursor;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

  class Axis : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() const override { return false; }
    Axis(u8 index, bool positive, const float* axis);
    ControlState GetState() const override;

  private:
    const float* m_axis;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

  class RelativeMouse : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() const override { return false; }
    RelativeMouse(u8 index, bool positive, const float* axis);
    ControlState GetState() const override;

  private:
    const float* m_axis;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

private:
  void UpdateCursor(bool should_center_mouse);

public:
  Core::DeviceRemoval UpdateInput() override;

  KeyboardMouse(Window window, int opcode, int pointer_deviceid, int keyboard_deviceid,
                double scroll_increment);
  ~KeyboardMouse();

  std::string GetName() const override;
  std::string GetSource() const override;
  int GetSortPriority() const override;

private:
  Window m_window;
  Display* m_display;
  State m_state{};
  const int xi_opcode;
  const int pointer_deviceid;
  const int keyboard_deviceid;
  const double scroll_increment;
  std::string name;
};
}  // namespace ciface::XInput2
