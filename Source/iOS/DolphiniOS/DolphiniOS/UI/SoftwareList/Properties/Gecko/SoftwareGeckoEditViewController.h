// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Core/GeckoCode.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareGeckoEditViewController : UITableViewController

@property (nonatomic) Gecko::GeckoCode* m_gecko_code;
@property (nonatomic) bool m_should_be_pushed_back;

@property (weak, nonatomic) IBOutlet UITextField* m_name_field;
@property (weak, nonatomic) IBOutlet UITextField *m_creator_field;
@property (weak, nonatomic) IBOutlet UITextView* m_code_view;
@property (weak, nonatomic) IBOutlet UITextView* m_notes_view;

@end

NS_ASSUME_NONNULL_END
