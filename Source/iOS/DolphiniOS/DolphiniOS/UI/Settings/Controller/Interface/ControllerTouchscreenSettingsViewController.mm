// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerTouchscreenSettingsViewController.h"

#import "ControllerSettingsUtils.h"

#import "Core/Core.h"
#import "Core/HW/Wiimote.h"
#import "Core/HW/WiimoteEmu/WiimoteEmu.h"

#import "DolphiniOS-Swift.h"

#import "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"
#import "InputCommon/ControllerInterface/ControllerInterface.h"
#import "InputCommon/InputConfig.h"

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
  
  if (self.traitCollection.forceTouchCapability == UIForceTouchCapabilityAvailable)
  {
    [self.m_3d_touch_switch setEnabled:true];
    [self.m_3d_touch_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"3d_touch_analog_triggers_enabled"]];
  }
  else
  {
    [self.m_3d_touch_switch setEnabled:false];
    [self.m_3d_touch_switch setOn:false];
  }
  
  [self.m_motion_switch setOn:[[TCDeviceMotion shared] getMotionMode] == 0];
  
  // The initial state of the motion pointer switch is ON when at least one
  // touch controller has IMU point enabled
  bool pointer_switch_on = false;
  
  for (int i = 0; i < 4; i++)
  {
    WiimoteSource wiimote_source = WiimoteCommon::GetSource(i);
    if (wiimote_source != WiimoteSource::Emulated)
    {
      continue;
    }
    
    ControllerEmu::EmulatedController* controller = Wiimote::GetConfig()->GetController(i);
    
    if ([ControllerSettingsUtils IsControllerConnectedToTouchscreen:controller])
    {
      ControllerEmu::IMUCursor* cursor = static_cast<ControllerEmu::IMUCursor*>(Wiimote::GetWiimoteGroup(i, WiimoteEmu::WiimoteGroup::IMUPoint));
      if (cursor->enabled)
      {
        pointer_switch_on = true;
        break;
      }
    }
  }
  
  [self.m_motion_pointer_switch setOn:pointer_switch_on];
  
  [self.m_recentering_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"tscontroller_stick_recentering"]];
  
  [self.m_button_opacity_slider setValue:[[NSUserDefaults standardUserDefaults] floatForKey:@"pad_opacity_value"]];
}

- (void)ShowEmulationWarning
{
  UIAlertController* alert_controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Note") message:NSLocalizedString(@"This change will take effect after emulation is stopped.", nil) preferredStyle:UIAlertControllerStyleAlert];
  [alert_controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
  
  [self presentViewController:alert_controller animated:true completion:nil];
}

- (IBAction)HapticChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:self.m_haptic_switch.on forKey:@"haptic_feedback_enabled"];
}

- (IBAction)ThreeDTouchChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:self.m_3d_touch_switch.on forKey:@"3d_touch_analog_triggers_enabled"];
  
  if (Core::IsRunning())
  {
    [self ShowEmulationWarning];
  }
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

- (IBAction)MotionPointerChanged:(id)sender
{
  bool should_enable = [self.m_motion_pointer_switch isOn];
  
  InputConfig* config = Wiimote::GetConfig();
  for (int i = 0; i < 4; i++)
  {
    WiimoteSource wiimote_source = WiimoteCommon::GetSource(i);
    if (wiimote_source != WiimoteSource::Emulated)
    {
      continue;
    }
    
    ControllerEmu::EmulatedController* controller = config->GetController(i);
    
    if ([ControllerSettingsUtils IsControllerConnectedToTouchscreen:controller])
    {
      ControllerEmu::IMUCursor* cursor = static_cast<ControllerEmu::IMUCursor*>(Wiimote::GetWiimoteGroup(i, WiimoteEmu::WiimoteGroup::IMUPoint));
      cursor->enabled = should_enable;
    }
  }
  
  config->SaveConfig();
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
