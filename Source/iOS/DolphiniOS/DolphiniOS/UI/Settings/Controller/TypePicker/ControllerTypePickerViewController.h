// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ControllerTypePickerViewController : UITableViewController

@property(nonatomic) int m_port;
@property(nonatomic) bool m_is_wii;
@property(nonatomic) NSInteger m_last_selected;

@end

NS_ASSUME_NONNULL_END
