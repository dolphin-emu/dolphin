// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ControllerDsuClientViewController : UITableViewController <UITextFieldDelegate>

@property (weak, nonatomic) IBOutlet UISwitch* m_enabled_switch;
@property (weak, nonatomic) IBOutlet UITextField* m_ip_field;
@property (weak, nonatomic) IBOutlet UITextField* m_port_field;

@end

NS_ASSUME_NONNULL_END
