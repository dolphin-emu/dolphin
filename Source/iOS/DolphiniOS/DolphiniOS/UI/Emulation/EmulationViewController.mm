// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "EmulationViewController.h"

#import "Core/ConfigManager.h"
#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"
#import "Core/HW/WiimoteEmu/Extension/Classic.h"
#import "Core/HW/WiimoteEmu/WiimoteEmu.h"
#import "Core/HW/SI/SI.h"
#import "Core/HW/SI/SI_Device.h"

#import "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#import "InputCommon/ControllerEmu/ControllerEmu.h"
#import "InputCommon/InputConfig.h"

#import "MainiOS.h"

#import "VideoCommon/RenderBase.h"

@interface EmulationViewController ()

@end

@implementation EmulationViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Setup renderer view
  if (SConfig::GetInstance().m_strVideoBackend == "Vulkan")
  {
    self.m_renderer_view = self.m_metal_view;
  }
  else
  {
    self.m_renderer_view = self.m_eagl_view;
  }
  
  [self.m_renderer_view setHidden:false];
  
  // Set default port
  [self PopulatePortDictionary];
  
  if (self->m_controllers.size() != 0)
  {
    self.m_ts_active_port = self->m_controllers[0].first;
  }
  else
  {
    self.m_ts_active_port = -1;
  }
  
  [self ChangeVisibleTouchControllerToPort:self.m_ts_active_port];
  
  // Hide navigation bar
  [self.navigationController setNavigationBarHidden:true animated:true];
  
  [NSThread detachNewThreadSelector:@selector(StartEmulation) toTarget:self withObject:nil];
}

- (void)StartEmulation
{
  [MainiOS startEmulationWithFile:[NSString stringWithUTF8String:self.m_game_file->GetFilePath().c_str()] viewController:self view:self.m_renderer_view];
  
  [[TCDeviceMotion shared] stopMotionUpdates];
  
  dispatch_sync(dispatch_get_main_queue(), ^{
    [self performSegueWithIdentifier:@"toSoftwareTable" sender:nil];
  });
}

- (bool)prefersHomeIndicatorAutoHidden
{
  return true;
}

#pragma mark - Navigation bar

- (IBAction)topEdgeRecognized:(id)sender
{
  // Don't do anything if already visible
  if (![self.navigationController isNavigationBarHidden])
  {
    return;
  }
  
  [self UpdateNavigationBar:false];
  
  // Automatic hide after 5 seconds
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    [self UpdateNavigationBar:true];
  });
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
  return UIRectEdgeTop;
}

- (bool)prefersStatusBarHidden
{
  return [self.navigationController isNavigationBarHidden];
}

- (void)UpdateNavigationBar:(bool)hidden
{
  [self.navigationController setNavigationBarHidden:hidden animated:true];
  [self setNeedsStatusBarAppearanceUpdate];
  
  // Adjust the safe area insets
  UIEdgeInsets insets = self.additionalSafeAreaInsets;
  if (hidden)
  {
    insets.top = 0;
  }
  else
  {
    // The safe area should extend behind the navigation bar
    // This makes the bar "float" on top of the content
    insets.top = -(self.navigationController.navigationBar.bounds.size.height);
  }
  
  self.additionalSafeAreaInsets = insets;
}

#pragma mark - Screen rotation specific

- (void)viewDidLayoutSubviews
{
  [[TCDeviceMotion shared] statusBarOrientationChanged];
  [MainiOS windowResized];
}

- (void)UpdateWiiPointer
{
  if (g_renderer && [self.m_ts_active_view isKindOfClass:[TCWiiPad class]])
  {
    const MathUtil::Rectangle<int>& draw_rect = g_renderer->GetTargetRectangle();
    CGRect rect = CGRectMake(draw_rect.left, draw_rect.top, draw_rect.GetWidth(), draw_rect.GetHeight());
    
    dispatch_async(dispatch_get_main_queue(), ^{
      TCWiiPad* wii_pad = (TCWiiPad*)self.m_ts_active_view;
      [wii_pad recalculatePointerValuesWithNew_rect:rect game_aspect:g_renderer->CalculateDrawAspectRatio()];
    });
  }
}

#pragma mark - Stop button

- (IBAction)StopButtonPressed:(id)sender
{
  UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Stop Emulation" message:@"Do you really want to stop the emulation? All unsaved data will be lost." preferredStyle:UIAlertControllerStyleAlert];
  
  [alert addAction:[UIAlertAction actionWithTitle:@"No" style:UIAlertActionStyleDefault handler:nil]];
  
  [alert addAction:[UIAlertAction actionWithTitle:@"Yes" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action) {
    [MainiOS stopEmulation];
  }]];
  
  [self presentViewController:alert animated:true completion:nil];
}

