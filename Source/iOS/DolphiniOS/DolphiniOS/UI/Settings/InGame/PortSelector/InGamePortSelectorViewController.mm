// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InGamePortSelectorViewController.h"

#import "MainiOS.h"

#import "PortSelectorCell.h"

@interface InGamePortSelectorViewController ()

@end

@implementation InGamePortSelectorViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.m_emulation_controller = (EmulationViewController*)[MainiOS getEmulationViewController];
  
  // Find the currently selected port
  self.m_last_selected = -1;
  
  if (self.m_emulation_controller.m_ts_active_port == -2)
  {
    self.m_last_selected = 0;
  }
  else
  {
    for (int i = 0; i < self.m_emulation_controller->m_controllers.size(); i++)
    {
      std::pair<int, UIView*> pair = self.m_emulation_controller->m_controllers[i];
      if (pair.first == self.m_emulation_controller.m_ts_active_port)
      {
        self.m_last_selected = i + 1;
      }
    }
  }
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self.m_emulation_controller->m_controllers.size() + 1;
}

- (UITableViewCell *)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  PortSelectorCell* cell = (PortSelectorCell*)[tableView dequeueReusableCellWithIdentifier:@"controller_cell" forIndexPath:indexPath];
  
  if (indexPath.row == 0)
  {
    [cell.m_controller_label setText:@"None"];
  }
  else
  {
    std::pair<int, UIView*> pair = self.m_emulation_controller->m_controllers[indexPath.row - 1];
    
    if ([pair.second isKindOfClass:[TCWiiPad class]])
    {
      [cell.m_controller_label setText:[NSString stringWithFormat:@"Wiimote %d", pair.first - 4 + 1]];
    }
    else
    {
      [cell.m_controller_label setText:[NSString stringWithFormat:@"GameCube Port %d", pair.first + 1]];
    }
  }
  
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

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (self.m_last_selected == indexPath.row)
  {
    [self.tableView deselectRowAtIndexPath:indexPath animated:true];
    
    return;
  }
  
  if (indexPath.row == 0)
  {
    [self.m_emulation_controller ChangeVisibleTouchControllerToPort:-2];
  }
  else
  {
    std::pair<int, UIView*> pair = self.m_emulation_controller->m_controllers[indexPath.row - 1];
    [self.m_emulation_controller ChangeVisibleTouchControllerToPort:pair.first];
  }
  
  UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [old_cell setAccessoryType:UITableViewCellAccessoryNone];
  
  UITableViewCell* new_cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  self.m_last_selected = (int)indexPath.row;
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
