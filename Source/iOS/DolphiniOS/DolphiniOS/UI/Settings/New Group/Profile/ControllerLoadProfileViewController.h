// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import <vector>

#import "InputCommon/ControllerEmu/ControllerEmu.h"
#import "InputCommon/InputConfig.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerLoadProfileViewController : UITableViewController
{
  std::vector<std::string> m_profiles;
}

@property(nonatomic) ControllerEmu::EmulatedController* m_controller;
@property(nonatomic) InputConfig* m_config;
@property(nonatomic) bool m_is_wii;

@end

NS_ASSUME_NONNULL_END
