// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <unistd.h>

// X.h defines None to be 0L, but other parts of Dolphin undef that so that
// None can be used in enums.  Work around that here by copying the definition
// before it is undefined.
#include <X11/X.h>
static constexpr auto X_None = None;

#include "DolphinNoGUI/Platform.h"

#include "Common/MsgHandler.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "Core/System.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <thread>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "UICommon/X11Utils.h"
#include "VideoCommon/Present.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

namespace
{
class PlatformX11 : public Platform
{
public:
  ~PlatformX11() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  void CloseDisplay();
  void UpdateWindowPosition();
  void ProcessEvents();

  Display* m_display = nullptr;
  Window m_window = {};
  Cursor m_blank_cursor = X_None;
#ifdef HAVE_XRANDR
  X11Utils::XRRConfiguration* m_xrr_config = nullptr;
#endif
  int m_window_x = Config::Get(Config::MAIN_RENDER_WINDOW_XPOS);
  int m_window_y = Config::Get(Config::MAIN_RENDER_WINDOW_YPOS);
  unsigned int m_window_width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  unsigned int m_window_height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
};

PlatformX11::~PlatformX11()
{
#ifdef HAVE_XRANDR
  delete m_xrr_config;
#endif

  if (m_display)
  {
    if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never)
      XFreeCursor(m_display, m_blank_cursor);

    XCloseDisplay(m_display);
  }
}

bool PlatformX11::Init()
{
  XInitThreads();
  m_display = XOpenDisplay(nullptr);
  if (!m_display)
  {
    PanicAlertFmt("No X11 display found");
    return false;
  }

  m_window = XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), m_window_x, m_window_y,
                                 m_window_width, m_window_height, 0, 0, BlackPixel(m_display, 0));
  XSelectInput(m_display, m_window, StructureNotifyMask | KeyPressMask | FocusChangeMask);
  Atom wmProtocols[1];
  wmProtocols[0] = XInternAtom(m_display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(m_display, m_window, wmProtocols, 1);
  pid_t pid = getpid();
  XChangeProperty(m_display, m_window, XInternAtom(m_display, "_NET_WM_PID", False), XA_CARDINAL,
                  32, PropModeReplace, reinterpret_cast<unsigned char*>(&pid), 1);
  char host_name[HOST_NAME_MAX] = "";
  if (!gethostname(host_name, sizeof(host_name)))
  {
    XTextProperty wmClientMachine = {reinterpret_cast<unsigned char*>(host_name), XA_STRING, 8,
                                     strlen(host_name)};
    XSetWMClientMachine(m_display, m_window, &wmClientMachine);
  }
  XMapRaised(m_display, m_window);
  XFlush(m_display);
  XSync(m_display, True);
  ProcessEvents();

  if (Config::Get(Config::MAIN_DISABLE_SCREENSAVER))
    X11Utils::InhibitScreensaver(m_window, true);

#ifdef HAVE_XRANDR
  m_xrr_config = new X11Utils::XRRConfiguration(m_display, m_window);
#endif

  if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never)
  {
    // make a blank cursor
    Pixmap Blank;
    XColor DummyColor;
    char ZeroData[1] = {0};
    Blank = XCreateBitmapFromData(m_display, m_window, ZeroData, 1, 1);
    m_blank_cursor = XCreatePixmapCursor(m_display, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
    XFreePixmap(m_display, Blank);
    XDefineCursor(m_display, m_window, m_blank_cursor);
  }

  // Enter fullscreen if enabled.
  if (Config::Get(Config::MAIN_FULLSCREEN))
  {
    m_window_fullscreen = X11Utils::ToggleFullscreen(m_display, m_window);
#ifdef HAVE_XRANDR
    m_xrr_config->ToggleDisplayMode(True);
#endif
    ProcessEvents();
  }

  UpdateWindowPosition();
  return true;
}

void PlatformX11::SetTitle(const std::string& string)
{
  XStoreName(m_display, m_window, string.c_str());
}

void PlatformX11::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());
    ProcessEvents();
    UpdateWindowPosition();

    // TODO: Is this sleep appropriate?
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformX11::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::X11;
  wsi.display_connection = static_cast<void*>(m_display);
  wsi.render_window = reinterpret_cast<void*>(m_window);
  wsi.render_surface = reinterpret_cast<void*>(m_window);
  return wsi;
}

void PlatformX11::UpdateWindowPosition()
{
  if (m_window_fullscreen)
    return;

  Window winDummy;
  unsigned int borderDummy, depthDummy;
  XGetGeometry(m_display, m_window, &winDummy, &m_window_x, &m_window_y, &m_window_width,
               &m_window_height, &borderDummy, &depthDummy);
}

void PlatformX11::ProcessEvents()
{
  XEvent event;
  KeySym key;
  for (int num_events = XPending(m_display); num_events > 0; num_events--)
  {
    XNextEvent(m_display, &event);
    switch (event.type)
    {
    case KeyPress:
      key = XLookupKeysym((XKeyEvent*)&event, 0);
      if (key == XK_Escape)
      {
        RequestShutdown();
      }
      else if (key == XK_F10)
      {
        if (Core::GetState(Core::System::GetInstance()) == Core::State::Running)
        {
          if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never)
            XUndefineCursor(m_display, m_window);
          Core::SetState(Core::System::GetInstance(), Core::State::Paused);
        }
        else
        {
          if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never)
            XDefineCursor(m_display, m_window, m_blank_cursor);
          Core::SetState(Core::System::GetInstance(), Core::State::Running);
        }
      }
      else if ((key == XK_Return) && (event.xkey.state & Mod1Mask))
      {
        m_window_fullscreen = !m_window_fullscreen;
        X11Utils::ToggleFullscreen(m_display, m_window);
#ifdef HAVE_XRANDR
        m_xrr_config->ToggleDisplayMode(m_window_fullscreen);
#endif
        UpdateWindowPosition();
      }
      else if (key >= XK_F1 && key <= XK_F8)
      {
        int slot_number = key - XK_F1 + 1;
        if (event.xkey.state & ShiftMask)
          State::Save(Core::System::GetInstance(), slot_number);
        else
          State::Load(Core::System::GetInstance(), slot_number);
      }
      else if (key == XK_F9)
        Core::SaveScreenShot();
      else if (key == XK_F11)
        State::LoadLastSaved(Core::System::GetInstance());
      else if (key == XK_F12)
      {
        if (event.xkey.state & ShiftMask)
          State::UndoLoadState(Core::System::GetInstance());
        else
          State::UndoSaveState(Core::System::GetInstance());
      }
      break;
    case FocusIn:
    {
      m_window_focus = true;
      if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never &&
          Core::GetState(Core::System::GetInstance()) != Core::State::Paused)
      {
        XDefineCursor(m_display, m_window, m_blank_cursor);
      }
    }
    break;
    case FocusOut:
    {
      m_window_focus = false;
      if (Config::Get(Config::MAIN_SHOW_CURSOR) == Config::ShowCursor::Never)
        XUndefineCursor(m_display, m_window);
    }
    break;
    case ClientMessage:
    {
      if ((unsigned long)event.xclient.data.l[0] ==
          XInternAtom(m_display, "WM_DELETE_WINDOW", False))
        Stop();
    }
    break;
    case ConfigureNotify:
    {
      if (g_presenter)
        g_presenter->ResizeSurface();
    }
    break;
    }
  }
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateX11Platform()
{
  return std::make_unique<PlatformX11>();
}
