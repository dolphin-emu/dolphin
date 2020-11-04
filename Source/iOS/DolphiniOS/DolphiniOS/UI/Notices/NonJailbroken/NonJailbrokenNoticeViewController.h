// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface NonJailbrokenNoticeViewController : UIViewController

@property (weak, nonatomic) IBOutlet UILabel* m_quit_label;
@property (weak, nonatomic) IBOutlet UILabel* m_bug_label;
@property (weak, nonatomic) IBOutlet UIButton* m_ok_button;

@end

NS_ASSUME_NONNULL_END
