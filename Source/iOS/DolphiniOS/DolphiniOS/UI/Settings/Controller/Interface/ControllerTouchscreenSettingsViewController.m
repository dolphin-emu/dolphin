// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerTouchscreenSettingsViewController.h"

#import "DolphiniOS-Swift.h"

@interface ControllerTouchscreenSettingsViewController ()

@end

@implementation ControllerTouchscreenSettingsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_haptic_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"haptic_feedback_enabled"]];
  
  [self.m_motion_switch setOn:[[TCDeviceMotion shared] getMotionMode] == 0];
  
  [self.m_recentering_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"tscontroller_stick_recentering"]];
  
  [self.m_button_opacity_slider setValue:[[NSUserDefaults standardUserDefaults] floatForKey:@"pad_opacity_value"]];
}

- (IBAction)HapticChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:self.m_haptic_switch.on forKey:@"haptic_feedback_enabled"];
}

- (IBAction)MotionChanged:(id)sender
{
  // Done this way in case a new motion mode is introduced
  int mode = -1;
  if (self.m_motion_switch.on)
  {
    mode = 0;
  }
  else
  {
    mode = 1;
  }
  
  [[TCDeviceMotion shared] setMotionMode:mode];
}

- (IBAction)RecenteringChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_recentering_switch isOn] forKey:@"tscontroller_stick_recentering"];
}

- (IBAction)OpacityChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setFloat:self.m_button_opacity_slider.value forKey:@"pad_opacity_value"];
}

@end
