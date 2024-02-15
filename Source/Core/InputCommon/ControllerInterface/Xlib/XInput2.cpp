// Copyright 2013 Max Eliaser
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Xlib/XInput2.h"

#include <X11/XKBlib.h>

#include <X11/extensions/XInput2.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/Host.h"

// This is an input plugin using the XInput 2.0 extension to the X11 protocol,
// loosely based on the old XLib plugin. (Has nothing to do with the XInput
// API on Windows.)

// This plugin creates one KeyboardMouse object for each master pointer/
// keyboard pair. Each KeyboardMouse object exports four types of controls:
// *    Mouse button controls: hardcoded at 32 of them, but could be made to
//      support infinitely many mouse buttons in theory; XInput2 has no limit.
// *    Mouse cursor controls: one for each cardinal direction. Calculated by
//      comparing the absolute position of the mouse pointer on screen to the
//      center of the emulator window.
// *    Mouse axis controls: one for each cardinal direction. Calculated using
//      a running average of relative mouse motion on each axis.
// *    Key controls: these correspond to a limited subset of the keyboard
//      keys.

// Mouse axis control tuning. Unlike absolute mouse position, relative mouse
// motion data needs to be tweaked and smoothed out a bit to be usable.

// Mouse axis control output is simply divided by this number. In practice,
// that just means you can use a smaller "dead zone" if you bind axis controls
// to a joystick. No real need to make this customizable.
#define MOUSE_AXIS_SENSITIVITY 8.0f

// The mouse axis controls use a weighted running average. Each frame, the new
// value is the average of the old value and the amount of relative mouse
// motion during that frame. The old value is weighted by a ratio of
// MOUSE_AXIS_SMOOTHING:1 compared to the new value. Increasing
// MOUSE_AXIS_SMOOTHING makes the controls smoother, decreasing it makes them
// more responsive. This might be useful as a user-customizable option.
#define MOUSE_AXIS_SMOOTHING 1.5f

// The scroll axis value should decay a lot faster than the mouse axes since
// it should ideally register each click of the scroll wheel. Decreasing this
// value makes it more likely that a scroll wheel input is registered, but less
// likely to differentiate between different inputs, while increasing it will
// more cleanly separate each scroll wheel click, but risks dropping some inputs
#define SCROLL_AXIS_DECAY 1.1f

namespace
{
// We need XInput 2.1 to get raw events on the root window even while another
// client has a grab.  If we request 2.2 or later, the server will not generate
// emulated button presses from touch events, so we want exactly 2.1.
constexpr int XINPUT_MAJOR = 2, XINPUT_MINOR = 1;
}  // namespace

namespace ciface::XInput2
{
// This function will add zero or more KeyboardMouse objects to devices.
void PopulateDevices(void* const hwnd)
{
  Display* dpy = XOpenDisplay(nullptr);

  // xi_opcode is important; it will be used to identify XInput events by
  // the polling loop in UpdateInput.
  int xi_opcode, event, error;

  // verify that the XInput extension is available
  if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error))
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "XInput extension not available (XQueryExtension)");
    return;
  }

  int major = XINPUT_MAJOR, minor = XINPUT_MINOR;
  if (XIQueryVersion(dpy, &major, &minor) != Success || major < XINPUT_MAJOR ||
      (major == XINPUT_MAJOR && minor < XINPUT_MINOR))
  {
    WARN_LOG_FMT(CONTROLLERINTERFACE, "XInput extension not available (XIQueryVersion)");
    return;
  }

  // register all master devices with Dolphin

  XIDeviceInfo* all_masters;
  XIDeviceInfo* current_master;
  double scroll_increment = 1.0f;
  int num_masters;

  all_masters = XIQueryDevice(dpy, XIAllMasterDevices, &num_masters);

  for (int i = 0; i < num_masters; i++)
  {
    current_master = &all_masters[i];

    if (current_master->use == XIMasterPointer)
    {
      // We need to query the master for the scroll wheel's increment, since the increment used
      // varies depending on what input driver is being used. For example, xf86-libinput uses 120.0.
      for (int j = 0; j < current_master->num_classes; j++)
      {
        if (current_master->classes[j]->type == XIScrollClass)
        {
          XIScrollClassInfo* scroll_event =
              reinterpret_cast<XIScrollClassInfo*>(current_master->classes[j]);
          scroll_increment = scroll_event->increment;
          break;
        }
      }
      // Since current_master is a master pointer, its attachment must
      // be a master keyboard.
      g_controller_interface.AddDevice(
          std::make_shared<KeyboardMouse>((Window)hwnd, xi_opcode, current_master->deviceid,
                                          current_master->attachment, scroll_increment));
    }
  }

  XCloseDisplay(dpy);

  XIFreeDeviceInfo(all_masters);
}

