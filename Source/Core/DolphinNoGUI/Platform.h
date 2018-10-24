// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/Flag.h"
#include "Common/WindowSystemInfo.h"

class Platform
{
public:
  virtual ~Platform();

  bool IsRunning() const { return m_running.IsSet(); }
  bool IsWindowFocused() const { return m_window_focus; }
  bool IsWindowFullscreen() const { return m_window_fullscreen; }

  virtual bool Init();
  virtual void SetTitle(const std::string& title);
  virtual void MainLoop() = 0;

  virtual WindowSystemInfo GetWindowSystemInfo() const = 0;

  // Requests a graceful shutdown, from SIGINT/SIGTERM.
  void RequestShutdown();

  // Request an immediate shutdown.
  void Stop();

  static std::unique_ptr<Platform> CreateHeadlessPlatform();
#ifdef HAVE_X11
  static std::unique_ptr<Platform> CreateX11Platform();
#endif

protected:
  void UpdateRunningFlag();

  Common::Flag m_running{true};
  Common::Flag m_shutdown_requested{false};
  Common::Flag m_tried_graceful_shutdown{false};

  bool m_window_focus = true;
  bool m_window_fullscreen = false;
};
