// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InGameSettingsViewController.h"

#import "EmulationViewController.h"

#import "MainiOS.h"

@interface InGameSettingsViewController ()

@end

@implementation InGameSettingsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  EmulationViewController* emulation_controller = (EmulationViewController*)[MainiOS getEmulationViewController];
  [emulation_controller PopulatePortDictionary];
  [emulation_controller ChangeVisibleTouchControllerToPort:emulation_controller.m_ts_active_port];
}

@end