#pragma mark - Touchscreen Controller Switcher

- (bool)IsControllerConnectedToTouchscreen:(ControllerEmu::EmulatedController*)controller
{
  const std::string android_str = "Android";
  std::string device = controller->GetDefaultDevice().ToString();
  
  return device.compare(0, android_str.size(), android_str) == 0;
}

- (void)PopulatePortDictionary
{
  self->m_controllers.clear();
  
  if (DiscIO::IsWii(self.m_game_file->GetPlatform()))
  {
    InputConfig* wii_input_config = Wiimote::GetConfig();
    for (int i = 0; i < 4; i++)
    {
      if (g_wiimote_sources[i] != 1) // not Emulated or not touchscreen
      {
        continue;
      }
      
      ControllerEmu::EmulatedController* controller = wii_input_config->GetController(i);
      if (![self IsControllerConnectedToTouchscreen:controller])
      {
        continue;
      }
      
      // TODO: A better way? This will break if more settings are added to the Options group.
      ControllerEmu::ControlGroup* group = Wiimote::GetWiimoteGroup(i, WiimoteEmu::WiimoteGroup::Options);
      ControllerEmu::NumericSetting<bool>* setting = static_cast<ControllerEmu::NumericSetting<bool>*>(group->numeric_settings[1].get());
      
      TCView* pad_view;
      if (setting->GetValue())
      {
        pad_view = self.m_wii_sideways_pad;
      }
      else
      {
        ControllerEmu::Attachments* attachments = static_cast<ControllerEmu::Attachments*>(Wiimote::GetWiimoteGroup(i, WiimoteEmu::WiimoteGroup::Attachments));
        
        switch (attachments->GetSelectedAttachment())
        {
          case WiimoteEmu::ExtensionNumber::CLASSIC:
            pad_view = self.m_wii_classic_pad;
            break;
          default:
            pad_view = self.m_wii_normal_pad;
            break;
        }
      }
      
      self->m_controllers.push_back(std::make_pair(i + 4, pad_view));
    }
  }
  
  InputConfig* gc_input_config = Pad::GetConfig();
  for (int i = 0; i < 4; i++)
  {
    ControllerEmu::EmulatedController* controller = gc_input_config->GetController(i);
    if (![self IsControllerConnectedToTouchscreen:controller])
    {
      continue;
    }
    
    SerialInterface::SIDevices device = SConfig::GetInstance().m_SIDevice[i];
    switch (device)
    {
      case SerialInterface::SIDEVICE_GC_CONTROLLER:
        self->m_controllers.push_back(std::make_pair(i, self.m_gc_pad));
      default:
        continue;
    }
  }
}

- (void)ChangeVisibleTouchControllerToPort:(int)port
{
  if (self.m_ts_active_view != nil)
  {
    [self.m_ts_active_view setHidden:true];
    [self.m_ts_active_view setUserInteractionEnabled:false];
  }
  
  if (port == -1)
  {
    [[TCDeviceMotion shared] stopMotionUpdates];
    self.m_ts_active_port = -1;
    self.m_ts_active_view = nil;
  }
  
  bool found_port = false;
  for (std::pair<int, TCView*> pair : self->m_controllers)
  {
    if (pair.first == port)
    {
      [pair.second setHidden:false];
      [pair.second setUserInteractionEnabled:true];
      pair.second.m_port = port;
      
      if (DiscIO::IsWii(self.m_game_file->GetPlatform()))
      {
        [[TCDeviceMotion shared] registerMotionHandlers];
        [[TCDeviceMotion shared] setPort:port];
      }
      else
      {
        [[TCDeviceMotion shared] stopMotionUpdates];
      }
      
      self.m_ts_active_port = port;
      self.m_ts_active_view = pair.second;
      
      found_port = true;
    }
  }
  
  if (!found_port)
  {
    [[TCDeviceMotion shared] stopMotionUpdates];
    self.m_ts_active_port = -1;
    self.m_ts_active_view = nil;
  }
}

#pragma mark - Top bar tutorial

- (void)viewWillAppear:(BOOL)animated
{
  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  
  if (![user_defaults boolForKey:@"seen_top_bar_swipe_down_notice"])
  {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Tutorial" message:@"To reveal the top bar, swipe down from the top of the screen.\n\nYou can change settings and stop the emulation from the top bar." preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:true completion:nil];
    
    [user_defaults setBool:true forKey:@"seen_top_bar_swipe_down_notice"];
  }
}

#pragma mark - Rewind segue

- (IBAction)unwindToEmulation:(UIStoryboardSegue*)segue {}

@end
