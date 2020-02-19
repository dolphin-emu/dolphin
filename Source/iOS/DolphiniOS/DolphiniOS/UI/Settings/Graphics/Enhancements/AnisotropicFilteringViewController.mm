// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AnisotropicFilteringViewController.h"

#import "Core/Config/GraphicsSettings.h"

@interface AnisotropicFilteringViewController ()

@end

@implementation AnisotropicFilteringViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY) inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSInteger last_anisotropy = Config::Get(Config::GFX_ENHANCE_MAX_ANISOTROPY);
  if (indexPath.row != last_anisotropy)
  {
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:last_anisotropy inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBaseOrCurrent(Config::GFX_ENHANCE_MAX_ANISOTROPY, (int)indexPath.row);
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