KeyboardMouse::KeyboardMouse(Window window, int opcode, int pointer, int keyboard,
                             double scroll_increment_)
    : m_window(window), xi_opcode(opcode), pointer_deviceid(pointer), keyboard_deviceid(keyboard),
      scroll_increment(scroll_increment_)
{
  // The cool thing about each KeyboardMouse object having its own Display
  // is that each one gets its own separate copy of the X11 event stream,
  // which it can individually filter to get just the events it's interested
  // in. So be aware that each KeyboardMouse object actually has its own X11
  // "context."
  m_display = XOpenDisplay(nullptr);

  int major = XINPUT_MAJOR, minor = XINPUT_MINOR;
  XIQueryVersion(m_display, &major, &minor);

  // should always be 1
  int unused;
  XIDeviceInfo* const pointer_device = XIQueryDevice(m_display, pointer_deviceid, &unused);
  name = std::string(pointer_device->name);
  XIFreeDeviceInfo(pointer_device);

  // Tell core X functions which keyboard is "the" keyboard for this
  // X connection.
  XISetClientPointer(m_display, None, pointer_deviceid);

  {
    unsigned char mask_buf[(XI_LASTEVENT + 7) / 8] = {};
    XISetMask(mask_buf, XI_RawButtonPress);
    XISetMask(mask_buf, XI_RawButtonRelease);
    XISetMask(mask_buf, XI_RawMotion);

    XIEventMask mask;
    mask.mask = mask_buf;
    mask.mask_len = sizeof(mask_buf);

    mask.deviceid = pointer_deviceid;
    XISelectEvents(m_display, DefaultRootWindow(m_display), &mask, 1);
  }

  {
    unsigned char mask_buf[(XI_LASTEVENT + 7) / 8] = {};
    XISetMask(mask_buf, XI_RawKeyPress);
    XISetMask(mask_buf, XI_RawKeyRelease);

    XIEventMask mask;
    mask.mask = mask_buf;
    mask.mask_len = sizeof(mask_buf);
    mask.deviceid = keyboard_deviceid;
    XISelectEvents(m_display, DefaultRootWindow(m_display), &mask, 1);
  }

  // Keyboard Keys
  int min_keycode, max_keycode;
  XDisplayKeycodes(m_display, &min_keycode, &max_keycode);
  for (int i = min_keycode; i <= max_keycode; ++i)
  {
    Key* const temp_key = new Key(m_display, i, m_state.keyboard.data());
    if (temp_key->m_keyname.length())
      AddInput(temp_key);
    else
      delete temp_key;
  }

  // Add combined left/right modifiers with consistent naming across platforms.
  AddCombinedInput("Alt", {"Alt_L", "Alt_R"});
  AddCombinedInput("Shift", {"Shift_L", "Shift_R"});
  AddCombinedInput("Ctrl", {"Control_L", "Control_R"});

  // Mouse Buttons
  for (int i = 0; i < 32; i++)
    AddInput(new Button(i, &m_state.buttons));

  // Mouse Cursor, X-/+ and Y-/+
  for (int i = 0; i != 4; ++i)
    AddInput(new Cursor(!!(i & 2), !!(i & 1), (i & 2) ? &m_state.cursor.y : &m_state.cursor.x));

  // Mouse Axis, X-/+, Y-/+ and Z-/+
  AddInput(new Axis(0, false, &m_state.axis.x));
  AddInput(new Axis(0, true, &m_state.axis.x));
  AddInput(new Axis(1, false, &m_state.axis.y));
  AddInput(new Axis(1, true, &m_state.axis.y));
  AddInput(new Axis(2, false, &m_state.axis.z));
  AddInput(new Axis(2, true, &m_state.axis.z));

  // Relative Mouse, X-/+, Y-/+ and Z-/+
  AddInput(new RelativeMouse(0, false, &m_state.relative_mouse.x));
  AddInput(new RelativeMouse(0, true, &m_state.relative_mouse.x));
  AddInput(new RelativeMouse(1, false, &m_state.relative_mouse.y));
  AddInput(new RelativeMouse(1, true, &m_state.relative_mouse.y));
  AddInput(new RelativeMouse(2, false, &m_state.relative_mouse.z));
  AddInput(new RelativeMouse(2, true, &m_state.relative_mouse.z));
}

