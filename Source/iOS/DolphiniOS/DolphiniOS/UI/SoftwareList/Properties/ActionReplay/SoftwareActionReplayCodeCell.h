// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareActionReplayCodeCell : UITableViewCell

@property (weak, nonatomic) IBOutlet UILabel* m_name_label;
@property (weak, nonatomic) IBOutlet UISwitch* m_enabled_switch;

@end

NS_ASSUME_NONNULL_END
