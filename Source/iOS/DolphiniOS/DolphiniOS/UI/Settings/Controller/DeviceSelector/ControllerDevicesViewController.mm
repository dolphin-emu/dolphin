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
  
  // Create a UIRefreshControl
  UIRefreshControl* refreshControl = [[UIRefreshControl alloc] init];
  [refreshControl addTarget:self action:@selector(RefreshDevices) forControlEvents:UIControlEventValueChanged];
  
  self.tableView.refreshControl = refreshControl;
  
  [self RefreshDevices];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [ControllerSettingsUtils SaveSettings:self.m_is_wii];
}

- (void)RefreshDevices
{
  self.m_last_selected = -1;
  self->m_devices.clear();
  
  std::vector<std::string> devices = g_controller_interface.GetAllDeviceStrings();
  const std::string android_str("Android");
  const std::string port_str = std::to_string(self.m_port + (self.m_is_wii ? 4 : 0)); // Wiimotes starts at 4

  // Populate our devices list
  for (size_t i = 0; i < devices.size(); i++)
  {
    std::string device = devices[i];
    
    // Check if this is a Touchscreen device
    if (device.compare(0, android_str.size(), android_str) == 0)
    {
      // Don't add this to the list if it isn't this controller's designated Touchscreen
      if (device.compare(android_str.size() + 1, port_str.size(), port_str) == 0)
      {
        self->m_devices.push_back(device);
      }
    }
    else
    {
      self->m_devices.push_back(device);
    }
  }
  
  // Get the current device
  const std::string current_device = self.m_controller->GetDefaultDevice().ToString();
  for (size_t i = 0; i < self->m_devices.size(); i++)
  {
    std::string device = self->m_devices[i];
    if (device == current_device)
    {
      self.m_last_selected = i;
    }
  }
  
  [self.tableView reloadData];
  [self.tableView.refreshControl endRefreshing];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self->m_devices.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  ControllerDeviceCell* cell = (ControllerDeviceCell*)[tableView dequeueReusableCellWithIdentifier:@"device_cell" forIndexPath:indexPath];
  
  NSString* device_name = [NSString stringWithUTF8String:self->m_devices[indexPath.row].c_str()];
  [cell.m_device_label setText:[ControllerSettingsUtils RemoveAndroidFromDeviceName:device_name]];
  
  if (indexPath.row == self.m_last_selected)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.row == self.m_last_selected)
  {
    [self.tableView deselectRowAtIndexPath:indexPath animated:true];
    return;
  }
  
  std::string last_device = self.m_controller->GetDefaultDevice().ToString();
  std::string device_name = std::string(self->m_devices[indexPath.row]);
  std::string android_str("Android");
  std::string mfi_str("MFi");
  
  if (device_name.compare(0, android_str.size(), android_str) == 0)
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Warning")
                                   message:NSLocalizedString(@"Your controller configuration will be overwritten by the Touchscreen default settings.\n\nIf you want to keep your button mappings and settings, save it as a profile on the previous screen before changing devices.\n\nProceed?", nil)
                                   preferredStyle:UIAlertControllerStyleAlert];
     
    UIAlertAction* stop_action = [UIAlertAction actionWithTitle:DOLocalizedString(@"No") style:UIAlertActionStyleDefault
       handler:^(UIAlertAction * action) {
      [self.tableView deselectRowAtIndexPath:indexPath animated:true];
    }];
    
    UIAlertAction* overwrite_action = [UIAlertAction actionWithTitle:DOLocalizedString(@"Yes") style:UIAlertActionStyleDestructive
       handler:^(UIAlertAction * action) {
      [self ChangeDevice:device_name];
      [ControllerSettingsUtils LoadDefaultProfileOnController:self.m_controller is_wii:self.m_is_wii type:@"Touch"];
     }];
    
    [alert addAction:stop_action];
    [alert addAction:overwrite_action];
    
    [self presentViewController:alert animated:true completion:nil];
  }
  else if (device_name.compare(0, mfi_str.size(), mfi_str) == 0 && last_device.compare(0, android_str.size(), android_str) == 0)
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:NSLocalizedString(@"Load Defaults", nil)
                                   message:NSLocalizedString(@"Do you want to load the default configuration for MFi controllers?\n\nThe default configuration contains button mappings that will work on many MFi controllers.", nil)
                                   preferredStyle:UIAlertControllerStyleAlert];
     
    UIAlertAction* stop_action = [UIAlertAction actionWithTitle:DOLocalizedString(@"No") style:UIAlertActionStyleDefault
       handler:^(UIAlertAction * action) {
      [self ChangeDevice:device_name];
    }];
    
    UIAlertAction* continue_action = [UIAlertAction actionWithTitle:DOLocalizedString(@"Yes") style:UIAlertActionStyleDefault handler:^(UIAlertAction * action) {
      [self ChangeDevice:device_name];
      [ControllerSettingsUtils LoadDefaultProfileOnController:self.m_controller is_wii:self.m_is_wii type:@"MFi"];
     }];
    
    [alert addAction:stop_action];
    [alert addAction:continue_action];
    
    [self presentViewController:alert animated:true completion:nil];
  }
  else
  {
    [self ChangeDevice:device_name];
  }
}

- (void)ChangeDevice:(std::string)new_device
{
  NSIndexPath* indexPath = [self.tableView indexPathForSelectedRow];
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  if (self.m_last_selected != -1)
  {
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
  }
  
  self.m_last_selected = indexPath.row;
  
  self.m_controller->SetDefaultDevice(new_device);
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
