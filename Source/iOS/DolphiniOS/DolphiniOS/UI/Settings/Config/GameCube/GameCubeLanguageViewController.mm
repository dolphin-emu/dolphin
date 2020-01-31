// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GameCubeLanguageViewController.h"

#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"

@interface GameCubeLanguageViewController ()

@end

@implementation GameCubeLanguageViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:SConfig::GetInstance().SelectedLanguage inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (SConfig::GetInstance().SelectedLanguage != indexPath.row)
  {
    UITableViewCell* cell = [tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:SConfig::GetInstance().SelectedLanguage inSection:0]];
    [cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* new_cell = [tableView cellForRowAtIndexPath:indexPath];
    [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    SConfig::GetInstance().SelectedLanguage = (int)indexPath.row;
    Config::SetBaseOrCurrent(Config::MAIN_GC_LANGUAGE, (int)indexPath.row);
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
