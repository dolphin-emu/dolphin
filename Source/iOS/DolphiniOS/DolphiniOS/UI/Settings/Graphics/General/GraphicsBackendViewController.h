// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import <string>

#import <vector>

NS_ASSUME_NONNULL_BEGIN

@interface GraphicsBackendViewController : UITableViewController
{
  std::vector<std::string> m_backends;
}

@property (nonatomic) NSInteger m_last_selected;

@end

NS_ASSUME_NONNULL_END
