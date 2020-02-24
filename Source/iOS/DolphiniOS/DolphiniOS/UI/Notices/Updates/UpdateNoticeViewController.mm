// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UpdateNoticeViewController.h"

#import "Core/State.h"

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
  
#ifdef PATREON
  [self.m_see_changes_button setHidden:true];
#endif
  
  if ((NSInteger)self.m_update_json[@"state_version"] != State::GetVersion())
  {
    [self.m_save_states_warning_label setHidden:false];
  }
}

- (IBAction)UpdateNowTouched:(id)sender
{
#ifndef PATREON
  NSString* string = @"cydia://url/https://cydia.saurik.com/api/share#?source=http://cydia.oatmealdome.me/&package=me.oatmealdome.dolphinios";
#else
  NSString* string = @"cydia://url/https://cydia.saurik.com/api/share#?source=http://cydia.oatmealdome.me/&package=me.oatmealdome.dolphinios-patreon-beta";
#endif
  
  NSURL* url = [NSURL URLWithString:string];
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
