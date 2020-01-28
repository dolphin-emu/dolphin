// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "EmulationViewController.h"

NS_ASSUME_NONNULL_BEGIN

@interface InGamePortSelectorViewController : UITableViewController

@property(nonatomic) EmulationViewController* m_emulation_controller;
@property(nonatomic) int m_last_selected;

@end

NS_ASSUME_NONNULL_END
