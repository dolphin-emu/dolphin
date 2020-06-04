// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "MainTabBarController.h"

@interface MainTabBarController ()

@end

@implementation MainTabBarController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Load SF Symbols on iOS 13 only, otherwise revert to legacy PNGs
  if (@available(iOS 13, *))
  {
    self.tabBar.items[0].image = [UIImage imageNamed:@"SF_gamecontroller"];
    self.tabBar.items[1].image = [UIImage imageNamed:@"SF_gear"];
  }
  else
  {
    self.tabBar.items[0].image = [UIImage imageNamed:@"gamecontroller_legacy.png"];
    self.tabBar.items[1].image = [UIImage imageNamed:@"gear_legacy.png"];
  }
}

@end
