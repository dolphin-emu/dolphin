// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ConfigInterfaceViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UISwitch* m_confirm_stop_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_panic_handlers_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_osd_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_top_bar_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_center_image_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_status_bar_switch;

@end

NS_ASSUME_NONNULL_END
