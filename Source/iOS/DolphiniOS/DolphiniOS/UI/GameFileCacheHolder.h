// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#import "UICommon/GameFileCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface GameFileCacheHolder : NSObject

@property(nonatomic) UICommon::GameFileCache* m_cache;
@property(atomic) bool m_is_busy;

+ (GameFileCacheHolder*)sharedInstance;

- (void)scanSoftwareFolder;
- (void)fetchMetadataWithCompletionHandler:(nullable void (^)())completion_handler;
- (void)completeRescanWithCompletionHandler:(nullable void (^)())completion_handler;

@end

NS_ASSUME_NONNULL_END
