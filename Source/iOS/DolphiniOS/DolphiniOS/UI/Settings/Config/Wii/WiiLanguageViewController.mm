// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "WiiLanguageViewController.h"

#import "Core/Config/SYSCONFSettings.h"

@interface WiiLanguageViewController ()

@end

@implementation WiiLanguageViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::SYSCONF_LANGUAGE) inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (Config::Get(Config::SYSCONF_LANGUAGE) != indexPath.row)
  {
    UITableViewCell* cell = [tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:Config::Get(Config::SYSCONF_LANGUAGE) inSection:0]];
    [cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* new_cell = [tableView cellForRowAtIndexPath:indexPath];
    [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBase<u32>(Config::SYSCONF_LANGUAGE, (int)indexPath.row);
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
