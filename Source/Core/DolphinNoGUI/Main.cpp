// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <signal.h>
#include <string>
#include <unistd.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

#include "Core/Analytics.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Host.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_emu.h"
#include "Core/IPC_HLE/WII_IPC_HLE_WiiMote.h"
#include "Core/State.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

static bool rendererHasFocus = true;
static bool rendererIsFullscreen = false;
static Common::Flag s_running{true};
static Common::Flag s_shutdown_requested{false};
static Common::Flag s_tried_graceful_shutdown{false};

static void signal_handler(int)
{
  const char message[] = "A signal was received. A second signal will force Dolphin to stop.\n";
  if (write(STDERR_FILENO, message, sizeof(message)) < 0)
  {
  }
  s_shutdown_requested.Set();
}

namespace ProcessorInterface
{
void PowerButton_Tap();
}

#include "DolphinNoGUI/Platform.h"

static Platform* platform;

void Host_NotifyMapLoaded()
{
}
void Host_RefreshDSPDebuggerWindow()
{
}

static Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
  if (Id == WM_USER_STOP)
  {
    s_running.Clear();
    updateMainFrameEvent.Set();
  }
}

static void* s_window_handle = nullptr;
void* Host_GetRenderHandle()
{
  return s_window_handle;
}

void Host_UpdateTitle(const std::string& title)
{
  platform->SetTitle(title);
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
  updateMainFrameEvent.Set();
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_SetStartupDebuggingParameters()
{
  SConfig& StartUp = SConfig::GetInstance();
  StartUp.bEnableDebugging = false;
  StartUp.bBootToPause = false;
}

bool Host_UIHasFocus()
{
  return false;
}

bool Host_RendererHasFocus()
{
  return rendererHasFocus;
}

bool Host_RendererIsFullscreen()
{
  return rendererIsFullscreen;
}

void Host_ConnectWiimote(int wm_idx, bool connect)
{
  if (Core::IsRunning() && SConfig::GetInstance().bWii &&
      !SConfig::GetInstance().m_bt_passthrough_enabled)
  {
    Core::QueueHostJob([=] {
      bool was_unpaused = Core::PauseAndLock(true);
      GetUsbPointer()->AccessWiiMote(wm_idx | 0x100)->Activate(connect);
      Host_UpdateMainFrame();
      Core::PauseAndLock(false, was_unpaused);
    });
  }
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_ShowVideoConfig(void*, const std::string&)
{
}

void Host_YieldToUI()
{
}

#if HAVE_X11
#include "DolphinNoGUI/X11.h"
#endif

static Platform* GetPlatform()
{
#if defined(USE_EGL) && defined(USE_HEADLESS)
  return new Platform();
#elif HAVE_X11
  return new PlatformX11();
#endif
  return nullptr;
}

int main(int argc, char* argv[])
{
  int ch, help = 0;
  struct option longopts[] = {{"exec", no_argument, nullptr, 'e'},
                              {"help", no_argument, nullptr, 'h'},
                              {"version", no_argument, nullptr, 'v'},
                              {nullptr, 0, nullptr, 0}};

  while ((ch = getopt_long(argc, argv, "eh?v", longopts, 0)) != -1)
  {
    switch (ch)
    {
    case 'e':
      break;
    case 'h':
    case '?':
      help = 1;
      break;
    case 'v':
      fprintf(stderr, "%s\n", scm_rev_str.c_str());
      return 1;
    }
  }

  if (help == 1 || argc == optind)
  {
    fprintf(stderr, "%s\n\n", scm_rev_str.c_str());
    fprintf(stderr, "A multi-platform GameCube/Wii emulator\n\n");
    fprintf(stderr, "Usage: %s [-e <file>] [-h] [-v]\n", argv[0]);
    fprintf(stderr, "  -e, --exec     Load the specified file\n");
    fprintf(stderr, "  -h, --help     Show this help message\n");
    fprintf(stderr, "  -v, --version  Print version and exit\n");
    return 1;
  }

  platform = GetPlatform();
  if (!platform)
  {
    fprintf(stderr, "No platform found\n");
    return 1;
  }

  UICommon::SetUserDirectory("");  // Auto-detect user folder
  UICommon::Init();

  Core::SetOnStoppedCallback([]() { s_running.Clear(); });
  platform->Init();

  // Shut down cleanly on SIGINT and SIGTERM
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  DolphinAnalytics::Instance()->ReportDolphinStart("nogui");

  if (!BootManager::BootCore(argv[optind]))
  {
    fprintf(stderr, "Could not boot %s\n", argv[optind]);
    return 1;
  }

  while (!Core::IsRunning() && s_running.IsSet())
  {
    Core::HostDispatchJobs();
    updateMainFrameEvent.Wait();
  }

  if (s_running.IsSet())
    platform->MainLoop();
  Core::Stop();

  Core::Shutdown();
  platform->Shutdown();
  UICommon::Shutdown();

  delete platform;

  return 0;
}
