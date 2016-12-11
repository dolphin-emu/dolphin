// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinUIKit/AppDelegate.h"
#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinUIKit/ViewController.h"
#include "UICommon/UICommon.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  NSString* documents =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
  UICommon::SetUserDirectory(documents.UTF8String);
  NSLog(@"UI: Using %@.", documents);

  UICommon::CreateDirectories();
  UICommon::Init();
  NSLog(@"UI: Init.");

  DolphinAnalytics::Instance()->ReportDolphinStart("uikit");
  NSLog(@"Analytics: Report.");

  SConfig::GetInstance().iCPUCore = PowerPC::MODE_INTERPRETER;
  NSLog(@"CPU: Using interpreter.");

  SConfig::GetInstance().m_strVideoBackend = "OGL";
  // SConfig::GetInstance().m_strVideoBackend = "Null";
  NSLog(@"Video: Using %s.", SConfig::GetInstance().m_strVideoBackend.c_str());

// TODO: Don't hardcode this.
#if !TARGET_IPHONE_SIMULATOR
  NSString* path = [documents stringByAppendingPathComponent:@"NAME_OF_ISO"];
  NSURL* URL = [NSURL fileURLWithPath:path];
#else
  NSURL* URL = [NSURL fileURLWithPath:@"PATH_TO_ISO"];
#endif
  NSLog(@"Boot: Using %@.", URL);

  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  self.window.rootViewController = [[ViewController alloc] initWithURL:URL];
  [self.window makeKeyAndVisible];

  return YES;
}

@end
