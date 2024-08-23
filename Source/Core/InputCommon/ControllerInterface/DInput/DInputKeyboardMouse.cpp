// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#include <algorithm>

#include "Common/Logging/Log.h"

#include "Core/Core.h"
#include "Core/Host.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"

// (lower would be more sensitive) user can lower sensitivity by setting range
// seems decent here ( at 8 ), I don't think anyone would need more sensitive than this
// and user can lower it much farther than they would want to with the range
#define MOUSE_AXIS_SENSITIVITY 8

// if input hasn't been received for this many ms, mouse input will be skipped
// otherwise it is just some crazy value
#define DROP_INPUT_TIME 250

namespace ciface::DInput
{
class RelativeMouseAxis final : public Core::Device::RelativeInput
{
public:
  std::string GetName() const override
  {
    return fmt::format("RelativeMouse {}{}", char('X' + m_index), (m_scale > 0) ? '+' : '-');
  }

  RelativeMouseAxis(u8 index, bool positive, const RelativeMouseState* state)
      : m_state(*state), m_index(index), m_scale(positive * 2 - 1)
  {
  }

  ControlState GetState() const override
  {
    return ControlState(m_state.GetValue().data[m_index] * m_scale);
  }

private:
  const RelativeMouseState& m_state;
  const u8 m_index;
  const s8 m_scale;
};

static const struct
{
  const BYTE code;
  const char* const name;
} named_keys[] = {
#include "InputCommon/ControllerInterface/DInput/NamedKeys.h"  // NOLINT
};

// Prevent duplicate keyboard/mouse devices. Modified by more threads.
static bool s_keyboard_mouse_exists;
static HWND s_hwnd;

void InitKeyboardMouse(IDirectInput8* const idi8, HWND hwnd)
{
  if (s_keyboard_mouse_exists)
    return;

  s_hwnd = hwnd;

  // Mouse and keyboard are a combined device, to allow shift+click and stuff
  // if that's dumb, I will make a VirtualDevice class that just uses ranges of inputs/outputs from
  // other devices
  // so there can be a separated Keyboard and mouse, as well as combined KeyboardMouse

  LPDIRECTINPUTDEVICE8 kb_device = nullptr;
  LPDIRECTINPUTDEVICE8 mo_device = nullptr;

  // These are "virtual" system devices, so they are always there even if we have no physical
  // mouse and keyboard plugged into the computer
  if (SUCCEEDED(idi8->CreateDevice(GUID_SysKeyboard, &kb_device, nullptr)) &&
      SUCCEEDED(kb_device->SetDataFormat(&c_dfDIKeyboard)) &&
      SUCCEEDED(kb_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) &&
      SUCCEEDED(idi8->CreateDevice(GUID_SysMouse, &mo_device, nullptr)) &&
      SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)) &&
      SUCCEEDED(mo_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
  {
    g_controller_interface.AddDevice(std::make_shared<KeyboardMouse>(kb_device, mo_device));
    return;
  }

  ERROR_LOG_FMT(CONTROLLERINTERFACE, "KeyboardMouse device failed to be created");

  if (kb_device)
    kb_device->Release();
  if (mo_device)
    mo_device->Release();
}

void SetKeyboardMouseWindow(HWND hwnd)
{
  s_hwnd = hwnd;
}

KeyboardMouse::~KeyboardMouse()
{
  s_keyboard_mouse_exists = false;

  // Independently of the order in which we do these, if we put a breakpoint on Unacquire() (or in
  // any place in the call stack before this), when refreshing devices from the UI, on the second
  // attempt, it will get stuck in an infinite (while) loop inside dinput8.dll. Given that it can't
  // be otherwise be reproduced (not even with sleeps), we can just ignore the problem.

  // kb
  m_kb_device->Unacquire();
  m_kb_device->Release();
  // mouse
  m_mo_device->Unacquire();
  m_mo_device->Release();
}

KeyboardMouse::KeyboardMouse(const LPDIRECTINPUTDEVICE8 kb_device,
                             const LPDIRECTINPUTDEVICE8 mo_device)
    : m_kb_device(kb_device), m_mo_device(mo_device), m_last_update(GetTickCount()), m_state_in()
{
  s_keyboard_mouse_exists = true;

  if (FAILED(m_kb_device->Acquire()))
    WARN_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to acquire. We'll retry later");
  if (FAILED(m_mo_device->Acquire()))
    WARN_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to acquire. We'll retry later");

  // KEYBOARD
  // add keys
  for (u8 i = 0; i < sizeof(named_keys) / sizeof(*named_keys); ++i)
    AddInput(new Key(i, m_state_in.keyboard[named_keys[i].code]));

  // Add combined left/right modifiers with consistent naming across platforms.
  AddCombinedInput("Alt", {"LMENU", "RMENU"});
  AddCombinedInput("Shift", {"LSHIFT", "RSHIFT"});
  AddCombinedInput("Ctrl", {"LCONTROL", "RCONTROL"});

  // MOUSE
  DIDEVCAPS mouse_caps = {};
  mouse_caps.dwSize = sizeof(mouse_caps);
  m_mo_device->GetCapabilities(&mouse_caps);
  // mouse buttons
  for (u8 i = 0; i < mouse_caps.dwButtons; ++i)
    AddInput(new Button(i, m_state_in.mouse.rgbButtons[i]));
  // mouse axes
  for (unsigned int i = 0; i < mouse_caps.dwAxes; ++i)
  {
    const LONG& ax = (&m_state_in.mouse.lX)[i];

    // each axis gets a negative and a positive input instance associated with it
    AddInput(new Axis(i, ax, (2 == i) ? -1 : -MOUSE_AXIS_SENSITIVITY));
    AddInput(new Axis(i, ax, -(2 == i) ? 1 : MOUSE_AXIS_SENSITIVITY));
  }

  // cursor, with a hax for-loop
  for (unsigned int i = 0; i < 4; ++i)
    AddInput(new Cursor(!!(i & 2), (&m_state_in.cursor.x)[i / 2], !!(i & 1)));

  // Raw relative mouse movement.
  for (unsigned int i = 0; i != mouse_caps.dwAxes; ++i)
  {
    AddInput(new RelativeMouseAxis(i, false, &m_state_in.relative_mouse));
    AddInput(new RelativeMouseAxis(i, true, &m_state_in.relative_mouse));
  }
}

void KeyboardMouse::UpdateCursorInput()
{
  // Get the size of the current window (in my case Rect.top and Rect.left was zero).
  RECT rect;
  GetClientRect(s_hwnd, &rect);

  // Width and height are the size of the rendering window. They could be 0
  const auto win_width = std::max(rect.right - rect.left, 1l);
  const auto win_height = std::max(rect.bottom - rect.top, 1l);

  POINT point = {};
  if (g_controller_interface.IsMouseCenteringRequested() &&
      (Host_RendererHasFocus() || Host_TASInputHasFocus()))
  {
    point.x = win_width / 2;
    point.y = win_height / 2;

    POINT screen_point = point;
    ClientToScreen(s_hwnd, &screen_point);
    SetCursorPos(screen_point.x, screen_point.y);
    g_controller_interface.SetMouseCenteringRequested(false);
  }
  else if (Host_TASInputHasFocus())
  {
    // When a TAS Input window has focus and "Enable Controller Input" is checked most types of
    // input should be read normally as if the render window had focus instead. The cursor is an
    // exception, as otherwise using the mouse to set any control in the TAS Input window will also
    // update the Wii IR value (or any other input controlled by the cursor).

    return;
  }
  else
  {
    GetCursorPos(&point);

    // Get the cursor position relative to the upper left corner of the current window
    // (separate or render to main)
    ScreenToClient(s_hwnd, &point);
  }

  const auto window_scale = g_controller_interface.GetWindowInputScale();

  // Convert the cursor position to a range from -1 to 1.
  m_state_in.cursor.x = (ControlState(point.x) / win_width * 2 - 1) * window_scale.x;
  m_state_in.cursor.y = (ControlState(point.y) / win_height * 2 - 1) * window_scale.y;
}

Core::DeviceRemoval KeyboardMouse::UpdateInput()
{
  UpdateCursorInput();

  DIMOUSESTATE2 tmp_mouse;

  // if mouse position hasn't been updated in a short while, skip a dev state
  DWORD cur_time = GetTickCount();
  if (cur_time - m_last_update > DROP_INPUT_TIME)
  {
    // set axes to zero
    m_state_in.mouse = {};
    m_state_in.relative_mouse = {};

    // skip this input state
    m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
  }

  m_last_update = cur_time;

  HRESULT mo_hr = m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
  if (DIERR_INPUTLOST == mo_hr || DIERR_NOTACQUIRED == mo_hr)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to get state");
    if (FAILED(m_mo_device->Acquire()))
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to re-acquire, we'll retry later");
  }
  else if (SUCCEEDED(mo_hr))
  {
    m_state_in.relative_mouse.Move({tmp_mouse.lX, tmp_mouse.lY, tmp_mouse.lZ});
    m_state_in.relative_mouse.Update();

    // need to smooth out the axes, otherwise it doesn't work for shit
    for (unsigned int i = 0; i < 3; ++i)
      ((&m_state_in.mouse.lX)[i] += (&tmp_mouse.lX)[i]) /= 2;

    // copy over the buttons
    std::copy_n(tmp_mouse.rgbButtons, std::size(tmp_mouse.rgbButtons), m_state_in.mouse.rgbButtons);
  }

