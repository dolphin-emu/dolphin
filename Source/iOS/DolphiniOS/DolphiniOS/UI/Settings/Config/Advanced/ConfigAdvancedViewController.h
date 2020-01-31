// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ConfigAdvancedViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UISwitch* m_mmu_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_overclock_switch;
@property (weak, nonatomic) IBOutlet UISlider* m_overclock_slider;
@property (weak, nonatomic) IBOutlet UILabel *m_overclock_label;

@end

NS_ASSUME_NONNULL_END
