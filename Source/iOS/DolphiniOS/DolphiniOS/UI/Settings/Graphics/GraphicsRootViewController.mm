// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsRootViewController.h"

#import "VideoCommon/VideoBackendBase.h"

@interface GraphicsRootViewController ()

@end

@implementation GraphicsRootViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  VideoBackendBase::PopulateBackendInfo();
}

@end
