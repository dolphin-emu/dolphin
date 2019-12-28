// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/Setting/NumericSetting.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerGroupDoubleCell : UITableViewCell <UITextFieldDelegate>

@property(nonatomic) ControllerEmu::NumericSetting<double>* m_setting;

@property (weak, nonatomic) IBOutlet UITextField *m_text_field;
@property (weak, nonatomic) IBOutlet UILabel *m_name_label;

- (void)SetupCellWithSetting:(ControllerEmu::NumericSetting<double>*)setting;

@end

NS_ASSUME_NONNULL_END
