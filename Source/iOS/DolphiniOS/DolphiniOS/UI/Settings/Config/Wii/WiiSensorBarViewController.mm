// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "WiiSensorBarViewController.h"

#import "Core/Config/SYSCONFSettings.h"

@interface WiiSensorBarViewController ()

@end

@implementation WiiSensorBarViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  NSIndexPath* cell_path = [NSIndexPath indexPathForRow:[self TranslatePosition:Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION)] inSection:0];
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:cell_path];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  // SYSCONF to table view ordering
  // See WiiPane.cpp for more information
  int position = [self TranslatePosition:(int)indexPath.row];
  
  if (Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION) != position)
  {
    NSIndexPath* cell_path = [NSIndexPath indexPathForRow:[self TranslatePosition:Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION)] inSection:0];
    UITableViewCell* cell = [tableView cellForRowAtIndexPath:cell_path];
    [cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* new_cell = [tableView cellForRowAtIndexPath:indexPath];
    [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBase<u32>(Config::SYSCONF_SENSOR_BAR_POSITION, position);
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (int)TranslatePosition:(int)position
{
  if (position == 0)
    return 1;
  if (position == 1)
    return 0;

  return position;
}


@end
