// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/X11Utils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#ifdef HAVE_LIBSYSTEMD
#include <cstdint>
#include <memory>  // std::unique_ptr
#include <optional>
#include <utility>  // std::move

#include <systemd/sd-bus.h>

#include "Common/ScopeGuard.h"
#endif

extern char** environ;

namespace X11Utils
{
bool ToggleFullscreen(Display* dpy, Window win)
{
  // Init X event structure for _NET_WM_STATE_FULLSCREEN client message
  XEvent event;
  event.xclient.type = ClientMessage;
  event.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
  event.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

  // Send the event
  if (!XSendEvent(dpy, DefaultRootWindow(dpy), False,
                  SubstructureRedirectMask | SubstructureNotifyMask, &event))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to switch fullscreen/windowed mode.");
    return false;
  }

  return true;
}

void InhibitScreensaver(bool suspend)
{
#ifdef HAVE_LIBSYSTEMD
  struct sd_bus_unref_t
  {
    void operator()(sd_bus* bus) { sd_bus_unref(bus); }
  };

  struct ScreensaverLock
  {
    uint32_t cookie;
    std::unique_ptr<sd_bus, sd_bus_unref_t> bus;
  };

  static std::optional<ScreensaverLock> lock;
  int ret;

  if (suspend)
  {
    if (lock)
    {
      INFO_LOG_FMT(VIDEO, "Screensaver is already inhibited (cookie = {})", lock->cookie);
      return;
    }

    // Connect to the D-Bus session bus
    sd_bus* _bus;
    ret = sd_bus_default_user(&_bus);
    if (ret < 0)
    {
      WARN_LOG_FMT(VIDEO, "Cannot connect to the D-Bus session bus: {}", ret);
      return;
    }
    std::unique_ptr<sd_bus, sd_bus_unref_t> bus{_bus};

    sd_bus_error error = SD_BUS_ERROR_NULL;
    Common::ScopeGuard error_free([&error] { sd_bus_error_free(&error); });
    sd_bus_message* _reply = nullptr;

    // Prepare the method call
    ret = sd_bus_call_method(bus.get(),
                             "org.freedesktop.ScreenSaver",   // Service name
                             "/org/freedesktop/ScreenSaver",  // Object path
                             "org.freedesktop.ScreenSaver",   // Interface
                             "Inhibit",                       // Method name
                             &error, &_reply,
                             "ss",                // Input signature: string, string
                             "Dolphin",           // First argument: application name
                             "Game is running");  // Second argument: reason
    std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> reply{_reply,
                                                                           sd_bus_message_unref};

    // Check for errors
    if (ret < 0)
    {
      WARN_LOG_FMT(VIDEO, "Failed to call Inhibit: {}", error.message);
      return;
    }

    // Store cookie to reenable screensaver.
    uint32_t cookie;
    ret = sd_bus_message_read(reply.get(), "u", &cookie);
    if (ret < 0)
    {
      WARN_LOG_FMT(VIDEO, "Failed to read Inhibit reply: {}", strerror(-ret));
      return;
    }

    INFO_LOG_FMT(VIDEO, "Inhibited screensaver (cookie = {})", cookie);
    lock = ScreensaverLock{
        .cookie = cookie,
        .bus = std::move(bus),
    };
  }
  else
  {
    if (!lock)
    {
      INFO_LOG_FMT(VIDEO, "Screensaver is already allowed");
      return;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    Common::ScopeGuard error_free([&error] { sd_bus_error_free(&error); });
    sd_bus_message* reply = nullptr;

    // Cancel previous wake request.
    ret = sd_bus_call_method(lock->bus.get(),
                             "org.freedesktop.ScreenSaver",   // Service name
                             "/org/freedesktop/ScreenSaver",  // Object path
                             "org.freedesktop.ScreenSaver",   // Interface
                             "UnInhibit",                     // Method name
                             &error, &reply,
                             "u",  // Input signature: uint32
                             lock->cookie);

    if (ret < 0)
    {
      WARN_LOG_FMT(VIDEO, "Failed to call UnInhibit: {}", error.message);
    }

    sd_bus_message_unref(reply);
    lock.reset();
  }
#endif
}

#ifdef HAVE_XRANDR
XRRConfiguration::XRRConfiguration(Display* _dpy, Window _win)
    : dpy(_dpy), win(_win), screenResources(nullptr), outputInfo(nullptr), crtcInfo(nullptr),
      fullMode(0), fs_fb_width(0), fs_fb_height(0), fs_fb_width_mm(0), fs_fb_height_mm(0),
      bValid(true), bIsFullscreen(false)
{
  int XRRMajorVersion, XRRMinorVersion;

  if (!XRRQueryVersion(dpy, &XRRMajorVersion, &XRRMinorVersion) ||
      (XRRMajorVersion < 1 || (XRRMajorVersion == 1 && XRRMinorVersion < 3)))
  {
    WARN_LOG_FMT(VIDEO, "XRRExtension not supported.");
    bValid = false;
    return;
  }

  screenResources = XRRGetScreenResourcesCurrent(dpy, win);

  screen = DefaultScreen(dpy);
  fb_width = DisplayWidth(dpy, screen);
  fb_height = DisplayHeight(dpy, screen);
  fb_width_mm = DisplayWidthMM(dpy, screen);
  fb_height_mm = DisplayHeightMM(dpy, screen);

  INFO_LOG_FMT(VIDEO, "XRRExtension-Version {}.{}", XRRMajorVersion, XRRMinorVersion);
  Update();
}

XRRConfiguration::~XRRConfiguration()
{
  if (bValid && bIsFullscreen)
    ToggleDisplayMode(False);
  if (screenResources)
    XRRFreeScreenResources(screenResources);
  if (outputInfo)
    XRRFreeOutputInfo(outputInfo);
  if (crtcInfo)
    XRRFreeCrtcInfo(crtcInfo);
}

void XRRConfiguration::Update()
{
  const std::string fullscreen_display_res = Config::Get(Config::MAIN_FULLSCREEN_DISPLAY_RES);
  if (fullscreen_display_res == "Auto")
    return;

  if (!bValid)
    return;

  if (outputInfo)
  {
    XRRFreeOutputInfo(outputInfo);
    outputInfo = nullptr;
  }
  if (crtcInfo)
  {
    XRRFreeCrtcInfo(crtcInfo);
    crtcInfo = nullptr;
  }
  fullMode = 0;

  // Get the resolution setings for fullscreen mode
  unsigned int fullWidth, fullHeight;
  char* output_name = nullptr;
  char auxFlag = '\0';
  if (fullscreen_display_res.find(':') == std::string::npos)
  {
    fullWidth = fb_width;
    fullHeight = fb_height;
  }
  else
  {
    sscanf(fullscreen_display_res.c_str(), "%m[^:]: %ux%u%c", &output_name, &fullWidth, &fullHeight,
           &auxFlag);
  }
  bool want_interlaced = ('i' == auxFlag);

  for (int i = 0; i < screenResources->noutput; i++)
  {
    XRROutputInfo* output_info =
        XRRGetOutputInfo(dpy, screenResources, screenResources->outputs[i]);
    if (output_info && output_info->crtc && output_info->connection == RR_Connected)
    {
      XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(dpy, screenResources, output_info->crtc);
      if (crtc_info)
      {
        if (!output_name || !strcmp(output_name, output_info->name))
        {
          // Use the first output for the default setting.
          if (!output_name)
          {
            output_name = strdup(output_info->name);
            Config::SetBaseOrCurrent(
                Config::MAIN_FULLSCREEN_DISPLAY_RES,
                fmt::format("{}: {}x{}", output_info->name, fullWidth, fullHeight));
          }
          outputInfo = output_info;
          crtcInfo = crtc_info;
          for (int j = 0; j < output_info->nmode && fullMode == 0; j++)
          {
            for (int k = 0; k < screenResources->nmode && fullMode == 0; k++)
            {
              if (output_info->modes[j] == screenResources->modes[k].id)
              {
                if (fullWidth == screenResources->modes[k].width &&
                    fullHeight == screenResources->modes[k].height &&
                    want_interlaced == !!(screenResources->modes[k].modeFlags & RR_Interlace))
                {
                  fullMode = screenResources->modes[k].id;
                  if (crtcInfo->x + (int)screenResources->modes[k].width > fs_fb_width)
                    fs_fb_width = crtcInfo->x + screenResources->modes[k].width;
                  if (crtcInfo->y + (int)screenResources->modes[k].height > fs_fb_height)
                    fs_fb_height = crtcInfo->y + screenResources->modes[k].height;
                }
              }
            }
          }
        }
        else
        {
          if (crtc_info->x + (int)crtc_info->width > fs_fb_width)
            fs_fb_width = crtc_info->x + crtc_info->width;
          if (crtc_info->y + (int)crtc_info->height > fs_fb_height)
            fs_fb_height = crtc_info->y + crtc_info->height;
        }
      }
      if (crtc_info && crtcInfo != crtc_info)
        XRRFreeCrtcInfo(crtc_info);
    }
    if (output_info && outputInfo != output_info)
      XRRFreeOutputInfo(output_info);
  }
  fs_fb_width_mm = fs_fb_width * DisplayHeightMM(dpy, screen) / DisplayHeight(dpy, screen);
  fs_fb_height_mm = fs_fb_height * DisplayHeightMM(dpy, screen) / DisplayHeight(dpy, screen);

  if (output_name)
    free(output_name);

  if (outputInfo && crtcInfo && fullMode)
  {
    INFO_LOG_FMT(VIDEO, "Fullscreen Resolution {}x{}", fullWidth, fullHeight);
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Failed to obtain fullscreen size.\n"
                         "Using current desktop resolution for fullscreen.");
  }
}

