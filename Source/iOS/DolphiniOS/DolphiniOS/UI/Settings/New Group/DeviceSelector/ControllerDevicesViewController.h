// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <string>
#import <vector>

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/ControllerEmu.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerDevicesViewController : UITableViewController
{
  std::vector<std::string> m_devices;
}

@property(nonatomic) NSInteger m_last_selected;
@property(nonatomic) ControllerEmu::EmulatedController* m_controller;
@property(nonatomic) int m_port;
@property(nonatomic) bool m_is_wii;

@end

NS_ASSUME_NONNULL_END
