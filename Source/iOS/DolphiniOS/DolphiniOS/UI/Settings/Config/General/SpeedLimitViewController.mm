// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SpeedLimitViewController.h"

#import "Core/ConfigManager.h"

#import "SpeedCell.h"

@interface SpeedLimitViewController ()

@end

@implementation SpeedLimitViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  self.m_last_selected = (NSInteger)(SConfig::GetInstance().m_EmulationSpeed / 0.1f);
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  // Unlimited and 10% to 200%, increments of 10%
  return 21;
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SpeedCell* cell = [tableView dequeueReusableCellWithIdentifier:@"speed_cell"];
  
  if (indexPath.row == 0)
  {
    [cell.m_speed_label setText:DOLocalizedString(@"Unlimited")];
  }
  else
  {
    [cell.m_speed_label setText:[NSString stringWithFormat:@"%ld%%", indexPath.row * 10]];
  }
  
  if (indexPath.row == self.m_last_selected)
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
  if (self.m_last_selected != indexPath.row)
  {
    SConfig::GetInstance().m_EmulationSpeed = indexPath.row * 0.1f;
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    
    self.m_last_selected = indexPath.row;
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
