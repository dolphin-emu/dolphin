// Copyright 2013 Max Eliaser
// Licensed under GPLv2+
// Refer to the license.txt file included.

// See XInput2.cpp for extensive documentation.

#pragma once

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
}

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface
{
namespace XInput2
{
void PopulateDevices(void* const hwnd);

class KeyboardMouse : public Core::Device
{
private:
  struct State
  {
    char keyboard[32];
    unsigned int buttons;
    struct
    {
      float x, y;
    } cursor, axis;
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
    Button(unsigned int index, unsigned int* buttons);
    ControlState GetState() const override;

  private:
    const unsigned int* m_buttons;
    const unsigned int m_index;
    std::string name;
  };

  class Cursor : public Input
  {
  public:
    std::string GetName() const override { return name; }
    bool IsDetectable() override { return false; }
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
    bool IsDetectable() override { return false; }
    Axis(u8 index, bool positive, const float* axis);
    ControlState GetState() const override;

  private:
    const float* m_axis;
    const u8 m_index;
    const bool m_positive;
    std::string name;
  };

private:
  void SelectEventsForDevice(Window window, XIEventMask* mask, int deviceid);
  void UpdateCursor();

public:
  void UpdateInput() override;

  KeyboardMouse(Window window, int opcode, int pointer_deviceid, int keyboard_deviceid);
  ~KeyboardMouse();

  std::string GetName() const override;
  std::string GetSource() const override;

private:
  Window m_window;
  Display* m_display;
  State m_state;
  int xi_opcode;
  const int pointer_deviceid, keyboard_deviceid;
  std::string name;
};
}
}
