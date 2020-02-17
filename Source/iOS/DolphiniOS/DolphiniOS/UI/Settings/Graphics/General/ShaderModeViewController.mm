// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ShaderModeViewController.h"

#import "Core/Config/GraphicsSettings.h"

#import "Common/Config/Config.h"

#import "GraphicsSettingsUtils.h"

@interface ShaderModeViewController ()

@end

@implementation ShaderModeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:(NSInteger)Config::Get(Config::GFX_SHADER_COMPILATION_MODE) inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  NSInteger last_mode = (NSInteger)Config::Get(Config::GFX_SHADER_COMPILATION_MODE);
  if (indexPath.row != last_mode)
  {
    UITableViewCell* old_cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:last_mode inSection:0]];
    [old_cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    Config::SetBaseOrCurrent(Config::GFX_SHADER_COMPILATION_MODE, (ShaderCompilationMode)indexPath.row);
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}
- (IBAction)SyncInfoPressed:(id)sender
{
  GSUShowHelpWithString(@"Ubershaders are never used. Stuttering will occur during shader "
                        "compilation, but GPU demands are low.\n\nRecommended for low-end hardware. "
                        "\n\nIf unsure, select this mode.");
}

- (IBAction)SyncUberInfoPressed:(id)sender
{
  GSUShowHelpWithString(@"Ubershaders will always be used. Provides a near stutter-free experience at the cost of "
                        "high GPU performance requirements.\n\nOnly recommended for high-end systems.");
}

- (IBAction)AsyncUberInfoPressed:(id)sender
{
  GSUShowHelpWithString(@"Ubershaders will be used to prevent stuttering during shader compilation, but "
                        "specialized shaders will be used when they will not cause stuttering.\n\nIn the "
                        "best case it eliminates shader compilation stuttering while having minimal "
                        "performance impact, but results depend on video driver behavior.");
}

- (IBAction)AsyncSkipInfoPressed:(id)sender
{
  GSUShowHelpWithString(@"Prevents shader compilation stuttering by not rendering waiting objects. Can work in "
                        "scenarios where Ubershaders doesn't, at the cost of introducing visual glitches and broken "
                        "effects.\n\nNot recommended, only use if the other options give poor results.");
}

@end
