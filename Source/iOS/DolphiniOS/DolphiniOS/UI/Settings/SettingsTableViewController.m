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
  [self.m_version_label setText:[info objectForKey:@"CFBundleVersion"]];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1)
  {
    NSString* url;
    if (indexPath.row == 0)
    {
      url = @"https://oatmealdome.me";
    }
    else
    {
      url = @"https://patreon.com/oatmealdome";
    }
    
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