KeyboardMouse::~KeyboardMouse()
{
  XCloseDisplay(m_display);
}

// Update the mouse cursor controls
void KeyboardMouse::UpdateCursor(bool should_center_mouse)
{
  double root_x, root_y, win_x, win_y;
  Window root, child;

  XWindowAttributes win_attribs;
  XGetWindowAttributes(m_display, m_window, &win_attribs);
  const auto win_width = std::max(win_attribs.width, 1);
  const auto win_height = std::max(win_attribs.height, 1);

  {
    XIButtonState button_state;
    XIModifierState mods;
    XIGroupState group;

    // Get the absolute position of the mouse pointer and the button state.
    XIQueryPointer(m_display, pointer_deviceid, m_window, &root, &child, &root_x, &root_y, &win_x,
                   &win_y, &button_state, &mods, &group);

    // X buttons are 1-indexed, so to get 32 button bits we need a larger type
    // for the shift.
    u64 buttons_zero_indexed = 0;
    std::memcpy(&buttons_zero_indexed, button_state.mask,
                std::min<size_t>(button_state.mask_len, sizeof(m_state.buttons)));
    m_state.buttons = buttons_zero_indexed >> 1;

    free(button_state.mask);
  }

  if (should_center_mouse)
  {
    win_x = win_width / 2;
    win_y = win_height / 2;

    XIWarpPointer(m_display, pointer_deviceid, None, m_window, 0.0, 0.0, 0, 0, win_x, win_y);

    g_controller_interface.SetMouseCenteringRequested(false);
  }

  const auto window_scale = g_controller_interface.GetWindowInputScale();

  // the mouse position as a range from -1 to 1
  m_state.cursor.x = (win_x / win_width * 2 - 1) * window_scale.x;
  m_state.cursor.y = (win_y / win_height * 2 - 1) * window_scale.y;
}

Core::DeviceRemoval KeyboardMouse::UpdateInput()
{
  XFlush(m_display);

  // for the axis controls
  float delta_x = 0.0f, delta_y = 0.0f, delta_z = 0.0f;
  double delta_delta;
  bool update_mouse = false, update_keyboard = false;

  // Iterate through the event queue, processing raw pointer motion events and
  // noting whether the button or key state has changed.
  XEvent event;
  while (XPending(m_display))
  {
    XNextEvent(m_display, &event);

    if (event.xcookie.type != GenericEvent)
      continue;
    if (event.xcookie.extension != xi_opcode)
      continue;
    if (!XGetEventData(m_display, &event.xcookie))
      continue;

    switch (event.xcookie.evtype)
    {
    case XI_RawButtonPress:
    case XI_RawButtonRelease:
      update_mouse = true;
      break;
    case XI_RawKeyPress:
    case XI_RawKeyRelease:
      update_keyboard = true;
      break;
    case XI_RawMotion:
    {
      update_mouse = true;

      XIRawEvent* raw_event = (XIRawEvent*)event.xcookie.data;
      float values[4] = {};
      size_t value_idx = 0;

      // We only care about the first 4 axes, which should always be available at minimum
      for (int i = 0; i < 4; ++i)
      {
        if (XIMaskIsSet(raw_event->valuators.mask, i))
        {
          values[i] = raw_event->raw_values[value_idx++];
        }
      }

      delta_delta = values[0];
      // test for inf and nan
      if (delta_delta == delta_delta && 1 + delta_delta != delta_delta)
        delta_x += delta_delta;

      delta_delta = values[1];
      // test for inf and nan
      if (delta_delta == delta_delta && 1 + delta_delta != delta_delta)
        delta_y += delta_delta;

      // Scroll wheel input gets scaled to be similar to the mouse axes
      delta_delta = values[3] * 8.0 / scroll_increment;
      // test for inf and nan
      if (delta_delta == delta_delta && 1 + delta_delta != delta_delta)
        delta_z += delta_delta;

      break;
    }
    }

    XFreeEventData(m_display, &event.xcookie);
  }

  m_state.relative_mouse.x = delta_x;
  m_state.relative_mouse.y = delta_y;
  m_state.relative_mouse.z = delta_z;

  // apply axis smoothing
  m_state.axis.x *= MOUSE_AXIS_SMOOTHING;
  m_state.axis.x += delta_x;
  m_state.axis.x /= MOUSE_AXIS_SMOOTHING + 1.0f;
  m_state.axis.y *= MOUSE_AXIS_SMOOTHING;
  m_state.axis.y += delta_y;
  m_state.axis.y /= MOUSE_AXIS_SMOOTHING + 1.0f;
  m_state.axis.z += delta_z;
  m_state.axis.z /= SCROLL_AXIS_DECAY;

  const bool should_center_mouse =
      g_controller_interface.IsMouseCenteringRequested() && Host_RendererHasFocus();
  if (update_mouse || should_center_mouse)
    UpdateCursor(should_center_mouse);

  if (update_keyboard)
    XQueryKeymap(m_display, m_state.keyboard.data());

  return Core::DeviceRemoval::Keep;
}

