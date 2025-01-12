// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <unistd.h>

#include "DolphinNoGUI/Platform.h"

#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "Core/System.h"

#include <climits>
#include <cstdio>
#include <thread>

#include <fcntl.h>
#include <sys/types.h>

namespace
{
class PlatformDRM : public Platform
{
public:
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;
};

void PlatformDRM::SetTitle(const std::string& string)
{
  std::fprintf(stdout, "%s\n", string.c_str());
}

void PlatformDRM::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());

    // TODO: Is this sleep appropriate?
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformDRM::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::DRM;
  wsi.display_connection = nullptr;  // EGL_DEFAULT_DISPLAY
  wsi.render_window = nullptr;
  wsi.render_surface = nullptr;
  return wsi;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateDRMPlatform()
{
  return std::make_unique<PlatformDRM>();
}
