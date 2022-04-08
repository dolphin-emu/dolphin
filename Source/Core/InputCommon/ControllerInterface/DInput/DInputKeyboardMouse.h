// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <windows.h>

#include "Common/Matrix.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/DInput/DInput8.h"


namespace ciface::DInput
{
void InitKeyboardMouse(IDirectInput8* const idi8, HWND hwnd);

extern void Save_Keyboard_and_Mouse_Settings();
extern void Load_Keyboard_and_Mouse_Settings();

using RelativeMouseState = RelativeInputState<Common::TVec3<LONG>>;
void SetKeyboardMouseWindow(HWND hwnd);

extern double cursor_sensitivity;  // 2 for full screen mapping
extern unsigned char center_mouse_key;
extern double snapping_distance;
extern bool octagon_gates_are_enabled;


class KeyboardMouse : public Core::Device
{
private:
  struct State
  {
    BYTE keyboard[256]{};

    // Old smoothed relative mouse movement.
    DIMOUSESTATE2 mouse{};

    // Normalized mouse cursor position.
    Common::TVec2<ControlState> cursor;

    // Raw relative mouse movement.
    RelativeMouseState relative_mouse;
  };

  // Keyboard key
  class Key : public Input
  {
  public:
    Key(u8 index, const BYTE& key) : m_key(key), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const BYTE& m_key;
    const u8 m_index;
  };

  // Mouse button
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

  // Mouse movement offset axis. Includes mouse wheel
  class Axis : public Input
  {
  public:
    Axis(u8 index, const LONG& axis, LONG range) : m_axis(axis), m_range(range), m_index(index) {}
    std::string GetName() const override;
    bool IsDetectable() const override { return false; }
    ControlState GetState() const override;

  private:
    const LONG& m_axis;
    const LONG m_range;
    const u8 m_index;
  };

  // Mouse from window center
  class Cursor : public Input
  {
  public:
    Cursor(u8 index, const ControlState& axis, const bool positive)
        : m_axis(axis), m_index(index), m_positive(positive)
    {
    }
    std::string GetName() const override;
    bool IsDetectable() const override { return false; }
    ControlState GetState() const override;

  private:
    const ControlState& m_axis;
    const u8 m_index;
    const bool m_positive;
  };

public:
  void UpdateInput() override;

  KeyboardMouse(const LPDIRECTINPUTDEVICE8 kb_device, const LPDIRECTINPUTDEVICE8 mo_device);
  ~KeyboardMouse();

  std::string GetName() const override;
  std::string GetSource() const override;
  int GetSortPriority() const override;
  bool IsVirtualDevice() const override;

private:
  void UpdateCursorInput();
  void Generate_Octagon_Points(POINT octagon_points[8]);
  bool Point_Is_Inside_Octagon(const POINT& mouse_point, const POINT octagon_points[8]);
  double Calculate_Distance_between_Points(const POINT& first_point,const POINT& second_point);
  long Find_Second_Line_Point(const POINT& mouse_point, const POINT octagon_points[8],
                              long index_of_min_octagon_point);
  void Move_Mouse_Point_Along_Gate(POINT& mouse_point,
                                   const POINT octagon_points[8]);
  void Lock_Mouse_In_Jail(POINT& mouse_point);

  bool player_requested_mouse_center = false;
  const double screen_height = static_cast<double>(GetSystemMetrics(SM_CYFULLSCREEN));
  const double screen_width = static_cast<double>(GetSystemMetrics(SM_CXFULLSCREEN));
  const double screen_ratio = screen_width / screen_height;
  const POINT center_of_screen =
      POINT{static_cast<long>(screen_width / 2.0), static_cast<long>(screen_height / 2.0)};
  enum octagon_points
  {
    SOUTH,
    SOUTH_EAST,
    EAST,
    NORTH_EAST,
    NORTH,
    NORTH_WEST,
    WEST,
    SOUTH_WEST
  };

  const LPDIRECTINPUTDEVICE8 m_kb_device;
  const LPDIRECTINPUTDEVICE8 m_mo_device;

  DWORD m_last_update;
  State current_state;
};
}  // namespace ciface::DInput
