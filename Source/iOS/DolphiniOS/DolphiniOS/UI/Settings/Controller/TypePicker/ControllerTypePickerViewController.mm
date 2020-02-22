// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerTypePickerViewController.h"

#import "Core/ConfigManager.h"
#import "Core/Core.h"
#import "Core/HW/SI/SI.h"
#import "Core/HW/WiimoteReal/WiimoteReal.h"

#import "UICommon/UICommon.h"

#import "ControllerSettingsUtils.h"
#import "ControllerTypeCell.h"

@interface ControllerTypePickerViewController ()

@end

@implementation ControllerTypePickerViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  if (self.m_is_wii)
  {
    self.m_last_selected = (NSInteger)WiimoteCommon::GetSource(self.m_port);
  }
  else
  {
    self.m_last_selected = [ControllerSettingsUtils SIDevicesToGCMenuIndex:SConfig::GetInstance().m_SIDevice[self.m_port]].value();
  }
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  if (self.m_is_wii)
  {
    return [ControllerSettingsUtils GetTotalAvailableWiimoteTypes];
  }
  else
  {
    return [ControllerSettingsUtils GetTotalAvailableSIDevices];
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  ControllerTypeCell* cell = (ControllerTypeCell*)[tableView dequeueReusableCellWithIdentifier:@"type_cell" forIndexPath:indexPath];
  
  NSString* type;
  if (self.m_is_wii)
  {
    type = [ControllerSettingsUtils GetLocalizedWiimoteStringFromSource:(WiimoteSource)indexPath.row];
  }
  else
  {
    type = [ControllerSettingsUtils GetLocalizedGameCubeControllerFromIndex:indexPath.row];
  }
  
  [cell.m_type_label setText:type];
  
  if (![self IsSelectable:indexPath])
  {
    [cell.m_type_label setTextColor:[UIColor systemGrayColor]];
  }
  
  // Set if this is checked
  if (self.m_last_selected == indexPath.row)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  else
  {
    [cell setAccessoryType:UITableViewCellAccessoryNone];
  }
  
  return cell;
}

- (NSIndexPath*)tableView:(UITableView*)tableView willSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (![self IsSelectable:indexPath])
  {
    return nil;
  }

  return indexPath;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (self.m_last_selected == indexPath.row)
  {
    [self.tableView deselectRowAtIndexPath:indexPath animated:true];
    return;
  }
  
  if (self.m_is_wii)
  {
    WiimoteCommon::SetSource(self.m_port, static_cast<WiimoteSource>(indexPath.row));
    
    UICommon::SaveWiimoteSources();
  }
  else
  {
    std::optional<SerialInterface::SIDevices> si_device = [ControllerSettingsUtils SIDevicesFromGCMenuIndex:(int)indexPath.row];
    if (si_device)
    {
      SConfig::GetInstance().m_SIDevice[self.m_port] = *si_device;
      
      if (Core::IsRunning())
      {
        SerialInterface::ChangeDevice(*si_device, static_cast<s32>(indexPath.row));
      }
    }
  }
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [old_cell setAccessoryType:UITableViewCellAccessoryNone];
  
  self.m_last_selected = indexPath.row;
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (bool)IsSelectable:(NSIndexPath*)index_path
{
  if (self.m_is_wii)
  {
    return index_path.row != 2; // not Real
  }
  else
  {
    return index_path.row < 2; // not GC Adapter or others
  }
}

@end
