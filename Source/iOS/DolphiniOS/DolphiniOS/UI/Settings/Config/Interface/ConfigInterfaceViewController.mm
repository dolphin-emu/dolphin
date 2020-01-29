// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigInterfaceViewController.h"

#import "Common/MsgHandler.h"

#import "Core/ConfigManager.h"

@interface ConfigInterfaceViewController ()

@end

@implementation ConfigInterfaceViewController

// Skipped settings:
// "User Interface" (all) - none needed from here
// "Keep Window on Top" - no concept of layered windows on iOS
// "Show Active Title in Window Title" - no window title in iOS
// "Pause on Focus Loss" - default behaviour for all iOS apps
// "Always Hide Mouse Cursor" - no concept of a mouse cursor on iOS

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_confirm_stop_switch setOn:SConfig::GetInstance().bConfirmStop];
  [self.m_panic_handlers_switch setOn:SConfig::GetInstance().bUsePanicHandlers];
  [self.m_osd_switch setOn:SConfig::GetInstance().bOnScreenDisplayMessages];
  [self.m_top_bar_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"always_show_top_bar"]];
  [self.m_center_image_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"do_not_raise_rendering_view"]];
}

- (IBAction)ConfirmStopChanged:(id)sender
{
  SConfig::GetInstance().bConfirmStop = [self.m_confirm_stop_switch isOn];
}

- (IBAction)UsePanicHandlersChanged:(id)sender
{
  SConfig::GetInstance().bUsePanicHandlers = [self.m_panic_handlers_switch isOn];
  Common::SetEnableAlert([self.m_panic_handlers_switch isOn]);
}

- (IBAction)ShowOsdChanged:(id)sender
{
  SConfig::GetInstance().bOnScreenDisplayMessages = [self.m_osd_switch isOn];
}

- (IBAction)ShowTopBarChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_top_bar_switch isOn] forKey:@"always_show_top_bar"];
}

- (IBAction)CenterImageChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_center_image_switch isOn] forKey:@"do_not_raise_rendering_view"];
}

@end
