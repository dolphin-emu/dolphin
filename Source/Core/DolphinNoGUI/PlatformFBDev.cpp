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
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

namespace
{
class PlatformFBDev : public Platform
{
public:
  ~PlatformFBDev() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  bool OpenFramebuffer();

  int m_fb_fd = -1;
};

PlatformFBDev::~PlatformFBDev()
{
  if (m_fb_fd >= 0)
    close(m_fb_fd);
}

bool PlatformFBDev::Init()
{
  if (!OpenFramebuffer())
    return false;

  return true;
}

bool PlatformFBDev::OpenFramebuffer()
{
  m_fb_fd = open("/dev/fb0", O_RDWR);
  if (m_fb_fd < 0)
  {
    std::fprintf(stderr, "open(/dev/fb0) failed\n");
    return false;
  }

  return true;
}

void PlatformFBDev::SetTitle(const std::string& string)
{
  std::fprintf(stdout, "%s\n", string.c_str());
}

void PlatformFBDev::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());

    // TODO: Is this sleep appropriate?
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformFBDev::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::FBDev;
  wsi.display_connection = nullptr;  // EGL_DEFAULT_DISPLAY
  wsi.render_window = nullptr;
  wsi.render_surface = nullptr;
  return wsi;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateFBDevPlatform()
{
  return std::make_unique<PlatformFBDev>();
}
