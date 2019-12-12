// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "UICommon/GameFileCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareTableViewController : UITableViewController <UIDocumentPickerDelegate>

@property(nonatomic) UICommon::GameFileCache* m_cache;
@property(nonatomic) bool m_cache_loaded;
@property(nonatomic) NSString* toLoadFile;
@property(nonatomic) bool toLoadIsWii;

@end

NS_ASSUME_NONNULL_END
