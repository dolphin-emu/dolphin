// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "MotionControlsSettingsViewController.h"
#import "DolphiniOS-Swift.h"

#import "Core/HW/Wiimote.h"

#import "MainiOS.h"

@implementation MotionControlsSettingsViewController

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
  // Set the current setting
  NSUInteger mode = (NSUInteger)[[TCDeviceMotion shared] getMotionMode];
  [[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:mode inSection:0]] setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1)
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Help"
                                   message:@"\"On (With Pointer Emulation)\" will use your device's accelerometer and gyroscope to emulate the Wiimote's motion controls. They will also be used to simulate the Wiimote pointer.  \n\n\"On (Without Pointer Emulation)\" will allow you to use the touch screen as the Wiimote's pointer, while still leaving motion control emulation enabled.\n\n\"Off\" disables motion control emulation entirely. The touch screen can be used as the Wiimote's pointer."
                                   preferredStyle:UIAlertControllerStyleAlert];
     
    UIAlertAction* dismiss_action = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
       handler:nil];
    [alert addAction:dismiss_action];
    
    [self presentViewController:alert animated:YES completion:nil];
    
    return;
  }
  
  // Check the last mode
  NSUInteger last_mode = (NSUInteger)[[TCDeviceMotion shared] getMotionMode];
  if (last_mode == indexPath.row)
  {
    [self.tableView deselectRowAtIndexPath:indexPath animated:true];
    return;
  }
  
  // Save the mode
  [[TCDeviceMotion shared] setMotionMode:(NSInteger)indexPath.row];
  
  // Set IMUIR
  [MainiOS toggleImuIr:indexPath.row == 0];
  
  // Reload the Wiimote config
  Wiimote::LoadConfig();
  
  // Set the row accessories
  [[self.tableView cellForRowAtIndexPath:indexPath] setAccessoryType:UITableViewCellAccessoryCheckmark];
  [[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:last_mode inSection:0]] setAccessoryType:UITableViewCellAccessoryNone];
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
