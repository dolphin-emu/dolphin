// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UnofficialBuildNoticeViewController.h"

@interface UnofficialBuildNoticeViewController ()

@end

@implementation UnofficialBuildNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (IBAction)OKPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

@end
