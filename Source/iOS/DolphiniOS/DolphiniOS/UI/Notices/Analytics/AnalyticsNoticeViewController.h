// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface AnalyticsNoticeViewController : UIViewController

@property (weak, nonatomic) IBOutlet UIButton* m_opt_in_button;
@property (weak, nonatomic) IBOutlet UIButton* m_opt_out_button;

@end

NS_ASSUME_NONNULL_END
