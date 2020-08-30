// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareGeckoViewController.h"

#import "Common/FileUtil.h"
#import "Common/IniFile.h"

#import "Core/ConfigManager.h"
#import "Core/GeckoCodeConfig.h"

#import "SoftwareActionReplayCodeCell.h"
#import "SoftwareGeckoEditViewController.h"

@interface SoftwareGeckoViewController ()

@end

@implementation SoftwareGeckoViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.navigationItem.rightBarButtonItem = self.editButtonItem;
  
  const std::string game_id = self.m_game_file->GetGameID();
  const u16 game_revision = self.m_game_file->GetRevision();
  
  IniFile game_ini_local;
  game_ini_local.Load(File::GetUserPath(D_GAMESETTINGS_IDX) +  game_id + ".ini");

  const IniFile game_ini_default = SConfig::LoadDefaultGameIni(game_id, game_revision);
  self->m_gecko_codes = Gecko::LoadCodes(game_ini_default, game_ini_local);
  
  [self.tableView reloadData];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  // Forcefully disable all codes when this menu is entered
  for (Gecko::GeckoCode& code : self->m_gecko_codes)
  {
    code.enabled = false;
  }
  
  [self SaveCodes];
  
  UIAlertController* alert_controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Error") message:NSLocalizedString(@"Gecko codes are currently broken on all phone and tablet versions of Dolphin (including Android).\n\nPlease wait until this issue is fixed in the official Dolphin builds.", nil) preferredStyle:UIAlertControllerStyleAlert];
  [alert_controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
    [self performSegueWithIdentifier:@"to_properties" sender:nil];
  }]];
  
  [self presentViewController:alert_controller animated:true completion:nil];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [self SaveCodes];
}

- (void)SaveCodes
{
  const auto ini_path =
  std::string(File::GetUserPath(D_GAMESETTINGS_IDX)).append(self.m_game_file->GetGameID()).append(".ini");

  IniFile game_ini_local;
  game_ini_local.Load(ini_path);
  Gecko::SaveCodes(game_ini_local, self->m_gecko_codes);
  game_ini_local.Save(ini_path);
}

- (IBAction)EnabledChanged:(UISwitch*)sender
{
  self->m_gecko_codes[sender.tag].enabled = [sender isOn];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 2;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return section == 0 ? 2 : self->m_gecko_codes.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 0)
  {
    if (indexPath.row == 0)
    {
      // Add cell
      return [tableView dequeueReusableCellWithIdentifier:@"add_cell" forIndexPath:indexPath];
    }
    else
    {
      // Download cell
      return [tableView dequeueReusableCellWithIdentifier:@"download_cell" forIndexPath:indexPath];
    }
  }
  else
  {
    SoftwareActionReplayCodeCell* cell = [tableView dequeueReusableCellWithIdentifier:@"code_cell" forIndexPath:indexPath];
    Gecko::GeckoCode code = self->m_gecko_codes[indexPath.row];
    
    [cell.m_name_label setText:CppToFoundationString(code.name)];
    [cell.m_enabled_switch setOn:code.enabled];
    [cell.m_enabled_switch setTag:indexPath.row];
    
    return cell;
  }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  // TODO: download cell
  [self performSegueWithIdentifier:@"to_code_edit" sender:nil];
}

- (BOOL)tableView:(UITableView*)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath
{
  return indexPath.section == 1;
}

- (void)tableView:(UITableView*)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle == UITableViewCellEditingStyleDelete)
  {
    self->m_gecko_codes.erase(self->m_gecko_codes.begin() + indexPath.row);
    [self.tableView deleteRowsAtIndexPaths:@[ indexPath ] withRowAnimation:UITableViewRowAnimationFade];
    
    [self SaveCodes];
  }
}

#pragma mark - Navigation

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_code_edit"])
  {
    NSIndexPath* selected_row = self.tableView.indexPathForSelectedRow;
    if (selected_row.section == 0)
    {
      return;
    }
    
    SoftwareGeckoEditViewController* edit_controller = (SoftwareGeckoEditViewController*)segue.destinationViewController;
    edit_controller.m_gecko_code = &self->m_gecko_codes[selected_row.row];
  }
}

- (IBAction)unwindToGeckoMenu:(UIStoryboardSegue*)segue
{
  SoftwareGeckoEditViewController* edit_controller = (SoftwareGeckoEditViewController*)segue.sourceViewController;
  if ([edit_controller m_should_be_pushed_back])
  {
    self->m_gecko_codes.push_back(std::move(*[edit_controller m_gecko_code]));
  }
  
  [self SaveCodes];
  [self.tableView reloadData];
}

@end
