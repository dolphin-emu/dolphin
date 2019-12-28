// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/Control/Control.h"
#import "InputCommon/ControllerEmu/ControllerEmu.h"
#import "InputCommon/ControlReference/ControlReference.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerGroupButtonCell : UITableViewCell

@property(nonatomic) ControllerEmu::EmulatedController* m_controller;
@property(nonatomic) ControlReference* m_reference;
@property(nonatomic) bool m_is_wii;

@property (weak, nonatomic) IBOutlet UILabel* m_button_name;
@property (weak, nonatomic) IBOutlet UILabel* m_user_setting_label;

- (void)SetupCellWithControl:(std::unique_ptr<ControllerEmu::Control>&)control controller:(ControllerEmu::EmulatedController*)controller;
- (void)ResetToDefault;

@end

NS_ASSUME_NONNULL_END
