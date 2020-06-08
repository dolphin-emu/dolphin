// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SettingsTableViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UILabel *m_version_label;
@property (weak, nonatomic) IBOutlet UILabel *m_core_label;

@end

NS_ASSUME_NONNULL_END
