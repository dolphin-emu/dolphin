// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AnalyticsNoticeViewController.h"

#import <AppCenterAnalytics/MSAnalytics.h>

#import "Core/Analytics.h"
#import "Core/ConfigManager.h"

@interface AnalyticsNoticeViewController ()

@end

@implementation AnalyticsNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)HandleResponse:(bool)response
{
  SConfig::GetInstance().m_analytics_permission_asked = true;
  SConfig::GetInstance().m_analytics_enabled = response;
  [MSAnalytics setEnabled:response];
  [self.navigationController popViewControllerAnimated:true];
}

- (IBAction)OptInPressed:(id)sender
{
  [self HandleResponse:true];
}

- (IBAction)OptOutPressed:(id)sender
{
  [self HandleResponse:false];
}

@end
