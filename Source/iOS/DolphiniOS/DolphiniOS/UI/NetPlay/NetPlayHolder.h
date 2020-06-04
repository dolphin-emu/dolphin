// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NetPlayHolder : NSObject

+ (NetPlayHolder*)sharedNetPlayHolder;

@end

NS_ASSUME_NONNULL_END
