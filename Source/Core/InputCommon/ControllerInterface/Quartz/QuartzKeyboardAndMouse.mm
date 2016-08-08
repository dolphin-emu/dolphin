// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

namespace ciface
{
namespace Quartz
{

KeyboardAndMouse::Key::Key(CGKeyCode keycode) : m_keycode(keycode)
{
  static const struct PrettyKeys
  {
    const CGKeyCode code;
    const char* const name;
  } named_keys[] = {
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
      // {kHIDUsage_KeyboardInsert, "Insert"},
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
      // {kHIDUsage_KeyboardNonUSBackslash, "Paragraph"},
      {kVK_ANSI_KeypadEquals, "Keypad ="},
      // {kHIDUsage_KeypadComma, "Keypad ,"},
      {kVK_Control, "Left Control"},
      {kVK_Shift, "Left Shift"},
      {kVK_Option, "Left Alt"},
      {kVK_Command, "Left Command"},
      {kVK_RightControl, "Right Control"},
      {kVK_RightShift, "Right Shift"},
      {kVK_RightOption, "Right Alt"},
      // {kVK_RightCommand, "Right Command"},
  };

  for (auto& named_key : named_keys)
  {
    if (named_key.code == m_keycode)
    {
      m_name = named_key.name;
      return;
    }
  }

  std::stringstream ss;
  ss << "Key " << keycode;
  m_name = ss.str();
}

ControlState KeyboardAndMouse::Key::GetState() const
{
  return CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, m_keycode) != 0;
}

std::string KeyboardAndMouse::Key::GetName() const
{
  return m_name;
}

KeyboardAndMouse::KeyboardAndMouse(void* window)
{
  // All ANSI keycodes in <HIToolbox/Events.h> are 0x7e or lower
  for (int i = 0; i < 0x80; i++)
    AddInput(new Key(i));

  m_windowid = [[reinterpret_cast<NSView*>(window) window] windowNumber];

  // cursor, with a hax for-loop
  for (unsigned int i = 0; i < 4; ++i)
    AddInput(new Cursor(!!(i & 2), (&m_cursor.x)[i / 2], !!(i & 1)));

  AddInput(new Button(kCGMouseButtonLeft));
  AddInput(new Button(kCGMouseButtonRight));
  AddInput(new Button(kCGMouseButtonCenter));
}

void KeyboardAndMouse::UpdateInput()
{
  CGRect bounds = CGRectZero;
  uint32_t windowid[1] = {m_windowid};
  CFArrayRef windowArray = CFArrayCreate(nullptr, (const void**)windowid, 1, nullptr);
  CFArrayRef windowDescriptions = CGWindowListCreateDescriptionFromArray(windowArray);
  CFDictionaryRef windowDescription =
      (CFDictionaryRef)CFArrayGetValueAtIndex((CFArrayRef)windowDescriptions, 0);

  if (CFDictionaryContainsKey(windowDescription, kCGWindowBounds))
  {
    CFDictionaryRef boundsDictionary =
        (CFDictionaryRef)CFDictionaryGetValue(windowDescription, kCGWindowBounds);

    if (boundsDictionary != nullptr)
      CGRectMakeWithDictionaryRepresentation(boundsDictionary, &bounds);
  }

  CFRelease(windowDescriptions);
  CFRelease(windowArray);

  CGEventRef event = CGEventCreate(nil);
  CGPoint loc = CGEventGetLocation(event);
  CFRelease(event);

  loc.x -= bounds.origin.x;
  loc.y -= bounds.origin.y;
  m_cursor.x = loc.x / bounds.size.width * 2 - 1.0;
  m_cursor.y = loc.y / bounds.size.height * 2 - 1.0;
}

std::string KeyboardAndMouse::GetName() const
{
  return "Keyboard & Mouse";
}

std::string KeyboardAndMouse::GetSource() const
{
  return "Quartz";
}

ControlState KeyboardAndMouse::Cursor::GetState() const
{
  return std::max(0.0, ControlState(m_axis) / (m_positive ? 1.0 : -1.0));
}

ControlState KeyboardAndMouse::Button::GetState() const
{
  return CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, m_button) != 0;
  // return (m_button != 0);
}

std::string KeyboardAndMouse::Cursor::GetName() const
{
  static char tmpstr[] = "Cursor ..";
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
}
}
