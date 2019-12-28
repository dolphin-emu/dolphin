// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerLoadProfileViewController.h"

#import "Common/FileSearch.h"
#import "Common/FileUtil.h"
#import "Common/IniFile.h"
#import "Common/StringUtil.h"

#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"

#import "ControllerProfileCell.h"

constexpr const char* PROFILES_DIR = "Profiles/";

@interface ControllerLoadProfileViewController ()

@end

@implementation ControllerLoadProfileViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  self.navigationItem.rightBarButtonItem = self.editButtonItem;
  
  if (self.m_is_wii)
  {
    self.m_config = Wiimote::GetConfig();
  }
  else
  {
    self.m_config = Pad::GetConfig();
  }
  
  const std::string profiles_path =
  File::GetUserPath(D_CONFIG_IDX) + PROFILES_DIR + self.m_config->GetProfileName();
  for (const auto& filename : Common::DoFileSearch({profiles_path}, {".ini"}))
  {
    self->m_profiles.push_back(filename);
  }
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  return 2;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  if (section == 0)
  {
    return 1;
  }
  
  return self->m_profiles.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  
  ControllerProfileCell* cell = (ControllerProfileCell*)[tableView dequeueReusableCellWithIdentifier:@"profile_cell" forIndexPath:indexPath];
  
  if (indexPath.section == 0)
  {
    [cell.m_profile_label setText:@"Default Configuration"];
  }
  else
  {
    std::string basename;
    SplitPath(self->m_profiles[indexPath.row], nullptr, &basename, nullptr);
    
    [cell.m_profile_label setText:[NSString stringWithUTF8String:basename.c_str()]];
  }
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  std::string profile_path;
  if (indexPath.section == 0)
  {
    NSString* bundle_path;
    if (self.m_is_wii)
    {
      bundle_path = [[NSBundle mainBundle] pathForResource:@"MFiWiimoteProfile" ofType:@"ini"];
    }
    else
    {
      bundle_path = [[NSBundle mainBundle] pathForResource:@"MFiGCPadProfile" ofType:@"ini"];
    }
    
    profile_path = std::string([bundle_path cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  else
  {
    profile_path = self->m_profiles[indexPath.row];
  }
  
  // Preserve the device if needed (ie loading defaults)
  std::string device;
  if (indexPath.section == 0)
  {
    device = self.m_controller->GetDefaultDevice().ToString();
  }
  
  IniFile ini;
  ini.Load(profile_path);
  self.m_controller->LoadConfig(ini.GetOrCreateSection("Profile"));
  
  if (indexPath.section == 0)
  {
    self.m_controller->SetDefaultDevice(device);
  }
  
  self.m_controller->UpdateReferences(g_controller_interface);
  
  self.m_config->SaveConfig();
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
  
  [self performSegueWithIdentifier:@"to_details" sender:nil];
}

- (BOOL)tableView:(UITableView*)tableView canEditRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 0)
  {
    return false;
  }
  
  return true;
}

- (void)tableView:(UITableView*)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle == UITableViewCellEditingStyleDelete)
  {
    std::string to_erase = self->m_profiles[indexPath.row];
    File::Delete(to_erase);
    
    self->m_profiles.erase(self->m_profiles.begin() + indexPath.row);
    
    [self.tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
  }
}

@end
