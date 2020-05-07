// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NonJailbrokenNoticeViewController.h"

@interface NonJailbrokenNoticeViewController ()

@end

@implementation NonJailbrokenNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (IBAction)OKPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

@end
