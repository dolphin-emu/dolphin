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

#import "DonationNoticeViewController.h"

#import "InputCommon/ControllerInterface/ControllerInterface.h"

#import "MainiOS.h"

#import <MetalKit/MetalKit.h>

#import "NoticeNavigationViewController.h"

#import "UICommon/UICommon.h"

#import "UnofficialBuildNoticeViewController.h"

#import "UpdateNoticeViewController.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
  // Check the device compatibility
#ifndef SUPPRESS_UNSUPPORTED_DEVICE
  // Provide a way to bypass this check for debugging purposes
  NSString* bypass_flag_file = [[MainiOS getUserFolder] stringByAppendingPathComponent:@"bypass_unsupported_device"];
  if (![[NSFileManager defaultManager] fileExistsAtPath:bypass_flag_file])
  {
    // Check for GPU Family 3
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
    if (![metalDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
    {
      // Show the incompatibilty warning
      self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
      self.window.rootViewController = [[UIViewController alloc] initWithNibName:@"UnsupportedDeviceNotice" bundle:nil];
      [self.window makeKeyAndVisible];
      
      return true;
    }
  }
#endif
  
  // Default settings values should be set in DefaultPreferences.plist in the future
  NSURL *defaultPrefsFile = [[NSBundle mainBundle] URLForResource:@"DefaultPreferences" withExtension:@"plist"];
  NSDictionary *defaultPrefs = [NSDictionary dictionaryWithContentsOfURL:defaultPrefsFile];
  [[NSUserDefaults standardUserDefaults] registerDefaults:defaultPrefs];
    
  Common::RegisterStringTranslator([](const char* text) -> std::string {
    NSString* localized_text = DOLocalizedString([NSString stringWithUTF8String:text]);
    return std::string([localized_text UTF8String]);
  });
  
  [MainiOS applicationStart];
  
  // Create a UINavigationController for alerts
  NoticeNavigationViewController* nav_controller = [[NoticeNavigationViewController alloc] init];
  [nav_controller setModalPresentationStyle:UIModalPresentationFormSheet];
  [nav_controller setNavigationBarHidden:true];
  
  // Get the number of launches
  NSInteger launch_times = [[NSUserDefaults standardUserDefaults] integerForKey:@"launch_times"];
  if (launch_times == 0)
  {
    [nav_controller pushViewController:[[UnofficialBuildNoticeViewController alloc] initWithNibName:@"UnofficialBuildNotice" bundle:nil] animated:true];
  }
  else if (launch_times % 10 == 0)
  {
#ifndef PATREON
    bool suppress_donation_message = [[NSUserDefaults standardUserDefaults] boolForKey:@"suppress_donation_message"];
  
    if (!suppress_donation_message)
    {
      [nav_controller pushViewController:[[DonationNoticeViewController alloc] initWithNibName:@"DonationNotice" bundle:nil] animated:true];
    }
#endif
  }
  
  // Present if the navigation controller isn't empty
  if ([[nav_controller viewControllers] count] != 0)
  {
    [self.window makeKeyAndVisible];
    [self.window.rootViewController presentViewController:nav_controller animated:true completion:nil];
  }
  
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
        
        if (![nav_controller isBeingPresented])
        {
          [nav_controller setViewControllers:@[update_controller]];
          [self.window makeKeyAndVisible];
          [self.window.rootViewController presentViewController:nav_controller animated:true completion:nil];
        }
        else
        {
          [nav_controller pushViewController:update_controller animated:true];
        }
      });
    }
  }] resume];
 #endif
  
  // Increment the launch count
  [[NSUserDefaults standardUserDefaults] setInteger:launch_times + 1 forKey:@"launch_times"];
  
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
