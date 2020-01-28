// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "InvalidCpuCoreNoticeViewController.h"

@interface InvalidCpuCoreNoticeViewController ()

@end

@implementation InvalidCpuCoreNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}
- (IBAction)OKPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

- (IBAction)MoreInfoPressed:(id)sender
{
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"https://dolphin-emu.org/docs/faq/#couldnt-dolphin-use-more-my-cpu-cores-go-faster"] options:@{} completionHandler:nil];
}

@end
