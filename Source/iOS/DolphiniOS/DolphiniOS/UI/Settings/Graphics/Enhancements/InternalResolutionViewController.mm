// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InternalResolutionViewController.h"

#import "Core/Config/GraphicsSettings.h"

@interface InternalResolutionViewController ()

@end

@implementation InternalResolutionViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::GFX_EFB_SCALE) inSection:0]];
  if (cell != nil)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSInteger last_scale = Config::Get(Config::GFX_EFB_SCALE);
  if (indexPath.row != last_scale)
  {
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:last_scale inSection:0]];
    if (old_cell != nil)
    {
      [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    }
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBaseOrCurrent(Config::GFX_EFB_SCALE, (int)indexPath.row);
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
