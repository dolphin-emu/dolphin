// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "KilledBuildNoticeViewController.h"

@interface KilledBuildNoticeViewController ()

@end

@implementation KilledBuildNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  // Do any additional setup after loading the view.
}

- (IBAction)UpdateNowPressed:(id)sender
{
  NSString* url = [[NSUserDefaults standardUserDefaults] stringForKey:@"killed_url"];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
}

@end
