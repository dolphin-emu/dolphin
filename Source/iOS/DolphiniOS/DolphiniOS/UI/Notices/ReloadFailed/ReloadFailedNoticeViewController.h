// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSUInteger, DOLReloadFailedReason)
{
  DOLReloadFailedReasonNone,
  DOLReloadFailedReasonOld,
  DOLReloadFailedReasonFileGone,
  DOLReloadFailedReasonSilent
};

NS_ASSUME_NONNULL_BEGIN

@interface ReloadFailedNoticeViewController : UIViewController

@property (nonatomic) DOLReloadFailedReason m_reason;
@property (weak, nonatomic) IBOutlet UILabel* m_game_name_label;
@property (weak, nonatomic) IBOutlet UILabel* m_reason_label;

@end

NS_ASSUME_NONNULL_END
