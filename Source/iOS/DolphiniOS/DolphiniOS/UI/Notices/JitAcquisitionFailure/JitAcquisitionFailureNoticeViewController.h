// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface JitAcquisitionFailureNoticeViewController : UIViewController

@property (weak, nonatomic) IBOutlet UILabel* m_error_label;
@property (weak, nonatomic) IBOutlet UILabel* m_specific_label;

@end

NS_ASSUME_NONNULL_END
