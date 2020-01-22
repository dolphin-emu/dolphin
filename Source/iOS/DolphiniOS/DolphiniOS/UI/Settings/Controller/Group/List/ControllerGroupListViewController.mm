// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupListViewController.h"

#import "Core/ConfigManager.h"
#import "Core/HW/GCPad.h"
#import "Core/HW/GCPadEmu.h"
#import "Core/HW/SI/SI_Device.h"
#import "Core/HW/Wiimote.h"
#import "Core/HW/WiimoteEmu/Extension/Classic.h"
#import "Core/HW/WiimoteEmu/Extension/Drums.h"
#import "Core/HW/WiimoteEmu/Extension/Guitar.h"
#import "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#import "Core/HW/WiimoteEmu/Extension/Turntable.h"
#import "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"
#import "Core/HW/WiimoteEmu/WiimoteEmu.h"

#import "ControllerExtensionsViewController.h"
#import "ControllerGroupCell.h"
#import "ControllerGroupExtensionButtonCell.h"
#import "ControllerGroupViewController.h"
#import "ControllerSettingsUtils.h"

#define WII_SECTION_EXTENSION_SELECTOR 0
#define WII_SECTION_WIIMOTE_CONTROLS 1
#define WII_SECTION_NORMAL_MOTION 2
#define WII_SECTION_IMU_MOTION 3
#define WII_SECTION_EXTENSION_CONTROLS 4

@interface ControllerGroupListViewController ()

@end

@implementation ControllerGroupListViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [self RefreshGroups];
  
  [self.tableView reloadData];
  
  [super viewWillAppear:animated];
}

