// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwarePatchesViewController.h"

#import "Common/FileUtil.h"
#import "Common/IniFile.h"

#import "Core/ConfigManager.h"
#import "Core/PatchEngine.h"

#import "SoftwareActionReplayCodeCell.h"

#import "UICommon/GameFile.h"

@interface SoftwarePatchesViewController ()

@end

@implementation SoftwarePatchesViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  const std::string game_id = self.m_game_file->GetGameID();
  const u16 game_revision = self.m_game_file->GetRevision();
  
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) +  game_id + ".ini");

  IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, game_revision);
  
  PatchEngine::LoadPatchSection("OnFrame", m_patches, game_ini_default, game_ini_local);
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [self SaveCodes];
}

- (void)SaveCodes
{
  std::vector<std::string> lines;
  std::vector<std::string> lines_enabled;

  for (const PatchEngine::Patch& patch : m_patches)
  {
    if (patch.active)
      lines_enabled.push_back("$" + patch.name);

    if (!patch.user_defined)
      continue;

    lines.push_back("$" + patch.name);

    for (const auto& entry : patch.entries)
    {
      lines.push_back(StringFromFormat("0x%08X:%s:0x%08X", entry.address,
                                       PatchEngine::PatchTypeAsString(entry.type), entry.value));
    }
  }
  
  const std::string game_id = self.m_game_file->GetGameID();

  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");

  game_ini_local.SetLines("OnFrame_Enabled", lines_enabled);
  game_ini_local.SetLines("OnFrame", lines);

  game_ini_local.Save(File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini");
}

- (IBAction)EnabledChanged:(UISwitch*)sender
{
  self->m_patches[sender.tag].active = [sender isOn];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self->m_patches.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  SoftwareActionReplayCodeCell* cell = [tableView dequeueReusableCellWithIdentifier:@"code_cell" forIndexPath:indexPath];
  PatchEngine::Patch code = self->m_patches[indexPath.row];
  
  [cell.m_name_label setText:CppToFoundationString(code.name)];
  [cell.m_enabled_switch setOn:code.active];
  [cell.m_enabled_switch setTag:indexPath.row];
  
  return cell;
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
