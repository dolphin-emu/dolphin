// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ConfigSoundViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UISlider* m_volume_slider;
@property (weak, nonatomic) IBOutlet UILabel* m_volume_perecentage_label;
@property (weak, nonatomic) IBOutlet UISwitch* m_stretching_switch;
@property (weak, nonatomic) IBOutlet UISlider* m_buffer_size_slider;
@property (weak, nonatomic) IBOutlet UILabel* m_buffer_size_label;

@end

NS_ASSUME_NONNULL_END
