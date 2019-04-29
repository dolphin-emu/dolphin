// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <thread>
#include "Core/Core.h"
#include "DolphinNoGUI/Platform.h"

namespace
{
class PlatformHeadless : public Platform
{
public:
  void SetTitle(const std::string& title) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;
};

void PlatformHeadless::SetTitle(const std::string& title)
{
  std::fprintf(stdout, "%s\n", title.c_str());
}

void PlatformHeadless::MainLoop()
{
  while (m_running.IsSet())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

WindowSystemInfo PlatformHeadless::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Headless;
  wsi.display_connection = nullptr;
  wsi.render_surface = nullptr;
  return wsi;
}

}  // namespace

std::unique_ptr<Platform> Platform::CreateHeadlessPlatform()
{
  return std::make_unique<PlatformHeadless>();
}
