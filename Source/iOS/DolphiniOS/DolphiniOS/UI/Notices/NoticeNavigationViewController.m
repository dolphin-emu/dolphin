// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NoticeNavigationViewController.h"

@interface NoticeNavigationViewController ()

@end

@implementation NoticeNavigationViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  // Do any additional setup after loading the view.
}

- (UIViewController*)popViewControllerAnimated:(BOOL)animated
{
  if (self.viewControllers.count == 1)
  {
    // Dismiss if the last one is being popped
    [self dismissViewControllerAnimated:true completion:nil];
  }
  
  return [super popViewControllerAnimated:animated];
}

@end
