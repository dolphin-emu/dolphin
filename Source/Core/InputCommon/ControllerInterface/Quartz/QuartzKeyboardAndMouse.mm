// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

#include <map>
#include <mutex>

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>

#include "Core/Host.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Quartz/Quartz.h"

/// Helper class to get window position data from threads other than the main thread
@interface DolWindowPositionObserver : NSObject

- (instancetype)initWithView:(NSView*)view;
@property(readonly) NSRect frame;

@end

@implementation DolWindowPositionObserver
{
  NSView* _view;
  NSWindow* _window;
  NSRect _frame;
  std::mutex _mtx;
}

- (NSRect)calcFrame
{
  return [_window convertRectToScreen:[_view frame]];
}

- (instancetype)initWithView:(NSView*)view
{
  self = [super init];
  if (self)
  {
    _view = view;
    _window = [view window];
    _frame = [self calcFrame];
    [_window addObserver:self forKeyPath:@"frame" options:0 context:nil];
  }
  return self;
}

- (NSRect)frame
{
  std::lock_guard<std::mutex> guard(_mtx);
  return _frame;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context
{
  if (object == _window)
  {
    NSRect new_frame = [self calcFrame];
    std::lock_guard<std::mutex> guard(_mtx);
    _frame = new_frame;
  }
}

- (void)dealloc
{
  [_window removeObserver:self forKeyPath:@"frame"];
}

@end

namespace ciface::Quartz
{
std::string KeycodeToName(const CGKeyCode keycode)
{
  static const std::map<CGKeyCode, std::string> named_keys = {
      {kVK_ANSI_A, "A"},
      {kVK_ANSI_B, "B"},
      {kVK_ANSI_C, "C"},
      {kVK_ANSI_D, "D"},
      {kVK_ANSI_E, "E"},
      {kVK_ANSI_F, "F"},
      {kVK_ANSI_G, "G"},
      {kVK_ANSI_H, "H"},
      {kVK_ANSI_I, "I"},
      {kVK_ANSI_J, "J"},
      {kVK_ANSI_K, "K"},
      {kVK_ANSI_L, "L"},
      {kVK_ANSI_M, "M"},
      {kVK_ANSI_N, "N"},
      {kVK_ANSI_O, "O"},
      {kVK_ANSI_P, "P"},
      {kVK_ANSI_Q, "Q"},
      {kVK_ANSI_R, "R"},
      {kVK_ANSI_S, "S"},
      {kVK_ANSI_T, "T"},
      {kVK_ANSI_U, "U"},
      {kVK_ANSI_V, "V"},
      {kVK_ANSI_W, "W"},
      {kVK_ANSI_X, "X"},
      {kVK_ANSI_Y, "Y"},
      {kVK_ANSI_Z, "Z"},
      {kVK_ANSI_1, "1"},
      {kVK_ANSI_2, "2"},
      {kVK_ANSI_3, "3"},
      {kVK_ANSI_4, "4"},
      {kVK_ANSI_5, "5"},
      {kVK_ANSI_6, "6"},
      {kVK_ANSI_7, "7"},
      {kVK_ANSI_8, "8"},
      {kVK_ANSI_9, "9"},
      {kVK_ANSI_0, "0"},
      {kVK_Return, "Return"},
      {kVK_Escape, "Escape"},
      {kVK_Delete, "Backspace"},
      {kVK_Tab, "Tab"},
      {kVK_Space, "Space"},
      {kVK_ANSI_Minus, "-"},
      {kVK_ANSI_Equal, "="},
      {kVK_ANSI_LeftBracket, "["},
      {kVK_ANSI_RightBracket, "]"},
      {kVK_ANSI_Backslash, "\\"},
      {kVK_ANSI_Semicolon, ";"},
      {kVK_ANSI_Quote, "'"},
      {kVK_ANSI_Grave, "Tilde"},
      {kVK_ANSI_Comma, ","},
      {kVK_ANSI_Period, "."},
      {kVK_ANSI_Slash, "/"},
      {kVK_CapsLock, "Caps Lock"},
      {kVK_F1, "F1"},
      {kVK_F2, "F2"},
      {kVK_F3, "F3"},
      {kVK_F4, "F4"},
      {kVK_F5, "F5"},
      {kVK_F6, "F6"},
      {kVK_F7, "F7"},
      {kVK_F8, "F8"},
      {kVK_F9, "F9"},
      {kVK_F10, "F10"},
      {kVK_F11, "F11"},
      {kVK_F12, "F12"},
      {kVK_Home, "Home"},
      {kVK_PageUp, "Page Up"},
      {kVK_ForwardDelete, "Delete"},
      {kVK_End, "End"},
      {kVK_PageDown, "Page Down"},
      {kVK_RightArrow, "Right Arrow"},
      {kVK_LeftArrow, "Left Arrow"},
      {kVK_DownArrow, "Down Arrow"},
      {kVK_UpArrow, "Up Arrow"},
      {kVK_ANSI_KeypadDivide, "Keypad /"},
      {kVK_ANSI_KeypadMultiply, "Keypad *"},
      {kVK_ANSI_KeypadMinus, "Keypad -"},
      {kVK_ANSI_KeypadPlus, "Keypad +"},
      {kVK_ANSI_KeypadEnter, "Keypad Enter"},
      {kVK_ANSI_Keypad1, "Keypad 1"},
      {kVK_ANSI_Keypad2, "Keypad 2"},
      {kVK_ANSI_Keypad3, "Keypad 3"},
      {kVK_ANSI_Keypad4, "Keypad 4"},
      {kVK_ANSI_Keypad5, "Keypad 5"},
      {kVK_ANSI_Keypad6, "Keypad 6"},
      {kVK_ANSI_Keypad7, "Keypad 7"},
      {kVK_ANSI_Keypad8, "Keypad 8"},
      {kVK_ANSI_Keypad9, "Keypad 9"},
      {kVK_ANSI_Keypad0, "Keypad 0"},
      {kVK_ANSI_KeypadDecimal, "Keypad ."},
      {kVK_ANSI_KeypadEquals, "Keypad ="},
      {kVK_Control, "Left Control"},
      {kVK_Shift, "Left Shift"},
      {kVK_Option, "Left Alt"},
      {kVK_Command, "Command"},
      {kVK_RightControl, "Right Control"},
      {kVK_RightShift, "Right Shift"},
      {kVK_RightOption, "Right Alt"},
  };

  if (named_keys.find(keycode) != named_keys.end())
    return named_keys.at(keycode);
  else
    return "Key " + std::to_string(keycode);
}

KeyboardAndMouse::Key::Key(CGKeyCode keycode) : m_keycode(keycode), m_name(KeycodeToName(keycode))
{
}

ControlState KeyboardAndMouse::Key::GetState() const
{
  return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, m_keycode) != 0;
}

std::string KeyboardAndMouse::Key::GetName() const
{
  return m_name;
}

KeyboardAndMouse::KeyboardAndMouse(void* view)
{
  // All keycodes in <HIToolbox/Events.h> are 0x7e or lower. If you notice
  // keys that aren't being recognized, bump this number up!
  for (int keycode = 0; keycode < 0x80; ++keycode)
    AddInput(new Key(keycode));

  // Add combined left/right modifiers with consistent naming across platforms.
  AddCombinedInput("Alt", {"Left Alt", "Right Alt"});
  AddCombinedInput("Shift", {"Left Shift", "Right Shift"});
  AddCombinedInput("Ctrl", {"Left Control", "Right Control"});

  // PopulateDevices may be called on the Emuthread, so we need to ensure that
  // these UI APIs are only ever called on the main thread.
  if ([NSThread isMainThread])
    MainThreadInitialization(view);
  else
    dispatch_sync(dispatch_get_main_queue(), [this, view] { MainThreadInitialization(view); });

  // cursor, with a hax for-loop
  for (unsigned int i = 0; i < 4; ++i)
    AddInput(new Cursor(!!(i & 2), (&m_cursor.x)[i / 2], !!(i & 1)));

  AddInput(new Button(kCGMouseButtonLeft));
  AddInput(new Button(kCGMouseButtonRight));
  AddInput(new Button(kCGMouseButtonCenter));
}

// Very important that this is here
// C++ and ObjC++ have different views of the header, and only ObjC++'s will deallocate properly
KeyboardAndMouse::~KeyboardAndMouse() = default;

void KeyboardAndMouse::MainThreadInitialization(void* view)
{
  NSView* cocoa_view = (__bridge NSView*)view;
  m_window_pos_observer = [[DolWindowPositionObserver alloc] initWithView:cocoa_view];
}

Core::DeviceRemoval KeyboardAndMouse::UpdateInput()
{
  NSRect bounds = [m_window_pos_observer frame];

  const double window_width = std::max(bounds.size.width, 1.0);
  const double window_height = std::max(bounds.size.height, 1.0);

  if (g_controller_interface.IsMouseCenteringRequested() &&
      (Host_RendererHasFocus() || Host_TASInputHasFocus()))
  {
    m_cursor.x = 0;
    m_cursor.y = 0;

    const CGPoint window_center_global_coordinates =
        CGPointMake(bounds.origin.x + window_width / 2.0, bounds.origin.y + window_height / 2.0);
    CGWarpMouseCursorPosition(window_center_global_coordinates);
    // Without this line there is a short but obvious delay after centering the cursor before it can
    // be moved again
    CGAssociateMouseAndMouseCursorPosition(true);

    g_controller_interface.SetMouseCenteringRequested(false);
  }
  else if (!Host_TASInputHasFocus())
  {
    // When a TAS Input window has focus and "Enable Controller Input" is checked most types of
    // input should be read normally as if the render window had focus instead. The cursor is an
    // exception, as otherwise using the mouse to set any control in the TAS Input window will also
    // update the Wii IR value (or any other input controlled by the cursor).

    NSPoint loc = [NSEvent mouseLocation];

    const auto window_scale = g_controller_interface.GetWindowInputScale();

    loc.x -= bounds.origin.x;
    loc.y -= bounds.origin.y;
    m_cursor.x = (loc.x / window_width * 2 - 1.0) * window_scale.x;
    m_cursor.y = (loc.y / window_height * 2 - 1.0) * -window_scale.y;
  }

  return Core::DeviceRemoval::Keep;
}

std::string KeyboardAndMouse::GetName() const
{
  return "Keyboard & Mouse";
}

std::string KeyboardAndMouse::GetSource() const
{
  return Quartz::GetSourceName();
}

int KeyboardAndMouse::GetSortPriority() const
{
  return DEFAULT_DEVICE_SORT_PRIORITY;
}

ControlState KeyboardAndMouse::Cursor::GetState() const
{
  return std::max(0.0, ControlState(m_axis) / (m_positive ? 1.0 : -1.0));
}

ControlState KeyboardAndMouse::Button::GetState() const
{
  return CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, m_button) != 0;
}

std::string KeyboardAndMouse::Cursor::GetName() const
{
  char tmpstr[] = "Cursor ..";
  tmpstr[7] = (char)('X' + m_index);
  tmpstr[8] = (m_positive ? '+' : '-');
  return tmpstr;
}

std::string KeyboardAndMouse::Button::GetName() const
{
  if (m_button == kCGMouseButtonLeft)
    return "Left Click";
  if (m_button == kCGMouseButtonCenter)
    return "Middle Click";
  if (m_button == kCGMouseButtonRight)
    return "Right Click";
  return std::string("Click ") + char('0' + m_button);
}
}  // namespace ciface::Quartz
