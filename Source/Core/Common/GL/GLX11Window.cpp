// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLX11Window.h"
#include "Common/GL/GLContext.h"
#include "Common/Thread.h"

GLX11Window::GLX11Window(Display* display, Window parent_window, Colormap color_map, Window window,
                         int width, int height)
    : m_display(display), m_parent_window(parent_window), m_color_map(color_map), m_window(window),
      m_width(width), m_height(height),
      m_event_thread(std::thread(&GLX11Window::XEventThread, this))
{
}

GLX11Window::~GLX11Window()
{
  Window window = m_window;
  m_window = None;
  if (m_event_thread.joinable())
    m_event_thread.join();
  XUnmapWindow(m_display, window);
  XDestroyWindow(m_display, window);
  XFreeColormap(m_display, m_color_map);
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

void GLX11Window::XEventThread()
{
  // There's a potential race here on m_window. But this thread will disappear soon.
  while (m_window)
  {
    XEvent event;
    for (int num_events = XPending(m_display); num_events > 0; num_events--)
    {
      XNextEvent(m_display, &event);
      switch (event.type)
      {
      case ConfigureNotify:
      {
        m_width = event.xconfigure.width;
        m_height = event.xconfigure.height;
        XResizeWindow(m_display, m_window, m_width, m_height);
        g_main_gl_context->SetBackBufferDimensions(m_width, m_height);
      }
      break;
      default:
        break;
      }
    }
    Common::SleepCurrentThread(20);
  }
}
