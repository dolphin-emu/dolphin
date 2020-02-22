// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerDetailsViewController.h"

#import "Common/FileUtil.h"
#import "Common/IniFile.h"

#import "Core/ConfigManager.h"
#import "Core/HW/GCPad.h"
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
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self UpdateViewContents];
}

- (void)UpdateViewContents
{
  bool configuration_enabled = true;
  bool can_change_devices = true;
  
  if (self.m_is_wii)
  {
    WiimoteSource wiimote_source = WiimoteCommon::GetSource(self.m_port);
    if (wiimote_source != WiimoteSource::Emulated)
    {
      configuration_enabled = false;
      can_change_devices = false;
    }
    
    [self.m_type_label setText:[ControllerSettingsUtils GetLocalizedWiimoteStringFromSource:wiimote_source]];
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
  
  if (can_change_devices)
  {
    // Get the current device
    const std::string current_device = self.m_controller->GetDefaultDevice().ToString();
    std::vector<std::string> all_connected_devices = g_controller_interface.GetAllDeviceStrings();
    
    // Check if the current device isn't connected
    if (std::find(all_connected_devices.begin(), all_connected_devices.end(), current_device) == all_connected_devices.end())
    {
      // Show an error
      NSString* alert_str = [NSString stringWithFormat:NSLocalizedString(@"The selected device \"%@\" for this emulated controller is not connected.", nil), [NSString stringWithUTF8String:current_device.c_str()]];
      
      UIAlertController* alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Warning")
                                     message:alert_str
                                     preferredStyle:UIAlertControllerStyleAlert];
      [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault
                                              handler:nil]];
      
      [self presentViewController:alert animated:true completion:nil];
      
      // Disable configuration
      configuration_enabled = false;
    }
  
    NSString* raw_device_name = [NSString stringWithUTF8String:current_device.c_str()];
    [self.m_device_label setText:[ControllerSettingsUtils RemoveAndroidFromDeviceName:raw_device_name]];
  }
  
  [self SetConfigurationEnabled:configuration_enabled device_change_enabled:can_change_devices];
  
  self.navigationItem.title = [NSString stringWithFormat:self.m_is_wii ? DOLocalizedStringWithArgs(@"Wii Remote %1", @"d") : DOLocalizedStringWithArgs(@"Port %1", @"d"), self.m_port + 1];
}

- (void)SetConfigurationEnabled:(bool)config_change_enabled device_change_enabled:(bool)device_change_enabled
{
  if (!device_change_enabled)
  {
    [self.m_device_label setText:DOLocalizedString(@"None")];
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

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1 && indexPath.row == 2)
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Save")
                                   message:NSLocalizedString(@"Please enter a name for this profile. If a profile with that name already exists, it will be overwritten.", nil)
                                   preferredStyle:UIAlertControllerStyleAlert];
    
    [alert addTextFieldWithConfigurationHandler:^(UITextField* field) {
      field.placeholder = DOLocalizedString(@"Name");
    }];
    
    [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Save") style:UIAlertActionStyleDefault handler:^(UIAlertAction* action) {
      UITextField* field = alert.textFields.firstObject;
      
      NSString* name = field.text;
      if ([name isEqualToString:@""])
      {
        UIAlertController* failed_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Error") message:NSLocalizedString(@"You have not entered a valid name.", nil) preferredStyle:UIAlertControllerStyleAlert];
        [failed_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:failed_alert animated:true completion:nil];
        
        return;
      }
      
      InputConfig* config;
      if (self.m_is_wii)
      {
        config = Wiimote::GetConfig();
      }
      else
      {
        config = Pad::GetConfig();
      }
      
      const std::string name_str = std::string([name cStringUsingEncoding:NSUTF8StringEncoding]);
      const std::string profile_path = File::GetUserPath(D_CONFIG_IDX) + "Profiles/" + config->GetProfileName() + "/" + name_str + ".ini";
      File::CreateFullPath(profile_path);

      IniFile ini;
      self.m_controller->SaveConfig(ini.GetOrCreateSection("Profile"));
      ini.Save(profile_path);
      
      UIAlertController* ok_alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Success") message:NSLocalizedString(@"The profile has been saved.", nil) preferredStyle:UIAlertControllerStyleAlert];
      [ok_alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleCancel handler:nil]];
      [self presentViewController:ok_alert animated:true completion:nil];
    }]];
    
    [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"Cancel") style:UIAlertActionStyleCancel handler:nil]];
    
    [self presentViewController:alert animated:true completion:nil];
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
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
    view_controller.m_port = self.m_port;
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
