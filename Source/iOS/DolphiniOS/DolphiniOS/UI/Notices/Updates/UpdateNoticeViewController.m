// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UpdateNoticeViewController.h"

@interface UpdateNoticeViewController ()

@end

@implementation UpdateNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Set the version label
  NSString* message = [NSString stringWithFormat:@"DolphiniOS version %@ is now available.", self.m_update_json[@"version"]];
  [self.m_version_label setText:message];
  
  // Set the changes label
  [self.m_changes_label setText:self.m_update_json[@"changes"]];
  
  // TODO: save states incompatibility label
}

- (IBAction)UpdateNowTouched:(id)sender
{
  NSURL* url = [NSURL URLWithString:@"cydia://url/https://cydia.saurik.com/api/share#?source=http://cydia.oatmealdome.me/&package=me.oatmealdome.dolphinios"];
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

- (IBAction)SeeChangesTouched:(id)sender
{
  NSURL* url = [NSURL URLWithString:self.m_update_json[@"url"]];
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

- (IBAction)NotNowTouched:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

@end
