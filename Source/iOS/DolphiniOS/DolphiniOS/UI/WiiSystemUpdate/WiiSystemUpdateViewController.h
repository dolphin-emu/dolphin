// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface WiiSystemUpdateViewController : UIViewController

@property(nonatomic) bool m_is_online;
@property(nonatomic) NSString* m_source;
@property(atomic) bool m_is_cancelled;

@property (weak, nonatomic) IBOutlet UILabel* m_progress_label;
@property (weak, nonatomic) IBOutlet UIProgressView* m_progress_bar;
@property (weak, nonatomic) IBOutlet UIButton *m_cancel_button;

@end

NS_ASSUME_NONNULL_END
