// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SettingsTableViewController.h"

@interface SettingsTableViewController ()

@end

@implementation SettingsTableViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Set the version from Info.plist
  NSDictionary* info = [[NSBundle mainBundle] infoDictionary];
  NSString* version_str = [info objectForKey:@"CFBundleShortVersionString"];
  version_str = [NSString stringWithFormat:@"%@ (%@)", [info objectForKey:@"CFBundleShortVersionString"], [info objectForKey:@"CFBundleVersion"]];
#ifdef DEBUG
  version_str = [version_str stringByAppendingString:@" (Debug)"];
#endif
#ifdef PATREON
  version_str = [version_str stringByAppendingString:@" (Patreon)"];
#endif
  [self.m_version_label setText:version_str];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  
  if (indexPath.section == 0)
  {
    NSString* url;
    if (indexPath.row == 2)
    {
      url = @"https://oatmealdome.me/dolphinios/";
    }
    else if (indexPath.row == 3)
    {
      url = @"https://patreon.com/oatmealdome";
    }
    else
    {
      return;
    }
    
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
  }
}

- (IBAction)UnwindToSettings:(UIStoryboardSegue*)segue {}

@end
