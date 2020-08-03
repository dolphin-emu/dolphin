// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Core/ActionReplay.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareActionReplayEditViewController : UITableViewController <UITextViewDelegate>

@property (nonatomic) ActionReplay::ARCode* m_ar_code;
@property (nonatomic) bool m_should_be_pushed_back;

@property (weak, nonatomic) IBOutlet UITextField* m_name_field;
@property (weak, nonatomic) IBOutlet UITextView* m_code_view;

@end

NS_ASSUME_NONNULL_END
