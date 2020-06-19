// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Core/ActionReplay.h"

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

#import <vector>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareActionReplayViewController : UITableViewController
{
  std::vector<ActionReplay::ARCode> m_ar_codes;
}

@property(nonatomic) UICommon::GameFile* m_game_file;

@end

NS_ASSUME_NONNULL_END
