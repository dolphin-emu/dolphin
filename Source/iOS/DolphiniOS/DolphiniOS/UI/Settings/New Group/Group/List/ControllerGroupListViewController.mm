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
    
    if ([ControllerSettingsUtils ShouldControllerSupportFullMotion:self.m_controller] || self.m_controller->GetDefaultDevice().source == "Android")
    {
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUPoint));
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUAccelerometer));
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::IMUGyroscope));
    }
    else
    {
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Shake));
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Point));
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Tilt));
      self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Swing));
    }
    
    self->m_controller_groups.push_back(Wiimote::GetWiimoteGroup(self.m_port, WiimoteEmu::WiimoteGroup::Options));
    
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
    return 3;
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
    if (section == 0)
    {
      return 1;
    }
    else if (section == 2)
    {
      return self->m_extension_groups.size();
    }
  }
  
  return self->m_controller_groups.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (self.m_is_wii && indexPath.section == 0)
  {
    ControllerGroupExtensionButtonCell* cell = (ControllerGroupExtensionButtonCell*)[tableView dequeueReusableCellWithIdentifier:@"extensions_cell" forIndexPath:indexPath];
    [cell.m_extension_name setText:[self GetCurrentExtensionName]];
    
    return cell;
  }
  
  ControllerGroupCell* cell = (ControllerGroupCell*)[tableView dequeueReusableCellWithIdentifier:@"group_cell" forIndexPath:indexPath];
  
  NSString* ui_name;
  if (self.m_is_wii && indexPath.section == 2)
  {
    ui_name = [NSString stringWithUTF8String:self->m_extension_groups[indexPath.row]->ui_name.c_str()];
  }
  else
  {
    ui_name = [NSString stringWithUTF8String:self->m_controller_groups[indexPath.row]->ui_name.c_str()];
  }
  
  [cell.m_group_label setText:ui_name];
  
  return cell;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
  if (self.m_is_wii)
  {
    switch (section)
    {
      case 0:
        return nil;
      case 1:
        return @"Wiimote";
      case 2:
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

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_group"])
  {
    ControllerGroupViewController* view_controller = (ControllerGroupViewController*)segue.destinationViewController;
    view_controller.m_controller = self.m_controller;
    
    NSIndexPath* index_path = [self.tableView indexPathForSelectedRow];
    if (self.m_is_wii && index_path.section == 2)
    {
      view_controller.m_control_group = self->m_extension_groups[[self.tableView indexPathForSelectedRow].row];
    }
    else
    {
      view_controller.m_control_group = self->m_controller_groups[[self.tableView indexPathForSelectedRow].row];
    }
    
    view_controller.m_is_wii = self.m_is_wii;
  }
  else if ([segue.identifier isEqualToString:@"to_extensions"])
  {
    ControllerExtensionsViewController* view_controller = (ControllerExtensionsViewController*)segue.destinationViewController;
    view_controller.m_attachments = self.m_attachments;
  }
}

- (NSString*)GetCurrentExtensionName
{
  u32 attachment = self.m_attachments->GetSelectedAttachment();
  std::string ui_name = self.m_attachments->GetAttachmentList()[attachment]->GetDisplayName();
  return [NSString stringWithUTF8String:ui_name.c_str()];
}

@end
