// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"
#import "DolphiniOS-Swift.h"

#import "Common/Config/Config.h"
#import "Common/MsgHandler.h"
#import "Common/StringUtil.h"

#import "Core/ConfigManager.h"
#import "Core/Core.h"
#import "Core/HW/GCKeyboard.h"
#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"

#import "InputCommon/ControllerInterface/ControllerInterface.h"

#import "MainiOS.h"

#import "UICommon/UICommon.h"

#import "UpdateNoticeViewController.h"

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
  
  Common::RegisterStringTranslator([](const char* text) -> std::string {
    NSString* localized_text = DOLocalizedString([NSString stringWithUTF8String:text]);
    return std::string([localized_text UTF8String]);
  });
  
  [MainiOS applicationStart];
  
  // Create a UINavigationController for alerts
  UINavigationController* nav_controller = [[UINavigationController alloc] init];
  [nav_controller setModalPresentationStyle:UIModalPresentationFormSheet];
  [nav_controller setNavigationBarHidden:true];
  
  [[[[UIApplication sharedApplication] keyWindow] rootViewController] presentViewController:nav_controller animated:true completion:nil];
  
  // Check for updates
#ifndef DEBUG
  NSString* update_url_string;
#ifndef PATREON
  update_url_string = @"https://cydia.oatmealdome.me/DolphiniOS/api/update.json";
#else
  update_url_string = @"https://cydia.oatmealdome.me/DolphiniOS/api/update_patreon.json";
#endif
  
  NSURL* update_url = [NSURL URLWithString:update_url_string];
  
  // Create en ephemeral session to avoid caching
  NSURLSession* session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration]];
  [[session dataTaskWithURL:update_url completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
    if (error != nil)
    {
      return;
    }
    
    // Get the version string
    NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
    NSString* version_str = [NSString stringWithFormat:@"%@ (%@)", [info objectForKey:@"CFBundleShortVersionString"], [info objectForKey:@"CFBundleVersion"]];
    
    // Deserialize the JSON
    NSDictionary* dict = (NSDictionary*)[NSJSONSerialization JSONObjectWithData:data options:kNilOptions error:nil];
    
    if (![dict[@"version"] isEqualToString:version_str])
    {
      dispatch_async(dispatch_get_main_queue(), ^{
        UpdateNoticeViewController* update_controller = [[UpdateNoticeViewController alloc] initWithNibName:@"UpdateNotice" bundle:nil];
        update_controller.m_update_json = dict;
        
        [nav_controller pushViewController:update_controller animated:true];
        
        if (![nav_controller isBeingPresented])
        {
          [[[[UIApplication sharedApplication] keyWindow] rootViewController] presentViewController:nav_controller animated:true completion:nil];
        }
      });
    }
  }] resume];
 #endif
  
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
