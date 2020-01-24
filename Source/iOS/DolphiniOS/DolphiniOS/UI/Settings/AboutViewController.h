// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface AboutViewController : UIViewController

@property (weak, nonatomic) IBOutlet UIScrollView* m_scroll_view;
@property (weak, nonatomic) IBOutlet UILabel* m_patrons_one;
@property (weak, nonatomic) IBOutlet UILabel* m_patrons_two;

@end

NS_ASSUME_NONNULL_END