- (void)RefreshGroups
{
  // Clear groups
  self->m_controller_groups.clear();
  self->m_extension_groups.clear();
  
  // Load the controller group
  if (self.m_is_wii)
  {
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Buttons));
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::DPad));
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Hotkeys));
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Rumble));
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Options));
    
    self->m_normal_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Shake));
    self->m_normal_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Point));
    self->m_normal_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Tilt));
    self->m_normal_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Swing));
    
    self->m_imu_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUPoint));
    self->m_imu_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUAccelerometer));
    self->m_imu_motion_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUGyroscope));
    
    // Set the attachments
    self.m_attachments = static_cast<ControllerEmu::Attachments*>(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Attachments));
    
    switch (self.m_attachments->GetSelectedAttachment())
    {
      case WiimoteEmu::ExtensionNumber::NUNCHUK:
        self->m_extension_groups.push_back(Wiimote::GetNunchukGroup(self.m_port, WiimoteEmu::NunchukGroup::Shake));
        self->m_extension_groups.push_back(Wiimote::GetNunchukGroup(self.m_port, WiimoteEmu::NunchukGroup::Buttons));
        self->m_extension_groups.push_back(Wiimote::GetNunchukGroup(self.m_port, WiimoteEmu::NunchukGroup::Stick));
        self->m_extension_groups.push_back(Wiimote::GetNunchukGroup(self.m_port, WiimoteEmu::NunchukGroup::Tilt));
        self->m_extension_groups.push_back(Wiimote::GetNunchukGroup(self.m_port, WiimoteEmu::NunchukGroup::Swing));
        break;
      case WiimoteEmu::ExtensionNumber::CLASSIC:
        self->m_extension_groups.push_back(Wiimote::GetClassicGroup(self.m_port, WiimoteEmu::ClassicGroup::Buttons));
        self->m_extension_groups.push_back(Wiimote::GetClassicGroup(self.m_port, WiimoteEmu::ClassicGroup::DPad));
        self->m_extension_groups.push_back(Wiimote::GetClassicGroup(self.m_port, WiimoteEmu::ClassicGroup::LeftStick));
        self->m_extension_groups.push_back(Wiimote::GetClassicGroup(self.m_port, WiimoteEmu::ClassicGroup::RightStick));
        self->m_extension_groups.push_back(Wiimote::GetClassicGroup(self.m_port, WiimoteEmu::ClassicGroup::Triggers));
        break;
    }
  }
  else
  {
    switch (SConfig::GetInstance().m_SIDevice[self.m_port])
    {
      case SerialInterface::SIDEVICE_GC_CONTROLLER:
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::Buttons));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::DPad));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::MainStick));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::CStick));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::Triggers));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::Rumble));
        self->m_controller_groups.push_back(Pad::GetGroup(self.m_port, PadGroup::Options));
        break;
      default:
        break;
    }
  }
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  if (self.m_is_wii)
  {
    return 5;
  }
  else
  {
    return 1;
  }
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  if (self.m_is_wii)
  {
    if (section == WII_SECTION_EXTENSION_SELECTOR)
    {
      return 1;
    }
    else if (section == WII_SECTION_NORMAL_MOTION)
    {
      return 4;
    }
    else if (section == WII_SECTION_IMU_MOTION)
    {
      return 4; // 3 groups, 1 help button
    }
    else if (section == WII_SECTION_EXTENSION_CONTROLS)
    {
      return self->m_extension_groups.size();
    }
  }
  
  return self->m_controller_groups.size();
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
  if (self.m_is_wii)
  {
    switch (section)
    {
      case WII_SECTION_EXTENSION_SELECTOR:
        return nil;
      case WII_SECTION_WIIMOTE_CONTROLS:
        return DOLocalizedString(@"Wii Remote");
      case WII_SECTION_NORMAL_MOTION:
        return DOLocalizedString(@"Motion Simulation");
      case WII_SECTION_IMU_MOTION:
        return DOLocalizedString(@"Motion Input");
      case WII_SECTION_EXTENSION_CONTROLS:
        // Don't show the header if there is nothing to customize
        if (self->m_extension_groups.size() == 0)
        {
          return nil;
        }
        
        return [self GetCurrentExtensionName];
    }
  }
  
  return nil;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  ControllerEmu::ControlGroup* group = nullptr;
  if (self.m_is_wii)
  {
    if (indexPath.section == WII_SECTION_EXTENSION_SELECTOR)
    {
      ControllerGroupExtensionButtonCell* cell = (ControllerGroupExtensionButtonCell*)[tableView dequeueReusableCellWithIdentifier:@"extensions_cell" forIndexPath:indexPath];
      [cell.m_extension_name setText:[self GetCurrentExtensionName]];
      
      return cell;
    }
    else if (indexPath.section == WII_SECTION_IMU_MOTION && indexPath.row == 0)
    {
      return [tableView dequeueReusableCellWithIdentifier:@"imu_help_cell" forIndexPath:indexPath];
    }
    else
    {
      group = [self GetGroupFromIndexPath:indexPath];
    }
  }
  else
  {
    group = self->m_controller_groups[indexPath.row];
  }
  
  if (group == nullptr)
  {
    // Guaranteed to crash
    return nil;
  }
  
  ControllerGroupCell* cell = (ControllerGroupCell*)[tableView dequeueReusableCellWithIdentifier:@"group_cell" forIndexPath:indexPath];
  [cell.m_group_label setText:DOLocalizedString([NSString stringWithUTF8String:group->ui_name.c_str()])];
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == WII_SECTION_IMU_MOTION && indexPath.row == 0)
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:NSLocalizedString(@"These settings are intended for use with real motion hardware. Compatible hardware includes your device's built-in motion sensors, and sensors on controllers connected with DSU Client.\n\nYou can enable IR pointer simulation using your motion hardware under \"Point\". IR pointer simulation using motion hardware is required for Wii MotionPlus games.", nil) preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:alert animated:true completion:nil];
    
    [self.tableView deselectRowAtIndexPath:indexPath animated:true];
  }
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_group"])
  {
    ControllerGroupViewController* view_controller = (ControllerGroupViewController*)segue.destinationViewController;
    view_controller.m_controller = self.m_controller;
    view_controller.m_control_group = [self GetGroupFromIndexPath:[self.tableView indexPathForSelectedRow]];
    view_controller.m_is_wii = self.m_is_wii;
  }
  else if ([segue.identifier isEqualToString:@"to_extensions"])
  {
    ControllerExtensionsViewController* view_controller = (ControllerExtensionsViewController*)segue.destinationViewController;
    view_controller.m_attachments = self.m_attachments;
  }
}

- (ControllerEmu::ControlGroup*)GetGroupFromIndexPath:(NSIndexPath*)indexPath
{
  ControllerEmu::ControlGroup* group = nullptr;
  if (self.m_is_wii)
  {
    if (indexPath.section == WII_SECTION_NORMAL_MOTION)
    {
      group = self->m_normal_motion_groups[indexPath.row];
    }
    else if (indexPath.section == WII_SECTION_IMU_MOTION)
    {
      group = self->m_imu_motion_groups[indexPath.row - 1];
    }
    else if (indexPath.section == WII_SECTION_WIIMOTE_CONTROLS)
    {
      group = self->m_controller_groups[indexPath.row];
    }
    else if (indexPath.section == WII_SECTION_EXTENSION_CONTROLS)
    {
      group = self->m_extension_groups[indexPath.row];
    }
  }
  else
  {
    group = self->m_controller_groups[indexPath.row];
  }
  
  return group;
}

- (NSString*)GetCurrentExtensionName
{
  u32 attachment = self.m_attachments->GetSelectedAttachment();
  std::string ui_name = self.m_attachments->GetAttachmentList()[attachment]->GetDisplayName();
  return DOLocalizedString([NSString stringWithUTF8String:ui_name.c_str()]);
}

@end
