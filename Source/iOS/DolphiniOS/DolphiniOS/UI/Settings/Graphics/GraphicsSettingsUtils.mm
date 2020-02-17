// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "GraphicsSettingsUtils.h"

#import <UIKit/UIKit.h>

@implementation GraphicsSettingsUtils

+ (void)SetInitialForBoolSetting:(const Config::ConfigInfo<bool>&)setting isInverted:(bool)inverted forSwitch:(UISwitch*)sw label:(UILabel*)label
{
  [sw setOn:Config::Get(setting) ^ inverted];
  
  if (Config::GetActiveLayerForConfig(setting) != Config::LayerType::Base)
  {
    [label setFont:[UIFont boldSystemFontOfSize:[[label font] pointSize]]];
  }
  else
  {
    [label setFont:[UIFont systemFontOfSize:[[label font] pointSize]]];
  }
}

+ (void)SetInitialForTransitionCell:(const Config::ConfigInfo<int>&)setting forLabel:(UILabel*)label
{
  if (Config::GetActiveLayerForConfig(setting) != Config::LayerType::Base)
  {
    [label setFont:[UIFont boldSystemFontOfSize:[[label font] pointSize]]];
  }
  else
  {
    [label setFont:[UIFont systemFontOfSize:[[label font] pointSize]]];
  }
}

+ (void)ShowInfoAlertForLocalizable:(NSString*)localizable onController:(UIViewController*)target_controller
{
  UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:DOLocalizedString(localizable) preferredStyle:UIAlertControllerStyleAlert];
  
  [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
  
  [target_controller presentViewController:controller animated:true completion:nil];
}

@end
