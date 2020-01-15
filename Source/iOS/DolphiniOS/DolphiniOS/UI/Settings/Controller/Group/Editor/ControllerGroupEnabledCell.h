// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerGroupEnabledCell : UITableViewCell

@property(nonatomic) ControllerEmu::ControlGroup* m_group;

@property(weak, nonatomic) IBOutlet UISwitch* m_switch;

- (void)SetupCellWithGroup:(ControllerEmu::ControlGroup*)group;

@end

NS_ASSUME_NONNULL_END
