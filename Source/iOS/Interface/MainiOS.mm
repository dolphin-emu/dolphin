// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <MetalKit/MetalKit.h>

#include "Core/Boot/Boot.h"

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/WindowSystemInfo.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/State.h"

#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

#include "MainiOS.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/RenderBase.h"

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

static bool MsgAlert(const char* caption, const char* text, bool yes_no, Common::MsgType /*style*/)
{
  @autoreleasepool
  {
    NSCondition* condition = [[NSCondition alloc] init];
    
    __block bool yes_pressed = false;

    dispatch_async(dispatch_get_main_queue(), ^{
      // Create a new UIWindow to host the UIAlertController
      // We don't have access to the EmulationViewController here, so this is the best we can do.
      // (This is what Apple supposedly does in their applications.)
      __block UIWindow* window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
      window.rootViewController = [UIViewController new];
      window.windowLevel = UIWindowLevelAlert + 1;

      UIAlertController* alert = [UIAlertController alertControllerWithTitle:[NSString stringWithUTF8String:caption]
                                    message:[NSString stringWithUTF8String:text]
                                    preferredStyle:UIAlertControllerStyleAlert];

      if (yes_no)
      {
        [alert addAction:[UIAlertAction actionWithTitle:@"Yes" style:UIAlertActionStyleDefault
          handler:^(UIAlertAction * action) {
            yes_pressed = true;

            [condition signal];

            window.hidden = YES;
            window = nil;
        }]];

        [alert addAction:[UIAlertAction actionWithTitle:@"No" style:UIAlertActionStyleDefault
          handler:^(UIAlertAction* action) {
            yes_pressed = false;

            [condition signal];

            window.hidden = YES;
            window = nil;
        }]];
      }
      else
      {
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
          handler:^(UIAlertAction* action) {
            [condition signal];

            window.hidden = YES;
            window = nil;
        }]];
      }

      [window makeKeyAndVisible];
      [window.rootViewController presentViewController:alert animated:YES completion:nil];
    });

    // Wait for a button press
    [condition lock];
    [condition wait];
    [condition unlock];

    return yes_pressed;
  }
}

@implementation MainiOS

+ (void) applicationStart
{
  Common::RegisterMsgAlertHandler(&MsgAlert);

  NSString* user_directory = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  UICommon::SetUserDirectory(std::string([user_directory UTF8String]));

  UICommon::CreateDirectories();
  UICommon::Init();

  // Get the Dolphin.ini path
  std::string dolphin_config_path = File::GetUserPath(F_DOLPHINCONFIG_IDX);

  // Check if the Dolphin.ini file exists already
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if (![file_manager fileExistsAtPath:[NSString stringWithUTF8String:dolphin_config_path.c_str()]])
  {
    // Create a new Dolphin.ini
    IniFile dolphin_config;
    dolphin_config.Load(dolphin_config_path);

    std::string gfx_backend;

    // Check if GPU Family 3 is supported
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
    if ([metalDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
    {
      gfx_backend = "Vulkan";
    }
    else
    {
      gfx_backend = "OGL";
    }

    dolphin_config.GetOrCreateSection("Core")->Set("GFXBackend", gfx_backend);

    dolphin_config.Save(dolphin_config_path);
  }
}

+ (void) startEmulationWithFile:(NSString*) file view:(UIView*) view
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::IPhoneOS;
  wsi.display_connection = nullptr;
  wsi.render_surface = (__bridge void*)view;
  wsi.render_surface_scale = [UIScreen mainScreen].scale;  

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

+ (void)windowResized
{
  if (g_renderer)
    g_renderer->ResizeSurface();
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
