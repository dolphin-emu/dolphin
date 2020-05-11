// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigGeneralViewController.h"

#import "Core/Analytics.h"
#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"
#import "Core/Core.h"

#import <FirebaseAnalytics/FirebaseAnalytics.h>
#import <FirebaseCrashlytics/FirebaseCrashlytics.h>

@interface ConfigGeneralViewController ()

@end

@implementation ConfigGeneralViewController

// Skipped settings:
// "Show Current Game on Discord" - no Discord integration on iOS
// "Auto Update Settings" (all) - auto-update not supported with Cydia

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
  [self.m_statistics_switch setOn:SConfig::GetInstance().m_analytics_enabled];
  [self.m_crash_report_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"crash_reporting_enabled"]];
  
  bool running = Core::GetState() != Core::State::Uninitialized;
  [self.m_dual_core_switch setEnabled:!running];
  [self.m_cheats_switch setEnabled:!running];
  [self.m_mismatched_region_switch setEnabled:!running];
  
#ifdef DEBUG
  [self.m_statistics_switch setEnabled:false];
  [self.m_crash_report_switch setEnabled:false];
#endif
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

- (IBAction)StatisticsChanged:(id)sender
{
  SConfig::GetInstance().m_analytics_enabled = [self.m_statistics_switch isOn];
  DolphinAnalytics::Instance().ReloadConfig();
#ifdef ANALYTICS
  [FIRAnalytics setAnalyticsCollectionEnabled:[self.m_statistics_switch isOn]];
#endif
}

- (IBAction)CrashReportingChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_crash_report_switch isOn] forKey:@"crash_reporting_enabled"];
  [[FIRCrashlytics crashlytics] setCrashlyticsCollectionEnabled:[self.m_crash_report_switch isOn]];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1 && indexPath.row == 2)
  {
    DolphinAnalytics::Instance().GenerateNewIdentity();
    DolphinAnalytics::Instance().ReloadConfig();
    [FIRAnalytics resetAnalyticsData];
    [[NSUserDefaults standardUserDefaults] setObject:[[NSArray alloc] init] forKey:@"unique_games"];
    
    UIAlertController* alert_controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Identity Generation") message:DOLocalizedString(@"New identity generated.") preferredStyle:UIAlertControllerStyleAlert];
    [alert_controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:alert_controller animated:true completion:nil];
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
