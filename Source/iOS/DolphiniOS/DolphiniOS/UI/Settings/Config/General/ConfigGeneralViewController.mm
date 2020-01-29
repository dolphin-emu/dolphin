// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigGeneralViewController.h"

#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"
#import "Core/Core.h"

@interface ConfigGeneralViewController ()

@end

@implementation ConfigGeneralViewController

// Skipped settings:
// "Show Current Game on Discord" - no Discord integration on iOS
// "Auto Update Settings" (all) - auto-update not supported with Cydia
// "Usage Statistics Reporting Settings" - no analytics server available for iOS

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_dual_core_switch setOn:SConfig::GetInstance().bCPUThread];
  [self.m_cheats_switch setOn:SConfig::GetInstance().bEnableCheats];
  [self.m_mismatched_region_switch setOn:SConfig::GetInstance().bOverrideRegionSettings];
  [self.m_change_discs_switch setOn:Config::Get(Config::MAIN_AUTO_DISC_CHANGE)];
  
  bool running = Core::GetState() != Core::State::Uninitialized;
  [self.m_dual_core_switch setEnabled:!running];
  [self.m_cheats_switch setEnabled:!running];
  [self.m_mismatched_region_switch setEnabled:!running];
}

- (IBAction)DualCoreChanged:(id)sender
{
  SConfig::GetInstance().bCPUThread = [self.m_dual_core_switch isOn];
  Config::SetBaseOrCurrent(Config::MAIN_CPU_THREAD, [self.m_dual_core_switch isOn]);
}

- (IBAction)EnableCheatsChanged:(id)sender
{
  SConfig::GetInstance().bEnableCheats = [self.m_cheats_switch isOn];
  Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, [self.m_cheats_switch isOn]);
}

- (IBAction)MismatchedRegionChanged:(id)sender
{
  SConfig::GetInstance().bOverrideRegionSettings = [self.m_mismatched_region_switch isOn];
  Config::SetBaseOrCurrent(Config::MAIN_OVERRIDE_REGION_SETTINGS,
                           [self.m_mismatched_region_switch isOn]);
}

- (IBAction)ChangeDiscsChanged:(id)sender
{
  Config::SetBase(Config::MAIN_AUTO_DISC_CHANGE, [self.m_change_discs_switch isOn]);
}

@end
