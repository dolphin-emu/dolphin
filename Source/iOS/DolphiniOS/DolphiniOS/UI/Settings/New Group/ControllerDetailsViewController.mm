// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerDetailsViewController.h"

#import "Core/ConfigManager.h"
#import "Core/HW/Wiimote.h"

#import "ControllerDevicesViewController.h"
#import "ControllerGroupListViewController.h"
#import "ControllerLoadProfileViewController.h"
#import "ControllerSettingsUtils.h"
#import "ControllerTypePickerViewController.h"

@interface ControllerDetailsViewController ()

@end

@implementation ControllerDetailsViewController



- (void)viewDidLoad
{
  [super viewDidLoad];
  
  [self UpdateViewContents];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
  [self UpdateViewContents];
}

- (void)UpdateViewContents
{
  bool configuration_enabled = true;
  bool can_change_devices = true;
  
  if (self.m_is_wii)
  {
    u32 wiimote_source = g_wiimote_sources[self.m_port];
    if (wiimote_source != 1)
    {
      configuration_enabled = false;
      can_change_devices = false;
    }
    
    [self.m_type_label setText:[ControllerSettingsUtils GetLocalizedWiimoteStringFromIndex:wiimote_source]];
  }
  else
  {
    const std::optional<int> gc_index = [ControllerSettingsUtils SIDevicesToGCMenuIndex:SConfig::GetInstance().m_SIDevice[self.m_port]];
    if (!gc_index.has_value() || gc_index.value() == 0)
    {
      configuration_enabled = false;
      can_change_devices = false;
    }
    
    [self.m_type_label setText:[ControllerSettingsUtils GetLocalizedGameCubeControllerFromIndex:gc_index.value()]];
  }
  
  NSString* raw_device_name = [NSString stringWithUTF8String:self.m_controller->GetDefaultDevice().ToString().c_str()];
  if ([raw_device_name hasPrefix:@"Android"])
  {
    configuration_enabled = false;
  }
  
  if (can_change_devices)
  {
    [self.m_device_label setText:[ControllerSettingsUtils RemoveAndroidFromDeviceName:raw_device_name]];
  }
  
  [self SetConfigurationEnabled:configuration_enabled device_change_enabled:can_change_devices];
  
  self.navigationItem.title = [NSString stringWithFormat:self.m_is_wii ? @"Wiimote %d" : @"Port %d", self.m_port + 1];
}

- (void)SetConfigurationEnabled:(bool)config_change_enabled device_change_enabled:(bool)device_change_enabled
{
  if (!device_change_enabled)
  {
    [self.m_device_label setText:@"N/A"];
  }
  
  [self SetLabelColor:self.m_device_header_label enabled:device_change_enabled is_link:false];
  [self SetLabelColor:self.m_device_configure_label enabled:config_change_enabled is_link:false];
  [self SetLabelColor:self.m_load_profile_label enabled:config_change_enabled is_link:true];
  [self SetLabelColor:self.m_save_profile_label enabled:config_change_enabled is_link:true];
  
  self.m_device_cell.userInteractionEnabled = device_change_enabled;
  self.m_configure_cell.userInteractionEnabled = config_change_enabled;
  self.m_load_profile_cell.userInteractionEnabled = config_change_enabled;
  self.m_save_profile_cell.userInteractionEnabled = config_change_enabled;
}

- (void)SetLabelColor:(UILabel*)label enabled:(bool)enabled is_link:(bool)is_link
{
  if (!enabled)
  {
      [label setTextColor:[UIColor systemGrayColor]];
  }
  else
  {
    if (@available(iOS 13, *))
    {
      [label setTextColor:is_link ? [UIColor linkColor] : [UIColor labelColor]];
    }
    else
    {
      [label setTextColor:is_link ? [UIColor blueColor] : [UIColor blackColor]];
    }
  }
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_types"])
  {
    ControllerTypePickerViewController* view_controller = (ControllerTypePickerViewController*)segue.destinationViewController;
    view_controller.m_port = self.m_port;
    view_controller.m_is_wii = self.m_is_wii;
  }
  else if ([segue.identifier isEqualToString:@"to_devices"])
  {
    ControllerDevicesViewController* view_controller = (ControllerDevicesViewController*)segue.destinationViewController;
    view_controller.m_controller = self.m_controller;
    view_controller.m_is_wii = self.m_is_wii;
  }
  else if ([segue.identifier isEqualToString:@"to_configure"])
  {
    ControllerGroupListViewController* view_controller = (ControllerGroupListViewController*)segue.destinationViewController;
    view_controller.m_controller = self.m_controller;
    view_controller.m_port = self.m_port;
    view_controller.m_is_wii = self.m_is_wii;
  }
  else if ([segue.identifier isEqualToString:@"to_profile_load"])
  {
    ControllerLoadProfileViewController* view_controller = (ControllerLoadProfileViewController*)segue.destinationViewController;
    view_controller.m_controller = self.m_controller;
    view_controller.m_is_wii = self.m_is_wii;
  }
}

- (IBAction)unwindToDetails:(UIStoryboardSegue*)segue {}

@end
