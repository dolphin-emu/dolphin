// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ConfigInterfaceViewController.h"

@interface ConfigInterfaceViewController ()

@end

@implementation ConfigInterfaceViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_top_bar_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"always_show_top_bar"]];
  [self.m_center_image_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"do_not_raise_rendering_view"]];
}

- (IBAction)ShowTopBarChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_top_bar_switch isOn] forKey:@"always_show_top_bar"];
}

- (IBAction)CenterImageChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_center_image_switch isOn] forKey:@"do_not_raise_rendering_view"];
}

@end
