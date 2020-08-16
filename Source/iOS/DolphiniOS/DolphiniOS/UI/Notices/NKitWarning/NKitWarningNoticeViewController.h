// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NKitWarningNoticeDelegate.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface NKitWarningNoticeViewController : UIViewController

@property (weak, nonatomic) id <NKitWarningNoticeDelegate> delegate;

@property (weak, nonatomic) IBOutlet UISwitch* m_dont_show_switch;
@property (weak, nonatomic) IBOutlet UIButton* m_cancel_button;
@property (weak, nonatomic) IBOutlet UIButton* m_continue_button;

@end

NS_ASSUME_NONNULL_END