void XRRConfiguration::ToggleDisplayMode(bool bFullscreen)
{
  if (!bValid || !screenResources || !outputInfo || !crtcInfo || !fullMode)
    return;
  if (bFullscreen == bIsFullscreen)
    return;

  XGrabServer(dpy);
  if (bFullscreen)
  {
    XRRSetCrtcConfig(dpy, screenResources, outputInfo->crtc, CurrentTime, crtcInfo->x, crtcInfo->y,
                     fullMode, crtcInfo->rotation, crtcInfo->outputs, crtcInfo->noutput);
    XRRSetScreenSize(dpy, win, fs_fb_width, fs_fb_height, fs_fb_width_mm, fs_fb_height_mm);
    bIsFullscreen = true;
  }
  else
  {
    XRRSetCrtcConfig(dpy, screenResources, outputInfo->crtc, CurrentTime, crtcInfo->x, crtcInfo->y,
                     crtcInfo->mode, crtcInfo->rotation, crtcInfo->outputs, crtcInfo->noutput);
    XRRSetScreenSize(dpy, win, fb_width, fb_height, fb_width_mm, fb_height_mm);
    bIsFullscreen = false;
  }
  XUngrabServer(dpy);
  XSync(dpy, false);
}

void XRRConfiguration::AddResolutions(std::vector<std::string>& resos)
{
  if (!bValid || !screenResources)
    return;

  // Get all full screen resolutions for the config dialog
  for (int i = 0; i < screenResources->noutput; i++)
  {
    XRROutputInfo* output_info =
        XRRGetOutputInfo(dpy, screenResources, screenResources->outputs[i]);

    if (output_info && output_info->crtc && output_info->connection == RR_Connected)
    {
      for (int j = 0; j < output_info->nmode; j++)
      {
        for (int k = 0; k < screenResources->nmode; k++)
        {
          if (output_info->modes[j] == screenResources->modes[k].id)
          {
            bool interlaced = !!(screenResources->modes[k].modeFlags & RR_Interlace);
            const std::string strRes = std::string(output_info->name) + ": " +
                                       std::string(screenResources->modes[k].name) +
                                       (interlaced ? "i" : "");
            // Only add unique resolutions
            if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
            {
              resos.push_back(strRes);
            }
          }
        }
      }
    }
    if (output_info)
      XRRFreeOutputInfo(output_info);
  }
}

#endif
}  // namespace X11Utils
