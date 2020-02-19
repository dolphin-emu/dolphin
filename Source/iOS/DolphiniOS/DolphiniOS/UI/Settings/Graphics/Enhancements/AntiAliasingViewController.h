// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <string>

#import <UIKit/UIKit.h>

#import <vector>

NS_ASSUME_NONNULL_BEGIN

@interface AntiAliasingViewController : UITableViewController
{
  int m_msaa_modes;
  std::vector<std::string> m_aa_modes;
}

@property (nonatomic) NSInteger m_last_selected;

@end

NS_ASSUME_NONNULL_END
