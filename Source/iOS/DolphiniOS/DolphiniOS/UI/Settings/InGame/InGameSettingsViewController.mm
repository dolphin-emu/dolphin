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
  if (!emulation_controller)
  {
    return;
  }
  
  bool was_dict_empty = emulation_controller->m_controllers.size() == 0;
  [emulation_controller PopulatePortDictionary];
  
  int target_port = emulation_controller.m_ts_active_port;
  if (was_dict_empty && emulation_controller->m_controllers.size() != 0)
  {
    target_port = emulation_controller->m_controllers[0].first;
  }
  
  [emulation_controller ChangeVisibleTouchControllerToPort:target_port];
}

@end
