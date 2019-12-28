// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerDeviceCell.h"
#import "ControllerDevicesViewController.h"
#import "ControllerSettingsUtils.h"

#import "InputCommon/ControllerInterface/ControllerInterface.h"

@interface ControllerDevicesViewController ()

@end

@implementation ControllerDevicesViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  [self RefreshDevices];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [ControllerSettingsUtils SaveSettings:self.m_is_wii];
}

- (void)RefreshDevices
{
  NSMutableArray* array = [NSMutableArray array];
  
  const std::string current_device = self.m_controller->GetDefaultDevice().ToString();
  std::vector<std::string> devices = g_controller_interface.GetAllDeviceStrings();
  
  for (size_t i = 0; i < devices.size(); i++)
  {
    std::string device = devices[i];
    if (device == current_device)
    {
      self.m_last_selected = i;
    }
    
    [array addObject:[NSString stringWithUTF8String:device.c_str()]];
  }
  
  self.m_devices = [array copy];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return [self.m_devices count];
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  ControllerDeviceCell* cell = (ControllerDeviceCell*)[tableView dequeueReusableCellWithIdentifier:@"device_cell" forIndexPath:indexPath];
  
  NSString* device_name = [self.m_devices objectAtIndex:indexPath.row];
  [cell.m_device_label setText:[ControllerSettingsUtils RemoveAndroidFromDeviceName:device_name]];
  
  if (indexPath.row == self.m_last_selected)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [old_cell setAccessoryType:UITableViewCellAccessoryNone];
  
  self.m_last_selected = indexPath.row;
  
  std::string device_name = std::string([[self.m_devices objectAtIndex:indexPath.row] cStringUsingEncoding:NSUTF8StringEncoding]);
  self.m_controller->SetDefaultDevice(device_name);
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
