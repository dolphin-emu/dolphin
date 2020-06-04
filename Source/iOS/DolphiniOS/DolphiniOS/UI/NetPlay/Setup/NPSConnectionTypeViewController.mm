// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NPSConnectionTypeViewController.h"

#import "Core/Config/NetplaySettings.h"

@interface NPSConnectionTypeViewController ()

@end

@implementation NPSConnectionTypeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(bool)animated
{
  [super viewWillAppear:animated];
  
  std::string choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  if (choice == "traversal")
  {
    self.m_last_selected = 0;
  }
  else
  {
    self.m_last_selected = 1;
  }
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (self.m_last_selected == indexPath.row)
  {
    return;
  }
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryNone];
  
  cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:indexPath.row inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  if (indexPath.row == 0)
  {
    Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, "traversal");
  }
  else
  {
    Config::SetBaseOrCurrent(Config::NETPLAY_TRAVERSAL_CHOICE, "direct");
  }
  
  self.m_last_selected = indexPath.row;
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}




@end
