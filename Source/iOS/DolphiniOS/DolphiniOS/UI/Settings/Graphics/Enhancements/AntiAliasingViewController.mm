// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AntiAliasingViewController.h"

#import <cmath>

#import "AntiAliasingCell.h"

#import "Core/Config/GraphicsSettings.h"
#import "Core/ConfigManager.h"

#import "UICommon/VideoUtils.h"

@interface AntiAliasingViewController ()

@end

@implementation AntiAliasingViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  int aa_selection = Config::Get(Config::GFX_MSAA);
  bool ssaa = Config::Get(Config::GFX_SSAA);
  
  for (const auto& option : VideoUtils::GetAvailableAntialiasingModes(self->m_msaa_modes))
  {
    self->m_aa_modes.push_back(option);
  }
  
  std::string aa_name = std::to_string(aa_selection) + "x " + (ssaa ? "SSAA" : "MSAA");
  for (size_t i = 0; i < self->m_aa_modes.size(); i++)
  {
    if (self->m_aa_modes[i] == aa_name)
    {
      self.m_last_selected = i;
    }
  }
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
  if (cell != nil)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self->m_aa_modes.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  AntiAliasingCell* cell = [tableView dequeueReusableCellWithIdentifier:@"aa_cell"];
  [cell.m_name_label setText:[NSString stringWithUTF8String:self->m_aa_modes[indexPath.row].c_str()]];
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (self.m_last_selected != indexPath.row)
  {
    NSInteger aa_value = indexPath.row;
    
    bool is_ssaa = false;
    if (aa_value == 0)
    {
      aa_value = 1;
    }
    else
    {
      if (aa_value > m_msaa_modes)
      {
        aa_value -= m_msaa_modes;
        is_ssaa = true;
      }
      aa_value = std::pow(2, aa_value);
    }
    
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_last_selected inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    self.m_last_selected = indexPath.row;
    
    Config::SetBaseOrCurrent(Config::GFX_MSAA, static_cast<unsigned int>(aa_value));
    Config::SetBaseOrCurrent(Config::GFX_SSAA, is_ssaa);
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