std::string KeyboardMouse::GetName() const
{
  // This is the name string we got from the X server for this master
  // pointer/keyboard pair.
  return name;
}

std::string KeyboardMouse::GetSource() const
{
  return "XInput2";
}

KeyboardMouse::Key::Key(Display* const display, KeyCode keycode, const char* keyboard)
    : m_display(display), m_keyboard(keyboard), m_keycode(keycode)
{
  int i = 0;
  KeySym keysym = 0;
  do
  {
    keysym = XkbKeycodeToKeysym(m_display, keycode, i, 0);
    i++;
  } while (keysym == NoSymbol && i < 8);

  // Convert to upper case for the keyname
  if (keysym >= 97 && keysym <= 122)
    keysym -= 32;

  // 0x0110ffff is the top of the unicode character range according
  // to keysymdef.h although it is probably more than we need.
  if (keysym == NoSymbol || keysym > 0x0110ffff || XKeysymToString(keysym) == nullptr)
    m_keyname = std::string();
  else
    m_keyname = std::string(XKeysymToString(keysym));
}

ControlState KeyboardMouse::Key::GetState() const
{
  return (m_keyboard[m_keycode / 8] & (1 << (m_keycode % 8))) != 0;
}

KeyboardMouse::Button::Button(unsigned int index, u32* buttons) : m_buttons(buttons), m_index(index)
{
  name = fmt::format("Click {}", m_index + 1);
}

ControlState KeyboardMouse::Button::GetState() const
{
  return ((*m_buttons & (1 << m_index)) != 0);
}

KeyboardMouse::Cursor::Cursor(u8 index, bool positive, const float* cursor)
    : m_cursor(cursor), m_index(index), m_positive(positive)
{
  name = fmt::format("Cursor {}{}", static_cast<char>('X' + m_index), (m_positive ? '+' : '-'));
}

ControlState KeyboardMouse::Cursor::GetState() const
{
  return std::max(0.0f, *m_cursor / (m_positive ? 1.0f : -1.0f));
}

KeyboardMouse::Axis::Axis(u8 index, bool positive, const float* axis)
    : m_axis(axis), m_index(index), m_positive(positive)
{
  name = fmt::format("Axis {}{}", static_cast<char>('X' + m_index), (m_positive ? '+' : '-'));
}

KeyboardMouse::RelativeMouse::RelativeMouse(u8 index, bool positive, const float* axis)
    : m_axis(axis), m_index(index), m_positive(positive)
{
  name =
      fmt::format("RelativeMouse {}{}", static_cast<char>('X' + m_index), (m_positive ? '+' : '-'));
}

ControlState KeyboardMouse::Axis::GetState() const
{
  return std::max(0.0f, *m_axis / (m_positive ? MOUSE_AXIS_SENSITIVITY : -MOUSE_AXIS_SENSITIVITY));
}

ControlState KeyboardMouse::RelativeMouse::GetState() const
{
  return std::max(0.0f, *m_axis / (m_positive ? MOUSE_AXIS_SENSITIVITY : -MOUSE_AXIS_SENSITIVITY));
}
}  // namespace ciface::XInput2
