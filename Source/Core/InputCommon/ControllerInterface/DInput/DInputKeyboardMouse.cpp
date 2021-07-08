// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#include <algorithm>

#include "Common/Logging/Log.h"
#include "Core/Core.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"

// Just a default value which works well at 800dpi.
// Users can multiply it anyway (lower is more sensitive)
#define MOUSE_AXIS_SENSITIVITY 17

namespace ciface::DInput
{
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
  // so there can be a separated Keyboard and Mouse, as well as combined KeyboardMouse

  LPDIRECTINPUTDEVICE8 kb_device = nullptr;
  LPDIRECTINPUTDEVICE8 mo_device = nullptr;

  DIPROPDWORD dw;
  dw.dwData = DIPROPAXISMODE_ABS;
  dw.diph.dwSize = sizeof(DIPROPDWORD);
  dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  dw.diph.dwHow = DIPH_DEVICE;
  dw.diph.dwObj = 0;

  // These are "virtual" system devices, so they are always there even if we have no physical
  // mouse and keyboard plugged into the computer
  if (SUCCEEDED(idi8->CreateDevice(GUID_SysKeyboard, &kb_device, nullptr)) &&
      SUCCEEDED(kb_device->SetDataFormat(&c_dfDIKeyboard)) &&
      SUCCEEDED(kb_device->SetCooperativeLevel(nullptr, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) &&
      SUCCEEDED(idi8->CreateDevice(GUID_SysMouse, &mo_device, nullptr)) &&
      SUCCEEDED(mo_device->SetDataFormat(&c_dfDIMouse2)) &&
      SUCCEEDED(mo_device->SetProperty(DIPROP_AXISMODE, &dw.diph)) &&  // Set absolute coordinates
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
    : m_kb_device(kb_device), m_mo_device(mo_device), m_state_in()
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
  mouse_caps.dwButtons = std::min(mouse_caps.dwButtons, (DWORD)sizeof(m_state_in.mouse.rgbButtons));
  for (u8 i = 0; i < mouse_caps.dwButtons; ++i)
    AddInput(new Button(i, m_state_in.mouse.rgbButtons[i]));
  // mouse axes
  mouse_caps.dwAxes = std::min(mouse_caps.dwAxes, (DWORD)3);
  for (unsigned int i = 0; i < mouse_caps.dwAxes; ++i)
  {
    // each axis gets a negative and a positive input instance associated with it
    Axis* axis = new Axis((2 == i) ? -1.0 : (-1.0 / MOUSE_AXIS_SENSITIVITY), i);
    m_mouse_axes.push_back(axis);
    AddInput(axis);
    axis = new Axis((2 == i) ? 1.0 : (1.0 / MOUSE_AXIS_SENSITIVITY), i);
    m_mouse_axes.push_back(axis);
    AddInput(axis);
  }
  // cursor, with a hax for-loop
  for (unsigned int i = 0; i <= 3; ++i)
    AddInput(new Cursor(!!(i & 2), (&m_state_in.cursor.x)[i / 2], !!(i & 1)));
}

void KeyboardMouse::UpdateCursorInput()
{
  POINT point = {};
  GetCursorPos(&point);

  // Get the cursor position relative to the upper left corner of the current window
  // (separate or render to main)
  ScreenToClient(s_hwnd, &point);

  // Get the size of the current window (in my case Rect.top and Rect.left was zero).
  RECT rect;
  GetClientRect(s_hwnd, &rect);

  // Width and height are the size of the rendering window. They could be 0
  const auto win_width = std::max(rect.right - rect.left, 1l);
  const auto win_height = std::max(rect.bottom - rect.top, 1l);

  const auto window_scale = g_controller_interface.GetWindowInputScale();

  // Convert the cursor position to a range from -1 to 1.
  m_state_in.cursor.x = (ControlState(point.x) / win_width * 2 - 1) * window_scale.x;
  m_state_in.cursor.y = (ControlState(point.y) / win_height * 2 - 1) * window_scale.y;
}

void KeyboardMouse::UpdateInput()
{
  HRESULT kb_hr = m_kb_device->GetDeviceState(sizeof(m_state_in.keyboard), &m_state_in.keyboard);
  if (DIERR_INPUTLOST == kb_hr || DIERR_NOTACQUIRED == kb_hr)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to get state");
    if (SUCCEEDED(m_kb_device->Acquire()))
      kb_hr = m_kb_device->GetDeviceState(sizeof(m_state_in.keyboard), &m_state_in.keyboard);
    else
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Keyboard device failed to re-acquire, we'll retry later");
  }

  UpdateCursorInput();

  DIMOUSESTATE2 tmp_mouse;

  HRESULT mo_hr = m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
  if (DIERR_INPUTLOST == mo_hr || DIERR_NOTACQUIRED == mo_hr)
  {
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to get state");
    // We assume in case the mouse device failed to retrieve the state once, that the
    // state will somehow be reset. This probably can't even happen as its an emulated device
    for (unsigned int i = 0; i < m_mouse_axes.size(); ++i)
      m_mouse_axes[i]->ResetAllStates();
    if (SUCCEEDED(m_mo_device->Acquire()))
      mo_hr = m_mo_device->GetDeviceState(sizeof(tmp_mouse), &tmp_mouse);
    else
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Mouse device failed to re-acquire, we'll retry later");
  }
  if (SUCCEEDED(mo_hr))
  {
    m_state_in.mouse = tmp_mouse;
    for (unsigned int i = 0; i < m_mouse_axes.size(); ++i)
    {
      const LONG& axis = (&m_state_in.mouse.lX)[i / 2];
      m_mouse_axes[i]->UpdateState(axis);
    }
  }
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
  return 5;
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
  static char tmpstr[] = "Axis ..";
  tmpstr[5] = (char)('X' + m_index);
  tmpstr[6] = (m_scale < 0.0 ? '-' : '+');
  return tmpstr;
}

std::string KeyboardMouse::Cursor::GetName() const
{
  static char tmpstr[] = "Cursor ..";
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

ControlState KeyboardMouse::Cursor::GetState() const
{
  return m_axis * (m_positive ? 1.0 : -1.0);
}
}  // namespace ciface::DInput
