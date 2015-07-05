// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if HAVE_X11
#include <X11/keysym.h>
#include <string>
#include "DolphinNoGUI/Platform.h"
#include "DolphinWX/X11Utils.h"

class PlatformX11 : public Platform
{
  Display* dpy;
  Window win;
  Cursor blankCursor = None;
#if defined(HAVE_XRANDR) && HAVE_XRANDR
  X11Utils::XRRConfiguration* XRRConfig;
#endif

  void Init() override
  {
    XInitThreads();
    dpy = XOpenDisplay(nullptr);
    if (!dpy)
    {
      PanicAlert("No X11 display found");
      exit(1);
    }

    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), SConfig::GetInstance().iRenderWindowXPos,
                              SConfig::GetInstance().iRenderWindowYPos,
                              SConfig::GetInstance().iRenderWindowWidth,
                              SConfig::GetInstance().iRenderWindowHeight, 0, 0, BlackPixel(dpy, 0));
    XSelectInput(dpy, win, StructureNotifyMask | KeyPressMask | FocusChangeMask);
    Atom wmProtocols[1];
    wmProtocols[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(dpy, win, wmProtocols, 1);
    XMapRaised(dpy, win);
    XFlush(dpy);
    s_window_handle = (void*)win;

    if (SConfig::GetInstance().bDisableScreenSaver)
      X11Utils::InhibitScreensaver(dpy, win, true);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
    XRRConfig = new X11Utils::XRRConfiguration(dpy, win);
#endif

    if (SConfig::GetInstance().bHideCursor)
    {
      // make a blank cursor
      Pixmap Blank;
      XColor DummyColor;
      char ZeroData[1] = {0};
      Blank = XCreateBitmapFromData(dpy, win, ZeroData, 1, 1);
      blankCursor = XCreatePixmapCursor(dpy, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
      XFreePixmap(dpy, Blank);
      XDefineCursor(dpy, win, blankCursor);
    }
  }

  void SetTitle(const std::string& string) override { XStoreName(dpy, win, string.c_str()); }
  void MainLoop() override
  {
    bool fullscreen = SConfig::GetInstance().bFullscreen;
    int last_window_width = SConfig::GetInstance().iRenderWindowWidth;
    int last_window_height = SConfig::GetInstance().iRenderWindowHeight;

    if (fullscreen)
    {
      rendererIsFullscreen = X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
      XRRConfig->ToggleDisplayMode(True);
#endif
    }

    // The actual loop
    while (s_running.IsSet())
    {
      if (s_shutdown_requested.TestAndClear())
      {
        const auto& stm = WII_IPC_HLE_Interface::GetDeviceByName("/dev/stm/eventhook");
        if (!s_tried_graceful_shutdown.IsSet() && stm &&
            std::static_pointer_cast<CWII_IPC_HLE_Device_stm_eventhook>(stm)->HasHookInstalled())
        {
          ProcessorInterface::PowerButton_Tap();
          s_tried_graceful_shutdown.Set();
        }
        else
        {
          s_running.Clear();
        }
      }

      XEvent event;
      KeySym key;
      for (int num_events = XPending(dpy); num_events > 0; num_events--)
      {
        XNextEvent(dpy, &event);
        switch (event.type)
        {
        case KeyPress:
          key = XLookupKeysym((XKeyEvent*)&event, 0);
          if (key == XK_Escape)
          {
            if (Core::GetState() == Core::CORE_RUN)
            {
              if (SConfig::GetInstance().bHideCursor)
                XUndefineCursor(dpy, win);
              Core::SetState(Core::CORE_PAUSE);
            }
            else
            {
              if (SConfig::GetInstance().bHideCursor)
                XDefineCursor(dpy, win, blankCursor);
              Core::SetState(Core::CORE_RUN);
            }
          }
          else if ((key == XK_Return) && (event.xkey.state & Mod1Mask))
          {
            fullscreen = !fullscreen;
            X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
            XRRConfig->ToggleDisplayMode(fullscreen);
#endif
          }
          else if (key >= XK_F1 && key <= XK_F8)
          {
            int slot_number = key - XK_F1 + 1;
            if (event.xkey.state & ShiftMask)
              State::Save(slot_number);
            else
              State::Load(slot_number);
          }
          else if (key == XK_F9)
            Core::SaveScreenShot();
          else if (key == XK_F11)
            State::LoadLastSaved();
          else if (key == XK_F12)
          {
            if (event.xkey.state & ShiftMask)
              State::UndoLoadState();
            else
              State::UndoSaveState();
          }
          break;
        case FocusIn:
          rendererHasFocus = true;
          if (SConfig::GetInstance().bHideCursor && Core::GetState() != Core::CORE_PAUSE)
            XDefineCursor(dpy, win, blankCursor);
          break;
        case FocusOut:
          rendererHasFocus = false;
          if (SConfig::GetInstance().bHideCursor)
            XUndefineCursor(dpy, win);
          break;
        case ClientMessage:
          if ((unsigned long)event.xclient.data.l[0] == XInternAtom(dpy, "WM_DELETE_WINDOW", False))
            s_shutdown_requested.Set();
          break;
        case ConfigureNotify:
        {
          if (last_window_width != event.xconfigure.width ||
              last_window_height != event.xconfigure.height)
          {
            last_window_width = event.xconfigure.width;
            last_window_height = event.xconfigure.height;

            // We call Renderer::ChangeSurface here to indicate the size has changed,
            // but pass the same window handle. This is needed for the Vulkan backend,
            // otherwise it cannot tell that the window has been resized on some drivers.
            if (g_renderer)
              g_renderer->ChangeSurface(s_window_handle);
          }
        }
        break;
        }
      }
      if (!fullscreen)
      {
        Window winDummy;
        unsigned int borderDummy, depthDummy;
        XGetGeometry(dpy, win, &winDummy, &SConfig::GetInstance().iRenderWindowXPos,
                     &SConfig::GetInstance().iRenderWindowYPos,
                     (unsigned int*)&SConfig::GetInstance().iRenderWindowWidth,
                     (unsigned int*)&SConfig::GetInstance().iRenderWindowHeight, &borderDummy,
                     &depthDummy);
        rendererIsFullscreen = false;
      }
      Core::HostDispatchJobs();
      usleep(100000);
    }
  }

  void Shutdown() override
  {
#if defined(HAVE_XRANDR) && HAVE_XRANDR
    delete XRRConfig;
#endif

    if (SConfig::GetInstance().bHideCursor)
      XFreeCursor(dpy, blankCursor);

    XCloseDisplay(dpy);
  }
};
#endif
