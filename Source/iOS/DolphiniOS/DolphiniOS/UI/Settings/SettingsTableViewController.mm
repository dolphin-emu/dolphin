// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SettingsTableViewController.h"

#import "AppDelegate.h"

#import "Common/CommonFuncs.h"

#import "JitAcquisitionUtils.h"

@interface SettingsTableViewController ()

@end

@implementation SettingsTableViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Set the version from Info.plist
  NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
  
  NSMutableArray<NSString*>* version_flags = [[NSMutableArray alloc] init];
#ifdef DEBUG
  [version_flags addObject:@"Debug"];
#endif
#ifdef PATREON
  [version_flags addObject:@"Patreon"];
#endif
#ifdef NONJAILBROKEN
  [version_flags addObject:@"NJB"];
#endif
  
  NSString* version_str = [NSString stringWithFormat:@"%@ (%@)", [info objectForKey:@"CFBundleShortVersionString"], [info objectForKey:@"CFBundleVersion"]];
  if ([version_flags count] > 0)
  {
    version_str = [version_str stringByAppendingString:[NSString stringWithFormat:@" (%@)", [version_flags componentsJoinedByString:@", "]]];
  }
  
  [self.m_version_label setText:version_str];
  
  [self.m_core_label setText:CppToFoundationString(GetBuildType())];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.tableView reloadData];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  
  if (indexPath.section == 0)
  {
    NSString* url;
    if (indexPath.row == 3)
    {
      url = @"https://oatmealdome.me/dolphinios/";
    }
    else if (indexPath.row == 4)
    {
      url = @"https://patreon.com/oatmealdome";
    }
    else
    {
      return;
    }
    
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
  }
  else if (indexPath.section == 2) // Quit button
  {
    [AppDelegate Shutdown];
  }
}

- (CGFloat)tableView:(UITableView*)tableView heightForRowAtIndexPath:(NSIndexPath*)indexPath
{
  CGFloat real_height = [super tableView:tableView heightForRowAtIndexPath:indexPath];
  if (indexPath.section == 2)
  {
#if !defined(NONJAILBROKEN) || DEBUG
    return CGFLOAT_MIN;
#else
    return HasJitWithPTrace() ? real_height : CGFLOAT_MIN;
#endif
  }
  
  return real_height;
}

- (IBAction)UnwindToSettings:(UIStoryboardSegue*)segue {}

@end
