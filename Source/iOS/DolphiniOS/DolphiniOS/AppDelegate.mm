// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"
#import "DolphiniOS-Swift.h"

#import "Common/Config/Config.h"

#import "Core/ConfigManager.h"
#import "Core/Core.h"
#import "Core/HW/GCKeyboard.h"
#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"

#import "InputCommon/ControllerInterface/ControllerInterface.h"

#import "MainiOS.h"

#import "UICommon/UICommon.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  // Default settings values should be set in DefaultPreferences.plist in the future
  NSURL *defaultPrefsFile = [[NSBundle mainBundle] URLForResource:@"DefaultPreferences" withExtension:@"plist"];
  NSDictionary *defaultPrefs = [NSDictionary dictionaryWithContentsOfURL:defaultPrefsFile];
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPrefs];
    
  // Override point for customization after application launch.
  
  [MainiOS applicationStart];
  
  return YES;
}

- (void)applicationWillTerminate:(UIApplication*)application
{
  Pad::Shutdown();
  Keyboard::Shutdown();
  Wiimote::Shutdown();
  g_controller_interface.Shutdown();
  
  Config::Save();
  
  Core::Shutdown();
  UICommon::Shutdown();
}

- (BOOL)application:(UIApplication*)app openURL:(NSURL*)url options:(NSDictionary<UIApplicationOpenURLOptionsKey,id>*)options
{
  [MainiOS importFiles:[NSSet setWithObject:url]];
  
  return YES;
}

@end
