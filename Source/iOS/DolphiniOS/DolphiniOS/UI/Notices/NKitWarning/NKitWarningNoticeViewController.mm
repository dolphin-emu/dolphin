// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NKitWarningNoticeViewController.h"

#import "EmulationViewController.h"

#import "Common/Config/Config.h"

#import "Core/Config/MainSettings.h"

@interface NKitWarningNoticeViewController ()

@end

@implementation NKitWarningNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  if (@available(iOS 13, *))
  {
    [self setModalInPresentation:true];
  }
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  [self.m_dont_show_switch setOn:Config::GetBase(Config::MAIN_SKIP_NKIT_WARNING)];
}

- (IBAction)CancelPressed:(id)sender
{
  [self.delegate WarningDismissedWithResult:false sender:self];
}

- (IBAction)ContinuePressed:(id)sender
{
  Config::SetBase(Config::MAIN_SKIP_NKIT_WARNING, [self.m_dont_show_switch isOn]);
  
  [self.delegate WarningDismissedWithResult:true sender:self];
}

@end
