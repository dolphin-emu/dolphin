// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <cstring>

#include "Core/ConfigManager.h"
#include "VideoCommon/VideoConfig.h"
#include "InputCommon/ControllerInterface/Xlib/Xlib.h"

namespace ciface
{
namespace Xlib
{
void Init(void* const hwnd)
{
  g_controller_interface.AddDevice(std::make_shared<KeyboardMouse>((Window)hwnd));
}

KeyboardMouse::KeyboardMouse(Window window) : m_window(window)
{
  memset(&m_state, 0, sizeof(m_state));

  m_display = XOpenDisplay(nullptr);

  int min_keycode, max_keycode;
  XDisplayKeycodes(m_display, &min_keycode, &max_keycode);

  // Keyboard Keys
  for (int i = min_keycode; i <= max_keycode; ++i)
  {
    Key* temp_key = new Key(m_display, i, m_state.keyboard);
    if (temp_key->m_keyname.length())
      AddInput(temp_key);
    else
      delete temp_key;
  }

  // Mouse Buttons
  AddInput(new Button(Button1Mask, m_state.buttons));
  AddInput(new Button(Button2Mask, m_state.buttons));
  AddInput(new Button(Button3Mask, m_state.buttons));
  AddInput(new Button(Button4Mask, m_state.buttons));
  AddInput(new Button(Button5Mask, m_state.buttons));

  // Mouse Cursor, X-/+ and Y-/+
  for (int i = 0; i != 4; ++i)
    AddInput(new Cursor(!!(i & 2), !!(i & 1), (&m_state.cursor.x)[!!(i & 2)]));
}

KeyboardMouse::~KeyboardMouse()
{
  XCloseDisplay(m_display);
}

void KeyboardMouse::UpdateInput()
{
  XQueryKeymap(m_display, m_state.keyboard);

  int root_x, root_y, win_x, win_y;
  Window root, child;
  XQueryPointer(m_display, m_window, &root, &child, &root_x, &root_y, &win_x, &win_y,
                &m_state.buttons);

  // update mouse cursor
  XWindowAttributes win_attribs;
  XGetWindowAttributes(m_display, m_window, &win_attribs);
  
  // the mouse position as a range from -1 to 1
  static const int& root_xpos    = SConfig::GetInstance().iPosX;
  static const int& root_ypos    = SConfig::GetInstance().iPosY;
  static const int& root_width   = SConfig::GetInstance().iWidth;
  static const int& root_height  = SConfig::GetInstance().iHeight;
  
  static const int& child_xpos   = SConfig::GetInstance().iRenderWindowXPos;
  static const int& child_ypos   = SConfig::GetInstance().iRenderWindowYPos;
  static const int& child_width  = SConfig::GetInstance().iRenderWindowWidth;
  static const int& child_height = SConfig::GetInstance().iRenderWindowHeight;
  
  static const bool& isfullscreen = !SConfig::GetInstance().bFullscreen;
  static const bool& rendertomain = !SConfig::GetInstance().bRenderToMain;
  
  static const int screen_number    = XScreenNumberOfScreen(win_attribs.screen);
  static const int m_display_width  = XDisplayWidth(m_display,screen_number);
  static const int m_display_height = XDisplayHeight(m_display,screen_number);
  
  static const double aspect = ASPECT_AUTO==2 ? 4.0/3.0: 16.0/9.0;
  
  double x_margin   = 0,   y_margin   = 0;
  
  int    x_offset   = 0,   y_offset   = 0,
         x_offset2  = 0,   y_offset2  = 0,
         win_width  = 640, win_height = 528,
         suboffset1 = 0,   suboffset2 = 0;
  
  if(isfullscreen)
  {
    if(rendertomain)
    {
	  win_height = child_height;
	  win_width  = child_width;
      x_offset   = root_xpos-child_xpos + 6;
      y_offset   = root_ypos-child_ypos - 5;
      x_offset2  = 11;
      y_offset2  = 10;
    }
	else
	{
      suboffset1 = 95;
      suboffset2 = 20;
	  win_height = root_height - suboffset1 - suboffset2;
	  win_width  = root_width;
      x_offset   = -3;
      y_offset   =  8;
      x_offset2  = -18;
      y_offset2  =  19;
    }
  }
  else
  {
	win_height = m_display_height;
	win_width  = m_display_width;
    x_offset   = root_xpos + 11;
    y_offset   = root_ypos -  5;
    x_offset2  = 11;
    y_offset2  = 10;
  }
  
  if(win_width/win_height > aspect)
  {
    x_margin = win_width  - win_height*aspect;
  }
  else if(win_width/win_height < aspect)
  {
    y_margin = win_height - win_width/aspect;
  }
  
  m_state.cursor.x = (float)(win_x + x_offset              - x_margin/2.0) / (float)(win_width  - x_margin + x_offset2) * 2 - 1;
  m_state.cursor.y = (float)(win_y + y_offset - suboffset1 - y_margin/2.0) / (float)(win_height - y_margin - y_offset2) * 2 - 1;
}

std::string KeyboardMouse::GetName() const
{
  return "Keyboard Mouse";
}

std::string KeyboardMouse::GetSource() const
{
  return "Xlib";
}

int KeyboardMouse::GetId() const
{
  return 0;
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

ControlState KeyboardMouse::Button::GetState() const
{
  return ((m_buttons & m_index) != 0);
}

ControlState KeyboardMouse::Cursor::GetState() const
{
  return std::max(0.0f, m_cursor / (m_positive ? 1.0f : -1.0f));
}

std::string KeyboardMouse::Key::GetName() const
{
  return m_keyname;
}

std::string KeyboardMouse::Cursor::GetName() const
{
  static char tmpstr[] = "Cursor ..";
  tmpstr[7] = (char)('X' + m_index);
  tmpstr[8] = (m_positive ? '+' : '-');
  return tmpstr;
}

std::string KeyboardMouse::Button::GetName() const
{
  char button = '0';
  switch (m_index)
  {
  case Button1Mask:
    button = '1';
    break;
  case Button2Mask:
    button = '2';
    break;
  case Button3Mask:
    button = '3';
    break;
  case Button4Mask:
    button = '4';
    break;
  case Button5Mask:
    button = '5';
    break;
  }

  static char tmpstr[] = "Click .";
  tmpstr[6] = button;
  return tmpstr;
}
}
}
