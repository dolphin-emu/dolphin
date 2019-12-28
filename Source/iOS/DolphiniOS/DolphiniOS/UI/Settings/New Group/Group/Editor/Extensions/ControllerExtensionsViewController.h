// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/ControlGroup/Attachments.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerExtensionsViewController : UITableViewController

@property(nonatomic) ControllerEmu::Attachments* m_attachments;

@end

NS_ASSUME_NONNULL_END
