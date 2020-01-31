// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "WiiAspectRatioViewController.h"

#import "Core/Config/SYSCONFSettings.h"

@interface WiiAspectRatioViewController ()

@end

@implementation WiiAspectRatioViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::SYSCONF_WIDESCREEN) ? 1 : 0 inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if ((Config::Get(Config::SYSCONF_WIDESCREEN) ? 1 : 0) != indexPath.row)
  {
    UITableViewCell* cell = [tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::SYSCONF_WIDESCREEN) ? 1 : 0 inSection:0]];
    [cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* new_cell = [tableView cellForRowAtIndexPath:indexPath];
    [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBase<bool>(Config::SYSCONF_WIDESCREEN, indexPath.row == 1);
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}


@end
