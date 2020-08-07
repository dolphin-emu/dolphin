// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <unistd.h>

#include "DolphinNoGUI/Platform.h"

#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/State.h"

#include <climits>
#include <cstdio>

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "VideoCommon/RenderBase.h"

namespace
{
class PlatformDRM : public Platform
{
public:
  ~PlatformDRM() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;
};

PlatformDRM::~PlatformDRM() = default;

bool PlatformDRM::Init()
{
  return true;
}

void PlatformDRM::SetTitle(const std::string& string)
{
  std::fprintf(stdout, "%s\n", string.c_str());
}

void PlatformDRM::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs();

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
