// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DonationNoticeViewController.h"

@interface DonationNoticeViewController ()

@end

@implementation DonationNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Unhide the don't show button if necessary
  NSInteger launch_times = [[NSUserDefaults standardUserDefaults] integerForKey:@"launch_times"];
  if (launch_times > 15)
  {
    [self.m_dont_show_button setHidden:false];
  }
}

- (IBAction)DonatePressed:(id)sender
{
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"https://www.patreon.com/oatmealdome"] options:@{} completionHandler:nil];
  [self.navigationController popViewControllerAnimated:true];
}

- (IBAction)NotNowPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

- (IBAction)DontShowAgainPressed:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:true forKey:@"suppress_donation_message"];
  [self.navigationController popViewControllerAnimated:true];
}

@end
