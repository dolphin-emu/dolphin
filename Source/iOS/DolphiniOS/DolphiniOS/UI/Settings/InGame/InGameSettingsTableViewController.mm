// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InGameSettingsTableViewController.h"

#import "MainiOS.h"

@interface InGameSettingsTableViewController ()

@end

@implementation InGameSettingsTableViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 0 && indexPath.row == 2)
  {
    [MainiOS gamepadEventIrRecenter:1];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      [MainiOS gamepadEventIrRecenter:0];
    });
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
