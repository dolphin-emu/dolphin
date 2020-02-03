// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file importd.

#import "ConfigAdvancedViewController.h"

#import <cmath>

#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"
#import "Core/Core.h"
#import "Core/HW/SystemTimers.h"
#import "Core/PowerPC/PowerPC.h"

@interface ConfigAdvancedViewController ()

@end

@implementation ConfigAdvancedViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_mmu_switch setOn:SConfig::GetInstance().bMMU];
  [self.m_overclock_switch setOn:SConfig::GetInstance().m_OCEnable];
  
  bool running = Core::GetState() != Core::State::Uninitialized;
  [[self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]] setUserInteractionEnabled:!running];
  [self.m_mmu_switch setEnabled:!running];
  [self.m_overclock_switch setEnabled:!running];
  
  [self.m_overclock_slider setValue:static_cast<int>(std::round(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f))];
  
  [self UpdateOverclockSlider];
}

- (IBAction)MMUChanged:(id)sender
{
  SConfig::GetInstance().bMMU = [self.m_mmu_switch isOn];
}

- (IBAction)OverclockEnabledChanged:(id)sender
{
  SConfig::GetInstance().m_OCEnable = [self.m_overclock_switch isOn];
  Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK_ENABLE, [self.m_overclock_switch isOn]);
  
  [self UpdateOverclockSlider];
}

- (IBAction)OverclockSliderChanged:(id)sender
{
  const float factor = std::exp2f(([self.m_overclock_slider value] - 100.f) / 25.f);
  SConfig::GetInstance().m_OCFactor = factor;
  Config::SetBaseOrCurrent(Config::MAIN_OVERCLOCK, factor);
  
  [self UpdateOverclockSlider];
}

- (void)UpdateOverclockSlider
{
  bool running = Core::GetState() != Core::State::Uninitialized;
  bool oc_slider_enabled = !running && [self.m_overclock_switch isOn];
  
  [self.m_overclock_slider setEnabled:oc_slider_enabled];
  
  int core_clock = SystemTimers::GetTicksPerSecond() / std::pow(10, 6);
  int percent = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * 100.f));
  int clock = static_cast<int>(std::round(SConfig::GetInstance().m_OCFactor * core_clock));
  NSString* str = [NSString stringWithFormat:@"%d%% (%d MHz)", percent, clock];
  
  [self.m_overclock_label setText:str];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1 && indexPath.row == 2)
  {
    NSString* msg = @"Adjusts the emulated CPU's clock rate.\n\n"
    "Higher values may make variable-framerate games run at a higher framerate, "
    "at the expense of performance. Lower values may activate a game's "
    "internal frameskip, potentially improving performance.\n\n"
    "WARNING: Changing this from the default (100%) can and will "
    "break games and cause glitches. Do so at your own risk. "
    "Please do not report bugs that occur with a non-default clock.";
    UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:DOLocalizedString(msg) preferredStyle:UIAlertControllerStyleAlert];
    [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:controller animated:true completion:nil];
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
