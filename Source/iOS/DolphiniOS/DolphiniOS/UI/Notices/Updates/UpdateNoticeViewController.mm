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
  
  [self.m_ok_button setHidden:true];
  
  [self.m_save_states_warning_label setHidden:(NSInteger)self.m_update_json[@"state_version"] != State::GetVersion()];
}

- (IBAction)UpdateNowTouched:(id)sender
{
  NSURL* url = [NSURL URLWithString:self.m_update_json[@"install_url"]];
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

- (IBAction)SeeChangesTouched:(id)sender
{
  NSURL* url = [NSURL URLWithString:self.m_update_json[@"changes_url"]];
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

- (IBAction)NotNowTouched:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

@end
