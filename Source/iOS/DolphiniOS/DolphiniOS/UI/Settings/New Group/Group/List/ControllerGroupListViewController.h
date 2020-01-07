// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <vector>

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#import "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#import "InputCommon/ControllerEmu/ControllerEmu.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerGroupListViewController : UITableViewController
{
  std::vector<ControllerEmu::ControlGroup*> m_controller_groups;
  std::vector<ControllerEmu::ControlGroup*> m_normal_motion_groups;
  std::vector<ControllerEmu::ControlGroup*> m_imu_motion_groups;
  std::vector<ControllerEmu::ControlGroup*> m_extension_groups;
}

@property(nonatomic) int m_port;
@property(nonatomic) bool m_is_wii;
@property(nonatomic) ControllerEmu::EmulatedController* m_controller;
@property(nonatomic) ControllerEmu::Attachments* m_attachments;

@end

NS_ASSUME_NONNULL_END
