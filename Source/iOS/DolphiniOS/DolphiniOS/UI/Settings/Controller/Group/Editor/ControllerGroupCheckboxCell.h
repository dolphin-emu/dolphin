// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/Setting/NumericSetting.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerGroupCheckboxCell : UITableViewCell

@property(nonatomic) ControllerEmu::NumericSetting<bool>* m_setting;

@property(weak, nonatomic) IBOutlet UILabel* m_name_label;
@property(weak, nonatomic) IBOutlet UISwitch* m_switch;

- (void)SetupCellWithSetting:(ControllerEmu::NumericSetting<bool>*)setting;

@end

NS_ASSUME_NONNULL_END
