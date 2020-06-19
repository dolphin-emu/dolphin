// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareActionReplayViewController.h"

#import "Common/FileUtil.h"
#import "Common/IniFile.h"

#import "Core/ConfigManager.h"

#import "SoftwareActionReplayCodeCell.h"

@interface SoftwareActionReplayViewController ()

@end

@implementation SoftwareActionReplayViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)viewWillAppear:(BOOL)animated
{
  const std::string game_id = self.m_game_file->GetGameID();
  const u16 game_revision = self.m_game_file->GetRevision();
  
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) +  game_id + ".ini");

  const IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, game_revision);
  self->m_ar_codes = ActionReplay::LoadCodes(game_ini_default, game_ini_local);
}

- (void)viewWillDisappear:(BOOL)animated
{
  const auto ini_path =
  std::string(File::GetUserPath(D_GAMESETTINGS_IDX)).append(self.m_game_file->GetGameID()).append(".ini");

  IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  ActionReplay::SaveCodes(&game_ini_local, self->m_ar_codes);
  game_ini_local.Save(ini_path);
}

- (IBAction)EnabledChanged:(UISwitch*)sender
{
  self->m_ar_codes[sender.tag].active = [sender isOn];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self->m_ar_codes.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareActionReplayCodeCell* cell = [tableView dequeueReusableCellWithIdentifier:@"code_cell" forIndexPath:indexPath];
  ActionReplay::ARCode code = self->m_ar_codes[indexPath.row];
  
  [cell.m_name_label setText:CppToFoundationString(code.name)];
  [cell.m_enabled_switch setOn:code.active];
  [cell.m_enabled_switch setTag:indexPath.row];
  
  return cell;
}

- (BOOL)tableView:(UITableView*)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath
{
  return true;
}

- (void)tableView:(UITableView*)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle == UITableViewCellEditingStyleDelete)
  {
    self->m_ar_codes.erase(self->m_ar_codes.begin() + indexPath.row);
    [self.tableView deleteRowsAtIndexPaths:@[ indexPath ] withRowAnimation:UITableViewRowAnimationFade];
  }
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
  // Get the new view controller using [segue destinationViewController].
  // Pass the selected object to the new view controller.
}
*/

@end
