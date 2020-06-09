// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwarePropertiesViewController : UITableViewController

@property(nonatomic) UICommon::GameFile* m_game_file;

@end

NS_ASSUME_NONNULL_END
