// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Core/Boot/Boot.h"

#include "Common/CommonTypes.h"
#include "Common/WindowSystemInfo.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/State.h"

#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

#include "MainiOS.h"

#include "UICommon/UICommon.h"

void Host_NotifyMapLoaded()
{
}
void Host_RefreshDSPDebuggerWindow()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

void Host_Message(HostMessageID id)
{
}

void Host_UpdateTitle(const std::string& title)
{
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_UINeedsControllerState()
{
  return false;
}

bool Host_RendererHasFocus()
{
  return true;
}

bool Host_RendererIsFullscreen()
{
  return true;
}

void Host_YieldToUI()
{
}

void Host_UpdateProgressDialog(const char* caption, int position, int total)
{
}

void Host_TitleChanged()
{
}

@implementation MainiOS

+ (void) startEmulationWithFile:(NSString*) file view:(UIView*) view
{
  NSString* userDirectory = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  UICommon::SetUserDirectory(std::string([userDirectory UTF8String]));

  UICommon::Init();

  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::IPhoneOS;
  wsi.display_connection = nullptr;
  wsi.render_surface = (__bridge void*)view;

  std::unique_ptr<BootParameters> boot;
  boot = BootParameters::GenerateFromFile([file UTF8String]);

  // No use running the loop when booting fails
  BootManager::BootCore(std::move(boot), wsi);

  while (true)
  {
    ButtonManager::Init(SConfig::GetInstance().GetGameID());
    Core::HostDispatchJobs();
    usleep(useconds_t(100 * 1000));
  }

  Core::Shutdown();
  UICommon::Shutdown();
}

+ (void)gamepadEventForButton:(int)button action:(int)action
{
  ButtonManager::GamepadEvent("Touchscreen", button, action);
}

+ (void)gamepadMoveEventForAxis:(int)axis value:(CGFloat)value
{
  ButtonManager::GamepadAxisEvent("Touchscreen", axis, value);
}

@end
