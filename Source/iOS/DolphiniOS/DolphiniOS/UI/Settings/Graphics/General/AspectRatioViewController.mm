// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AspectRatioViewController.h"

#import "Core/Config/GraphicsSettings.h"

#import "Common/Config/Config.h"

@interface AspectRatioViewController ()

@end

@implementation AspectRatioViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:(NSInteger)Config::Get(Config::GFX_ASPECT_RATIO) inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSInteger last_ratio = (NSInteger)Config::Get(Config::GFX_ASPECT_RATIO);
  if (indexPath.row != last_ratio)
  {
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:last_ratio inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBaseOrCurrent(Config::GFX_ASPECT_RATIO, (AspectMode)indexPath.row);
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