  HRESULT kb_hr = m_kb_device->GetDeviceState(sizeof(m_state_in.keyboard), &m_state_in.keyboard);
  if (kb_hr == DIERR_INPUTLOST || kb_hr == DIERR_NOTACQUIRED)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to get state");
    if (SUCCEEDED(m_kb_device->Acquire()))
      m_kb_device->GetDeviceState(sizeof(m_state_in.keyboard), &m_state_in.keyboard);
    else
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to re-acquire, we'll retry later");
  }

  return Core::DeviceRemoval::Keep;
}

std::string KeyboardMouse::GetName() const
{
  return "Keyboard Mouse";
}

std::string KeyboardMouse::GetSource() const
{
  return DINPUT_SOURCE_NAME;
}

// Give this device a higher priority to make sure it shows first
int KeyboardMouse::GetSortPriority() const
{
  return DEFAULT_DEVICE_SORT_PRIORITY;
}

bool KeyboardMouse::IsVirtualDevice() const
{
  return true;
}

// names
std::string KeyboardMouse::Key::GetName() const
{
  return named_keys[m_index].name;
}

std::string KeyboardMouse::Button::GetName() const
{
  return std::string("Click ") + char('0' + m_index);
}

std::string KeyboardMouse::Axis::GetName() const
{
  char tmpstr[] = "Axis ..";
  tmpstr[5] = (char)('X' + m_index);
  tmpstr[6] = (m_range < 0 ? '-' : '+');
  return tmpstr;
}

std::string KeyboardMouse::Cursor::GetName() const
{
  char tmpstr[] = "Cursor ..";
  tmpstr[7] = (char)('X' + m_index);
  tmpstr[8] = (m_positive ? '+' : '-');
  return tmpstr;
}

// get/set state
ControlState KeyboardMouse::Key::GetState() const
{
  return (m_key != 0);
}

ControlState KeyboardMouse::Button::GetState() const
{
  return (m_button != 0);
}

ControlState KeyboardMouse::Axis::GetState() const
{
  return ControlState(m_axis) / m_range;
}

ControlState KeyboardMouse::Cursor::GetState() const
{
  return m_axis / (m_positive ? 1.0 : -1.0);
}
}  // namespace ciface::DInput
