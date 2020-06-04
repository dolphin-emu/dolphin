// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "UICommon/GameFileCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface InGameChangeDiscViewController : UITableViewController

@property (nonatomic) UICommon::GameFileCache* m_cache;
@property (nonatomic) NSArray<NSNumber*>* m_valid_indices;

@end

NS_ASSUME_NONNULL_END
