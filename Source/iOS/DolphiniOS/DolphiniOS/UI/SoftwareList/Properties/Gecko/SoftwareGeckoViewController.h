// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Core/GeckoCode.h"

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

#import <vector>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareGeckoViewController : UITableViewController
{
  std::vector<Gecko::GeckoCode> m_gecko_codes;
}

@property(nonatomic) UICommon::GameFile* m_game_file;

@end

NS_ASSUME_NONNULL_END
