// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareVerifyStartViewController.h"

#import "SoftwareVerifyComputeViewController.h"

@interface SoftwareVerifyStartViewController ()

@end

@implementation SoftwareVerifyStartViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(bool)animated
{
  [super viewWillAppear:animated];
  
  [[UIApplication sharedApplication] setIdleTimerDisabled:false];
  
  self.navigationItem.hidesBackButton = false;
  self.navigationController.interactivePopGestureRecognizer.enabled = true;
  
  if (@available(iOS 13, *))
  {
    self.navigationController.modalInPresentation = false;
  }
  
  [self.navigationController setNavigationBarHidden:false];
}

- (IBAction)Crc32Changed:(id)sender
{
  [self UpdateRedumpSwitch];
}

- (IBAction)Sha1Changed:(id)sender
{
  [self UpdateRedumpSwitch];
}

- (void)UpdateRedumpSwitch
{
  bool should_be_enabled = [self.m_md5_switch isOn] || [self.m_sha1_switch isOn];
  [self.m_redump_switch setEnabled:should_be_enabled];
  
  if (!should_be_enabled)
  {
    [self.m_redump_switch setOn:false];
  }
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_compute"])
  {
    SoftwareVerifyComputeViewController* controller = (SoftwareVerifyComputeViewController*)segue.destinationViewController;
    controller.m_game_file = self.m_game_file;
    controller.m_calc_crc32 = [self.m_crc32_switch isOn];
    controller.m_calc_md5 = [self.m_md5_switch isOn];
    controller.m_calc_sha1 = [self.m_sha1_switch isOn];
    controller.m_verify_redump = [self.m_redump_switch isOn];
  }
}

- (IBAction)unwindToVerifyStart:(UIStoryboardSegue*)segue {}

@end
