// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <memory>
#include <thread>

class GLX11Window
{
public:
  GLX11Window(Display* display, Window parent_window, Colormap color_map, Window window, int width,
              int height);
  ~GLX11Window();

  Display* GetDisplay() const { return m_display; }
  Window GetParentWindow() const { return m_parent_window; }
  Window GetWindow() const { return m_window; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }

  void UpdateDimensions();

  static std::unique_ptr<GLX11Window> Create(Display* display, Window parent_window,
                                             XVisualInfo* vi);

private:
  Display* m_display;
  Window m_parent_window;
  Colormap m_color_map;
  Window m_window;

  int m_width;
  int m_height;
};
