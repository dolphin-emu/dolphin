// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareVerifyResultsHashCell : UITableViewCell

@property (weak, nonatomic) IBOutlet UILabel* m_hash_function_label;
@property (weak, nonatomic) IBOutlet UILabel* m_hash_label;

@end

NS_ASSUME_NONNULL_END
