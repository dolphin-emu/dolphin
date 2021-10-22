// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/GL/GLX11Window.h"
#include "Common/GL/GLContext.h"

GLX11Window::GLX11Window(Display* display, Window parent_window, Colormap color_map, Window window,
                         int width, int height)
    : m_display(display), m_parent_window(parent_window), m_color_map(color_map), m_window(window),
      m_width(width), m_height(height)
{
}

GLX11Window::~GLX11Window()
{
  XUnmapWindow(m_display, m_window);
  XDestroyWindow(m_display, m_window);
  XFreeColormap(m_display, m_color_map);
}

void GLX11Window::UpdateDimensions()
{
  XWindowAttributes attribs;
  XGetWindowAttributes(m_display, m_parent_window, &attribs);
  XResizeWindow(m_display, m_window, attribs.width, attribs.height);
  m_width = attribs.width;
  m_height = attribs.height;
}

std::unique_ptr<GLX11Window> GLX11Window::Create(Display* display, Window parent_window,
                                                 XVisualInfo* vi)
{
  // Set color map for the new window based on the visual.
  Colormap color_map = XCreateColormap(display, parent_window, vi->visual, AllocNone);
  XSetWindowAttributes attribs = {};
  attribs.colormap = color_map;

  // Get the dimensions from the parent window.
  XWindowAttributes parent_attribs = {};
  XGetWindowAttributes(display, parent_window, &parent_attribs);

  // Create the window
  Window window =
      XCreateWindow(display, parent_window, 0, 0, parent_attribs.width, parent_attribs.height, 0,
                    vi->depth, InputOutput, vi->visual, CWColormap, &attribs);
  XSelectInput(display, parent_window, StructureNotifyMask);
  XMapWindow(display, window);
  XSync(display, True);

  return std::make_unique<GLX11Window>(display, parent_window, color_map, window,
                                       parent_attribs.width, parent_attribs.height);
}
